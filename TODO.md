# TODOs

## Required

### test/README.md
* Create a list of tests to run

### doc/atem_protocol.txt
* Add links to skaarhojs library, nrks library, openswitcher.org and so on

### src/atem.c
* Create function to convert atem camera control protocol to sdi camera control protocol (requires length and data pointer)
* Add functions/macros for converting atem tally data to sdi tally data (requires length and data pointer)

### test/client.c
* Add packet delay feature?



## Not required

### src/atem.h
* Move tally status variable from client.c into the atem_t struct?
* Should there be a way to only read in header, synack and commands separately to not buffer entire packet at once? The biggest problem is that I don't know how big to make the buffer since commands could technically be up to 2^16 bytes long.

### test/client.c
* Should there be some kind of auto-detect to find a server with port 9910 open?
* Use ATEM_CMDNAME_VERSION to indicate connection status
* Add incrementing packet id individually for send and recv?
* Maybe add feature for packet reordering? (probably not)
* Add feature to customise how many bytes recv and cmds are clamped to

### test/server.c
* Add support for emulating a switcher
* GPIO tally on rpi for tally from other switchers
* Extend heartbeat interval?



## Research
* Find out why all clients on my computer disconnects sometimes (has kinda stopped now)
* Should warn messages be filtered out?
* Is ATEM maxing out on session ids or sockets? If it is sockets, I could write a proxy software that just sends content to switcher and makes it much easier to dump all atem states since there will not be any need to buffer it in the proxy software.



## Check other hardware
* Check if CCdP parameters sent are the same with control software and hardware panel
* Check what TlIn looks like on big switcher (make sure length and index work correctly)
* Test connecting with bluetooth to the camera and send payload
* Put SDI to HDMI and HDMI to SDI converters between URSA and URSAs studio monitor. Does it still display its camera id?
