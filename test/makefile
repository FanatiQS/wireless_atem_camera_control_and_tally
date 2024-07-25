# Output directory for test executables
BUILD_DIR = ../build/test

# Defaults to running all tests
all:

# All tests specific to device configuration
EXECS_DEVICE = http_parser http_close http_connect dns_parser
$(EXECS_DEVICE:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: http/%.c http/http_sock.c
SCRIPTS_DEVICE += config/test_conf http/dns_lookup
.PHONY: config_device
config_device: $(EXECS_DEVICE) $(SCRIPTS_DEVICE)
EXECS += $(EXECS_DEVICE)
SCRIPTS += $(SCRIPTS_DEVICE)

# All ATEM client tests for both device and official clients
EXECS_ATEM_CLIENT = atem_client_open atem_client_close atem_client_data
EXECS_ATEM_CLIENT_EXTENDED += atem_client_open_extended atem_client_close_extended
.PHONY: atem_client atem_client_extended
atem_client: $(EXECS_ATEM_CLIENT)
atem_client_extended: $(EXECS_ATEM_CLIENT_EXTENDED)
EXECS_ATEM_CLIENT += $(EXECS_ATEM_CLIENT_EXTENDED)
$(EXECS_ATEM_CLIENT:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: atem_client/%.c
EXECS += $(EXECS_ATEM_CLIENT)

# All ATEM server tests for both proxy server and official switchers
EXECS_ATEM_SERVER = atem_server_open atem_server_close
EXECS_ATEM_SERVER_EXTENDED += atem_server_open_extended
.PHONY: atem_server atem_server_extended
atem_server: $(EXECS_ATEM_SERVER)
atem_server_extended: $(EXECS_ATEM_SERVER_EXTENDED)
EXECS_ATEM_SERVER += $(EXECS_ATEM_SERVER_EXTENDED)
$(EXECS_ATEM_SERVER:%=$(BUILD_DIR)/%): $(BUILD_DIR)/%: atem_server/%.c
EXECS += $(EXECS_ATEM_SERVER)

# Builds all tests and runs unit tests
$(BUILD_DIR)/build: utils/main.c
build: $(EXECS:%=$(BUILD_DIR)/%)
EXECS += build

# Ensures everything is ready for release
$(BUILD_DIR)/release: release/main.c
EXECS += release

# Playgrounds for messing around in
PLAYGROUNDS = $(patsubst playground/%.c,$(BUILD_DIR)/playground_%,$(wildcard playground/*.c))
$(PLAYGROUNDS): $(BUILD_DIR)/playground_%: playground/%.c http/http_sock.c



# Shared source files
$(EXECS:%=$(BUILD_DIR)/%) $(PLAYGROUNDS): \
	utils/simple_socket.c \
	utils/atem_sock.c \
	utils/atem_header.c \
	utils/atem_handshake.c \
	utils/atem_command.c \
	utils/simple_socket.c \
	utils/logs.c \
	utils/atem_acknowledge.c \
	utils/timediff.c \
	utils/runner.c \
	../core/atem.c

# Includes dependency files for test executables
-include $(EXECS:%=$(BUILD_DIR)/%.d)
-include $(PLAYGROUNDS:%=%.d)

# Creates dist directory if it does not exist
$(BUILD_DIR):
	mkdir -p $@

# Builds test executable
$(EXECS:%=$(BUILD_DIR)/%) $(PLAYGROUNDS): | $(BUILD_DIR)
	$(CC) $(filter %.c,$^) -o $@ -g -Wall -Wextra -Wpedantic -Wmissing-prototypes $(CFLAGS) $(CPPFLAGS) $(LDFLAGS)
	$(CC) $(filter %.c,$^) -MM -MT $@ > $@.d

# Runs test
.PHONY: $(EXECS)
$(EXECS) $(PLAYGROUNDS:$(BUILD_DIR)/%=%): %: $(BUILD_DIR)/%
	./$<

# Runs script
.PHONY: $(SCRIPTS)
$(SCRIPTS):
	./$@.sh

# Runs all tests
.PHONY: all
all: $(EXECS) $(SCRIPTS)

# Launches LLDB with test
.PHONY: lldb_%
lldb_%: $(BUILD_DIR)/%
	lldb $<

# Removes all compiled tests
RMRF = rm -rf
.PHONY: clean
clean:
	$(RM) $(EXECS:%=$(BUILD_DIR)/%) $(PLAYGROUNDS)
	$(RM) $(EXECS:%=$(BUILD_DIR)/%.d) $(PLAYGROUNDS:%=%.d)
	$(RMRF) $(EXECS:%=$(BUILD_DIR)/%.dSYM) $(PLAYGROUNDS:%=%.dSYM)

# Lists all tests except playgrounds
.PHONY: list
list:
	echo $(EXECS)
	echo $(SCRIPTS)
