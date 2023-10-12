#include <stdio.h> // printf
#include <string.h> // strlen

#include "./http_sock.h" // http_socket_create, http_socket_body_send, http_socket_body_write, http_socket_recv_cmp_status, http_socket_recv_cmp, http_socket_close, http_socket_send
#include "../utils/runner.h" // RUN_TEST



// Sends HTTP POST request expecting success
void test_post_success(const char* body) {
	int sock = http_socket_create();
	http_socket_body_send(sock, body);
	http_socket_recv_cmp_status(sock, 200);
	http_socket_recv_cmp(sock, "success");
	http_socket_close(sock);
}

// Sends segmented HTTP POST request expecting success
void test_post_success_segment(const char* body1, const char* body2) {
	int sock = http_socket_create();
	http_socket_body_write(sock, body1, strlen(body1) + strlen(body2));
	http_socket_send(sock, body2);
	http_socket_recv_cmp_status(sock, 200);
	http_socket_recv_cmp(sock, "success");
	http_socket_close(sock);
}



// Runs all tests verifying HTTP server works as expected
int main(void) {
	// HTTP POST body string
	RUN_TEST(test_post_success("name=")); // Tests empty string value
	RUN_TEST(test_post_success("name=12")); // Tests valid string value
	RUN_TEST(test_post_success_segment("nam", "e=12")); // Tests segmented key in valid string value
	RUN_TEST(test_post_success_segment("name=1", "2")); // Tests segmented value in valid string value
	RUN_TEST(test_post_success("name=1&")); // Tests terminator as last character
	RUN_TEST(test_post_success("name=1&iplocal=1")); // Tests concatenated string property before
	RUN_TEST(test_post_success("iplocal=1&name=1")); // Tests concatenated string property after
	RUN_TEST(test_post_success("name=12345678901234567890123456789012")); // Tests max string length

	// HTTP POST body integer value
	RUN_TEST(test_post_success("dest=")); // Tests empty uint8 value
	RUN_TEST(test_post_success("dest=12")); // Tests valid uint8 value
	RUN_TEST(test_post_success_segment("des", "t=12")); // Tests segmented key in valid uint8 value
	RUN_TEST(test_post_success_segment("dest=1", "2")); // Tests segmented value in valid uint8 value
	RUN_TEST(test_post_success("dest=1&")); // Tests terminator as last character
	RUN_TEST(test_post_success("iplocal=1&dest=1")); // Tests concatenated uint8 property before
	RUN_TEST(test_post_success("dest=1&iplocal=1")); // Tests concatenated uint8 property after
	RUN_TEST(test_post_success("dest=254")); // Tests max integer value
	RUN_TEST(test_post_success("dest=1")); // Tests min integer value

	// HTTP POST body IPV4 value
	RUN_TEST(test_post_success("atem=")); // Tests empty ipv4 value
	RUN_TEST(test_post_success("atem=192.168.1.240")); // Tests valid ipv4 value
	RUN_TEST(test_post_success_segment("ate", "m=192.168.1.240")); // Tests segmented key in valid ipv4 value
	RUN_TEST(test_post_success_segment("atem=1", "92.168.1.240")); // Tests segmented value in valid ipv4 value
	RUN_TEST(test_post_success("atem=192.168.1.240&")); // Tests terminator as last character
	RUN_TEST(test_post_success("ipmask=255.255.255.0&atem=192.168.1.240")); // Tests concatennated ipv4 property

	// HTTP POST body flag value
	RUN_TEST(test_post_success("static=")); // Tests empty flag value
	// @todo RUN_TEST(test_post_success("static=1")); // Tests valid flag value (enable) // @todo this would make the device dissapear from the network
	RUN_TEST(test_post_success("static=0")); // Tests valid flag value (disable)
	RUN_TEST(test_post_success_segment("static=", "0")); // Tests segmented valid flag value
	RUN_TEST(test_post_success_segment("stati", "c=0")); // Tests segmented key in valid flag value
	RUN_TEST(test_post_success("static=0&")); // Tests terminator as last character
	RUN_TEST(test_post_success("static=0&dest=1")); // Tests concatenated flag property before
	RUN_TEST(test_post_success("dest=1&static=0")); // Tests concatenated flag property after
	RUN_TEST(test_post_success_segment("static=0", "&dest=1")); // Tests segmented concatenated flag property

	printf("All tests successfully completed\n");
}
