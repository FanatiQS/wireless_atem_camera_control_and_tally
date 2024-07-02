# Documentation of the ATEM protocol
The ATEM protocol is a binary proprietary protocol used by Blackmagic Design to control their ATEM switchers over the network.
It is based on [UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol) and a client-server architecture where the server is available on port 9910 of the ATEM switchers.

The protocol starts off with an [opening handshake](#opening-handshake) initiated by the client.
After that, data can be [sent](#sending-commands) to and [received](#receiving-commands) from the server.
It uses a constant and frequent [heartbeat](#heartbeat) to ensure the clients are still alive and has a [closing handshake](#closing-handshake) to properly disconnect.

## Other resources
Since it is a proprietary protocol it has been very helpful to read what others have found and documented while reverse engineering it.
Here I have listed the resources that have been the most helpful to me.
* [Skaarhojs Arduino library](https://github.com/kasperskaarhoj/SKAARHOJ-Open-Engineering/tree/master/ArduinoLibs/ATEMbase)
* [Skaarhojs command documentation](https://www.skaarhoj.com/discover/blackmagic-atem-switcher-protocol)
* [NRKs NodeJS library](https://github.com/nrkno/tv-automation-atem-connection)
* [Documentation on openswitcher.org](https://docs.openswitcher.org/udptransport.html)
* [Documentation on node-atem](https://github.com/miyukki/node-atem/blob/master/specification.md)
* [Blackmagics camera control protocol documentation](https://documents.blackmagicdesign.com/DeveloperManuals/BlackmagicCameraControl.pdf)

## Testing
This documentation can be confirmed to be accurate with the test suite.
If Blackmagic would change something about the protocol in future firmware versions, the tests are designed to detect that and help keeping the documentation up-to-date.

## Data structure
The ATEM protocol has a fairly basic structure.
It consists of a required 12 byte header with an optional variable length payload.

```
 0               1               2               3
 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0
+ ------- + ------------------- + ----------------------------- +
|  flags  |       length        |           session id          |
+ ------- + ------------------- + ----------------------------- +
|             ack id            |         local packet id       |
+ ----------------------------- + ----------------------------- +
|           unknown id          |        remote packet id       |
+ ----------------------------- + ----------------------------- +
|                           payload...                          |
+ ------------------------------------------------------------- +
```

### Flags
The first 5 bits of the ATEM protocol are flags that play a vital role in identifying what parts of the header contain meaningful information and how the payload should be parsed if available.

#### Acknowledge request flag: *0x08*
Undocumented

#### SYN flag: *0x10*
The SYN flag is set for SYN packets used in the [opening handshake](#opening-handshake) and [closing handshake](#closing-handshake) and can only be combined with the [retransmit flag](#retransmit-flag-0x20).
A SYN packet has an 8 byte payload, giving it a total [length](#length) of 20 bytes including the header, where the first byte of the payload is the [opcode](#opcode).

#### Retransmit flag: *0x20*
The retransmit flag is an additive flag that gets added to a packet that is being [retransmitted](#retransmitting-packets), indicating to the receiving side that this is not the first time this packet is sent.
More details on when retransmits occur can be found in [this](#retransmitting-packets) section.

#### Retransmit request flag: *0x40*
Undocumented

#### Acknowledge flag: *0x80*
Undocumented

### Length
This indicates the length of the packet.
It is defined with 11 bits, giving it a maximum packet length of 2047 bytes, including the 12 byte header.
The minimum packet length is 12 bytes when no payload is available.

### Session id
The ATEM protocol uses a session id to be connection-oriented.
This is necessary since it operates over [UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol), which is inherently connectionless at the [transport level](https://en.wikipedia.org/wiki/Transport_layer), unlike [TCP](https://en.wikipedia.org/wiki/Transmission_Control_Protocol).

The session id is used slighly differently during the [opening handshake](#opening-handshake) and for all furtuer communication.
During the opening handshake, the session id is randomly assigned by the client to any value between 0x0000 and 0x7fff.
Since the [MSB](https://en.wikipedia.org/wiki/Bit_numbering) is clear for these values, this indicates that it is a session id assigned by the client.
In the [opening handshake](#opening-handshake), the server assigns the client a server assigned session id, also with a value between between 0x0000 and 0x7fff, that should always be used with the [MSB](https://en.wikipedia.org/wiki/Bit_numbering) set to indicate it was assigned by the server.
This results in a practical server assigned session id value between 0x8000 and 0xffff.
All further communication after the [opening handshake](#opening-handshake) is completed should use the new server assigned session id.

When the server assignes a session id, it uses a continuously incrementing counter, wrapping around after 0x7fff back to 0x0000.
The server assigned session id counter starts at 0x0001 instead of 0x0000 and is not persistent, so if the switcher looses power or is rebooted the counter resets.

### Acknowledgement packet id
Undocumented

### Local packet id
Undocumented

### Unknown id
This unknown id is not an incrementing id in the same way the local and packet ids are.
The high byte seems to always be clear and it seems no one knows what these two bytes are used for, even though they are used.
Keeping them both set to 0 also seems to work, even though that is not what the ATEM software control does.

### Remote packet id
Undocumented

### Payload
The payload is the remaining length of the packet after the header.
A packet is allowed to have a payload length of 0 (total packet [length](#length) of 12 bytes) indicating there is no payload available after the header.
The data the payload contains depends on the [flags](#flags) that are set.

### Opcode
Opcodes are available in SYN packets that are defined by the [SYN flag](#syn-flag-0x10) being set.
It is located at the first byte of the SYN packets payload.

#### Opening opcode 0x01
This opcode is sent by the client to initiate an [opening handshake](#opening-handshake) and is always used with a [client assigned session id](#session-id).

#### Accepted opcode 0x02
This opcode, together with the [rejected](#rejected-opcode-0x03) opcode, is one of two possible opcodes received from the server during an [opening handshake](#opening-handshake).
It indicates that the client was successfully connected to the ATEM switcher.
The third and fourth byte of the payload is a 16 bit integer indicates the [server assigned session id](#session-id) that should be used for everything other than the [opening handshake](#opening-handshake).
The [server assigned session id](#session-id) has the [MSB](https://en.wikipedia.org/wiki/Bit_numbering) clear when received during the [opening handshake](#opening-handshake), but when used for future communication with the server, the bit should always be set.

#### Rejected opcode 0x03
This opcode, together with the [accepted](#accepted-opcode-0x02) opcode, is one of two possible opcodes received from the server during an [opening handshake](#opening-handshake).
It indicates that the clients request to connect to the server has failed failed.

#### Closing opcode 0x04
This opcode is used when initiating a [closing handshake](#closing-handshake).

#### Closed opcode 0x05
This opcode is used during the [closing handshake](#closing-handshake) as a response to a [closing request](#closing-opcode-0x04), informing the closing initiator that the request was received and that the connection has been terminated.

## Opening handshake
When connecting to an ATEM switcher, it all starts with an opening handshake.
The ATEM protocol uses a 3-way opening handshake, very similar to the [TCP handshake](https://developer.mozilla.org/en-US/docs/Glossary/TCP_handshake), consisting of a SYN packet, a SYN/ACK packet and lastly an ACK packet.
This section goes through these packets in detail and explains how an opening handshake is performed.

The first packet in the handshake, the [SYN packet](#syn-flag-0x10), is sent by the client with the [opcode](#opcode) set to indicate it wants to [open](#opening-opcode-0x01) a new session.
It uses a randomly generated [client assigned session id](#session-id) that should be used for all packets during the opening handshake.
The ATEM server responds to that request with a SYNACK packet (also has the [SYN flag](#syn-flag-0x10) set like the SYN packet) where the [opcode](#opcode) either indicates that the connection was a [success](#successful-opcode-0x02) or that it was [rejected](rejected-opcode-0x03).
In the case that the client does not receive a response in a timely manner, it should [resend](#retransmit-flag-0x20) the packet, normally a maximum of 10 times before [closing the session](#closing-handshake).
If a handshake is rejected, it is not going to receive any more data and does not require sending anything in response.
A successful handshake on the other hand has to be responded to with an ACK packet to complete the 3-way handshake.
It is also here, in the success response, we get the [server assigned session id](#session-id).
The ACK for the SYNACK is the last packet to use the [client assigned session id](#session-id) and all future packets should use the new [server assigned session id](#session-id).
Normally, [ACK packets](#ack-flag-0x80) are only sent as a response to packets [requesting acknowledgement](#ack-request-flag-0x80), but during the handshake it is sent without specifically being requested as a response to the [success opcode](#success-opcode-0x02).
The [ACK packet](#ack-flag-0x80) does not have a payload, giving it a packet [length](#length) of only 12 bytes for the required header.
If the client does not acknowledge the SYNACK by sending the ACK in a timely manner, or the packet gets lost on the way, the ATEM server will resend the SYNACK packet again, this time with the [resend flag](#retransmit-flag-0x20) set.
The ATEM server will resend the SYNACK packet 10 times before trying to [close the connection](#closing-handshake).

After these 3 packets, the opening handshake is considered complete.
From this point forward, the [client assigned session id](#session-id) should not be used.
Instead, the [server assigned session id](#session-id), that was received from the [accept response](#accepted-opcode-0x02), should be used, but with the [MSB](https://en.wikipedia.org/wiki/Bit_numbering) set.
The server will also at this point send all of its internal states to the client.
These states will be spread out over multiple packets as it does not fit within the maximum [length](#length) of an ATEM packet.
Whenever an internal state changes, the client will receive the updated state as long as the connection is [kept alive](#heartbeat).
Read more about receiving commands from the switcher [here](#receiving-commands).

### Example
In this example, the client randomly generated the [client assigned session id](#session-id) 0x7440 and the [server assigned session id](#session-id) is 0x0001.

```
                         header                          payload
        + ----------------------------------- + ----------------------- +
Client: | 10 14 74 40 00 00 00 00 00 00 00 00 | 01 00 00 00 00 00 00 00 |
        + ----------------------------------- + ----------------------- +

        + ----------------------------------- + ----------------------- +
Server: | 10 14 74 40 00 00 00 00 00 00 00 00 | 02 00 00 01 00 00 00 00 |
        + ----------------------------------- + ----------------------- +

        + ----------------------------------- +
Client: | 80 0c 74 40 00 00 00 00 00 00 00 00 |
        + ----------------------------------- +

```

## Closing handshake
A closing handshake starts when either the server or the client sends a [SYN packet](#syn-flag-0x10) with the [opcode](#opcode) set to [closing](#closing-opcode-0x04).
When the other side receives the closing request, it needs to respond to it with a [SYN packet](#syn-flag-0x10) that has the [opcode](#opcode) set to [closed](#closed-opcode-0x05).
Only after receiving either of these opcodes should the session be considered terminated.
In the case that the initiator does not receive a response in a timely manner, it is allowed to [resend](#retransmit-flag-0x20) the packet.
The ATEM server [resends](#retransmit-flag-0x20) its closing request a maximum of one time while the [ATEM software control](https://www.blackmagicdesign.com/se/products/atemmini/software) doesn't [resend](#retransmit-flag-0x20) at all.
If the response packet is lost, the peer will have closed the session while the initiater is still allowed to [retransmit](#retransmit-flag-0x20) the closing request, resulting in the closing response not being completely dependable.

The [session id](#session-id) in a closing handshake always uses the [server assigned session id](#session-id), even if the connection is closed during the [opening handshake](#opening-handshake).
This means that the session can not be closed before the opening handshake accept response is received.

Any data received after either side has initiated the closing handshake should be ignored.

### Example
In this example, the [session id](#session-id) received from the server in the [opening handshake](#opening-handshake) is 0x0001.

```
                            header                          payload
           + ----------------------------------- + ----------------------- +
Initiator: | 10 14 80 01 00 00 00 00 00 00 00 00 | 04 00 00 00 00 00 00 00 |
           + ----------------------------------- + ----------------------- +

           + ----------------------------------- + ----------------------- +
Responder: | 10 14 80 01 00 00 00 00 00 00 00 00 | 05 00 00 00 00 00 00 00 |
           + ----------------------------------- + ----------------------- +

```

## Retransmitting packets
Every packet have a maximum number of [resends](#retransmit-flag-0x20), depending on what type of packet it is.
When that limit is reached, a [closing handshake](#closing-handshake) should be initiated.
It might sound unnecessary to start a [closing handshake](#closing-handshake) if the peer doesn't respond, rather than just dropping the session right away.
However, sometimes packets are not really dropped but are instead delayed.
In that case, when the peer later receives all the delayed packets, the [closing request](#closing-handshake) lets it know that the session has been terminated.

## Receiving commands
Undocumented

## Sending commands
Undocumented

## Heartbeat
Undocumented

<!--
# Acknowledgements
* Acknowledgements can be required by both the server and the client.
* When an acknowledgement is required, the "ack request" (0x08) flag is set.
* If other side does not acknowledge a packet that requires an acknowledgement, it should be resent.
* ATEM resends packets fairly quickly, so they might not even have arrived at the client before ATEM resends the packet.
* Resending packets is essential since UDP does not guarantee the packet will not get lost on the way.
* When a packet is resent, the flag "is retransmit" (0x20) is set. It seems to work even if this flag is not set though.
* An acknowledge request has the "remote packet id" set. It is an incrementing number local to the session.
* An acknowledgement is done in response to an acknowlege request and sets the "ack packet id" to the value of "remote packet id".



# Heartbeat
* ATEM uses a constant heartbeat to know when a connection is lost.
* Both the server and client ping the other end about twice a second.
* The ping packet has both the "ack request" (0x08) and "ack reply" 0x80 flags set.
* The length of the packet is just 12 bytes, so there is no data sent.



# Commands
* After the handshake, all data after the header is command data.
If packet is only the length of the header, there is no command data.
* A packet can contain multiple commands concatenated together within the max payload length of the packet.
* Commands consist of a length (2 bytes), padding (2 bytes), a command name (4 bytes) and comand data (n bytes).

```
 0                   1                   2
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-------------------------------+-------------------------------+
|        command length         |            padding            |
+-------------------------------+-------------------------------+
|                         command name                          |
+---------------------------------------------------------------+
|                        command data...                        |
+---------------------------------------------------------------+
```

## Command length
* The command length is the length of the command data, including the command header.

## Command name
* The command names are 4 ASCII characters and a compiled list can be found [here](https://www.skaarhoj.com/discover/blackmagic-atem-switcher-protocol).

## Command data
* Length of command data is the same as the value of `command length`.
* Content depends on the command name.

# Timeout
* The ATEM switcher seems to kill connections after not receiving acknowledgements for about 4 seconds.
-->

