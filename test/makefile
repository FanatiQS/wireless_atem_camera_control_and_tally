# Defaults to running all tests
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



# Includes dependency files for test executables
DEP_FILES = $(TESTS_ALL:run_%=$(BUILD_DIR)/%.d) $(BUILD_DIR)/playground.d
.PHONY: $(DEP_FILES)
-include $(DEP_FILES)

# Runs playground
$(BUILD_DIR)/playground: playground.c http/http_sock.c
.PHONY: playground
run_playground: $(BUILD_DIR)/playground
playground: run_playground

# Runs all tests
.PHONY: all
all: $(TESTS_ALL)

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
