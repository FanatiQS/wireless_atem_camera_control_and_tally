## Summary
Wireless tally and camera control for Blackmagic cameras connected directly to an ATEM switcher over WiFi.

## Description
Blackmagic supports camera control and tally signals back over SDI to many of their cameras.
They utilize a program return feed back to the camera to send it both video and ancillary data.
When you need a camera to be wireless, just adding a wireless video transmitter from the camera to the switcher looses the return feed including camera control and tally.
This project aims to add camera control and tally to a wireless camera with as little complexity as possible.

## FAQ

### Why not just add a cheap SDI receiver on the camera?
This does not work a lot of the time as the chipset used in most wireless video systems is designed for HDMI and therefore strips out the ancillary data only adding the return video feed without camera control and tally.
This also has the additional benefit of transmitting less wireless data since it skips the heavy return video signal entirely.

### How can this project work without a transmitter?
This project only uses a receiver on the camera.
The reason this works is that it connects to the ATEM switcher directly over WiFi, the same way the control software and hardware panels do.
The camera control and tally commands are then translated and sent out over SDI to the camera.

### Does it work for HDMI based cameras?
It has not been tested yet, but should work with a `Micro Converter BiDirectional SDI/HDMI`.



## Support
Camera models and protocol versions that have been tested to work.
If a camera model is not tested, it does not indicate it does not work, just that if it has some special features not available on other cameras, there is a small chance those features might no work correctly.

| Camera Model             | Status   | Features  |
| ------------------------ | -------- | --------- |
| URSA Broadcast           | Testing  | SDI, BLE  |
| Micro Studio Camera      | Untested | SDI       |
| Pocket Cinema Camera     | Untested | BLE, HDMI |
<!-- | URSA Mini Pro            | Untested | SDI, BLE  | -->
<!-- | URSA Mini                | Untested | SDI       | -->
<!-- | Pocket Cinema Camera Pro | Untested | BLE, HDMI | -->

| Protocol version | ATEM firmware version | Status  |
| ---------------- | --------------------- | ------- |
| 2.30             | 8.1.1 - 8.6.3         | Testing |

## Build

### Arduino
This project is by default set up to work with the Wemos D1 Mini board, but can be modified to work with other ESP8266 boards as well.

#### Components
* Wemos D1 Mini (or other ESP8266 board but other boards might require some modifications to the code to work)
* Blackmagic 3G SDI shield for Arduino + logic level converter (optional)
* LEDs for tally (optional)

#### Flashing the firmware
1. Download Arduino IDE and install ESP8266 in boards manager (instructions [here](https://github.com/esp8266/Arduino#installing-with-boards-manager)).
2. Download this repository and open `arduino/arduino.ino` in the Arduino IDE.
3. Change the board to the Wemos D1 Mini (unless other board is used).
4. Modify the `arduino/user_config.h` file to enable/disable the features wanted (if other board than Wemos D1 Mini is used, pins might have to be changed).
5. Debug logs are enabled by default, they can easily be disabled by commenting out or removing the `DEBUG`, `DEBUG_TALLY` and/or `DEBUG_CC` lines in `arduino/user_config.h`.
6. Plug in the Wemos D1 Mini to the computer and upload the sketch from the Arduino IDE to the board.

##### Adding tally LEDs
1. Solder LEDs to the pins defined for `PIN_PGM`, `PIN_PVW` and/or `PIN_CONN` (`PIN_CONN` lights up when connected to an ATEM switcher and default shares pin with the Wemos D1 Minis builtin LED).

##### Enabling SDI control (optional)
1. Download and install BMDSDIControl library from blackmagics website (instructions [here](https://documents.blackmagicdesign.com/UserManuals/ShieldForArduinoManual.pdf)).
2. Fix a bug in the library by adding `#define min _min` after `#pragma once` in `BMDSDIControl/include/BMDSDIControlShieldRegisters.h`.
3. Enable the lines `PIN_SCL` and `PIN_SDA` in `arduino/user_config.h`.
4. Add a logic level converter between the Wemos and the SDI shield. This is required to converter the 3.3v I2C from the microcontroller to the required 5v by the SDI shield.

##### Enabling battery level monitoring (optional)
Not yet complete.

#### Configuration
Not yet complete.

## Test
The test directory contains a client that can be used to test the protocol parser.
Whenever the ATEM firmware updates, there is a risk of something breaking since the protocol is proprietary.
Using that test client will help in finding out if something affecting this project has changed.

## Notes

#### Concurrent connections
In my experience testing with an ATEM Television Studio, no more than 5 connections can be handled at the same time. This includes ATEM Software Control instances and hardware panels too.

## Thanks
A big thanks to Scenteknik AVL for sponsoring this project with components for a prototype.
