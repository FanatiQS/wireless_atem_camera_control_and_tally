message("----------------")
message("Looking for user_config.h in ${CMAKE_SOURCE_DIR}")
message("----------------")

set(WACCAT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
idf_component_register(
	SRCS
		${WACCAT_ROOT}/core/atem.c
		${WACCAT_ROOT}/firmware/atem_sock.c
		${WACCAT_ROOT}/firmware/dns.c
		${WACCAT_ROOT}/firmware/flash_esp_idf.c
		${WACCAT_ROOT}/firmware/http_respond.c
		${WACCAT_ROOT}/firmware/http.c
		${WACCAT_ROOT}/firmware/main_esp_idf.c
		${WACCAT_ROOT}/firmware/sdi.c
		${WACCAT_ROOT}/firmware/wlan.c
		${WACCAT_ROOT}/firmware/ws2812_esp_idf.c
	INCLUDE_DIRS
		${CMAKE_SOURCE_DIR}
	REQUIRES
		esp_wifi
		driver
		nvs_flash
)
