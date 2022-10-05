// Include guard
#ifndef HANDSHAKE_SERVER_H
#define HANDSHAKE_SERVER_H

void handshake_test1();
void handshake_test2();
void handshake_test3();
void handshake_test4();
void handshake_test5();
void handshake_test6();
void handshake_test7();

#define HANDSHAKE_TESTS\
	GET_TEST(handshake_test1),\
	GET_TEST(handshake_test2),\
	GET_TEST(handshake_test3),\
	GET_TEST(handshake_test4),\
	GET_TEST(handshake_test5),\
	GET_TEST(handshake_test6),\
	GET_TEST(handshake_test7)

// Ends include guard
#endif
