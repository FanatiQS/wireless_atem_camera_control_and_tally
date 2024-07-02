// Include guard
#ifndef ATEM_COMMAND_H
#define ATEM_COMMAND_H

#include <stdint.h> // uint8_t, uint16_t

void atem_command_append(uint8_t* packet, char* cmd_name, void* payload, uint16_t len);
void atem_command_send(
	int sock,
	uint16_t session_id, uint16_t remote_id,
	char* cmd_name, void* payload, uint16_t len
);
void atem_command_queue(int sock, uint8_t* packet, char* cmd_name, void* payload, uint16_t len);

#endif // ATEM_COMMAND_H
