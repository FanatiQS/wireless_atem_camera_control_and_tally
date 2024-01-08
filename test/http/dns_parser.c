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

#define DNS_INDEX_ID         (0)
#define DNS_INDEX_FLAGS_HIGH (2)
#define DNS_INDEX_FLAGS_LOW  (3)
#define DNS_INDEX_QDCOUNT    (4)

#define DNS_RCODE_FORMERR  (1)
#define DNS_RCODE_NXDOMAIN (3)
#define DNS_RCODE_NOTIMP   (4)

#define DNS_QTYPE_A   (1)
#define DNS_QTYPE_ANY (255)

#define DNS_QCLASS_IN  (1)
#define DNS_QCLASS_ANY (255)



// Get/set 16bit value in DNS packet
#define DNS_GET16(ptr) ntohs(*((uint16_t*)(ptr)))
#define DNS_SET16(ptr, value) *((uint16_t*)(ptr)) = htons(value)



// Creates a UDP socket connected to DNS server at address from environment variable
int dns_socket_create(void) {
	int sock = simple_socket_create(SOCK_DGRAM);
	simple_socket_connect_env(sock, DNS_PORT, "DEVICE_ADDR");
	return sock;
}

// Sends binary DNS data request to server
void dns_socket_send(int sock, uint8_t* reqBuf, size_t reqLen) {
	if (logs_find("dns_send")) {
		printf("Request packet:\n");
		logs_print_buffer(stdout, reqBuf, reqLen);
	}
	simple_socket_send(sock, reqBuf, reqLen);
}

// Receives binary DNS data response from server
size_t dns_socket_recv(int sock, uint8_t* resBuf, size_t resSize) {
	size_t resLen = simple_socket_recv(sock, resBuf, resSize);
	if (logs_find("dns_recv")) {
		printf("Response packet:\n");
		logs_print_buffer(stdout, resBuf, resLen);
	}
	return resLen;
}

// Prints received DNS packet
void dns_socket_recv_print(int sock) {
	uint8_t buf[DNS_LEN_MAX];
	size_t recvLen = dns_socket_recv(sock, buf, sizeof(buf));
	logs_print_buffer(stdout, buf, recvLen);
}



// Appends query name label to DNS request buffer
void dns_append_label(uint8_t* buf, size_t* bufLen, const char* label) {
	size_t labelLen = strlen(label);
	assert(labelLen < UINT8_MAX);
	buf[*bufLen] = (uint8_t)labelLen;
	*bufLen += 1;
	memcpy(&buf[*bufLen], label, labelLen);
	*bufLen += labelLen;
}

// Completes DNS query after appended labels
void dns_append_complete(uint8_t* buf, size_t* bufLen, int qtype, int qclass) {
	// Terminates query name
	buf[*bufLen] = 0x00;
	*bufLen += 1;

	// Sets qtype and qclass
	DNS_SET16(&buf[*bufLen], qtype);
	*bufLen += sizeof(uint16_t);
	DNS_SET16(&buf[*bufLen], qclass);
	*bufLen += sizeof(uint16_t);

	// Increments qdcount in DNS buffer
	DNS_SET16(&buf[DNS_INDEX_QDCOUNT], DNS_GET16(&buf[DNS_INDEX_QDCOUNT]) + 1);
}

// Appends query name for google.com without completing it
size_t dns_append_default_name(uint8_t* reqBuf, size_t reqLen) {
	dns_append_label(reqBuf, &reqLen, "google");
	dns_append_label(reqBuf, &reqLen, "com");
	return reqLen;
}

// Appends query for google.com
size_t dns_append_default(uint8_t* reqBuf, size_t reqLen) {
	reqLen = dns_append_default_name(reqBuf, reqLen);
	dns_append_complete(reqBuf, &reqLen, DNS_QTYPE_A, DNS_QCLASS_IN);
	return reqLen;
}



// Ensures invalid DNS request is not responded to
void dns_expect_timeout(uint8_t* reqBuf, size_t reqLen) {
	// Sends DNS request
	int sock = dns_socket_create();
	dns_socket_send(sock, reqBuf, reqLen);

	// Expects timeout
	if (simple_socket_poll(sock, 4000) == 1) {
		fprintf(stderr, "Unexpectedly got data:\n");
		dns_socket_recv_print(sock);
		abort();
	}
	close(sock);
}

