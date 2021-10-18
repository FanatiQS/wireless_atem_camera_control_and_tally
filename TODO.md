# TODOs

## Required

### test/README.md
* Create a list of tests to run

### doc/atem_protocol.txt
* Add links to skaarhojs library, nrks library, openswitcher.org and so on

### src/atem.c
* Add functions/macros for converting atem tally data to sdi tally data (requires length and data pointer)
* Add function/macro to validate the protocol version is tested
* Find out why length of tally command does not match the number of indexes it has (tested 3 switchers)



## Not required

### src/atem.c
* Is there a reason to actually process all packets dumping ATEM state after handshake before sending first acknowledge or send acknowledges for each packet?

### src/atem.h
* Move tally status variable from client.c into the atem_t struct?
* Should there be a way to only read in header, synack and commands separately to not buffer entire packet at once? The biggest problem is that I don't know how big to make the buffer since commands could technically be up to 2^16 bytes long.

### test/client.c
* Should there be some kind of auto-detect to find a server with port 9910 open?
* Use ATEM_CMDNAME_VERSION to indicate connection status
* Add incrementing packet id individually for send and recv?
* Maybe add feature for packet reordering? (probably not)

### test/server.c
* Add support for emulating a switcher
* GPIO tally on rpi for tally from other switchers
* Extend heartbeat interval?



## Research
* Find out why all clients on my computer disconnects sometimes (has kinda stopped now)
* Should warn messages be filtered out?
* What happens when assigned session id passes 0x8fff. Would probably wrap around to 0x8000.



## Check other hardware
* Check if CCdP parameters sent are the same with control software and hardware panel
* Check what TlIn looks like on big switcher (make sure length and index work correctly)
* Put SDI to HDMI and HDMI to SDI converters between URSA and URSAs studio monitor. Does it still display its camera id?

### hardware tests more priority
* Test if camera updates gain in same step size as my bluetooth code when connected over SDI.
	* Currently only updates -6, 0, 6, 12, 18
* Update initial state (states chanced when software was not connected)
	* Try only relaying packets for selected camera to not overload initial bluetooth gatt writes.
* Try updating luma mix



## Untested
* PTZ control is not tested at all, neither updating it from the switcher side or sending it to the camera.



## Future maybe plans
* Feature for device to chance aux input on switcher to be able to switch return feed on the camera. Was able to switch between 2 sdi feeds for return on monitor on LDK8000.
* Extract PTZ from ATEM and output it on a d-sub connector.
