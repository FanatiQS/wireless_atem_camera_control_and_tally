## Summary
Wireless tally and camera control for Blackmagic cameras connected directly to an ATEM switcher over WiFi.

## Description
Blackmagic supports camera control and tally signals back over SDI to many of their cameras.
They utilize a program return feed back to the camera to send it both video and ancillary data.
When you need a camera to be wireless, just adding a wireless video transmitter from the camera to the switcher looses the return feed including camera control and tally.
This project aims to add camera control and tally to a wireless camera with as little complexity as possible.

## FAQ

### Why not just add a cheap HDMI or SDI receiver on the camera?
This only adds the return video feed without camera control and tally.
The reason is because SDI ancillary data is stripped out when converted to HDMI.
Even a lot of SDI systems use a chipset designed for HDMI and therefore has the same limitation.
Transmitting an entire return video feed is also very heavy, increasing the risk for interference over only transmitting the small amount of data camera control and tally takes up.

### How can this project work without a transmitter?
This project only uses a receiver on the camera.
The reason this works is that it connects to the ATEM switcher directly over WiFi, the same way the control software does.
The camera control and tally commands are then translated on device to be sent out over SDI to the camera.

### Does it work for HDMI based cameras?
It has not been tested yet, but should work with a `Micro Converter BiDirectional SDI/HDMI`.



## Getting Started
This readme is fairly incomplete.
Some parts might be documented incorrectly and some not at all.

