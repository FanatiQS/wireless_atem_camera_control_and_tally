## Summary
Wireless tally and camera control for a Blackmagic camera connected to an ATEM switcher

## Description
Blackmagics URSA cameras are great cameras to use with from their ATEM switchers.
Just connect two BNC cables between the camera and switcher to not only get video from the camera to the switcher, but also return video to see what is currently live on the switcher from the camera.
That is great feature, but what is arguably even more important in that SDI signal is the camera control.
Connect the ATEM Software Control or a hardware camera control panel and change settings on the camera from the switcher.
This is an important feature to be able to match multiple cameras with each other.

The problem with transmitting the camera control data with the SDI signal though is when a camera needs to go wireless.
The reason is because most wireless video transmitters strip out the part of the signal where the camera control resides unless it is a really expensive transmitter.
Using only a basic wireless video transmitter to send the camera feed to the switcher works, but all camera control from the control room is lost and either relies on that cameras operator to match the cameras or that camera is not matched to the others.

That is where this project comes in to play.
It connects directly to the ATEM switcher over Wi-Fi, reads the tally and camera control data, then sends that to the camera over SDI using the Blackmagic 3G-SDI Shield for Arduino.
<!-- For a more inexpensive solution, it is also possible to send the camera control data to some Blackmagic cameras over Bluetooth.
There might be apps created in the future to display tally and send camera control data over bluetooth. -->

## Requirements
##### Tools
* Soldering equipment

##### Components / Material
* ESP8266
* Blackmagic 3G-SDI Shield for Arduino
* Some wires
* 2x short BNC cables
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

