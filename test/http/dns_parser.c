#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t, NULL
#include <string.h> // strlen, memcpy, memcmp
#include <stdlib.h> // abort
#include <stdio.h> // fprintf, stderr, printf, perror, stdout
#include <assert.h> // assert

#include <unistd.h> // close
#include <sys/socket.h> // SOCK_DGRAM
#include <arpa/inet.h> // htons, ntohs

#include <arpa/nameser_compat.h> // HEADER
#include <resolv.h> // res_nmkquery, struct __res_state
#include <nameser.h> // ns_r_formerr, ns_r_nxdomain, ns_r_notimp, ns_t_a, ns_c_in, NS_DEFAULTPORT, NS_PACKETSZ, NS_HFIXEDSZ, ns_class, ns_type, ns_o_query, ns_rcode, ns_msg, ns_r_notimpl, ns_r_noerror

#include "../utils/simple_socket.h" // simple_socket_create, simple_socket_connect_env, simple_socket_send, simple_socket_recv, simple_socket_poll
#include "../utils/logs.h" // logs_print_buffer, logs_find
#include "../utils/runner.h" // RUN_TEST, runner_exit



// Easily modifiable DNS packet
struct dns_packet {
	size_t len;
	union {
		uint8_t buf[NS_PACKETSZ];
		HEADER header;
	};
};



// Appends a DNS query name to the packet
static void dns_append(struct dns_packet* packet, const char* qname, ns_class qclass, ns_type qtype) {
	// Creates DNS query
	uint8_t buf[NS_PACKETSZ];
	struct __res_state res = {0};
	int len = res_nmkquery(&res, ns_o_query, qname, qclass, qtype, NULL, 0, NULL, buf, sizeof(buf));
	if (len == -1) {
		perror("Failed to create DNS query");
		abort();
	}
	assert(len > 0);

	// Only QDCount should have been set
	const HEADER cmp_buf = { .qdcount = htons(1) };
	assert(memcmp(buf + 2, (const uint8_t*)&cmp_buf + 2, sizeof(cmp_buf) - 2) == 0);

	// Sets packet length if not initialized
	if (packet->len == 0) {
		packet->len = NS_HFIXEDSZ;
	}

	// Copies over query to packet
	const size_t query_len = (size_t)len - NS_HFIXEDSZ;
	assert((sizeof(packet->buf) - packet->len) > query_len);
	memcpy(packet->buf + packet->len, buf + NS_HFIXEDSZ, query_len);
	packet->len += query_len;
	packet->header.qdcount = htons(ntohs(packet->header.qdcount) + 1);
}

// Appends query for google.com
static void dns_append_default(struct dns_packet* packet) {
	dns_append(packet, "google.com", ns_c_in, ns_t_a);
}



// Sends binary DNS data request to server
static int dns_send(struct dns_packet* packet) {
	// Creates UDP socket connected to DNS server at address from environment variable
	int sock = simple_socket_create(SOCK_DGRAM);
	simple_socket_connect_env(sock, NS_DEFAULTPORT, "DEVICE_ADDR");

	// Sets packet length if not initialized
	if (packet->len == 0) {
		packet->len = NS_HFIXEDSZ;
	}

	if (logs_find("dns_send")) {
		printf("DNS query packet:\n");
		logs_print_buffer(stdout, packet->buf, packet->len);
	}

	// Sends DNS query packet to server
	simple_socket_send(sock, packet->buf, packet->len);

	return sock;
}