If you do not feel like building your own device, feel free to reach out to me and we might be able to sort something out for you.
You can find my contact information in the [contact section](#contact).

Before building your own device.
Read the [license](#license) section as that states the how you are allowed to use this project in an easy to understand format.

This project is fairly modulear. Multiple components can be enabled/disabled through the `arduino/user_config.h` file. Note that some also need additional hardware added to work. They are described in more detain below.

### Microcontroller
This project is based around the ESP8266 microcontroller.
It has been tested with a Wemos D1 Mini 2.2.0, though any ESP8266 based development board should work fine with some configuration modifications.

### Tools
* Soldering tools
* Multimeter

### Pre-requisites
* A USB cable to connect the ESP8266 development board to a computer for flashing.
* Arduino IDE with ESP8266 boards manager installed (instructions [here](https://github.com/esp8266/Arduino#installing-with-boards-manager))

Download this repository from github.
Either using commandline
```sh
git clone https://github.com/FanatiQS/wireless_atem_camera_control.git
```
or manually using this [link](https://github.com/FanatiQS/wireless_atem_camera_control/archive/refs/heads/master.zip).

Open the Arduino IDE and configure the flashing tools under the tools menu:
* Set the `Board` to match the ESP8266 development board. (for Wemos D1 Mini, select `LOLIN(WEMOS) D1 R2 & Mini`)
* Set `Erase Flash` to `All Flash Contents` (otherwise the configure access point might not be joinable, it can be set back to `Only Sketch` after the device has been configured)

### Debug logging
Debug logging prints information about what the microcontroller is doing over UART.
On most development boards, the UART data is converted to USB and can be read in the Arduino IDEs serial monitor.
By default debugging is enabled but can easily be disabled either as a whole or in part.
The main reason to disable debugging is for performance reasons.

Debugging is controlled using preprocessor macros definitions in the file `arduino/user_config.h`.
* To disable debug logging for camera control only, comment out definition for `DEBUG_CC`.
* To disable debug logging for tally only, comment out definition for `DEBUG_TALLY`.
* To disable all debug logging, comment out definitions for `DEBUG`, `DEBUG_CC` and `DEBUG_TALLY`.

### Camera Control and Tally over SDI (optional)
Camera control and tally over SDI is an optional feature.
It gets the camera control data directly from the ATEM switcher and sends it to a Blackmagic camera over SDI just as if the camera was connected directly to the switcher.

#### Components
The SDI communication is done with the [Blackmagic 3G SDI shield for Arduino](https://www.blackmagicdesign.com/se/developer/product/arduino).
Since this is a shield for an Arduino UNO, we also need a logic level shifter to handle the different communication voltages between the 3.3v ESP8266 chip and the 5v SDI shield.

The SDI shield requires 7-12v DC power while the ESP8266 chip only works on 3.3v.
The logic level shifter needs both 3.3v and 5v, however most development boards also support a 5v input and supplies a 3.3v source for the chip that could be used for the logic level shifter as well.
Powering everything from the same power source requires some kind of voltage regulator like a Wemos D1 Mini DC shield.
It is designed to work with the Wemos D1 Mini development board and is easy to attach as they have the same footprint and pinouts.

Since the SDI shield only supports a maximum of 12v, it should NOT be powered by a V-Lock battery without an additional voltage regurlator.

#### Building
@todo
* Add the logic level shifter between the Wemos D1 Minis I2C pins and the SDI shield.
* The default pins used are D1 for SDA and D2 for SCL. This is swiched around compared to what is normally documented for the Wemos D1 Mini to get the rotation of the board correct.
* If they are powered from separate sources, they might need a shared ground to work correctly.
* **A word of warning**. If the DC shield is connected to the Wemos D1 Mini when plugging in power over USB for firmware update, it might fry both boards and maybe even the computer if you are really unlucky.
The way I have solved it is to scrape off the traces from the Wemos D1 Mini between the USB port and the rest of the board to only allow power from the DC shield and not USB.

#### Prepare firmware
Download and install BMDSDIControl library from blackmagics website (instructions [here](https://documents.blackmagicdesign.com/UserManuals/ShieldForArduinoManual.pdf)).

Fix a compatibility bug in the library by changing line 166 of the file `BMDSDICameraControl.cpp` from
```cpp
const uint8_t kParamLength     = min(strlen(string), kMaxStringLength);
```
to
```cpp
const uint8_t kParamLength     = _min(strlen(string), kMaxStringLength);

```

Enable the lines `PIN_SCL` and `PIN_SDA` in `arduino/user_config.h` and set them to the pins the SDI shield is connected to (through the level shifter).

### Tally and status LEDs (optional)
Tally and status LEDs can be enabled or disabled individually.

#### Valid GPIO pins
On the ESP8266, any of the 16 pins between GPIO0 and GPIO15 work.
GPIO16 (D0 on Wemos D1 Mini) is technically not a GPIO pin and will not work for tally or status LEDs.

#### Pin definitions
Here are the pins available for triggering LED.

##### PIN_PGM
This pin is ON when the ATEMs input with the configured camera id is in the PGM/Program bus.

##### PIN_PVW
This pin is ON when the ATEMs input with the configured camera id is in the PVW/Preview bus but not in the PGM/Program bus since PGM takes priority over PVW.

##### PIN_CONN
This pin is ON when the device has an active connection to the ATEM switcher.

#### Components
One LED with and appropriately sized resistor for each tally/status (red and green LEDs should work with a 100 Ohm resistor on a Wemos D1 Mini).

#### Building
Attach the resistors to the LEDs.

Tally LEDs are connected with the anodes to ground and the cathodes resistors to GPIO pins.

Status LEDs are connected the other way around with the cathodes resistors connected to 3.3v and the anodes connected to GPIO pins.

#### Prepare firmware
Set `PIN_PGM`, `PIN_PVW` and/or `PIN_CONN` in `arduino/user_config.h` to the GPIO pin the LED is attached to.
To disable one of them, either remove the line or comment it out.

### Battery level monitoring (optional)
Battery level monitoring has not been fully developed yet.

#### Components
@todo

#### Prepare firmware
@todo

#### Building
@todo

### Flashing the firmware
When the hardware has been put together and the firmware has been configured, it is ready to be flashed.

1. Open `arduino/arduino.ino` in the Arduino IDE.
2. Plug in the ESP8266 board to the computer with a USB cable.
3. Select the device to use under the `Tools` menu and `Port`.
4. Upload the sketch from the Arduino IDE to the device.

If everything was set up correctly, the upload should successfully complete and the device should be ready to be configured.

## Configuration
@todo
* done over http
* it creates a wireless ap at boot that is removed when successfully connected to atem
* can still configure over same network as atem is on
* has mdns service with device name
* wireless ap is enabled again after device reboots when updating settings
* wireless ap is not enabled again if connection is lost until device restarts (by design)

## Proxy Server (optional)
The proxy server is a piece of software that can run on MacOS, Windows or Linux and sits between the clients and the ATEM switcher to increse the maximum number of connected clients allowed at one time.
An ATEM switcher can only handle a maximum of 5 to 8 concurrent connections depending on the model.
This includes both software and hardware panels. 
For situations where that is not enough, the proxy server ups the number of connections available for clients only interested in camera control and/or tally to a theoretical maximum of 32768.
Instead of the clients connecting directly to the ATEM switcher, they connect to it through the proxy server.

### MacOS and Linux

#### Pre-requisites Linux
* make
* clang/gcc

Install from your package manager

#### Pre-requisites MacOS
* make
* clang/gcc

Everything can be installed with `xcode-select --install`.

#### Building
* Open a terminal window
* Download this repository if not already done, either manually or from the command line
```sh
git clone https://github.com/FanatiQS/wireless_atem_camera_control.git
```
* Navigate to `proxy_server` directory in the downloaded repository and run the build toolchain
```sh
cd wireless_atem_camera_control/proxy_server
make
cd build
```
* Run it with `./server` followed by the IP address of the ATEM switcher delimited by a space. For example:
```sh
./server 192.168.1.240
```

### Windows

#### Pre-requisites
* nmake
* MSVC compiler

Both nmake and the MSVC compiler are installed with Visual Studio C++.

#### Building
* Download and extract this repository if not already done
* Locate and run the `Developer Command Prompt for VS XXXX` file where `XXXX` is the release year for the version of Visual Studio installed
* Navigare to the `proxy_server` directory in the downloaded repository and run the build toolchain
```sh
cd wireless_atem_camera_control/proxy_server
nmake
cd build
```
* Run it with `server.exe` followed by the IP address of the ATEM switcher delimited by a space. For example:
```sh
./server.exe 192.168.1.240
```


## Support
Camera models and protocol versions that have been tested to work.
If a camera model is not tested, it does not indicate it does not work, just that if it has some special features not available on other cameras, there is a small chance those features might no work correctly.

| Camera Model             | Status   |
| ------------------------ | -------- |
| URSA Broadcast           | Testing  |
| Micro Studio Camera      | Untested |
| Pocket Cinema Camera 4k  | Untested |
<!-- | URSA Mini Pro            | Untested | SDI, BLE  | -->
<!-- | URSA Mini                | Untested | SDI       | -->
<!-- | Pocket Cinema Camera Pro | Untested | BLE, HDMI | -->

| Protocol version | ATEM firmware version | Status  |
| ---------------- | --------------------- | ------- |
| 2.30             | 8.1.1+                | Testing |

## Test
The test directory contains a client that can be used to test the protocol parser.
Whenever the ATEM firmware updates, there is a risk of something breaking since the protocol is proprietary.
Using that test client will help in finding out if something affecting this project has changed.

## Thanks
A big thanks to Scenteknik AVL for sponsoring this project with components for a prototype.

## License
This project does not have a license specified.
Note that this does not make it open source or free to use.
Feel free to test it out, but please get in touch before using it in a production environment as that is not permitted without a license.
You can find my contact information in the [contact section](#contact).

## Contact
You can reach me at <a href = "mailto: fanatiqs@me.com">fanatiqs@me.com</a>.
