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

## Requirements
##### Tools
* Soldering equipment

##### Components / Material
* ESP8266
* Blackmagic 3G-SDI Shield for Arduino
* Some wires
* Short BNC cable
* 4-pin XLR to barrel connector (or other power solution)
* Connector for tally (like RJ11 or 3.5mm TRS)
* Some kind of housing
* Red and Green LEDs

## Build
No data yet, the arduino implementation is not ready for documentation.

## Configuration
No data yet, the arduino implementation is not ready for documentation.

## Usage
No data yet, the arduino implementation is not ready for documentation.

## Test
The test directory contains a client that can be used to test the protocol parser.
Whenever the ATEM firmware updates, there is a risk of something breaking since the protocol is proprietary.
Using that test client will help in finding out if something affecting this project has changed.

## Notes

#### Concurrent connections
In my experience testing with an ATEM Television Studio, no more than 5 connections can be handled at the same time. This includes ATEM Software Control instances and hardware panels too.
