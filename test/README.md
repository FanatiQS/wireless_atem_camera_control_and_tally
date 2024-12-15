# Test

## ATEM protocol
Test understanding of ATEM protocol on official client and server.

### Prerequisites
* C compiler with POSIX and C11 support (gcc/clang)
* GNUMake
* ATEM switcher (any model)
* ATEM Software Control

### Usage
Launch the ATEM Software Control and set the "Manual IP Address" to the address of the test runner machine.

Execute the tests with the IP addresses set for the ATEM Software Control machine and ATEM switcher.

```sh
make atem_server atem_server_extended atem_client atem_client_extended ATEM_SERVER_ADDR=192.168.1.240 ATEM_CLIENT_ADDR=127.0.0.1
```

## Wireless device
Test ATEM communication and HTTP configuration for a wireless WACCAT device.

### Prerequisites
* C compiler with POSIX and C11 support (gcc/clang)
* GNUMake
* Wireless device

### Usage
Configure the ATEM address on the wireless device to the address of the test runner machine.

Execute the tests with the IP addresses set for wireless device address.

```sh
make atem_client config_device ATEM_CLIENT_ADDR=192.168.1.66 DEVICE_ADDR=192.168.1.66
```

# Manual tests (@todo needs update)
This file contains a list of tests to perform to make sure that everything works correctly.
It is recommended to run these tests if either the camera model or the protocol version the switcher uses is not confirmed to work yet.
A list of all confirmed camera models and protocol version can be found in the main readme.


## Requirements
* An ATEM switcher
* The ATEM Software Control with connection to the switcher
* GCC
* Make



## Switcher tests

### Stress test on stabile connection
This tests that the connection is kept even during times with lots traffic.

1. Open the ATEM Software Control and set its transition speed to 10 seconds.
2. Launch the test client with the flags: --printLastRemoteId
3. Switch over to ATEM Software Control and start doing auto transitions. As soon as an auto transition is complete, start a new one. Keep doing this for something like a minute to ensure the connection is not dropped.
4. It should reach about remote id 3000 for 1 minute of transitions.
5. Exit the test client manually.



### Stress test on unstabile connection
This tests that the connection is kept even during times with lots of traffic with packets being dropped.

1. Open the ATEM Software Control and set its transition speed to 10 seconds.
2. Launch the test client with the flags: --printLastRemoteId --packetDropStartSend 1 --packetDropChanceSend 30 --packetDropChanceRecv 30
3. Switch over to ATEM Software Control and start doing auto transitions. As soon as an auto transition is complete, start a new one. Keep doing this for something like a minute to ensure the connection is not dropped. This time, the remote ids should be printed at a more irregular interval including stutters and just feeling uneven. If the connection to the server is timed out, it is okay to restart the test as long as it does not keep happening.
4. It should reach about remote id 3000 for 1 minute of transitions.
5. Exit the test cient manually.



### Delayed packets
This tests that delayed packets arriving after connection is restarted does not mess up the connection.

1. Launch the test client with the flags: --printProtocolVersion --packetTimeoutAt 10
2. Wait for the client to connect (print protocol version), time out and reconnect.
3. Wait an aditional 10 seconds to ensure the connection is not going to drop.
4. Exit the test client manually.



### Wrapping remote id
This tests that remote id wrapps around to 0 after reaching limit of signed 16 bit int. It takes about 10 minutes to perform.

1. Open the ATEM Software Control and set its transition speed to 10 seconds.
2. Launch the test client with the flags: --printLastRemoteId
3. Keep the transition going (enter key) until lastRemoteId wraps around to 0 from 32767.
4. Exit the test client manually if it wrapped at the expected position.



### Tally states
This tests that the tally commands contain the expected number of sources.

