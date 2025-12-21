#include "./atem_server.h"

// Runs all base ATEM server tests
void atem_server(void) {
	atem_server_open();
	atem_server_close();
	atem_server_data();
	atem_server_cc();
}

// Runs all exteended ATEM server tests
void atem_server_extended(void) {
	atem_server_open_extended();
	atem_server_data_extended();
}
