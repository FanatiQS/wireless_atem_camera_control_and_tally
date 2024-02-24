#include <errno.h> // ECONNRESET
#include <string.h> // strlen

#include <unistd.h> // sleep

#include "./http_sock.h" // http_socket_create, http_socket_send_string, http_socket_recv_cmp_status, http_socket_recv_len, http_socket_close, http_socket_recv_close, http_socket_recv_error, http_socket_body_send_buffer, http_socket_body_send_string
#include "../utils/runner.h" // RUN_TEST, runner_exit



// Checks HTTP response
void test_recv_cmp_response(int sock, int code) {
	// Checks status line and flushes headers
	http_socket_recv_cmp_status(sock, code);

	// Does not check HTTP GET response
	if (code == 200) {
		while (http_socket_recv_len(sock));
	}
	// Checks body for errors
	else {
		http_socket_recv_cmp(sock, http_status(code));
	}

	// Closes socket
	http_socket_recv_close(sock);
	http_socket_close(sock);
}

// Delays next test to let device restart on successful POST
void test_delay(const char* req, int code) {
	if (memcmp("POST", req, 4) && code != 200) return;
	sleep(1);
}

// Sends HTTP request expecting specified response code
void test_code(const char* req, int code) {
	int sock = http_socket_create();
	http_socket_send_string(sock, req);
	test_recv_cmp_response(sock, code);
	test_delay(req, code);
}

// Sends HTTP request in two segments expecting specified response code
void test_code_segment(const char* req1, const char* req2, int code) {
	int sock = http_socket_create();
	http_socket_send_string(sock, req1);
	http_socket_send_string(sock, req2);
	test_recv_cmp_response(sock, code);
	test_delay(req1, code);
}

// Sends HTTP request in to segments expecting specified response code and closed by server before send complete
void test_code_segment_reset(const char* req1, const char* req2, int code) {
	int sock = http_socket_create();
	http_socket_send_string(sock, req1);
	http_socket_send_string(sock, req2);

	// Flushes data in stream
	http_socket_recv_cmp_status(sock, code);

	// Expecting last packet to be peer closed error
	http_socket_recv_flush(sock);
	http_socket_recv_error(sock, ECONNRESET);

	http_socket_close(sock);
}

// Sends HTTP request expecting timeout
void test_timeout(const char* req) {
	int sock = http_socket_create();
	http_socket_send_string(sock, req);
	http_socket_recv_close(sock);
}



// Sends HTTP POST request expecting error message
void test_body_err(const char* body, const char* errMsg) {
	int sock = http_socket_create();
	http_socket_body_send_string(sock, body);
	http_socket_recv_cmp_status(sock, 400);
	http_socket_recv_cmp(sock, errMsg);
	http_socket_close(sock);
}

// Sends segmented HTTP POST request expecting error message without closing
int test_body_err_segment_helper(const char* body1, const char* body2, const char* errMsg) {
	int sock = http_socket_create();
	http_socket_body_send_buffer(sock, body1, strlen(body1) + strlen(body2));
	http_socket_send_string(sock, body2);
	http_socket_recv_cmp_status(sock, 400);
	http_socket_recv_cmp(sock, errMsg);
	return sock;
}

// Sends segmented HTTP POST request expecting error message
void test_body_err_segment(const char* body1, const char* body2, const char* errMsg) {
	int sock = test_body_err_segment_helper(body1, body2, errMsg);
	http_socket_close(sock);
}

// Sends segmented HTTP POST request expecting error message and closed on first segment causing reset by peer error
void test_body_err_segment_reset(const char* body1, const char* body2, const char* errMsg) {
	int sock = test_body_err_segment_helper(body1, body2, errMsg);
	http_socket_recv_error(sock, ECONNRESET);
	http_socket_close(sock);
}



