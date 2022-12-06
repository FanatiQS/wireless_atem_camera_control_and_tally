#include <stdint.h> // uint8_t
#include <stdbool.h> // bool, true, false

#include <lwip/tcp.h>

#include "./http.h"
#include "./debug.h"

bool http_init() {
#ifdef ESP8266
	// @todo
	struct softap_config softapConfig;
	if (!wifi_softap_get_config(&softapConfig)) {
		DEBUG_PRINTF("Failed to read soft ap configuration\n");
		return;
	}

	DEBUG_PRINTF(
		"Soft AP SSID: \"%.*s\"\n"
		"Soft AP PSK: \"%.*s\"\n"
		"Soft AP Channel: %d\n",
		softapConfig.ssid_len, softapConfig.ssid,
		sizeof(softapConfig.password), softapConfig.password,
		softapConfig.channel
	);
#endif // ESP8266

	return true;
}
