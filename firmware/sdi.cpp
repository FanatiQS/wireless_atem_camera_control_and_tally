#include <stdint.h> // uint8_t

#include "./user_config.h" // PIN_SCL, PIN_SDA
#include "./debug.h" // DEBUG_PRINTF
#include "./sdi.h" // SDI_ENABLE

#ifdef SDI_ENABLED
#include <Wire.h>
#include <BMDSDIControl.h> // BMD_SDITallyControl_I2C, BMD_SDICameraControl_I2C
#endif // SDI_ENABLED

// Expected SDI shield versions
#define EXPECTED_SDI_LIBRARY_VERSION 1,0
#define EXPECTED_SDI_FIRMWARE_VERSION 0,13
#define EXPECTED_SDI_PROTOCOL_VERSION 1,0

// ATEM communication data that percists between loop cycles
#ifdef SDI_ENABLED
static BMD_SDITallyControl_I2C sdiTallyControl(0x6E);
static BMD_SDICameraControl_I2C sdiCameraControl(0x6E);
#endif // SDI_ENABLED

// Prints a type of version from the BMDSDIControl library
#ifdef SDI_ENABLED
static void printBMDVersion(char* label, BMD_Version version, uint8_t expectedMajor, uint8_t expectedMinor) {
	// Prints currently used version from the SDI shield
	DEBUG_PRINTF("SDI shield %s version: %d.%d\n", label, version.Major, version.Minor);

	// Prints warning if current version does not match expected version
	if (expectedMajor != version.Major || expectedMinor != version.Minor) {
		DEBUG_PRINTF("WARNING: Expected SDI shield %s version: %d.%d\n", label, expectedMajor, expectedMinor);
	}
}
#endif // SDI_ENABLED

#ifdef SDI_ENABLED
extern "C" bool _sdi_connect() {
	// Initializes camera control and tally over SDI
	Wire.begin(PIN_SDA, PIN_SCL);
	sdiCameraControl.begin();
	sdiCameraControl.setOverride(true);
	sdiTallyControl.begin();
	sdiTallyControl.setOverride(true);

	// Prints versions used by SDI shield and its library if debugging is enabled
	printBMDVersion("library", sdiCameraControl.getLibraryVersion(), EXPECTED_SDI_LIBRARY_VERSION);
	printBMDVersion("firmware", sdiCameraControl.getFirmwareVersion(), EXPECTED_SDI_FIRMWARE_VERSION);
	printBMDVersion("protocol", sdiCameraControl.getProtocolVersion(), EXPECTED_SDI_PROTOCOL_VERSION);

	return true;
}

extern "C" void _sdi_write_tally(uint8_t dest, bool pgm, bool pvw) {
	sdiTallyControl.setCameraTally(dest, pgm, pvw);
}

extern "C" void _sdi_write_cc(uint8_t* buf, uint16_t len) {
	sdiCameraControl.write(buf, len);
}

#endif // SDI_ENABLED
