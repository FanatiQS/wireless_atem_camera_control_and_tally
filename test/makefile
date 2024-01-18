# Defaults to running all tests
all:

# Output directory for test executables
BUILD_DIR = ../build/test

# Shared source files
UTILS_SOURCE = utils/simple_socket.c utils/atem_sock.c utils/atem_header.c utils/atem_handshake.c utils/simple_socket.c utils/logs.c utils/atem_acknowledge.c utils/timediff.c utils/runner.c



# Tests normal DNS lookups
script_dns_lookup:
	./http/dns_lookup.sh
device: script_dns_lookup
all: script_dns_lookup

# Tests device configuration
script_config:
	./config/test_conf.sh
device: script_config
all: script_config

# All tests specific to device configuration
TESTS_DEVICE = http_parser http_connect dns_parser
$(TESTS_DEVICE:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: http/%.c http/http_sock.c $(UTILS_SOURCE)
.PHONY: device
device: $(TESTS_DEVICE:%=run_%)
TESTS_ALL += $(TESTS_DEVICE)



# All ATEM client tests applying for both device and official clients
TESTS_ATEM_CLIENT_DEVICE = atem_client_open atem_client_close
TESTS_ATEM_CLIENT += $(TESTS_ATEM_CLIENT_DEVICE)
device: $(TESTS_ATEM_CLIENT_DEVICE:%=run_%)

# All ATEM client tests
TESTS_ATEM_CLIENT += atem_client_open_extended atem_client_close_extended
$(TESTS_ATEM_CLIENT:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: atem_client/%.c $(UTILS_SOURCE)
TESTS_ALL += $(TESTS_ATEM_CLIENT)
.PHONY: atem_client
atem_client: $(TESTS_ATEM_CLIENT:%=run_%)



# All ATEM server tests applying to both proxy server and official switchers
TESTS_ATEM_SERVER_PROXY = atem_server_open atem_server_close
TESTS_ATEM_SERVER += $(TESTS_ATEM_SERVER_PROXY)
.PHONY: atem_server_proxy
atem_server_proxy: $(TESTS_ATEM_SERVER_PROXY:%=run_%)

# All tests for testing ATEM server
TESTS_ATEM_SERVER += atem_server_open_extended
$(TESTS_ATEM_SERVER:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: atem_server/%.c $(UTILS_SOURCE)
TESTS_ALL += $(TESTS_ATEM_SERVER)
.PHONY: atem_server
atem_server: $(TESTS_ATEM_SERVER:%=run_%)



# Builds all tests and runs unit tests
$(BUILD_DIR)/build: utils/main.c $(UTILS_SOURCE)
run_build: $(TESTS_ALL:%=$(BUILD_DIR)/%)
TESTS_ALL += build

# Ensures everything is ready for release
$(BUILD_DIR)/release: release/main.c ../firmware/init.h
run_release: run_build
TESTS_ALL += release



# Playgrounds for messing around in
PLAYGROUNDS = $(patsubst playground/%.c,$(BUILD_DIR)/playground_%,$(wildcard playground/*.c))
$(PLAYGROUNDS): $(BUILD_DIR)/playground_%: playground/%.c ../core/atem.c http/http_sock.c $(UTILS_SOURCE)

# Creates dist directory if it does not exist
$(BUILD_DIR):
	mkdir -p $@

# Includes dependency files for test executables
-include $(TESTS_ALL:%=$(BUILD_DIR)/%.d)
-include $(PLAYGROUNDS:%=%.d)

# Builds test executable
$(TESTS_ALL:%=$(BUILD_DIR)/%) $(PLAYGROUNDS): | $(BUILD_DIR)
	$(CC) $(filter %.c,$^) -o $@ -g -Wall -Wextra -Wpedantic $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)
	$(CC) $(filter %.c,$^) -MM -MT $@ > $@.d

# Runs test
.PHONY: run_%
run_%: $(BUILD_DIR)/%
	$<

# Launches LLDB with test
.PHONY: lldb_%
lldb_%: $(BUILD_DIR)/%
	lldb $<

# Runs all tests
.PHONY: all
all: $(TESTS_ALL:%=run_%)

# Removes all compiled tests
RMRF = rm -rf
.PHONY: clean
clean:
	$(RM) $(TESTS_ALL:%=$(BUILD_DIR)/%) $(PLAYGROUNDS)
	$(RM) $(TESTS_ALL:%=$(BUILD_DIR)/%.d) $(PLAYGROUNDS:%=%.d)
	$(RMRF) $(TESTS_ALL:%=$(BUILD_DIR)/%.dSYM) $(PLAYGROUNDS:%=%.dSYM)

# Lists all tests except playgrounds
.PHONY: list
list:
	echo $(TESTS_ALL)
