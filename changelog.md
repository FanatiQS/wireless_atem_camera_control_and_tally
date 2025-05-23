# Changelog

## Version 0.9.0 (development)

### Arduino
* Simplified arduino install process.
* Sets configuration network password at first boot for ESP32.
* Fixes configuration network not set correctly on ESP8266.
* Deprecated single source header completely.

### Documentation
* Improved protocol documentation.

### Core
* **BREAKING** Renamed core components.
* Added retransmit request when receiving out-of-order packets.
* Added POSIX core API.
* Refactored core API macro functions to inline functions.
* Improved code documentation.

### Firmware
* Fixed firefox popup error on HTTP form submit.
* Added experimental esp-idf support for ESP32.
* Renamed and restructured internal components related to persistent storage.
* Allows cross origin configuration.
* Updated configuration page.
* Updated HTML generation.
* Updated HTTP parser.
* Fix pin 0 being logged incorrectly.
* Added virtual tapif network interface as a compilation target.
* Fixed default configuration inverted ESP8266 built-in LED.
* Refactored HTTP template engine.

### Test Suite
* Added more tests for atem_server and atem_client.
* Restructured make rules.
* Added test for existence of camera control parameters.
* Updated test utilities.
* Add HTML and HTTP linters.
* Check for warnings in arduino sketch for ESP8266 and ESP32.
* Added tests for core API.
* Add usage instructions for test suite.
* Added test for `tools/configure.sh` script.
* Test utils on every test run.
* Replaced environment variable `SINGLE_RUNNER` with more powerful `RUNNER_FILTER`.
* Added `LISTEN_ADDR` environment variable.
* `atem_handshake_fill` returns number of connected sessions.
* Renamed build rule `config_device` to `device_config`.
* Document available environment variables.
* Document playground build rule.
* Document debugger build rule.

### Tools
* Added HTTP server for generated HTML to auto-reload browser on file change.
* Extract HTML tool now works on POSIX shells other than bash.
* Fix issue with spaces in arguments when using `tools/configure.sh`.
* Added script module to overlay device configuration over existing webpage.
* Make `/tools/configure.sh` work in any POSIX compliant shell.
* Extract html tool now returns errors to client instead of blank page.



## Version 0.8.0

### Arduino
* Moved single source include file.
* Deprecated Arduino HTTP configuration server on port 81
* Sets Arduino OTA host name
* Sets password for configuration network in arduino without having to use old arduino HTTP server.

### Core
* `ATEM_MAX_PACKET_LEN` renamed to `ATEM_PACKET_LEN_MAX`.

### Firmware
* Sanitizes ssid, psk and device name in HTML to display quotation character correctly.
* Huge styling rewrite to HTML configuration page.
* Fixed IP address parser rejecting valid octets.
* Fixed unable to update configuration when connected to ATEM.
* Prints DNS debug state at boot.
* Fixed crash on HTTP client close before server sent response.
* Fixed empty `dest=` causing successful configuration of invalid 0 value.
* Updated successful HTTP configuration response message.
* Fixed `DEBUG_ATEM` message prefix.
* Reworked configuration page mobile scaling.
* Added experimental ESP32 support.
* Added compiler flag to invert `PIN_CONN`, previously was always inverted.
* Do not abort on non critical initialization errors (for example captive portal or hostname).
* Changed order or debug boot messages.
* Added ws2812 (neopixel) support on ESP32.
* Deprecated compiler macro `VERSIONS_ANY` (uses warnings instead).
* Moved ESP8266 platform code to its own files and deprecated `/firmware/init.c` and `/firmware/flash.c`.
* Updated HTML POST response message.
* Added debug printing for HTTP configuration values.
* Made `timespec_get` shim easier to use.
* Added devices current ip on network with ATEM switcher to HTML configuration page.
* Added tally and camera control debug log labels.
* Upgraded framework versions for firmware.
* Enables DNS debug printing by default.

### Test Suite
* Added more tests for atem_server.
* Added test for static ip and dhcp.
* Added environment variable to clamp number of lines printed from buffer.
* Fixed tests not running correctly on linux.
* Removed `run_*` prefix in make.
* Added MSVC shims for posix APIs.
* Added environment variable for clamping buffer prints.
* Added more HTTP tests.
* Overhauled `logs_print_string` output styling (uses ansi escape codes).
* Separated make rule `device` into `config_device` and `atem_client_device`.
* Added tests for closing before HTTP response.
* Locks ATEM server tests to specified client address.

### Tools
* Added tool to generate HTML and HTTP on host.



## Version 0.7.1

