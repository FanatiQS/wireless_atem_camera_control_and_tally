# Changelog

## Version 0.5.0
No changelog available.



## Version 0.6.0

### Firmware
* BREAKING change to configuration layout in memory. (requires reconfiguration)

* Does not crash if unable to connect to SDI shield.
* Does not use BMDSDIControl library anymore.
* SoftAP PSK is not set until first configuration.
* Compilation fails if platform is not supported.
* Aborts boot if wlan or lwip init fails and prints debug to serial if enabled.
* Prints errors when failing to send UDP data if debug printing is enabled.
* Improved many error messages.
* Aborts parsing protocol data if reading data failed.
* Resets timeout for reject and closing to always wait 5s before trying to reconnect.
* Sends response before processing request payload (this might reduce resends as it receives ack faster).
* Turns off connection led when connection is closed by server.
* Waits to send handshake until network interface has connected.
* Prints GPIO pin as their name instead of their number if variable is used.
* Turns off tally when connection is lost.
* Prints IP info when connected to wlan.
* Device name can now be up to 32 characters instead of 16.
* Renamed "Name" in HTTP configuration to "Device name".

* Requires copying over files with install script to work in Arduino IDE.
* No longer warns about unexpected versions.
* Does not print device name on boot.
* Does not print statuses on timeout.

### Source API
* `atem_parse` now returns an `enum` to allow compiler to warn about missing cases.
* `atem_parse` has more return states.
* `/src/atem.c` now depends on `/src/atem_protocol.h`.
* `atem_parse` returning `ATEM_STATUS_CLOSED` does not automatically reset state causing reconnect to be a resend if relying on timeout for reconnect. To automatically reconnect, manually call `atem_connection_reset`.
* `atem_parse` now sends closed response to closing request.
* It is no longer safe to call `atem_cmd_available` when `atem_parse` did not return `ATEM_STATUS_WRITE`.

### Test Suite
Started implementing test suite.

### Proxy server
Proxy server currently broken.



## Version 0.6.1

### Firmware
* Fixed unstable configuration access point.
* Fixed `arduino/arduino_install` not working on linux.

### Proxy server
* Proxy server now works with the 0.6.0 reworked source api.

## Version 0.6.2

### Firmware
* Fixed platforms not implementing LED driver not having LED macro function defined.
* Enabled fix for unstabile configuration access point when debug is not enabled.
* Fixed compiler failure for ESP8266 Arduino framework version 3.0.0 and newer.
