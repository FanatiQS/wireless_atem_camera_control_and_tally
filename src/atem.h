#include <stdbool.h>
#include <stdint.h>

// Include guard
#ifndef ATEM_H
#define ATEM_H

// The Blackmagic bluetooth UUIDs
#define ATEM_BLE_UUID_SERVICE "291D567A-6D75-11E6-8B77-86F30CA893D3"
#define ATEM_BLE_UUID_CHARACTERISTIC "5DD3465F-1AEE-4299-8493-D2ECA2F8E1BB"

// Commands in 32 bit integer format for easier comparison
#define ATEM_CMDNAME_CAMERACONTROL ('C' << 24) | ('C' << 16) | ('d' << 8) | 'P'
#define ATEM_CMDNAME_TALLY ('T' << 24) | ('l' << 16) | ('I' << 8) | 'n'
#define ATEM_CMDNAME_VERSION ('_' << 24) | ('v' << 16) | ('e' << 8) | 'r'

// ATEM opcodes available as return value from parseAtemData
#define ATEM_CONNECTION_OK 0
#define ATEM_CONNECTION_ERROR -1
#define ATEM_CONNECTION_REJECTED 0x03
#define ATEM_CONNECTION_CLOSING 0x04
#define ATEM_CONNECTION_CLOSED 0x05

// Default port for ATEM
#define ATEM_PORT 9910

// Number of seconds before switcher kills the connection for no acknowledge sent
#define ATEM_TIMEOUT 5

// Contains incoming and outgoing ATEM socket data
#define ATEM_MAX_PACKET_LEN 2048
struct atem_t {
typedef struct atem_t {
	uint16_t dest;
	bool pvwTally;
	bool pgmTally;
	uint8_t readBuf[ATEM_MAX_PACKET_LEN];
	uint16_t readLen;
	uint8_t *writeBuf;
	uint16_t writeLen;
	uint8_t *cmdBuf;
	uint16_t cmdLen;
	uint16_t cmdIndex;
	uint16_t lastRemoteId;
} atem_t;

// Function prototypes
#ifdef __cplusplus
extern "C" {
#endif
void resetAtemState(struct atem_t *atem);
void closeAtemConnection(struct atem_t *atem);
int8_t parseAtemData(struct atem_t *atem);
uint32_t nextAtemCommand(struct atem_t *atem);
bool tallyHasUpdated(struct atem_t *atem);
void translateAtemTally(struct atem_t *atem);
void translateAtemCameraControl(struct atem_t *atem);
#ifdef __cplusplus
}
#endif

// Gets boolean indicating if there are more commands to parse
#define hasAtemCommand(atem) ((atem)->cmdIndex < (atem)->readLen)

// Gets the major and minor version of protocol for version command
#define protocolVersionMajor(atem) ((atem)->cmdBuf[0] << 8 | (atem)->cmdBuf[1])
#define protocolVersionMinor(atem) ((atem)->cmdBuf[2] << 8 | (atem)->cmdBuf[3])

// Gets the camera destination number for a camera control command
#define getAtemCameraControlDest(atem) ((atem)->cmdBuf[0])

// Ends include guard
#endif
