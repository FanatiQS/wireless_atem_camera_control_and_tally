# TODOs

## Required

### src/atem.c
* Add BLE bmd uuid and cc characteristic as macros
* Test function for converting atem tally data to sdi tally data
	* This function has not been tested since I have not had the opertunity to send it to a camera.
* Figure out why ATEM reconnect times out after 10 retries sometimes
	* Has experienced that my test client retries to connect to the switcher but stops retrying after the 10th retry. I have no idea when this is.

### /readme.md
* Add notes about what version of control software was tested?
	* I have been testing with 8.6.3 so far
* Add some info about hardware control panel working but not been fully tested.
* Document camera id 0 is invalid and does not work (gives PGM tally always if switcher has odd number of sources and never PVW tally)



## Not required

### test/
* Try to automate some tests
	* Some tests can not be automated since they require interacting with external hardware.

### src/atem.c
* Add optional functions to allow getting tally for multiple destinations.
	* `tallyHasUpdated` has previous tally states built in to the struct, that only allows the struct to check for a single destination. There are situations where it is required to keep track of multiple destinations.
* Is there a reason to actually process all packets dumping ATEM state after handshake before sending first acknowledge or send acknowledges for each packet?
	* All other atem libraries process the incoming packets that get sent right after the handshake together.
	Basically they send 1 acknowledgement for all the packets at once instead of sending one for each packet.
	This is not what I am doint. I send an acknowledgement for each packet.
* Add function/macro to validate the protocol version is tested
	* When the protocol gets an update in the future, there should be something available to know what versions of the protocol this library is compatible with.

### src/atem.h
* Should there be a way to only read in header, synack and commands separately to not buffer entire packet at once?
	* The biggest problem is that I don't know how big to make the buffer since commands could technically be up to 2^16 bytes long.
	* The main reason to do something like this is to add support for devices with less memory.

### test/client.c
* Add incrementing packet id individually for send and recv?
* Maybe add feature for packet reordering? (probably not)
	* Getting the packets in the wrong order is not tested, but it should not be an issue, hopefully.

### test/server.c
* The main feature of the server should be to redirect only the important data from the packets to other clients.
	* So it should grab the camera control and tally data and put it in new valid atem packets to send.
	* Should the server know what ids the cameras have and only send the data to devices listening that camera id? This is fairly easy for camera control as it already has a "send to all" id with 255. Tally does not and would probably require sending data with the same tally info for every camera since no matter what id the device has, it will grab the same tally info. This would obviously require some kind of configuration system where devices, or more accurately, their UDP sockets on the server are connected to a specific camera id.
	* Does this feature require the server to buffer all camera control data for all cameras?
* Add support for emulating a switcher
	* Lets not just clients running my library connect, but also the control software and hardware.
* GPIO tally on rpi for tally from other switchers
* Extend heartbeat interval?
* Another feature of the server thing is that it reduces the data sent over wifi
	* This is probably the most important feature since it reduces stress on the network.



## Research
* Find out why all clients on my computer disconnects sometimes (has kinda stopped now)
	* It was probably just my computer being slow to process the UDP packets with so much other network traffic going on.



## Check other hardware
* Put SDI to HDMI and HDMI to SDI converters between URSA and URSAs studio monitor. Does it still display its camera id?
	* The idea for this was to get the camera id set on the camera into my code by grabbing it from the sdi out.
	* This is not important and migh be a stupid way of handling it even if it works.

### Camera test
* Try updating luma mix
	* I tried this but did not understand how lumamix works. Tested over both BLE and SDI.
* Test if ND works, requires Blackmagic Pocket Cinema Camera 6k Pro
* Figure out why message grouping does not work.
	* Is it just an SDI feature but not documented as such?
* Test if BMPCC4K updates with `0x01 0x0d` at all and if it does, does it update the same as URSA Broadcast with `0x01 0x01`?
	* There should be more details about the issue in `research/gain_issue.md`.
* PTZ control sending to the camera.
	* Have tested that my library receives the data from the switcher but have not been able to send that data on to a camera to test that it is correct.



## Future maybe plans
* Feature for device to change aux input on switcher to be able to switch return feed to another source on the camera.
	* Was able to switch between 2 sdi feeds for return on monitor on LDK8000.
* Extract PTZ from ATEM and output it on a d-sub connector.
	* That would probably be RS422/RS485/RS232 protocol based to work with other devices.
* Send PTZ data to gimbals.
	* This is a cool feature I heard some other product had.
	* There are some APIs for gimbals.
* Add HDMI CEC capability to support BMPCC over HDMI to not require bluetooth.
	* HDMI CEC seems to be slow enough to work on the ESP. Bit-banging might work.
	* Raspberry pi has support for outputting CEC on micro HDMI using `cec-client`.
* Add feature to read input voltage to SDI shield.
	* This requires a voltage divider if 3.3v is always 3.3v. With 1kohm (R1) and 5.1kohm (R2), 20.13v would be reduced to exactly 3.3v.



## Arduino implementation features
* Camera control and tally should transmit all data over SDI.
* Filter out tally and output pgm and pvw state for a specific camera on digital pints.
* Some kind of logging system. Maybe one that transmits logs over tcp to a log server?
* Connectors for tally should be some kind of 3+ pin connector. Maybe RJ11 or 3.5mm TRS.
* Have an LED to indicate connection to switcher status. Turn on light when receiving protocol version?
* Find a good way to set camera id.
	* Maybe reading data from SDI out to find camera id (studio viewfinder displays camera id)
	* Maybe set with rotary switch like skaarhojs devices
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
* A good way to know if it should update serial data or run normally is to identify if the sdi shield is available. If not, read from serial. If it is, run normally. That way, just connect a usb cable to the microcontroller to update data and connect power to sdi shield to handle cc data.
* The connector for the tally could be a 4-pin connector
	* Maybe it could be 3d printed with just "stift" and "hylsor" in metal. They can be bought for cheap at "kjell.com".
	* There should be at least 2 connectors for tally
	* Using 4-pins would allow for pvw pin, pgm pin, gnd and 3.3v to be available. This allows for my monitor to hook into it with gnd and pgm pin but it can also be used to power leds with 3.3v and pgm pin.
	* A 4-pin 3.5mm connector could work, but is not lockable.

### Configuration
* Maybe build it like this:
1. User presses and holds a button on the device
2. An access point start on the device
3. As long as the button is held down, the access point is available
4. User connects a device to the access point
5. User releases button, but since there is a device connected, the access point remains open
6. All future devices that tries to connect to that access point get rejected
7. User can configure device
8. When HTTP POST configuration packet is received by the device, the access point is closed
* Configuration can both be done over existing network or over AP
* Pressing button should flash a light indicating AP is active
* If no one is connected to the AP when the button is released, the AP gets closed
* After configuration is done, the AP shuts down
* The problem with configuring over existing network is that the IP is not known unless set to static ip.
This makes it so you need to know the IP to configure the device
* Configure over AP could use captive portals



## Other platforms
* iOS
* Android
* Linux(rpi)
