#include <stddef.h> // NULL
#include <stdint.h> // uint8_t, uint16_t

#include <lwip/udp.h> // struct udp_pcb, udp_new, udp_recv, udp_bind, udp_sendto
#include <lwip/ip_addr.h> // ip_addr_t, IP_ADDR_ANY
#include <lwip/arch.h> // LWIP_UNUSED_ARG
#include <lwip/pbuf.h> // struct pbuf, pbuf_free, pbuf_realloc, pbuf_alloc, PBUF_TRANSPORT, PBUF_POOL, pbuf_take, pbuf_take_at, pbuf_cat, pbuf_get_at, pbuf_put_at, pbuf_memcmp
#include <lwip/def.h> // lwip_ntohl, lwip_htons, lwip_htonl
#include <lwip/ip.h> // ip_current_dest_addr

#include "./debug.h" // DEBUG_PRINTF
#include "./dns.h" // captive_portal_init



// Default DNS port
#define DNS_PORT 53

// DNS header indexes
#define DNS_INDEX_FLAGS  2
#define DNS_INDEX_RCODE  3
#define DNS_INDEX_COUNTS 4

// DNS header lengths
#define DNS_LEN_HEADER 12
#define DNS_LEN_ANSWER 16
#define DNS_LEN_MAX    512
#define DNS_LEN_COUNTS (DNS_LEN_HEADER - DNS_INDEX_COUNTS)

// DNS flag masks
#define DNS_FLAGS_MASK_QR     0x80
#define DNS_FLAGS_MASK_OPCODE (0x08 | 0x10 | 0x20 | 0x40)

// DNS rcode response codes
#define DNS_RCODE_FORM_ERROR          1
#define DNS_RCODE_NON_EXISTENT_DOMAIN 3
#define DNS_RCODE_NOT_IMPLEMENTED     4

// DNS query classes
#define DNS_QCLASS_IN  0x01
#define DNS_QCLASS_ANY 0xff

// DNS query types
#define DNS_QTYPE_A    0x01
#define DNS_QTYPE_ANY  0xff



// Reuses pbuf for response
static void dns_prepare_send(struct pbuf* p, uint16_t len) {
	pbuf_put_at(p, DNS_INDEX_FLAGS, pbuf_get_at(p, DNS_INDEX_FLAGS) | DNS_FLAGS_MASK_QR);
	pbuf_realloc(p, len);
}

// Reuses pbuf for header parsing error response
static void dns_header_error(struct pbuf* p, uint8_t rcode) {
	pbuf_put_at(p, DNS_INDEX_RCODE, rcode);
	pbuf_take_at(p, (uint16_t[]){ 0x0000, 0x0000, 0x0000, 0x0000 }, DNS_LEN_COUNTS, DNS_INDEX_COUNTS);
	dns_prepare_send(p, DNS_LEN_HEADER);
}

