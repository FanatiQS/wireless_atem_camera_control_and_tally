## Usage
Since this is just for testing as of right now, this documentation will be fairly basic.
1. Open the URSAs menu and go to the `SETUP` tab and navigate to page 2 to either get or set the camera id.
2. Launch the WebSocket relay server with the ATEMs ip address and camera id by navigating to `research/ws_relay` and running it something like this `node index.js 192.168.1.128 4`.
3. Start Google Chrome and navigate to `localhost:8080`.
4. Go back to the URSAs menu and on the 6th page under the `SETUP` tab, turn bluetooth on.
5. Press the `Connect` button in Chrome and select the camera
6. First time, enter the code in the cameras menu on the computer
7. Change parameters for the camera in the control software for the switcher
