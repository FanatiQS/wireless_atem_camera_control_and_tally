#include "./atem_header.h" // atem_header_init
#include "./atem_handshake.h" // atem_handshake_init

int main(void) {
	atem_header_init();
	atem_handshake_init();
	return 0;
}
