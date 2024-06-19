#include "./lib_include.h"

#ifdef ESP8266
#include LIB_INCLUDE(firmware/main_esp8266_nonos.c)
#endif // ESP8266

#ifdef ESP32
#include LIB_INCLUDE(firmware/main_esp_idf.c)
#endif // ESP32
