#include "./atem_header.h" // atem_header_init
#include "./atem_handshake.h" // atem_handshake_init
#include "./atem_acknowledge.h" // atem_acknowledge_init

int main(void) {
	atem_header_init();
	atem_handshake_init();
	atem_acknowledge_init();
	return 0;
}
