#include <stdint.h> // uint8_t, uint16_t
#include <stddef.h> // size_t
#include <string.h> // strlen, memcpy, memcmp
#include <stdlib.h> // abort
#include <stdio.h> // fprintf, stderr, printf
#include <assert.h> // assert

#include <unistd.h> // close
#include <sys/socket.h> // SOCK_DGRAM
#include <arpa/inet.h> // htons, ntohs

#include "../utils/simple_socket.h" // simple_socket_create, simple_socket_connect_env, simple_socket_send, simple_socket_recv, simple_socket_poll
#include "../utils/logs.h" // logs_print_buffer, logs_find
#include "../utils/runner.h" // RUN_TEST, runner_exit



#define DNS_PORT (53)

#define DNS_LEN_MAX (512)
#define DNS_LEN_MIN (12)

#define DNS_INDEX_FLAGS_HIGH (2)
#define DNS_INDEX_FLAGS_LOW  (3)
#define DNS_INDEX_QDCOUNT    (4)

#define DNS_RCODE_FORMERR  (1)
#define DNS_RCODE_NXDOMAIN (3)
#define DNS_RCODE_NOTIMP   (4)

#define DNS_QTYPE_A   (1)
#define DNS_QCLASS_IN  (1)



// Gets 16bit value from DNS packet
static uint16_t dns_u16_get(void* ptr) {
	uint16_t value;
	memcpy(&value, ptr, sizeof(value));
	return ntohs(value);
}

// Sets 16bit value in DNS packet
static void dns_u16_set(void* ptr, uint16_t value) {
	uint16_t host_order_value = htons(value);
	memcpy(ptr, &host_order_value, sizeof(host_order_value));
}



// Creates a UDP socket connected to DNS server at address from environment variable
static int dns_socket_create(void) {
	int sock = simple_socket_create(SOCK_DGRAM);
	simple_socket_connect_env(sock, DNS_PORT, "DEVICE_ADDR");
	return sock;
}

// Sends binary DNS data request to server
static void dns_socket_send(int sock, uint8_t* req_buf, size_t req_len) {
	if (logs_find("dns_send")) {
		printf("Request packet:\n");
		logs_print_buffer(stdout, req_buf, req_len);
	}
	simple_socket_send(sock, req_buf, req_len);
}

// Receives binary DNS data response from server
static size_t dns_socket_recv(int sock, uint8_t* res_buf, size_t res_size) {
	size_t res_len = simple_socket_recv(sock, res_buf, res_size);
	if (logs_find("dns_recv")) {
		printf("Response packet:\n");
		logs_print_buffer(stdout, res_buf, res_len);
	}
	return res_len;
}

// Prints received DNS packet
static void dns_socket_recv_print(int sock) {
	uint8_t buf[DNS_LEN_MAX];
	size_t recv_len = dns_socket_recv(sock, buf, sizeof(buf));
	logs_print_buffer(stdout, buf, recv_len);
}



// Appends query name label to DNS request buffer
static void dns_append_label(uint8_t* buf, size_t* buf_len, const char* label) {
	size_t label_len = strlen(label);
	assert(label_len < UINT8_MAX);
	buf[*buf_len] = (uint8_t)label_len;
	*buf_len += 1;
	memcpy(&buf[*buf_len], label, label_len);
	*buf_len += label_len;
}

// Completes DNS query after appended labels
static void dns_append_complete(uint8_t* buf, size_t* buf_len, uint16_t qtype, uint16_t qclass) {
	// Terminates query name
	buf[*buf_len] = 0x00;
	*buf_len += 1;

	// Sets qtype and qclass
	dns_u16_set(&buf[*buf_len], qtype);
	*buf_len += sizeof(uint16_t);
	dns_u16_set(&buf[*buf_len], qclass);
	*buf_len += sizeof(uint16_t);

	// Increments qdcount in DNS buffer
	dns_u16_set(&buf[DNS_INDEX_QDCOUNT], dns_u16_get(&buf[DNS_INDEX_QDCOUNT]) + 1);
}

// Appends query name for google.com without completing it
static size_t dns_append_default_name(uint8_t* req_buf, size_t req_len) {
	dns_append_label(req_buf, &req_len, "google");
	dns_append_label(req_buf, &req_len, "com");
	return req_len;
}

// Appends query for google.com
static size_t dns_append_default(uint8_t* req_buf, size_t req_len) {
	req_len = dns_append_default_name(req_buf, req_len);
	dns_append_complete(req_buf, &req_len, DNS_QTYPE_A, DNS_QCLASS_IN);
	return req_len;
}



// Ensures invalid DNS request is not responded to
static void dns_expect_timeout(uint8_t* req_buf, size_t req_len) {
	// Sends DNS request
	int sock = dns_socket_create();
	dns_socket_send(sock, req_buf, req_len);

	// Expects timeout
	if (simple_socket_poll(sock, 4000) == 1) {
		fprintf(stderr, "Unexpectedly got data:\n");
		dns_socket_recv_print(sock);
		abort();
	}
	close(sock);
}

