# Requirements:
# 	GNU tools (sh, cd, cp, gnumake, grep, gzip, mkdir, mv, rm, sh, tar, uname, unzip)
# 	curl
# 	python3

ROOT = ..
BUILD_DIR = $(ROOT)/build/arduino
ARDUINO_CLI = $(BUILD_DIR)/bin/arduino-cli

# Strict compilation flags
CFLAGS = -Wall -Wextra -Werror
CFLAGS_ESP8266 = --build-property "compiler.c.extra_flags=$(CFLAGS)"
CFLAGS_ESP32 = --build-property "compiler.warning_flags.all=$(CFLAGS)"

# Arduino cli temporary directories and configurations
export ARDUINO_BUILD_CACHE_PATH = $(BUILD_DIR)
export ARDUINO_DIRECTORIES_DATA = $(BUILD_DIR)
export ARDUINO_LOGGING_LEVEL = warn
export ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS = \
	https://arduino.esp8266.com/stable/package_esp8266com_index.json \
	https://espressif.github.io/arduino-esp32/package_esp32_index.json

# Search directory for python libraries (only pyserial is required by esp32)
export PYTHONPATH = $(BUILD_DIR)/pyserial-master

# Compiles arduino firmwares
.PHONY: all
all: esp8266/d1_mini esp32/esp32c3

# Installs arduino-cli in build directory
$(ARDUINO_CLI):
	mkdir -p $(dir $@)
	curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=$(dir $@) sh

# Installs pyserial dependency for ESP32
$(PYTHONPATH)/serial:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && curl -fsSL https://github.com/pyserial/pyserial/archive/refs/heads/master.tar.gz | tar xvz

# Installs arduino board support
$(BUILD_DIR)/packages/%: $(ARDUINO_CLI)
	$(ARDUINO_CLI) core update-index
	$(ARDUINO_CLI) core install $*:$*

# Installs firmware sources for arduino
$(ROOT)/arduino/src/lib_path.h:
	$(ROOT)/arduino/install

# Compiles ESP8266 firmware
.PHONY: esp8266/%
esp8266/%: $(BUILD_DIR)/packages/esp8266 $(ROOT)/arduino/src/lib_path.h
	$(ARDUINO_CLI) compile $(ROOT)/arduino -b esp8266:esp8266:$* --warnings=all $(CFLAGS_ESP8266) --clean

# Compiles ESP32 firmware
.PHONY: esp32/%
esp32/%: $(BUILD_DIR)/packages/esp32 $(PYTHONPATH)/serial $(ROOT)/arduino/src/lib_path.h
	$(ARDUINO_CLI) compile $(ROOT)/arduino -b esp32:esp32:$* --warnings=all $(CFLAGS_ESP32) --clean

# Cleans up all arduino dependencies, caches and build files
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
