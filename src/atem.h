#include <stdbool.h>
#include <stdint.h>

// Include guard
#ifndef ATEM_H
#define ATEM_H

// Commands in 32 bit integer format for easier comparison
#define ATEM_CMDNAME_CAMERACONTROL ('C' << 24) | ('C' << 16) | ('d' << 8) | 'P'
#define ATEM_CMDNAME_TALLY ('T' << 24) | ('l' << 16) | ('I' << 8) | 'n'
#define ATEM_CMDNAME_VERSION ('_' << 24) | ('v' << 16) | ('e' << 8) | 'r'

// Tally flags
#define ATEM_TALLY_PGM 0x01
#define ATEM_TALLY_PVW 0x02

// Default port for ATEM
#define ATEM_PORT 9910

// Contains incoming and outgoing ATEM socket data
#define ATEM_MAX_PACKET_LEN 2048
struct atem_t {
	uint8_t readBuf[ATEM_MAX_PACKET_LEN];
	uint16_t readLen;
	uint8_t *writeBuf;
	uint16_t writeLen;
	uint8_t *cmdBuf;
	uint16_t cmdLen;
	uint16_t cmdIndex;
	uint16_t lastRemoteId;
};

// Function prototypes
void resetAtemState(struct atem_t *atem);
int8_t parseAtemData(struct atem_t *atem);
uint32_t parseAtemCommand(struct atem_t *atem);
int8_t parseAtemTally(struct atem_t *atem, uint16_t index, uint8_t *tally);
void translateAtemTally(struct atem_t *atem);
void translateAtemCameraControl(struct atem_t *atem);

// Gets boolean indicating if there are more commands to parse
#define hasAtemCommand(atem) ((atem)->cmdIndex < (atem)->readLen)

// Ends include guard
#endif
