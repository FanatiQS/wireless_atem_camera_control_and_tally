# Defaults to running all tests
.PHONY: all
all:

# Output directory for test executables
BUILD_DIR = ../build/test

# Creates dist directory if it does not exist
$(BUILD_DIR):
	mkdir $@

# Builds test executable
$(BUILD_DIR)/%: | $(BUILD_DIR)
	$(CC) $(filter %.c,$^) -o $@ -g -Wall -Wextra -Wpedantic
	$(CC) $(filter %.c,$^) -MM -MT $@ -MF $@.d

# Runs test
.PHONY: run_%
run_%: $(BUILD_DIR)/%
	$<



$(BUILD_DIR)/http_parser: http/http_parser.c http/http_sock.c utils/simple_socket.c utils/logs.c
TESTS_DEVICE += run_http_parser

$(BUILD_DIR)/http_connect: http/http_connect.c http/http_sock.c utils/simple_socket.c utils/logs.c
TESTS_DEVICE += run_http_connect

$(BUILD_DIR)/dns_parser: http/dns_parser.c utils/simple_socket.c utils/logs.c
TESTS_DEVICE += run_dns_parser

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
TESTS_DEVICE += run_dns_lookup

# Runs all tests for device
.PHONY: device
device: $(TESTS_DEVICE)
TESTS_ALL += $(TESTS_DEVICE)
ifdef DEVICE_ADDR
all: $(TESTS_DEVICE)
endif



# Includes dependency files for test executables
DEP_FILES = $(TESTS_ALL:run_%=$(BUILD_DIR)/%.d) $(BUILD_DIR)/playground.d
.PHONY: $(DEP_FILES)
-include $(DEP_FILES)

# Runs playground
$(BUILD_DIR)/playground: playground.c http/http_sock.c utils/simple_socket.c utils/logs.c
.PHONY: playground
run_playground: $(BUILD_DIR)/playground
playground: run_playground

# Removes all compiled tests
RMRF = rm -rf
.PHONY: clean
clean:
	$(RM) $(TESTS_ALL:run_%=$(BUILD_DIR)/%) $(BUILD_DIR)/playground
	$(RM) $(TESTS_ALL:run_%=$(BUILD_DIR)/%.d) $(BUILD_DIR)/playground.d
	$(RMRF) $(TESTS_ALL:run_%=$(BUILD_DIR)/%.dSYM) $(BUILD_DIR)/playground.dSYM

# Lists all tests except playground
.PHONY: list
list:
	echo $(TESTS_ALL)
