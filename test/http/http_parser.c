#include <stdio.h> // printf
#include <assert.h> // assert
#include <errno.h> // errno, ECONNRESET

#include <sys/socket.h> // recv, ssize_t

#include "../utils/http_sock.h" // http_socket_create, http_socket_send, http_socket_recv_cmp_status_line, http_socket_recv_len, http_socket_close, http_socket_recv_close
#include "../utils/runner.h" // RUN_TEST



// Sends HTTP request expecting specified response code
void test_code(const char* req, int code) {
	int sock = http_socket_create();
	http_socket_send(sock, req);
	http_socket_recv_cmp_status_line(sock, code);
	while (http_socket_recv_len(sock));
	http_socket_close(sock);
	// printf("Test for %d success\n", code);
}

// Sends HTTP request in two segments expecting specified response code
void test_code_segment(const char* req1, const char* req2, int code) {
	int sock = http_socket_create();
	http_socket_send(sock, req1);
	http_socket_send(sock, req2);
	http_socket_recv_cmp_status_line(sock, code);
	while (http_socket_recv_len(sock));
	http_socket_close(sock);
	// printf("Test for %d (segmented) success\n", code);
}

// Sends HTTP request in to segments expecting specified response code and closed by server before send complete
void test_code_segment_reset(const char* req1, const char* req2, int code) {
	int sock = http_socket_create();
	http_socket_send(sock, req1);
	http_socket_send(sock, req2);

	// Flushes data in stream
	http_socket_recv_cmp_status_line(sock, code);
	char buf[1024];
	ssize_t recvLen;
	do {
		recvLen = recv(sock, buf, sizeof(buf), 0);
	} while (recvLen > 0);

	// Expecting last packet to be peer closed error
	assert(recvLen == -1);
	assert(errno == ECONNRESET);

	http_socket_close(sock);
	// printf("Test for %d (segmented, resettable) success\n", code);
}

// Sends HTTP request expecting timeout
void test_timeout(const char* req) {
	int sock = http_socket_create();
	http_socket_send(sock, req);
	http_socket_recv_close(sock);
	// printf("Test timed out as expected\n");
}



// Runs all tests verifying HTTP server works as expected
int main(void) {
	// HTTP GET method

	// Tests incomplete GET method
	RUN_TEST(test_timeout("GET "));

	// Tests invalid GET method short
	RUN_TEST(test_code("GEF", 405));

	// Tests segmented invalid GET method short
	RUN_TEST(test_code_segment("G", "EF", 405));

	// Tests invalid method full
	RUN_TEST(test_code("GEF / HTTP/1.1\r\n\r\n", 405));

	// Tests segmented invalid GET method full
	RUN_TEST(test_code_segment_reset("GEF ", "/ HTTP/1.1\r\n\r\n", 405));



	// HTTP GET paths

	// Tests successful GET request short
	RUN_TEST(test_code("GET /", 200));

	// Tests segmented successful GET request short
	RUN_TEST(test_code_segment("GET", " / ", 200));

	// Tests successful GET request full
	RUN_TEST(test_code("GET / HTTP/1.1\r\n\r\n", 200));

	// Tests segmented successful GET request full
	RUN_TEST(test_code_segment("GET / ", "HTTP/1.1\r\n\r\n", 200));



	// HTTP POST method

	// Tests incomplete POST method
	RUN_TEST(test_timeout("POST "));

	// Tests back compare method matching short
	RUN_TEST(test_code("GO", 405));

	// Tests segmented back compare method matching short
	RUN_TEST(test_code_segment("G", "O", 405));

	// Tests back comparing method matching full
	RUN_TEST(test_code("GOST / HTTP/1.1\r\n\r\n", 405));

	// Tests segmented back compare method matching full
	RUN_TEST(test_code_segment_reset("GOS", "T / HTTP/1.1\r\n\r\n", 405));

	// Tests invalid POST method short
	RUN_TEST(test_code("POSF", 405));

	// Tests segmented invalid POST method short
	RUN_TEST(test_code_segment("PO", "SF", 405));

	// Tests invalid POST method full
	RUN_TEST(test_code("POSF / HTTP/1.1\r\n\r\n", 405));

	// Tests segmented invalid POST method full
	RUN_TEST(test_code_segment_reset("POSF ", "/ HTTP/1.1\r\n\r\n", 405));



	// HTTP POST headers

	// Tests incomplete POST URI
	RUN_TEST(test_timeout("POST /"));

	// Tests POST with no headers
	RUN_TEST(test_code("POST / HTTP/1.1\r\n\r\n", 411));

	// Tests segmented in first CRLF
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r", "\n\r\n", 411));

	// Tests segmented in second CRLF
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\n\r", "\n", 411));

	// Tests missing content length header
	RUN_TEST(test_code("POST / HTTP/1.1\r\nContent-Type: test/plain\r\n\r\n", 411));

	// Tests content length key in value
	RUN_TEST(test_code("POST / HTTP/1.1\r\nCookie: Content-Length: 1\r\n\r\n", 411));

	// Tests invalid content length value
	RUN_TEST(test_code("POST / HTTP/1.1\r\nContent-Length: 123abc\r\n\r\n", 400));

	// Tests segmented invalid content length value
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\nCont", "ent-Length: 123abc\r\n\r\n", 400));

	// Tests waiting for data
	RUN_TEST(test_timeout("POST / HTTP/1.1\r\nContent-Length: 1\r\n\r\n"));

	// Tests max content length value
	RUN_TEST(test_timeout("POST / HTTP/1.1\r\nContent-Length: 2 147 483 647\r\n\r\n"));

	// Tests overflowing content length value
	RUN_TEST(test_code("POST / HTTP/1.1\r\nContent-Length: 2 147 483 648\r\n\r\n", 413));

	// Tests segmented overflowing content length value
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\nContent-Length: 2 147 483", "648", 413));

	// Tests multiple headers with content length being last
	RUN_TEST(test_code("POST / HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n", 200));

	// Tests segmented multiple headers with content length being last
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\nCon", "tent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n", 200));

	// Tests multiple headers with content length not being last
	RUN_TEST(test_code("POST / HTTP/1.1\r\nContent-Length: 0\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n", 200));

	// Tests segmented multiple headers with content length not being last
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\nContent-Length: 0\r\nCon", "tent-Type: application/x-www-form-urlencoded\r\n\r\n", 200));

	// Tests no body
	RUN_TEST(test_code("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n", 200));

	// Tests segmented no body
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\nContent-Length: 0\r", "\n\r\n", 200));

	// Tests segmented no body
	RUN_TEST(test_code_segment("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r", "\n", 200));



	printf("All tests successfully completed\n");
}
