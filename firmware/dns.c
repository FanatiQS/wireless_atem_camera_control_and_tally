#include <stddef.h> // NULL
#include <stdint.h> // uint8_t, uint16_t

#include <lwip/udp.h> // struct udp_pcb, udp_new, udp_recv, udp_bind, udp_sendto
#include <lwip/ip_addr.h> // ip_addr_t, IP_ADDR_ANY
#include <lwip/arch.h> // LWIP_UNUSED_ARG
#include <lwip/pbuf.h> // struct pbuf, pbuf_free, pbuf_realloc, pbuf_alloc, PBUF_TRANSPORT, PBUF_POOL, pbuf_take, pbuf_take_at, pbuf_cat, pbuf_get_at, pbuf_put_at, pbuf_memcmp
#include <lwip/def.h> // htons
#include <lwip/ip.h> // ip_current_dest_addr

#include "./debug.h" // DEBUG_ERR_PRINTF, DEBUG_DNS_PRINTF
#include "./dns.h" // captive_portal_init



// Default DNS port
#define DNS_PORT (53)

// DNS header indexes
#define DNS_INDEX_FLAGS   (2)
#define DNS_INDEX_RCODE   (3)
#define DNS_INDEX_QDCOUNT (4)
#define DNS_INDEX_COUNTS  (DNS_INDEX_QDCOUNT)

// DNS header lengths
#define DNS_LEN_HEADER (12)
#define DNS_LEN_ANSWER (16)
#define DNS_LEN_COUNTS (DNS_LEN_HEADER - DNS_INDEX_COUNTS)

// DNS flag masks
#define DNS_FLAGS_MASK_QR     (0x80)
#define DNS_FLAGS_MASK_OPCODE (0x08 | 0x10 | 0x20 | 0x40)

// DNS rcode response codes
#define DNS_RCODE_FORMERR  (1)
#define DNS_RCODE_SERVFAIL (2)
#define DNS_RCODE_NXDOMAIN (3)
#define DNS_RCODE_NOTIMP   (4)

// DNS query classes
#define DNS_QCLASS_IN  (0x01)
#define DNS_QCLASS_ANY (0xff)

