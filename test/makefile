# Defaults to running all tests
all:

# Output directory for test executables
BUILD_DIR = ../build/test

# Shared source files
UTILS_SOURCE = utils/simple_socket.c utils/atem_sock.c utils/atem_header.c utils/atem_handshake.c utils/simple_socket.c utils/logs.c utils/payload.c utils/timer.c



# Tests normal DNS lookups
run_dns_lookup:
# Ensures unsupported qtype and qclass rejects
	! nslookup -class=ch google.com $(DEVICE_ADDR) > /dev/null
	! nslookup -type=ns google.com $(DEVICE_ADDR) > /dev/null

# Ensures requests work with or without recursive flag
	nslookup -recurse google.com $(DEVICE_ADDR) > /dev/null
	nslookup -norecuse google.com $(DEVICE_ADDR) > /dev/null

# Ensures supported qtype and qclass work
	nslookup -class=IN google.com $(DEVICE_ADDR) > /dev/null
	nslookup -class=ANY google.com $(DEVICE_ADDR) > /dev/null
	nslookup -type=A google.com $(DEVICE_ADDR) > /dev/null
	nslookup -type=ANY google.com $(DEVICE_ADDR) > /dev/null

# Ensures longest name supported by nslookup works correctly
	nslookup aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc $(DEVICE_ADDR) > /dev/null
device: run_dns_lookup
all: run_dns_lookup

# All tests specific to device configuration
TESTS_DEVICE = http_parser http_connect dns_parser
$(TESTS_DEVICE:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: http/%.c http/http_sock.c $(UTILS_SOURCE)
.PHONY: device
device: $(TESTS_DEVICE:%=run_%)
TESTS_ALL += $(TESTS_DEVICE)

# Ensures everything is ready for release
$(BUILD_DIR)/release: release/main.c ../firmware/init.h
.PHONY: release
TESTS_ALL += release



# Playground for messing around in
$(BUILD_DIR)/playground: playground.c http/http_sock.c $(UTILS_SOURCE)
.PHONY: playground
run_playground: $(BUILD_DIR)/playground
playground: run_playground

# Creates dist directory if it does not exist
$(BUILD_DIR):
	mkdir -p $@

# Includes dependency files for test executables
-include $(TESTS_ALL:%=$(BUILD_DIR)/%.d)
-include $(BUILD_DIR)/playground.d

# Builds test executable
$(TESTS_ALL:%=$(BUILD_DIR)/%) $(BUILD_DIR)/playground: | $(BUILD_DIR)
	$(CC) $(filter %.c,$^) -o $@ -g -Wall -Wextra -Wpedantic $(CFLAGS) $(LDFLAGS)
	$(CC) $(filter %.c,$^) -MM -MT $@ > $@.d

# Runs test
.PHONY: run_%
run_%: $(BUILD_DIR)/%
	$<

# Runs all tests
.PHONY: all
all: $(TESTS_ALL:%=run_%)

# Removes all compiled tests
RMRF = rm -rf
.PHONY: clean
clean:
	$(RM) $(TESTS_ALL:%=$(BUILD_DIR)/%) $(BUILD_DIR)/playground
	$(RM) $(TESTS_ALL:%=$(BUILD_DIR)/%.d) $(BUILD_DIR)/playground.d
	$(RMRF) $(TESTS_ALL:%=$(BUILD_DIR)/%.dSYM) $(BUILD_DIR)/playground.dSYM

# Lists all tests except playground
.PHONY: list
list:
	echo $(TESTS_ALL)
