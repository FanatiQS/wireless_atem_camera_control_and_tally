// Include guard
#ifndef ATEM_EXTRA_H
#define ATEM_EXTRA_H

// Lengths of components in ATEM packets
#define ATEM_LEN_SYN 20
#define ATEM_LEN_HEADER 12
#define ATEM_LEN_CMDHEADER 8

// Indexes for ATEM packet information
#define ATEM_INDEX_FLAGS 0
#define ATEM_INDEX_LEN_HIGH 0
#define ATEM_INDEX_LEN_LOW 1
#define ATEM_INDEX_SESSION_HIGH 2
#define ATEM_INDEX_SESSION_LOW 3
#define ATEM_INDEX_ACKID_HIGH 4
#define ATEM_INDEX_ACKID_LOW 5
#define ATEM_INDEX_LOCALID_HIGH 6
#define ATEM_INDEX_LOCALID_LOW 7
#define ATEM_INDEX_REMOTEID_HIGH 10
#define ATEM_INDEX_REMOTEID_LOW 11
#define ATEM_INDEX_OPCODE 12
#define ATEM_INDEX_NEW_SESSION_HIGH 14
#define ATEM_INDEX_NEW_SESSION_LOW 15

// ATEM flags
#define ATEM_FLAG_ACKREQUEST 0x08
#define ATEM_FLAG_SYN 0x10
#define ATEM_FLAG_RETRANSMIT 0x20
#define ATEM_FLAG_ACK 0x80

// ATEM connection types not defined in ../src/atem.h
#define ATEM_CONNECTION_OPENING 0x01
#define ATEM_CONNECTION_SUCCESS 0x02

// ATEM resends
#define ATEM_RESEND_OPENINGHANDSHAKE 10
#define ATEM_RESEND_CLOSINGHANDSHAKE 1

// ATEM timings
#define ATEM_PING_INTERVAL 500
#define ATEM_RESEND_TIME 100

// End include guard
#endif