// Reads and validates DNS answer against comparison buffer
static void dns_query(struct dns_packet* packet, ns_rcode rcode, size_t cmp_len) {
	// Sends DNS query to server and reads DNS answer from server
	int sock = dns_send(packet);
	uint8_t answer_buf[NS_PACKETSZ];
	size_t answer_len = simple_socket_recv(sock, answer_buf, sizeof(answer_buf));
	close(sock);
	assert(answer_len > 0);
	if (logs_find("dns_recv")) {
		printf("DNS answer packet:\n");
		logs_print_buffer(stdout, answer_buf, answer_len);
	}
	assert(answer_len >= NS_HFIXEDSZ);

	// Creates expected response to compare against
	struct dns_packet cmp_packet = *packet;
	cmp_packet.header.qr = 1;
	assert(rcode < 0x0f);
	assert(rcode >= 0);
	cmp_packet.header.rcode = rcode & 0x0f;
	if (cmp_len == NS_HFIXEDSZ) {
		cmp_packet.header.qdcount = 0;
	}

	// Sets up to validate successful DNS query
	if (rcode == ns_r_noerror) {
		assert(cmp_len == 0);
		cmp_len = packet->len;
		cmp_packet.header.ancount = htons(1);
	}
	// Ensures response buffer matches length of compare buffer
	else if (answer_len != cmp_len) {
		fprintf(stderr, "Unexpected length: %zu, %zu\n", answer_len, cmp_len);
		abort();
	}

	// Ensures response buffers content matches compare buffers content
	if (memcmp(answer_buf, cmp_packet.buf, cmp_len) != 0) {
		fprintf(stderr, "Response buffer not matching\n");
		logs_print_buffer(stderr, answer_buf, cmp_len);
		logs_print_buffer(stderr, cmp_packet.buf, cmp_len);
		abort();
	}

}

// Ensures invalid DNS request is not responded to
static void dns_timeout(struct dns_packet* packet) {
	int sock = dns_send(packet);
	if (simple_socket_poll(sock, 4000) == 1) {
		uint8_t answer_buf[NS_PACKETSZ];
		size_t answer_len = simple_socket_recv(sock, answer_buf, sizeof(answer_buf));
		fprintf(stderr, "Unexpectedly got data:\n");
		logs_print_buffer(stderr, answer_buf, answer_len);
		abort();
	}
	close(sock);
}



// Runs all DNS tests
int main(void) {
	// Tests under minimum packet length
	RUN_TEST() {
		struct dns_packet packet = { .len = NS_HFIXEDSZ - 1 };
		dns_timeout(&packet);
	}

	// Tests QR flag being set for request
	RUN_TEST() {
		struct dns_packet packet = { .header.qr = 1 };
		dns_append_default(&packet);
		dns_timeout(&packet);
	}

	// Tests unsupported opcodes
	RUN_TEST() {
		for (unsigned int i = 1; i < 16; i++) {
			struct dns_packet packet = { .header.opcode = i };
			dns_append_default(&packet);
			dns_query(&packet, ns_r_notimpl, NS_HFIXEDSZ);
		}
	}

	// Tests unsupported zero queries
	RUN_TEST() {
		struct dns_packet packet = {0};
		dns_query(&packet, ns_r_formerr, NS_HFIXEDSZ);
	}

	// Tests unsupported more than 1 query
	RUN_TEST() {
		struct dns_packet packet = {0};
		dns_append_default(&packet);
		dns_append_default(&packet);
		dns_query(&packet, ns_r_formerr, NS_HFIXEDSZ);
	}

	// Tests invalid query length
	RUN_TEST() {
		struct dns_packet packet = {0};
		dns_append_default(&packet);
		packet.len--;
		dns_query(&packet, ns_r_formerr, NS_HFIXEDSZ);
	}

	// Tests label expanding out of packet
	RUN_TEST() {
		struct dns_packet packet = {0};
		dns_append_default(&packet);
		packet.buf[packet.len - sizeof(uint16_t) * 2 - 2 - strlen("com")] += 1;
		dns_query(&packet, ns_r_formerr, NS_HFIXEDSZ);
	}

	// Ensures unsupported qclass is rejected
	RUN_TEST() {
		struct dns_packet packet = {0};
		dns_append(&packet, "google.com", ns_c_in, ns_t_a + 1);
		dns_query(&packet, ns_r_nxdomain, packet.len);
	}

	// Ensures unsupported qtype is rejected
	RUN_TEST() {
		struct dns_packet packet = {0};
		dns_append(&packet, "google.com", ns_c_in + 1, ns_t_a);
		dns_query(&packet, ns_r_nxdomain, packet.len);
	}

	return runner_exit();
}
