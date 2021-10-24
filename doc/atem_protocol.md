# Documentation of the ATEM protocol
The ATEM protocol is sent between Blackmagic ATEM switchers and their clients connecting over a UDP socket on port 9910.

### Other resources
Since it is a proprietary protocol it has been very helpful to read what others have found out and documented while reverse engineering it.
I have listed the ones that have been the most useful to me.
* Skaarhojs Arduino library
* Skaarhojs command documentation
* NRKs NodeJS library
* Documentation on openswitcher.org
* Documentation on node-atem
* Blackmagics camera control protocol documentation

# The header
The ATEM protocol header is always 12 bytes long.

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+---------+---------------------+-------------------------------+
|  flags  |         len         |           session id          |
+---------+---------------------+-------------------------------+
|         ack packet id         |         local packet id       |
+-------------------------------+-------------------------------+
|               ?               |        remote packet id       |
+-------------------------------+-------------------------------+
|                           payload...                          |
+---------------------------------------------------------------+
```

### Flags
###### 0x80: ack reply
Is set when the other party has acknowledged a packet that requested an ack.
It is also sent with the heartbeat for some reason even if an ack is not requested.
The packet that was acknowledged is the packet with the id "ack packet id"
###### 0x40: retransmit request
###### 0x20: is retransmit
###### 0x10: SYN
###### 0x08: ack request

### Length
The length part is 11 bytes long and represents the entire length of the packet including the header.
With 11 bytes, the packet length has a maximum length of 2048 bytes.

### Session id
* The first byte in the session id seems to be a flag indicating if the session id was given by the ATEM switcher.
* The session id is first randomly generated between 0x0000 and 0x7fff. When the handshake is complete, the server assigns the a new id that is between 0x8000 and 0xffff.
* In the handshake, the new session id the server assigns seems to be defined in byte 14 and 15 of the SYNACK but without the MSB set.
* ATEM can only keep a maximum of 5 connections open at a time. It uses the session id to know how many active connections it has.
* Server assigned session ids are incremented by 1 for every new client connected. Wraps around to 0x0000 when reached 0x7fff in handshake, or to 0x8000 from 0xffff in header.



# Handshake opcodes
* 0x01: Opening connection
* 0x02: Connection opened successfully
* 0x03: Connection failed to open
* 0x04: Closing connection (has been documented as a restart opcode by others, but I belive it is a closing opcode similar to how it works in the websocket protocol)
* 0x05: Connection closed
