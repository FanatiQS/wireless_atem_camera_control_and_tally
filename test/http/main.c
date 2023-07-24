#include <stdio.h> // printf, perror, fprintf, stderr, sprintf, snprintf, NULL
#include <stdlib.h> // exit, EXIT_FAILURE, abort
#include <assert.h> // assert
#include <string.h> // strlen, strcmp, memcmp
#include <errno.h> // errno, EPIPE
#include <signal.h> // signal, SIGPIPE, SIG_IGN
#include <assert.h> // assert
#include <stdint.h> // UINT32_MAX

#include <unistd.h> // close, usleep, sleep
#include <sys/socket.h> // socket, AF_INET, SOCK_STREAM, struct sockaddr, htons
#include <arpa/inet.h> // struct sockaddr_in, inet_addr

#define HTTP_PORT 8080
#define BUF_LEN 2048



// Prints line where test was called from
void test_start(int linenumber) {
	printf("Test started on line %d\n", linenumber);
}

// Prints line where test was called from and runs test
#define RUN(test) do { test_start(__LINE__); test; } while(0)



// The shared address to the HTTP server to test, initialized with test_init
struct sockaddr_in sockAddr = {
	.sin_family = AF_INET
};

// Sets the address to the HTTP server to test
void test_init(int argc, char** argv) {
	// Validates argument is available
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <ip-address>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// Initialize the HTTP server address
	sockAddr.sin_port = htons(HTTP_PORT);
	sockAddr.sin_addr.s_addr = inet_addr(argv[1]);

	// Fixes program termination when writing to closed socket
	signal(SIGPIPE, SIG_IGN);
}



// Gets HTTP status message from status code
const char* test_http_status(int code) {
	switch (code) {
		case 200: return "200 OK";
		case 400: return "400 Bad Request";
		case 405: return "405 Method Not Allowed";
		case 404: return "404 Not Found";
		case 411: return "411 Length Required";
		case 413: return "413 Payload Too Large";
		default: {
			fprintf(stderr, "HTTP status code not implemented in test library\n");
			abort();
		}
	}
}



// Context for test
struct test_t {
	int sock;
	ssize_t recvLen;
	char recvBuf[BUF_LEN];
};

// Creates TCP context connected to HTTP server
struct test_t test_create() {
	struct test_t test;

	// Creates HTTP socket
	test.sock = socket(AF_INET, SOCK_STREAM, 0);
	if (test.sock == -1) {
		perror("Failed to create socket");
		abort();
	}

	// Connect socket to HTTP server on default port
	if (connect(test.sock, (const struct sockaddr*)&sockAddr, sizeof(sockAddr))) {
		perror("Failed to connect socket to server");
		abort();
	}

	return test;
}

// Sends HTTP string with fixed length
void test_write(struct test_t* test, const char* buf, size_t len) {
	if (len == 0) return;
	ssize_t sendLen = send(test->sock, buf, len, 0);
	if (sendLen == -1) {
		perror("Failed to send data");
		abort();
	}
}

// Sends HTTP string getting length from string terminator
void test_send(struct test_t* test, const char* buf) {
	test_write(test, buf, strlen(buf));
}

// Reads HTTP data from TCP stream
void test_recv(struct test_t* test) {
	test->recvLen = recv(test->sock, test->recvBuf, sizeof(test->recvBuf) - 1, 0);
	if (test->recvLen == -1) {
		perror("Failed to recv data");
		abort();
	}
	test->recvBuf[test->recvLen] = '\0';
}

// Prints received HTTP buffer
void test_recvprint(struct test_t* test) {
	printf("====START====\n%s\n====END====\n", test->recvBuf);
}

// Closes the tests HTTP socket
void test_close(struct test_t* test) {
	close(test->sock);
}



