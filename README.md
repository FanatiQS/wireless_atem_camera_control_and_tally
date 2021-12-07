## Summary
Wired or wireless tally and camera control for a Blackmagic cameras connected to an ATEM switcher over IP.

## Description
Blackmagics URSA cameras are great cameras to use with their ATEM switchers.
Just connecting two BNC cables between the camera and switcher to not only get video to and from the camera, but also getting tally and camera control over the SDI return signal is a fantastic feature for multicamera productions.

The problem with transmitting tally and camera control data over the SDI signal though is when a camera needs to go wireless.
The reason is because most wireless video transmitters strip out the part of the signal where the camera control resides.
Using a basic wireless video transmitter to send the camera feed to the switcher and skipping the return video feed altogether works, but all camera control Data from the switcher is lost and either relies on that cameras operator to set all the settings to match the other cameras or that camera is not going to match the others.

That is where this project comes in to play.
It connects directly to the ATEM switcher over IP, the same way the ATEM control software connects. By reading the tally and camera control data and sending that to the camera, either over SDI using the Blackmagic 3G-SDI Shield for Arduino or over bluetooth, The camera can get these features wirelessly using Wi-Fi.

## Support
Camera models and protocol versions that have been tested to work.
If a camera model is not tested, it does not indicate it does not work, just that if it has some special features not available on other cameras, there is a small chance those features might no work correctly.

| Camera Model      | Status   |
| ----------------- | -------- |
| URSA Broadcast    | Testing  |
| Pocket Cinema Camera 4k | Untested |
| Micro Studio Camera | Untested |

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