// Runs all tests verifying HTTP server works as expected
int main(void) {
	// HTTP unsupported method
	RUN_TEST() {
		test_code("PUT /", 405);
	}

	// HTTP GET method

	// Tests incomplete GET method
	RUN_TEST() {
		test_timeout("GET ");
	}

	// Tests invalid GET back compare method short
	RUN_TEST() {
		test_code("PE", 405);
	}

	// Tests segmented invalid GET back compare method short
	RUN_TEST() {
		test_code_segment("P", "E", 405);
	}

	// Tests invalid GET back comparing method full
	RUN_TEST() {
		test_code("PET / HTTP/1.1\r\n\r\n", 405);
	}

	// Tests segmented invalid GET back compare method full
	RUN_TEST() {
		test_code_segment_reset("PE", "T / HTTP/1.1\r\n\r\n", 405);
	}

	// Tests invalid GET method short
	RUN_TEST() {
		test_code("GEF", 405);
	}

	// Tests segmented invalid GET method short
	RUN_TEST() {
		test_code_segment("G", "EF", 405);
	}

	// Tests invalid method full
	RUN_TEST() {
		test_code("GEF / HTTP/1.1\r\n\r\n", 405);
	}

	// Tests segmented invalid GET method full
	RUN_TEST() {
		test_code_segment_reset("GEF ", "/ HTTP/1.1\r\n\r\n", 405);
	}



	// HTTP GET paths

	// Tests successful GET request short
	RUN_TEST() {
		test_code("GET /", 200);
	}

	// Tests segmented successful GET request short
	RUN_TEST() {
		test_code_segment("GET", " / ", 200);
	}

	// Tests successful GET request full
	RUN_TEST() {
		test_code("GET / HTTP/1.1\r\n\r\n", 200);
	}

	// Tests segmented successful GET request full
	RUN_TEST() {
		test_code_segment("GET / ", "HTTP/1.1\r\n\r\n", 200);
	}



	// HTTP POST method

	// Tests incomplete POST method
	RUN_TEST() {
		test_timeout("POST ");
	}

	// Tests invalid POST back compare method short
	RUN_TEST() {
		test_code("GO", 405);
	}

	// Tests segmented invlaid POST back compare method short
	RUN_TEST() {
		test_code_segment("G", "O", 405);
	}

	// Tests invalid POST back comparing method full
	RUN_TEST() {
		test_code("GOST / HTTP/1.1\r\n\r\n", 405);
	}

	// Tests segmented invalid POST back compare method full
	RUN_TEST() {
		test_code_segment_reset("GOS", "T / HTTP/1.1\r\n\r\n", 405);
	}

	// Tests invalid POST method short
	RUN_TEST() {
		test_code("POSF", 405);
	}

	// Tests segmented invalid POST method short
	RUN_TEST() {
		test_code_segment("PO", "SF", 405);
	}

	// Tests invalid POST method full
	RUN_TEST() {
		test_code("POSF / HTTP/1.1\r\n\r\n", 405);
	}

	// Tests segmented invalid POST method full
	RUN_TEST() {
		test_code_segment_reset("POSF ", "/ HTTP/1.1\r\n\r\n", 405);
	}



	// HTTP POST headers

	// Tests incomplete POST URI
	RUN_TEST() {
		test_timeout("POST /");
	}

	// Tests POST with no headers
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\n\r\n", 411);
	}

	// Tests segmented in first CRLF
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r", "\n\r\n", 411);
	}

	// Tests segmented in second CRLF
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\n\r", "\n", 411);
	}

	// Tests missing content length header
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nContent-Type: test/plain\r\n\r\n", 411);
	}

	// Tests content length key in value
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nCookie: Content-Length: 1\r\n\r\n", 411);
	}

	// Tests invalid content length value
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nContent-Length: 123abc\r\n\r\n", 400);
	}

	// Tests segmented invalid content length value
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\nCont", "ent-Length: 123abc\r\n\r\n", 400);
	}

	// Tests waiting for data
	RUN_TEST() {
		test_timeout("POST / HTTP/1.1\r\nContent-Length: 1\r\n\r\n");
	}

	// Tests max content length value
	RUN_TEST() {
		test_timeout("POST / HTTP/1.1\r\nContent-Length: 2 147 483 647\r\n\r\n");
	}

	// Tests overflowing content length value
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nContent-Length: 2 147 483 648\r\n\r\n", 413);
	}

	// Tests segmented overflowing content length value
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\nContent-Length: 2 147 483", "648", 413);
	}

	// Tests multiple headers with content length being last
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n", 200);
	}

	// Tests segmented multiple headers with content length being last
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\nCon", "tent-Type: application/x-www-form-urlencoded\r\nContent-Length: 0\r\n\r\n", 200);
	}

	// Tests multiple headers with content length not being last
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nContent-Length: 0\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\n", 200);
	}

	// Tests segmented multiple headers with content length not being last
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\nContent-Length: 0\r\nCon", "tent-Type: application/x-www-form-urlencoded\r\n\r\n", 200);
	}

	// Tests no body
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r\n", 200);
	}

	// Tests segmented no body
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\nContent-Length: 0\r", "\n\r\n", 200);
	}

	// Tests segmented no body
	RUN_TEST() {
		test_code_segment("POST / HTTP/1.1\r\nContent-Length: 0\r\n\r", "\n", 200);
	}

	// Tests unexpected but valid header key caseness
	RUN_TEST() {
		test_code("POST / HTTP/1.1\r\ncOnTeNt-leNGth: 0\r\n\r\n", 200);
	}



	// HTTP POST body key invalid

	// Tests invalid body key short
	RUN_TEST() {
		test_body_err("x", "Invalid POST body key");
	}

	// Tests invalid body key full
	RUN_TEST() {
		test_body_err("xyz=123", "Invalid POST body key");
	}

	// Tests segmented invalid body key full
	RUN_TEST() {
		test_body_err_segment_reset("x", "yz=123", "Invalid POST body key");
	}

	// Tests incomplete key
	RUN_TEST() {
		test_body_err("ssi", "Invalid POST body key");
	}

	// Tests back comparing body key
	RUN_TEST() {
		test_body_err("ssk=1", "Invalid POST body key");
	}

	// Tests segmented back comparing body key
	RUN_TEST() {
		test_body_err_segment("ss", "k=1", "Invalid POST body key");
	}



	// HTTP POST body invalid value

	// Tests overflowing string length
	RUN_TEST() {
		test_body_err("name=123456789012345678901234567890123", "String value too long");
	}
	RUN_TEST() {
		test_body_err("ssid=123456789012345678901234567890123", "String value too long");
	}
	RUN_TEST() {
		test_body_err("psk=12345678901234567890123456789012345678901234567890123456789012345", "String value too long");
	}

	// Tests invalid characters in uint8
	RUN_TEST() {
		test_body_err("dest=abc123", "Invalid character in integer");
	}

	// Tests segmented invalid characters in uint8
	RUN_TEST() {
		test_body_err_segment_reset("dest=ab", "c123", "Invalid character in integer");
	}

	// Tests overflowing int
	RUN_TEST() {
		test_body_err("dest=256", "Integer value out of range");
	}

	// Tests int under min
	RUN_TEST() {
		test_body_err("dest=0", "Integer value out of range");
	}

	// Tests int over max
	RUN_TEST() {
		test_body_err("dest=255", "Integer value out of range");
	}

	// Tests out-of-range dest value
	RUN_TEST() {
		test_body_err("dest=0", "Integer value out of range");
	}
	RUN_TEST() {
		test_body_err("dest=255", "Integer value out of range");
	}

	// Tests empty dest value
	RUN_TEST() {
		test_body_err("dest=", "Invalid empty integer");
	}

	// Tests invalid character in ipv4 short
	RUN_TEST() {
		test_body_err("atem=19a", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=19a", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=19a", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=19a", "Invalid IPV4 address");
	}

	// Tests invalid character in ipv4 full
	RUN_TEST() {
		test_body_err("atem=19a.168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=19a.168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=19a.168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=19a.168.1.240", "Invalid IPV4 address");
	}

	// Tests segmented invalid characters in ipv4
	RUN_TEST() {
		test_body_err_segment_reset("atem=19a", ".168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err_segment_reset("localip=19a", ".168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err_segment_reset("netmask=19a", ".168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err_segment_reset("gateway=19a", ".168.1.240", "Invalid IPV4 address");
	}

	// Tests invalid character in ipv4 last octet
	RUN_TEST() {
		test_body_err("atem=192.168.1.24b", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=192.168.1.24b", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=192.168.1.24b", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=192.168.1.24b", "Invalid IPV4 address");
	}

	// Tests overflowing octet
	RUN_TEST() {
		test_body_err("atem=256.168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=256.168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=256.168.1.240", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=256.168.1.240", "Invalid IPV4 address");
	}

	// Tests too many ip segments
	RUN_TEST() {
		test_body_err("atem=192.168.1.240.1", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=192.168.1.240.1", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=192.168.1.240.1", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=192.168.1.240.1", "Invalid IPV4 address");
	}

	// Tests too few ip segments
	RUN_TEST() {
		test_body_err("atem=192.168.1", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=192.168.1", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=192.168.1", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=192.168.1", "Invalid IPV4 address");
	}

	// Tests empty ipv4 value
	RUN_TEST() {
		test_body_err("atem=", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("localip=", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("netmask=", "Invalid IPV4 address");
	}
	RUN_TEST() {
		test_body_err("gateway=", "Invalid IPV4 address");
	}

	// Tests invalid character in flag value
	RUN_TEST() {
		test_body_err("dhcp=2", "Invalid character in boolean");
	}

	// Tests too many characters
	RUN_TEST() {
		test_body_err("dhcp=12", "Invalid character in boolean");
	}

	return runner_exit();
}