// Ensures the response contains the specified response code
void test_cmp_code(struct test_t* test, int code) {
	const char ver0[] = "HTTP/1.0 ";
	const char ver1[] = "HTTP/1.1 ";
	const char* status = test_http_status(code);
	const char* res = test->recvBuf;
	if (test->recvLen == 0) {
		fprintf(stderr, "Failed to compare HTTP status code: socket closed\n");
		abort();
	}
	if (
		(memcmp(res, ver0, strlen(ver0)) && memcmp(res, ver1, strlen(ver1))) ||
		memcmp(res + strlen(ver0), status, strlen(status))
	) {
		fprintf(stderr, "Failed to compare HTTP status code:\nres: %s\nstatus: %s\n", res, status);
		abort();
	}
}

// Sends HTTP request expecting specified response code
void test_code(int code, const char* req) {
	struct test_t test = test_create();
	test_send(&test, req);
	test_recv(&test);
	test_cmp_code(&test, code);
	printf("Test for %d success\n", code);
	test_close(&test);
}

// Sends HTTP request in two separate segments expecting specified response code
void test_code_segment(int code, const char* req1, const char* req2) {
	struct test_t test = test_create();
	test_send(&test, req1);
	test_send(&test, req2);
	test_recv(&test);
	test_cmp_code(&test, code);
	printf("Test for %d (segmented) success\n", code);
	test_close(&test);
}

// Sends HTTP request expecting timeout
void test_timeout(const char* req) {
	struct test_t test = test_create();
	test_send(&test, req);
	test_recv(&test);
	if (test.recvLen != 0) {
		fprintf(stderr, "Socket did not timeout\nrecved: %s\n", test.recvBuf);
		abort();
	}
	printf("Test timed out as expected\n");
	test_close(&test);
}

// HTTP template used for sending POST requests
#define HTTP_POST_TEMPLATE "POST / HTTP/1.1\r\nContent-Length: %zu\r\n\r\n%s"

// Sends HTTP request with specified body expecting specified response code
void test_post(int code, const char* body) {
	char req[1024];
	assert(sizeof(req) > (snprintf(NULL, 0, HTTP_POST_TEMPLATE, (size_t)UINT32_MAX, body)));
	sprintf(req, HTTP_POST_TEMPLATE, strlen(body), body);
	test_code(code, req);
}

// Sends HTTP request with specified body in two separate segments expecting specified response code
void test_post_segment(int code, const char* body1, const char* body2) {
	char req[1024];
	assert(sizeof(req) > (snprintf(NULL, 0, HTTP_POST_TEMPLATE "%s", (size_t)UINT32_MAX, body1, body2)));
	sprintf(req, HTTP_POST_TEMPLATE, strlen(body1) + strlen(body2), body1);
	test_code_segment(code, req, body2);
}