// Validates DNS response against comparison buffer
static void dns_expect_error_validate(int sock, uint8_t* cmp_buf, size_t cmp_len) {
	// Reads DNS response buffer
	uint8_t res_buf[DNS_LEN_MAX] = {0};
	size_t res_len = dns_socket_recv(sock, res_buf, sizeof(res_buf));

	// Ensures response buffer matches length of compare buffer
	if (res_len != cmp_len) {
		fprintf(stderr, "Unexpected length: %zu, %zu\n", res_len, cmp_len);
		abort();
	}

	// Ensures response buffers content matches compare buffers content
	if (memcmp(res_buf, cmp_buf, cmp_len)) {
		fprintf(stderr, "Response buffer not matching\n");
		abort();
	}

	// Closes UDP socket when done testing
	close(sock);
}

// Sends DNS request expecting specific rcode error not including query
static void dns_expect_error_short(uint8_t* req_buf, size_t req_len, uint8_t code) {
	// Sends DNS request
	int sock = dns_socket_create();
	dns_socket_send(sock, req_buf, req_len);

	// Creates expected response
	uint8_t cmp_buf[DNS_LEN_MIN] = {0};
	memcpy(cmp_buf, req_buf, 3);
	cmp_buf[DNS_INDEX_FLAGS_HIGH] |= 0x80;
	cmp_buf[DNS_INDEX_FLAGS_LOW] = code;

	// Validates response
	dns_expect_error_validate(sock, cmp_buf, sizeof(cmp_buf));
}

// Sends DNS request expecting specific rcode error including query
static void dns_expect_error_long(uint8_t* req_buf, size_t req_len, uint8_t code) {
	// Sends DNS request
	int sock = dns_socket_create();
	dns_socket_send(sock, req_buf, req_len);

	// Creates expected response
	uint8_t cmp_buf[DNS_LEN_MAX] = {0};
	memcpy(cmp_buf, req_buf, req_len);
	cmp_buf[DNS_INDEX_FLAGS_HIGH] |= 0x80;
	cmp_buf[DNS_INDEX_FLAGS_LOW] = code;

	// Validates reqsponse
	dns_expect_error_validate(sock, cmp_buf, req_len);
}



// Runs all DNS tests
int main(void) {
	// Tests under minimum packet length
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = {0};
		dns_expect_timeout(req_buf, DNS_LEN_MIN - 1);
	}

	// Tests QR flag being set for request
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = { [DNS_INDEX_FLAGS_HIGH] = 0x80 };
		dns_expect_timeout(req_buf, DNS_LEN_MIN);
	}

	// Tests unsupported opcodes
	RUN_TEST() {
		for (int i = 1; i < 16; i++) {
			uint8_t req_buf[DNS_LEN_MAX] = { [DNS_INDEX_FLAGS_HIGH] = (uint8_t)(i << 3) };
			dns_expect_error_short(req_buf, dns_append_default(req_buf, DNS_LEN_MIN), DNS_RCODE_NOTIMP);
		}
	}

	// Tests unsupported zero queries
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = {0};
		dns_expect_error_short(req_buf, DNS_LEN_MIN, DNS_RCODE_FORMERR);
	}

	// Tests unsupported more than 1 query
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = {0};
		size_t req_len = dns_append_default(req_buf, DNS_LEN_MIN);
		req_len = dns_append_default(req_buf, req_len);
		dns_expect_error_short(req_buf, req_len, DNS_RCODE_FORMERR);
	}

	// Tests invalid query length
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		size_t req_len = dns_append_default(buf, DNS_LEN_MIN);
		dns_expect_error_short(buf, req_len - 1, DNS_RCODE_FORMERR);
	}

	// Tests label expanding out of packet
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = {0};
		size_t req_len = dns_append_default(req_buf, DNS_LEN_MIN);
		req_buf[req_len - sizeof(uint16_t) * 2 - 2 - strlen("com")] += 1;
		dns_expect_error_short(req_buf, req_len, DNS_RCODE_FORMERR);
	}

	// Ensures unsupported qclass is rejected
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = {0,0,1};
		size_t req_len = dns_append_default_name(req_buf, DNS_LEN_MIN);
		dns_append_complete(req_buf, &req_len, 2, DNS_QCLASS_IN);
		dns_expect_error_long(req_buf, req_len, DNS_RCODE_NXDOMAIN);
	}

	// Ensures unsupported qtype is rejected
	RUN_TEST() {
		uint8_t req_buf[DNS_LEN_MAX] = {0};
		size_t req_len = dns_append_default_name(req_buf, DNS_LEN_MIN);
		dns_append_complete(req_buf, &req_len, DNS_QTYPE_A, 2);
		dns_expect_error_long(req_buf, req_len, DNS_RCODE_NXDOMAIN);
	}

	return runner_exit();
}