### Firmware
* DNS parser no longer has a maximum packet length it can handle.
* Simplified LED control.
* Added compiler error if I2C is not implemented but SDI is configured to be used.
* Renamed static ip related HTTP fields.
* **BREAKING CONFIG** Replaced static ip flag with DHCP.
* Arduino mDNS resolver uses device name again.
* HTTP POST body parser rejects IP address with less than 4 segments.
* Rephrased HTTP POST body error messages.
* HTTP POST body parser rejects integer without value.
* Fixed incorrectly calculating remaining length in HTTP POST body parser using flags.

### Core
* Renamed directory `/src/` to `/core/`.
* Correctly resets retransmit flag after opening handshake rejection.

### Documentation
* Updated flag styling.

### Test Suite
* Huge rewrite of multiple parts of the test suite.
* Added and updated tests for atem open, atem close, http and dns.
* Uses environment variables to enable different log types.
* Added test to ensure everything is ready for release.
* Environment variables `CFLAGS`, `CPPFLAGS` and `LDFLAGS` are now used when compiling tests.
* Renamed environment variable specifying address of ATEM switcher to connect to.
* Added rule to run test in LLDB.
* Added rule to build all tests.
* Reworked playground structure to handle multiple playground files.
* Include `/core/atem.c` in playgrounds.

### Tools
* Added shim for `timespec_get` function for use with systems not supporting C11.



## Version 0.7.0

### Firmware
* Ported Arduinos DNS captive portal to C.
* Renamed file `/firmware/udp.c` to `/firmware/atem_sock.c`.
* Renamed file `/firmware/udp.h` to `/firmware/atem_sock.h`.
* Renamed `atem_init` to `waccat_init`.
* Renamed `atem_udp_init` to `atem_init`.
* Improved debug logs.
* Added debug flag to print acknowledged ATEM packets.
* Added optional single source include file
* Moved HTTP POST parser to custom streaming parser (available on port 8080 until GET parser is moved as well).
* Changed HTTP POST keys.
* HTML input fields for IP addresses are now a single text box rather than 4 individual ones.
* Configuration properties are only updated if available in HTTP POST (static ip is switched to '1' or '0' instead of 'on' or undefined).
* Validates HTTP POST data before updating.
* Added HTTP debug flag.
* Fixed crash when configuring device over softap.
* Uses devices name as hostname.
* Prints compiler version used to build.
* Checks libraries/sdk versions at compile-time with user_config macro to disable strict version checking (VERSIONS_ANY = 1).
* Default configuration for DEBUG_CC is now to be disabled for performance issues.
* Does not disable wlan station when connecting to wlan softap if station has a working connection.
* Added pattern checking for ip address input.
* Added HTTP GET handling for non-arduino http server.
* Default HTTP server is now the custom implementation and the old arduino one can be found on port 81.
* Function `http_init` now returns the http server pcb.
* Error debug messages now has as an `[ ERR ]` prefix.
* No longer prints wlan channel at boot.
* HTML request time now uses 24h clock.
* Uses `printf` instead of `LWIP_PLATFORM_DIAG` for debugging since lwip debugging can be disabled on some platforms.
* Arduino mDNS responder is unreliable.
* No longer prints MAC address at boot.
* **BREAKING** Default I2C configuration now conforms to default ESP8266 pinout.
* Added DNS debug flag.

### Source API
* Now only updates property `readLen` when it is allowed to be used.
* Fixed warnings after bit shift type promotion.
* Huge improvement to source API documentation.
* Deprecated unused and broken `atem_tally_translate` function.
* Predefined ATEM command names is now in an enum instead of being macros.
* Replaced `atem_cc_dest` function with `atem_cc_updated` to do similar thing.
* Atem struct changed `dest` parameter to a uint8_t type to match other code.
* Source API should now be thread safe.

### Test Suite
* Added tests for firmware HTTP parser.
* Started collecting compilation and running of tests from makefile.
* Started deprecating test client (requires compiling manually).
* Added tests for firmware DNS captive portal.

### Tools
* Added simple configuration bash script.



## Version 0.6.2

### Firmware
* Fixed platforms not implementing LED driver not having LED macro function defined.
* Enabled fix for unstable configuration access point when debug is not enabled.
* Fixed compiler failure for ESP8266 Arduino framework version 3.0.0 and newer.



## Version 0.6.1

### Firmware
* Fixed unstable configuration access point.
* Fixed `arduino/arduino_install` not working on linux.

### Proxy server
* Proxy server now works with the 0.6.0 reworked source api.



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



## Version 0.5.0
No changelog available.
