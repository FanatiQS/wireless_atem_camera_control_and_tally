message("----------------")
message("Looking for waccat_config.h in ${PROJECT_DIR}/")
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
	REQUIRES
		esp_wifi
		driver
		nvs_flash
)
target_compile_options(${COMPONENT_LIB} PRIVATE -include ${PROJECT_DIR}/waccat_config.h)