// Runs all tests verifying HTTP server works as expected
int main(int argc, char** argv) {
	test_init(argc, argv);

	// HTTP GET method
	RUN(test_timeout(  "GET ")); // Tests incomplete GET method
	RUN(test_code(405, "GEF")); // Tests invalid GET method short
	RUN(test_code_segment(405, "G", "EF")); // Tests segmented invalid GET method short
	RUN(test_code(405, "GEF / HTTP/1.1\r\n\r\n")); // Tests invalid method full
	RUN(test_code_segment(405, "GEF ", "/ HTTP/1.1\r\n\r\n")); // Tests segmented invalid GET method full

	// HTTP POST method
	RUN(test_timeout(  "POST ")); // Tests incomplete POST method
	RUN(test_code(405, "GO")); // Tests back compare method matching short
	RUN(test_code_segment(405, "G", "O")); // Tests segmented back compare method matching short
	RUN(test_code(405, "GOST / HTTP/1.1\r\n\r\n")); // Tests back comparing method matching full
	RUN(test_code_segment(405, "GOS", "T / HTTP/1.1\r\n\r\n")); // Tests segmented back compare method matching full
	RUN(test_code(405, "POSF")); // Tests invalid POST method short
	RUN(test_code_segment(405, "PO", "SF")); // Tests segmented invalid POST method short
	RUN(test_code(405, "POSF / HTTP/1.1\r\n\r\n")); // Tests invalid POST method full
	RUN(test_code_segment(405, "POSF ", "/ HTTP/1.1\r\n\r\n")); // Tests segmented invalid POST method full

#if 0 // @todo add GET tests
	// HTTP GET paths
	test_timeout(  "GET /"); // Tests incomplete GET URI
	test_code(200, "GET / "); // Tests successful GET request short
	test_code_segment(200, "GET", " / "); // Tests segmented successful GET request short
	test_code(200, "GET / HTTP/1.1\r\n\r\n"); // Tests successful GET request full
	test_code_segment(200, "GET / ", "HTTP/1.1\r\n\r\n"); // Tests segmented successful GET request full
	test_code(404, "GET /i"); // Tests invalid URI short
	test_code_segment(404, "GET /", "i"); // Tests segmented invalid URI short
	test_code(404, "GET /invalid HTTP/1.1\r\n\r\n"); // Tests invalid URI full
	test_code_segment(404, "GET /i", "nvalid HTTP/1.1\r\n\r\n"); // Tests segmented invalid URI full
#endif // 0

	// HTTP POST headers
	RUN(test_timeout(  "POST /")); // Tests incomplete POST URI
	RUN(test_code(411, "POST / HTTP/1.1\r\n\r\n")); // Tests POST with no headers
	RUN(test_code_segment(411, "POST / HTTP/1.1\r", "\n\r\n")); // Tests segmented in first CRLF
	RUN(test_code_segment(411, "POST / HTTP/1.1\r\n\r", "\n")); // Tests segmented in second CRLF
	RUN(test_code(411, "POST / HTTP/1.1\r\nContent-Type: test/plain\r\n\r\n")); // Tests missing content length header
	RUN(test_code(411, "POST / HTTP/1.1\r\nCookie: Content-Length: 1\r\n\r\n")); // Tests content length key in value
	RUN(test_code(400, "POST / HTTP/1.1\r\nContent-Length: 123abc\r\n\r\n")); // Tests invalid content length value
	RUN(test_code_segment(400, "POST / HTTP/1.1\r\nCont", "ent-Length: 123abc\r\n\r\n")); // Tests segmented invalid content length value
	RUN(test_timeout(  "POST / HTTP/1.1\r\nContent-Length: 1\r\n\r\n")); // Tests waiting for data
	RUN(test_timeout(  "POST / HTTP/1.1\r\nContent-Length: 2 147 483 647\r\n\r\n")); // Tests max content length value
	RUN(test_code(413, "POST / HTTP/1.1\r\nContent-Length: 2 147 483 648\r\n\r\n")); // Tests overflowing content length value
	RUN(test_code_segment(413, "POST / HTTP/1.1\r\nContent-Length: 2 147 483", "648")); // Tests segmented overflowing content length value
	RUN(test_code(200, "POST / HTTP/1.1\r\nContent-Type: text/plain\r\nContent-Length: 0\r\n\r\n")); // Tests multiple headers with content length being last
	RUN(test_code_segment(200, "POST / HTTP/1.1\r\nCon", "tent-Type: text/plain\r\nContent-Length: 0\r\n\r\n")); // Tests segmented multiple headers with content length being last
	RUN(test_code(200, "POST / HTTP/1.1\r\nContent-Length: 0\r\nContent-Type: text/plain\r\n\r\n")); // Tests multiple headers with content length not being last
	RUN(test_code_segment(200, "POST / HTTP/1.1\r\nContent-Length: 0\r\nCon", "tent-Type: text/plain\r\n\r\n")); // Tests segmented multiple headers with content length not being last
	test_code(200, "POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n"); // Tests no body
	RUN(test_code_segment(200, "POST / HTTP/1.1\r\nContent-Length: 0\r", "\n\r\n")); // Tests segmented no body
	RUN(test_code_segment(200, "POST / HTTP/1.1\r\nContent-Length: 0\r\n\r", "\n")); // Tests segmented no body



	// HTTP POST body key
	RUN(test_post(400, "x")); // Tests invalid body key short
	RUN(test_post(400, "xyz=123")); // Tests invalid body key full
	RUN(test_post_segment(400, "x", "yz=123")); // Tests segmented invalid body key full
	RUN(test_post(400, "ssi")); // Tests incomplete key
	RUN(test_post(400, "ssk=1")); // Tests back comparing body key
	RUN(test_post_segment(400, "ss", "k=1")); // Tests segmented back comparing body key

	// HTTP POST body string
	RUN(test_post(200, "ssid=")); // Tests empty string value
	RUN(test_post(200, "ssid=12")); // Tests valid string value
	RUN(test_post_segment(200, "ssi", "d=12")); // Tests segmented key in valid string value
	RUN(test_post_segment(200, "ssid=1", "2")); // Tests segmented value in valid string value
	RUN(test_post(200, "ssid=1&")); // Tests terminator as last character
	RUN(test_post(200, "psk=1&ssid=1")); // Tests concatenated string property
	RUN(test_post(200, "ssid=12345678901234567890123456789012")); // Tests max string length
	RUN(test_post(400, "ssid=123456789012345678901234567890123")); // Tests overflowing string length

	// HTTP POST body integer value
	RUN(test_post(200, "dest=")); // Tests empty uint8 value
	RUN(test_post(200, "dest=12")); // Tests valid uint8 value
	RUN(test_post_segment(200, "des", "t=12")); // Tests segmented key in valid uint8 value
	RUN(test_post_segment(200, "dest=1", "2")); // Tests segmented value in valid uint8 value
	RUN(test_post(200, "dest=1&")); // Tests terminator as last character
	RUN(test_post(200, "psk=1&dest=1")); // Tests concatenated uint8 property before
	RUN(test_post(200, "dest=1&psk=1")); // Tests concatenated uint8 property after
	RUN(test_post(400, "dest=abc123")); // Tests invalid characters in uint8
	RUN(test_post_segment(400, "dest=ab", "c123")); // Tests segmented invalid characters in uint8
	RUN(test_post(400, "dest=256")); // Tests overflowing int
	RUN(test_post(400, "dest=0")); // Tests int under min
	RUN(test_post(400, "dest=255")); // Tests int over max
	RUN(test_post(200, "dest=254")); // Tests max integer value
	RUN(test_post(200, "dest=1")); // Tests min integer value

	// HTTP POST body IPV4 value
	RUN(test_post(200, "atem=")); // Tests empty ipv4 value
	RUN(test_post(200, "atem=192.168.1.240")); // Tests valid ipv4 value
	RUN(test_post_segment(200, "ate", "m=192.168.1.240")); // Tests segmented key in valid ipv4 value
	RUN(test_post_segment(200, "atem=1", "92.168.1.240")); // Tests segmented value in valid ipv4 value
	RUN(test_post(200, "atem=192.168.1.240&")); // Tests terminator as last character
	RUN(test_post(200, "ipmask=255.255.255.0&atem=192.168.1.240")); // Tests concatennated ipv4 property
	RUN(test_post(400, "atem=19a")); // Tests invalid character in ipv4 short
	RUN(test_post(400, "atem=19a.168.1.240")); // Tests invalid character in ipv4 full
	RUN(test_post_segment(400, "atem=19a", ".168.1.240")); // Tests segmented invalid characters in ipv4
	RUN(test_post(400, "atem=192.168.1.24b")); // Tests invalid character in ipv4 last octet
	RUN(test_post(400, "atem=256.168.1.240")); // Tests overflowing octet
	RUN(test_post(400, "atem=192.168.1.240.1")); // Tests too many ip segments

	// HTTP POST body flag value
	RUN(test_post(200, "static=")); // Tests empty flag value
	RUN(test_post(200, "static=1")); // Tests valid flag value (enable)
	RUN(test_post(200, "static=0")); // Tests valid flag value (disable)
	RUN(test_post_segment(200, "static=", "1")); // Tests segmented valid flag value
	RUN(test_post_segment(200, "stati", "c=1")); // Tests segmented key in valid flag value
	RUN(test_post(200, "static=1&")); // Tests terminator as last character
	RUN(test_post(200, "static=1&psk=1")); // Tests concatenated flag property before
	RUN(test_post(200, "psk=1&static=1")); // Tests concatenated flag property after
	RUN(test_post_segment(200, "static=1", "&psk=1")); // Tests segmented concatenated flag property
	RUN(test_post(400, "static=2")); // Tests invalid character in flag value
	RUN(test_post(400, "static=12")); // Tests too many characters
}
