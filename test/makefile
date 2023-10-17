# Defaults to running all tests
all:

# Output directory for test executables
BUILD_DIR = ../build/test

# Creates dist directory if it does not exist
$(BUILD_DIR):
	mkdir $@

# Builds test executable
$(BUILD_DIR)/%: | $(BUILD_DIR)
	$(CC) $^ -o $@ -g -Wall -Wextra -Wpedantic

# Runs test
.PHONY: run_%
run_%: $(BUILD_DIR)/%
	$<



$(BUILD_DIR)/http_parser: http/http_parser.c http/http_sock.c
TESTS_DEVICE += run_http_parser

$(BUILD_DIR)/http_connect: http/http_connect.c http/http_sock.c
TESTS_DEVICE += run_http_connect

# Runs all tests for device
.PHONY: device
device: $(TESTS_DEVICE)
ifdef DEVICE_ADDR
TESTS_ALL += $(TESTS_DEVICE)
endif



# Runs playground
$(BUILD_DIR)/playground: playground.c http/http_sock.c
.PHONY: playground
run_playground: $(BUILD_DIR)/playground
playground: run_playground

# Runs all tests
.PHONY: all
all: $(TESTS_ALL)

# Removes all compiled tests
.PHONY: clean
clean:
	$(RM) $(TESTS_ALL:run_%=$(BUILD_DIR)/%) $(BUILD_DIR)/playground

# Lists all tests except playground
.PHONY: list
list:
	echo $(TESTS_ALL)