1. Figure out how many inputs/sources the switcher to test with has to use with --tallySources flag.
2. Open ATEM Software Control
3. Launch the test client with the flags: --cameraId 2 --printTally --tallySources <sources>
4. Switch PGM and PVW to CAM1.
5. Switch inputs using the ATEM Software Control and make sure all instructions that requires a print do get a print in the test client
	1. Switch PGM to cam2 (print pgm)
	2. Switch PVW to cam3 (no print)
	3. Switch PGM to cam1 (print none)
	4. Switch PVW to cam2 (print pvw)
	5. Switch PGM to cam3 (no print)
	6. Switch PGM to cam2 (print pgm)
	7. Switch PVW to cam1 (no print)
	8. Switch PVW to cam2 (no print)
	9. Switch PGM to cam1 (print pvw)
	10. Switch PGM to cam2 (print pgm)
	11. Switch PGM to cam3 (print pvw)
	12. Switch PVW to cam1 (print none)
	13. Switch PGM to cam1 (no print)
6. Exit the test client manually.



### Camera Control Paramters
This tests that all known camera control parameters are still available and no new ones are sent.

1. Open ATEM Software Control
2. Launch the test client with the flags: --cameraId 2 --printCameraControl
3. Go through all the messages sent to the client and make sure all are known categories and parameters. If something is not known, it will print as `? [%d] ?` where the integer in brackets is the unknown data.
4. Make sure all of these parameters are printed:
	* Lens - Focus
	* Lens - Aperture (f-stop)
	* Video - Gain
	* Video - Manual While Balance
	* Video - Exposure (us)
	* Video - Video sharpening level
	* Video - Gain (db)
	* Video - ND
	* Display - Color bars display time (seconds)
	* Color Correction - Lift Adjust
	* Color Correction - Gamma Adjust
	* Color Correction - Gain Adjust
	* Color Correction - Offset Adjust
	* Color Correction - Contrast Adjust
	* Color Correction - Luma mix
	* Color Correction - Color Adjust
	* PTZ Control - Pan/Tilt Velocity
5. Adjust these parameters manually from a control panel on camera 2 and make sure they are printed:
	* Zoom should be printed as "Lens - Set continuous zoom (speed)"
	* Auto focus should be printed as "Lens - Instantaneous autofocus"
	* Auto aperture (not available from the software, requires hardware panel) should be printed as "Lens - Instantaneous auto aperture"



## Camera tests

### Camera Control initial value
This tests that the camera receives current state when connecting to the switcher

1. Set up camera, wireless device, switcher and control software/hardware without connecting the switcher and wireless device.
2. Set iris or some obvious setting to some setting the camera does not have. (example: set iris to completely closed if the camera has its iris open)
3. Connect the wireless device to the switcher.
4. The camera should update with the setting from the switcher.



### Camera Control update
1. Open ATEM Software Control or use hardware panel.
2. Connect camera control device to a camera.
3. Go through all settings in the control software/hardware. Update them through their entire span and compare setting in camera and in switcher and make sure they match.
	* Iris
	* Auto iris (not available in ATEM Software Control)
	* Focus
	* Auto focus
	* Zoom
	* White balance
	* Shutter
	* Gain/ISO (only updates on -6, 0, 6, 12 and 18 for both the wireless device and over SDI)
	* ND (only available on Blackmagic Pocket Cinema Camera 6k Pro)
	* Color bars
	* Sharpening / Detail
	* Tint
	* Hue
	* Saturation
	* Lum mix (I do not understand how it works, so have not been able to test it)
	* Contrast
	* Pivot
	* Lift Adjust [luma, red, green, blue]
	* Gamma Adjust [luma, red, green, blue]
	* Gain Adjust [luma, red, green, blue]



### Tally over SDI (untested)
This tests that the translation of the tally data is done correctly

1. Open the ATEM Software Control
2. Set PGM and PVW to anything other than CAM1
2. Connect the arduino with SDI shield to a camera
3. Change ATEM ID of camera to 1
4. Make sure the camera does not show any tally
5. Switch the camera to PVW on the switcher and make sure the camera shows green tally
6. Switch the camera to PGM on the switcher and make sure the camera shows red tally
7. Increment cameras ATEM ID by 1 and repeat from step 4 until all inputs have been checked



## Not implemented
* Test client limits of switcher
	1. Connect test client to switcher with no flags
	2. Repeat step one with another client until the newest test client quits with "Connection rejected"
