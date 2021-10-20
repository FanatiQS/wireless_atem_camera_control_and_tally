# Tests
This file contains a list of tests to perform to make sure that everything works correctly.
It is recommended to run these tests if either the camera model or the protocol version the switcher uses is not confirmed to work yet.
A list of all confirmed camera models and protocol version can be found in the main readme.



## Switcher tests
* Test timing out restarts
	1. Launch the test client with the flags: --packetTimeoutAt 0 --autoReconnect --printProtocolVersion
	2. The client should print "Connection timed out Restarting connection" and then print the protocol version
	3. Disconnect network connection to the switcher
	4. The test client should now print the same timeout message over and over again
	5. Reconnect the network to the switcher
	6. The test client should after a while print the protocol version again

* Test getting closing connection state (0x04)
	1. Launch the test client with the flags: --packetDropChanceSend 100 --packetDropStartSend 10
	2. Wait for client to quit (takes a few seconds)
	3. Test client should quit with the message "Connection wanted to restart, exiting"

* Test tally states
	1. Open ATEM Software Control
	2. Launch the test client with the flags: --printTally --cameraId 2
	3. Switch PGM and PVW to cam1
	4. Switch inputs using the ATEM Software Control and make sure all instructions that requires a print do get a print in the test client
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

* Test tally index length
	1. Find out how many inputs the ATEM switcher tested has
	2. Launch the test client with the flags: --printTally --cameraId <maxid + 1>
	3. The test client should complain that the camera id is out of range
	4. Relaunch the test client with the flags: --printTally --cameraId <maxid>
	5. It should not complain this time

* Test camera control parameters
	1. Launch the test client with the flags: --printCameraControl --cameraId 1
	2. Go through all the messages sent to the client and make sure all are known categories and parameters (if they are not known, they will print as `? [%d] ?` where the integer in brackets is the unknown data)
	3. Make sure all of these parameters are printed:
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
	4. Adjust these parameters manually from a control panel and make sure they are printed:
		1. Zoom [ Lens - Set continuous zoom (speed) ]
		2. Auto focus [ Lens - Instantaneous autofocus ]
		3. Auto aperture [ Lens - Instantaneous auto aperture ] Not available from the software, requires hardware panel

* Test wrapping remote id
	This test takes about 10 minutes to perform
	1. Open the ATEM Software Control
	2. Launch the test client with the flags: --printLastRemoteId
	3. Set ATEM Software Controls transition speed to 10 seconds
	4. Keep the transition going (enter key) until lastRemoteId wraps at 32767



## Camera tests
* Test tally to camera (untested)
	1. Connect the production code to a camera
	2. Change id of camera to first input
	3. Switch PVW to first input
	4. Switch PGM to first input
	5. Switch PVW to any other input
	6. Change id of camera to second input
	7. Switch PVW to second input
	8. Switch PGM to second input
	9. Continue like this until all inputs have been checked

* Test camera control to camera init
	1. Set up camera, wireless device, switcher and control software/hardware without connecting the switcher and wireless device.
	2. Set iris or some obvious setting to some setting the camera does not have. (example: set iris to completely closed if the camera has its iris open)
	3. Connect the wireless device to the switcher.
	4. The camera should update with the setting from the switcher.

* Test camera control to camera on demand
	Go through all settings in the control software/hardware and make sure everything updates correctly.
	* Iris
	* Auto iris (not available in control software)
	* Focus
	* Auto focus
	* Zoom
	* White balance
	* Shutter
	* Gain/ISO (only updates on -6, 0, 6, 12 and 18 for both the wireless device and over SDI)
	* ND (not available on URSA cameras)
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



## Not important
* Test client limits of switcher
	1. Connect test client to switcher with no flags
	2. Repeat step one with another client until the newest test client quits with "Connection rejected"
