## Summary
Wireless tally and camera control for Blackmagic cameras connected directly to an ATEM switcher over WiFi.

## Current state
Currently, camera control has not been fully implemented.
Tally works, both with LEDs and bridged GPIO.

## Description
Blackmagics URSA cameras are great to use with their ATEM switchers.
Just connecting two BNC cables between the camera and switcher to not only get video to and from the camera, but to also getting tally and camera control over the SDI return signal is a fantastic feature for multicamera productions.

When you need a camera to be wireless, the simplest sollution is to just add a wireless video transmitter to the camera side and plug the receiver into the switcher.
With that approach however, all camera control and tally is data is lost since there is no communication back from the switcher to the camera.
Solving this is not as easy as just adding another wireless video transmitter to send return video back to the camera as most wireless video transmitters ignore the part of the SDI signal that carries the camera control and tally data.

This is where this project comes in to play.
It connects directly to the ATEM switcher over IP, the same way the ATEM control software connects.
By relaying the camera control and tally data to the camera, either over SDI using the Blackmagic 3G-SDI Shield for Arduino or over bluetooth low energy, the camera can get these features wirelessly using Wi-Fi.

This has the additional benefit of transmitting less data since it skips the heavy return video signal entirely.

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
