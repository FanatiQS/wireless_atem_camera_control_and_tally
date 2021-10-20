## Usage
Since this is just for testing as of right now, this documentation will be fairly basic.
1. Launch the WebSocket relay server by navigating to `research/ws_relay` and running it with `node index.js`
2. Start Google Chrome and open the local file `research/ws_relay/index.html`
3. Go in to the URSAs menu and at the 6th page under the `SETUP` tab, turn on bluetooth
4. Press the `Connect` button in Chrome and select the URSA camera
5. Get the camera id from page 2 in the `SETUP` menu tab on the camera.
6. Launch the test client with flags: `--tpcRelay --printCameraControl --cameraId 2` or whatever camera id the camera had
7. First time, enter the code in the cameras menu on the computer
8. Change parameters for camera 2 (or whatever id it had) in the control software for the switcher
