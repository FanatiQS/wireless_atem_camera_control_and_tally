#include "./utils/utils.h"

#ifdef MAIN
int main(void) {
	atem_header_init();
	atem_handshake_init();
	atem_acknowledge_init();

	MAIN();
	return runner_exit();
}
#endif // MAIN
