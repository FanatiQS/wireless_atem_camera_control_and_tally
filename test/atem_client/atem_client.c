#include "./atem_client.h"

// Runs all base ATEM client tests
void atem_client(void) {
	atem_client_open();
	atem_client_close();
	atem_client_data();
}

// Runs all extended ATEM client tests
void atem_client_extended(void) {
	atem_client_open_extended();
	atem_client_close_extended();
	atem_client_data_extended();
}
