#include <stdbool.h> // bool, true, false
#include <stdio.h> // FILE, fopen, fread, fclose, fwrite
#include <errno.h> // errno, ENOENT
#include <stddef.h> // NULL
#include <string.h> // memcmp, strerror
#include <stdlib.h> // exit, EXIT_SUCCESS
#include <errno.h> // errno

#include "./flash.h" // struct flash_cache_t
#include "./debug.h" // DEBUG_HTTP_PRINTF

#define DEBUG_PERROR(msg) DEBUG_ERR_PRINTF(msg ": %s\n", strerror(errno))

static const char* const config_file_path = "./config.bin";



// Reads flash configuration and other platform specific configurations for HTTP cache
bool flash_cache_read(struct flash_cache* cache) {
	// Opens config file if available
	FILE* config_file = fopen(config_file_path, "rb+");
	if (config_file == NULL) {
		// Sets default values
		if (errno == ENOENT) {
			memset(cache, 0, sizeof(*cache));
			return true;
		}

		DEBUG_PERROR("Failed to open config file for reading");
		return false;
	}

	// Reads contents of config file
	if (fread(cache, sizeof(*cache), 1, config_file) != 1) {
		DEBUG_PERROR("Failed to read data from config file");
		fclose(config_file);
		return false;
	}

	// Closes open config file before exit
	fclose(config_file);
	return true;
}

// Writes flash configuration and other platform specific configurations for HTTP cache
void flash_cache_write(struct flash_cache* cache) {
	// Gets current configration for comparison
	struct flash_cache cache_current;
	if (!flash_cache_read(&cache_current)) {
		return;
	}

	// Writes new configuration to persistent storage if changed
	if (memcmp(&cache_current, &cache, sizeof(cache_current)) != 0) {
		FILE* config_file = fopen(config_file_path, "wb");
		if (config_file == NULL) {
			DEBUG_PERROR("Configuration file descriptor not open");
			return;
		}

		if (fwrite(cache, sizeof(*cache), 1, config_file) != 1) {
			DEBUG_PERROR("Failed to write data to config file");
			return;
		}

		fclose(config_file);
	}

	// Restarts device after flash write
	DEBUG_HTTP_PRINTF("Rebooting...\n");
	exit(EXIT_SUCCESS);
}