// Responds to captive portal DNS queries
static void dns_recv_callback(void* arg, struct udp_pcb* pcb, struct pbuf* p, const ip_addr_t* addr, uint16_t port) {
	// Prevents compiler warnings
	LWIP_UNUSED_ARG(arg);

	// Ignores packet chunks that are too small or too large
	if (p->tot_len > DNS_LEN_MAX || p->tot_len < DNS_LEN_HEADER) {
		pbuf_free(p);
		return;
	}

	// Only supports query packets
	uint8_t flagsAndOpcode = pbuf_get_at(p, DNS_INDEX_FLAGS);
	if (flagsAndOpcode & (DNS_FLAGS_MASK_QR | DNS_FLAGS_MASK_OPCODE)) {
		// Ignores non query packets
		if (flagsAndOpcode & DNS_FLAGS_MASK_QR) {
			pbuf_free(p);
			return;
		}

		// Responds with error for non query opcode requests
		dns_header_error(p, DNS_RCODE_NOT_IMPLEMENTED);
		udp_sendto(pcb, p, addr, port);
		return;
	}

	// Only supports single question and ANCount, NSCount and ARCount should be clear for non answers
	if (pbuf_memcmp(p, DNS_INDEX_COUNTS, (uint16_t[]){ lwip_htons(0x0001), 0x0000, 0x0000, 0x0000 }, DNS_LEN_COUNTS)) {
		dns_header_error(p, DNS_RCODE_FORM_ERROR);
		udp_sendto(pcb, p, addr, port);
		return;
	}

	// Parses question label to get its type and class
	uint16_t questionIndex = DNS_LEN_HEADER;
	int labelLen;
	while ((labelLen = pbuf_get_at(p, questionIndex)) != 0) {
		questionIndex += labelLen + 1;
		if (questionIndex > p->len) {
			pbuf_put_at(p, DNS_INDEX_RCODE, DNS_RCODE_FORM_ERROR);
			dns_prepare_send(p, DNS_LEN_HEADER);
			udp_sendto(pcb, p, addr, port);
			return;
		}
	}
	questionIndex++;

	// Ensures label doesn't extend outside packet
	if ((questionIndex + 4) > p->len) {
		pbuf_put_at(p, DNS_INDEX_RCODE, DNS_RCODE_FORM_ERROR);
		dns_prepare_send(p, DNS_LEN_HEADER);
		udp_sendto(pcb, p, addr, port);
		return;
	}

	// Gets questions type and class
	int qtype = pbuf_get_at(p, questionIndex) << 8 | pbuf_get_at(p, questionIndex + 1);
	int qclass = pbuf_get_at(p, questionIndex + 2) << 8 | pbuf_get_at(p, questionIndex + 3);
	questionIndex += 4;

	// Validates question type
	if (qtype != DNS_QTYPE_A && qtype != DNS_QTYPE_ANY) {
		pbuf_put_at(p, DNS_INDEX_RCODE, DNS_RCODE_NON_EXISTENT_DOMAIN);
		dns_prepare_send(p, questionIndex);
		udp_sendto(pcb, p, addr, port);
		return;
	}
	
	// Validates question class
	if (qclass != DNS_QCLASS_IN && qclass != DNS_QCLASS_ANY) {
		pbuf_put_at(p, DNS_INDEX_RCODE, DNS_RCODE_NON_EXISTENT_DOMAIN);
		dns_prepare_send(p, questionIndex);
		udp_sendto(pcb, p, addr, port);
		return;
	}

	// Use header and query for response with QDCount and ANCount in header set to 1
	pbuf_take_at(p, (uint16_t[]){ lwip_htons(0x0001), lwip_htons(0x0001), 0x0000, 0x0000 }, DNS_LEN_COUNTS, DNS_INDEX_COUNTS);
	dns_prepare_send(p, questionIndex);

	// Creates DNS answer
	struct pbuf* answer = pbuf_alloc(PBUF_TRANSPORT, DNS_LEN_ANSWER, PBUF_POOL);
	uint16_t answerBuf[] = {
		// Pointer to the queried name already defined in the packet
		lwip_htons(0xc000 | DNS_LEN_HEADER),
		// Defines the response query type
		lwip_htons(DNS_QTYPE_A),
		// Defines the response query internet class
		lwip_htons(DNS_QCLASS_IN),
		// Defines the TTL
		0x0000, lwip_htons(60),
		// Defines the IP length
		lwip_htons(4)
	};
	pbuf_take(answer, answerBuf, sizeof(answerBuf));
	pbuf_take_at(answer, ip_current_dest_addr(), 4, sizeof(answerBuf));

	// Sends DNS response with header, query and answer
	pbuf_cat(p, answer);
	udp_sendto(pcb, p, addr, port);
}

// Initializes captive portal dns server
struct udp_pcb* captive_portal_init(void) {
	// Creates protocol control buffer for captive portal DNS server
	struct udp_pcb* pcb = udp_new();
	if (pcb == NULL) {
		DEBUG_PRINTF("Failed to create captive portal dns pcb\n");
		return NULL;
	}

	// Connects captive portal DNS callback function to pcb
	udp_recv(pcb, dns_recv_callback, NULL);

	// Binds captive portal DNS pcb to any IP address on default DNS port
	err_t err = udp_bind(pcb, IP_ADDR_ANY, DNS_PORT);
	if (err != ERR_OK) {
		DEBUG_PRINTF("Failed to bind captive portal DNS to its port: %d\n", (int)err);
		udp_remove(pcb);
		return NULL;
	}

	return pcb;
}
