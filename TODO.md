# TODOs

## Required

### test/README.md
* Create a list of tests to run

### doc/atem_protocol.txt
* Add links to skaarhojs library, nrks library, openswitcher.org and so on

### src/atem.c
* Add functions/macros for converting atem tally data to sdi tally data (requires length and data pointer)



## Not required

### src/atem.c
* Is there a reason to actually process all packets dumping ATEM state after handshake before sending first acknowledge or send acknowledges for each packet?
* Find out why length of tally command does not match the number of indexes it has (tested 3 switchers)
* Add function/macro to validate the protocol version is tested

### src/atem.h
* Move tally status variable from client.c into the atem_t struct?
* Should there be a way to only read in header, synack and commands separately to not buffer entire packet at once? The biggest problem is that I don't know how big to make the buffer since commands could technically be up to 2^16 bytes long.

### test/client.c
* Should there be some kind of auto-detect to find a server with port 9910 open?
* Add incrementing packet id individually for send and recv?
* Maybe add feature for packet reordering? (probably not)

### test/server.c
* Add support for emulating a switcher
* GPIO tally on rpi for tally from other switchers
* Extend heartbeat interval?



## Research
* Find out why all clients on my computer disconnects sometimes (has kinda stopped now)
* Should warn messages be filtered out?



## Check other hardware
* Check what TlIn looks like on big switcher (check out `research/tally_cmd_len.md`)
* Put SDI to HDMI and HDMI to SDI converters between URSA and URSAs studio monitor. Does it still display its camera id?

### Camera test
* Figure out why Gain is 6db higher on the camera when updated wirelessly while over SDI it updates correctly
* Try updating luma mix



## Untested
* PTZ control is not tested at all, neither updating it from the switcher side or sending it to the camera.



## Future maybe plans
* Feature for device to chance aux input on switcher to be able to switch return feed on the camera. Was able to switch between 2 sdi feeds for return on monitor on LDK8000.
* Extract PTZ from ATEM and output it on a d-sub connector.



## Arduino implementation features
* Camera control and tally should transmit all data over SDI.
* Filter out tally and output pgm and pvw state for a specific camera on digital pints.
* Some kind of logging system. Maybe one that transmits logs over tcp to a log server?
* Connectors for tally should be some kind of 3+ pin connector. Maybe RJ11 or 3.5mm TRS.
* Have an LED to indicate connection to switcher status. Turn on light when receiving protocol version?
* Find a good way to set camera id.
	* Maybe reading data from SDI out to find camera id (studio viewfinder displays camera id)
	* Maybe set with rotary dial like skaarhojs devices
	* Maybe update over serial
	* Maybe update in web interface
	* Maybe have OLED and buttons on device
* Find a good way to handle unvalidated protocol versions
* Needs some way to see signal strength, either on server or client.
	* Maybe signal strength could be indicated over audio? An audio signal goes in to the camera representing the signal strength. That signal can be viewed on both the camera and in the switcher software.
	* The signal strength should be able to be indicated on the AP. That direction is probably better anyway since the AP would have a stronger signal than the device.
	* If there is an OLED on the client, it could display the signal strength there.
	* Signal strength for wifi is called RSSI and is measured in dB.
* Some way to connect another ESP to this client to allow wireless tally.
	* Another way is to create some kind of hub software to extend number of clients connected to the switcher.
* Needs to be able to configure these settings:
	* SSID
	* SSID passphrase
	* ATEM ip address
	* Camera ID



## Other platforms
* iOS
* Android
* Linux(rpi)
