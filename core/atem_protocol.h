// Include guard
#ifndef ATEM_PROTOCOL_H
#define ATEM_PROTOCOL_H

#include "./atem.h" // ATEM_PACKET_LEN_MAX

// Mask to use when filtering out flags
#define ATEM_MASK_LEN_HIGH (ATEM_PACKET_LEN_MAX >> 8)

// ATEM protocol flags
#define ATEM_FLAG_ACKREQ  0x08
#define ATEM_FLAG_SYN     0x10
#define ATEM_FLAG_RETX    0x20
#define ATEM_FLAG_RETXREQ 0x40
#define ATEM_FLAG_ACK     0x80

// ATEM protocol indexes
#define ATEM_INDEX_FLAGS             0
#define ATEM_INDEX_LEN_HIGH          0
#define ATEM_INDEX_LEN_LOW           1
#define ATEM_INDEX_SESSIONID_HIGH    2
#define ATEM_INDEX_SESSIONID_LOW     3
#define ATEM_INDEX_ACKID_HIGH        4
#define ATEM_INDEX_ACKID_LOW         5
#define ATEM_INDEX_LOCALID_HIGH      6
#define ATEM_INDEX_LOCALID_LOW       7
#define ATEM_INDEX_UNKNOWNID_HIGH    8
#define ATEM_INDEX_UNKNOWNID_LOW     9
#define ATEM_INDEX_REMOTEID_HIGH     10
#define ATEM_INDEX_REMOTEID_LOW      11
#define ATEM_INDEX_OPCODE            12
#define ATEM_INDEX_NEWSESSIONID_HIGH 14
#define ATEM_INDEX_NEWSESSIONID_LOW  15

// ATEM protocol opcodes
#define ATEM_OPCODE_OPEN    0x01
#define ATEM_OPCODE_ACCEPT  0x02
#define ATEM_OPCODE_REJECT  0x03
#define ATEM_OPCODE_CLOSING 0x04
#define ATEM_OPCODE_CLOSED  0x05

// ATEM protocol lengths
#define ATEM_LEN_HEADER    12
#define ATEM_LEN_SYN       20
#define ATEM_LEN_ACK       12
#define ATEM_LEN_CMDHEADER 8

// ATEM remote id range mask
#define ATEM_LIMIT_REMOTEID 0x7fff

// Byte offset from start of command block to command name
#define ATEM_OFFSET_CMDNAME 4

// ATEM resends
#define ATEM_RESENDS         10
#define ATEM_RESENDS_CLOSING 1

// ATEM timings
#define ATEM_RESEND_TIME   200
#define ATEM_PING_INTERVAL 500

#endif // ATEM_PROTOCOL_H