// DNS query types
#define DNS_QTYPE_A    (0x01)
#define DNS_QTYPE_ANY  (0xff)



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
	if (p->tot_len < DNS_LEN_HEADER) {
		DEBUG_DNS_PRINTF("Invalid packet size: %u\n", p->tot_len);
		pbuf_free(p);
		return;
	}

	// Only supports query packets
	uint8_t flags_and_opcode = pbuf_get_at(p, DNS_INDEX_FLAGS);
	if (flags_and_opcode & (DNS_FLAGS_MASK_QR | DNS_FLAGS_MASK_OPCODE)) {
		// Ignores non query packets
		if (flags_and_opcode & DNS_FLAGS_MASK_QR) {
			DEBUG_DNS_PRINTF("Packet is not a query\n");
			pbuf_free(p);
			return;
		}

		// Responds with error for non query opcode requests
		dns_header_error(p, DNS_RCODE_NOTIMP);
		udp_sendto(pcb, p, addr, port);
		DEBUG_DNS_PRINTF("Unsupported opcode: %d\n", flags_and_opcode);
		pbuf_free(p);
		return;
	}

	// Only supports single queries
	if (pbuf_memcmp(p, DNS_INDEX_QDCOUNT, (uint16_t[]){ htons(0x0001) }, sizeof(uint16_t))) {
		DEBUG_DNS_PRINTF(
			"Only supports single queries: %d\n",
			pbuf_get_at(p, DNS_INDEX_QDCOUNT) << 8 | pbuf_get_at(p, DNS_INDEX_QDCOUNT + 1)
		);
		dns_header_error(p, DNS_RCODE_FORMERR);
		udp_sendto(pcb, p, addr, port);
		pbuf_free(p);
		return;
	}

	// Skips query labels to get to its type and class
	uint16_t index = DNS_LEN_HEADER;
	int label_len;
	while (index < p->tot_len && (label_len = pbuf_get_at(p, index)) > 0) {
		index += label_len + 1;
	}
	index++;

	// Ensures query doesn't extend outside packet
	if ((index + 4) > p->tot_len) {
		dns_header_error(p, DNS_RCODE_FORMERR);
		udp_sendto(pcb, p, addr, port);
		DEBUG_DNS_PRINTF("Query expands out of packet\n");
		pbuf_free(p);
		return;
	}

	// Gets query type and class
	int qtype = pbuf_get_at(p, index) << 8 | pbuf_get_at(p, index + 1);
	int qclass = pbuf_get_at(p, index + 2) << 8 | pbuf_get_at(p, index + 3);
	index += 4;

	// Validates query type
	if (qtype != DNS_QTYPE_A && qtype != DNS_QTYPE_ANY) {
		pbuf_put_at(p, DNS_INDEX_RCODE, DNS_RCODE_NXDOMAIN);
		dns_prepare_send(p, index);
		udp_sendto(pcb, p, addr, port);
		DEBUG_DNS_PRINTF("Unsupported query qtype: %d\n", qtype);
		pbuf_free(p);
		return;
	}

	// Validates query class
	if (qclass != DNS_QCLASS_IN && qclass != DNS_QCLASS_ANY) {
		pbuf_put_at(p, DNS_INDEX_RCODE, DNS_RCODE_NXDOMAIN);
		dns_prepare_send(p, index);
		udp_sendto(pcb, p, addr, port);
		DEBUG_DNS_PRINTF("Unsupported query qclass: %d\n", qclass);
		pbuf_free(p);
		return;
	}

	// Use header and query for response with QDCount and ANCount in header set to 1
	pbuf_take_at(p, (uint16_t[]){ htons(0x0001), htons(0x0001), 0x0000, 0x0000 }, DNS_LEN_COUNTS, DNS_INDEX_COUNTS);
	dns_prepare_send(p, index);

	// Creates DNS answer
	struct pbuf* answer = pbuf_alloc(PBUF_TRANSPORT, DNS_LEN_ANSWER, PBUF_POOL);
	if (answer == NULL) {
		dns_header_error(p, DNS_RCODE_SERVFAIL);
		udp_sendto(pcb, p, addr, port);
		DEBUG_DNS_PRINTF("Not enough memory for answer\n");
		pbuf_free(p);
		return;
	}
	uint16_t answer_buf[] = {
		htons(0xc000 | DNS_LEN_HEADER), // Pointer to the queried name already defined in the packet
		htons(DNS_QTYPE_A), // Defines the response type
		htons(DNS_QCLASS_IN), // Defines the response internet class
		0x0000, htons(60), // Defines the TTL
		htons(4) // Defines the IP length
	};
	pbuf_take(answer, answer_buf, sizeof(answer_buf));
	pbuf_take_at(answer, ip_current_dest_addr(), 4, sizeof(answer_buf));

	// Sends DNS response
	pbuf_cat(p, answer);
	udp_sendto(pcb, p, addr, port);
	DEBUG_DNS_PRINTF("Successfully responded to DNS query\n");
	pbuf_free(p);
}

// Initializes captive portal dns server
struct udp_pcb* captive_portal_init(void) {
	// Creates protocol control buffer for captive portal DNS server
	struct udp_pcb* pcb = udp_new();
	if (pcb == NULL) {
		DEBUG_ERR_PRINTF("Failed to create captive portal dns pcb\n");
		return NULL;
	}

	// Connects captive portal DNS callback function to pcb
	udp_recv(pcb, dns_recv_callback, NULL);

	// Binds captive portal DNS pcb to any IP address on default DNS port
	err_t err = udp_bind(pcb, IP_ADDR_ANY, DNS_PORT);
	if (err != ERR_OK) {
		DEBUG_ERR_PRINTF("Failed to bind captive portal DNS to its port: %d\n", (int)err);
		udp_remove(pcb);
		return NULL;
	}

	return pcb;
}