// Validates DNS response against comparison buffer
void dns_expect_error_validate(int sock, uint8_t* cmpBuf, size_t cmpLen) {
	// Reads DNS response buffer
	uint8_t resBuf[DNS_LEN_MAX] = {0};
	size_t resLen = dns_socket_recv(sock, resBuf, sizeof(resBuf));
	
	// Ensures response buffer matches length of compare buffer
	if (resLen != cmpLen) {
		fprintf(stderr, "Unexpected length: %zu, %zu\n", resLen, cmpLen);
		abort();
	}

	// Ensures response buffers content matches compare buffers content
	if (memcmp(resBuf, cmpBuf, cmpLen)) {
		fprintf(stderr, "Response buffer not matching\n");
		abort();
	}

	// Closes UDP socket when done testing
	close(sock);
}

// Sends DNS request expecting specific rcode error not including query
void dns_expect_error_short(uint8_t* reqBuf, size_t reqLen, uint8_t code) {
	// Sends DNS request
	int sock = dns_socket_create();
	dns_socket_send(sock, reqBuf, reqLen);

	// Creates expected response
	uint8_t cmpBuf[DNS_LEN_MIN] = {0};
	memcpy(cmpBuf, reqBuf, 3);
	cmpBuf[DNS_INDEX_FLAGS_HIGH] |= 0x80;
	cmpBuf[DNS_INDEX_FLAGS_LOW] = code;

	// Validates response
	dns_expect_error_validate(sock, cmpBuf, sizeof(cmpBuf));
}

// Sends DNS request expecting specific rcode error including query
void dns_expect_error_long(uint8_t* reqBuf, size_t reqLen, uint8_t code) {
	// Sends DNS request
	int sock = dns_socket_create();
	dns_socket_send(sock, reqBuf, reqLen);

	// Creates expected response
	uint8_t cmpBuf[DNS_LEN_MAX] = {0};
	memcpy(cmpBuf, reqBuf, reqLen);
	cmpBuf[DNS_INDEX_FLAGS_HIGH] |= 0x80;
	cmpBuf[DNS_INDEX_FLAGS_LOW] = code;

	// Validates reqsponse
	dns_expect_error_validate(sock, cmpBuf, reqLen);
}



// Runs all DNS tests
int main(void) {
	// Tests under minimum packet length
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		dns_expect_timeout(buf, DNS_LEN_MIN - 1);
	}

	// Tests QR flag being set for request
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = { [DNS_INDEX_FLAGS_HIGH] = 0x80 };
		dns_expect_timeout(buf, DNS_LEN_MIN);
	}

	// Tests unsupported opcodes
	RUN_TEST() {
		for (int i = 1; i < 16; i++) {
			uint8_t buf[DNS_LEN_MAX] = { [DNS_INDEX_FLAGS_HIGH] = (uint8_t)(i << 3) };
			dns_expect_error_short(buf, dns_append_default(buf, DNS_LEN_MIN), DNS_RCODE_NOTIMP);
		}
	}

	// Tests unsupported zero queries
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		dns_expect_error_short(buf, DNS_LEN_MIN, DNS_RCODE_FORMERR);
	}

	// Tests unsupported more than 1 query
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		size_t reqLen = dns_append_default(buf, DNS_LEN_MIN);
		reqLen = dns_append_default(buf, reqLen);
		dns_expect_error_short(buf, reqLen, DNS_RCODE_FORMERR);
	}

	// Tests invalid query length
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		size_t reqLen = dns_append_default(buf, DNS_LEN_MIN);
		dns_expect_error_short(buf, reqLen - 1, DNS_RCODE_FORMERR);
	}

	// Tests label expanding out of packet
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		size_t bufLen = dns_append_default(buf, DNS_LEN_MIN);
		buf[bufLen - sizeof(uint16_t) * 2 - 2 - strlen("com")] += 1;
		dns_expect_error_short(buf, bufLen, DNS_RCODE_FORMERR);
	}

	// Ensures unsupported qclass is rejected
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0,0,1};
		size_t bufLen = dns_append_default_name(buf, DNS_LEN_MIN);
		dns_append_complete(buf, &bufLen, 2, DNS_QCLASS_IN);
		dns_expect_error_long(buf, bufLen, DNS_RCODE_NXDOMAIN);
	}

	// Ensures unsupported qtype is rejected
	RUN_TEST() {
		uint8_t buf[DNS_LEN_MAX] = {0};
		size_t bufLen = dns_append_default_name(buf, DNS_LEN_MIN);
		dns_append_complete(buf, &bufLen, DNS_QTYPE_A, 2);
		dns_expect_error_long(buf, bufLen, DNS_RCODE_NXDOMAIN);
	}

	return runner_exit();
}
