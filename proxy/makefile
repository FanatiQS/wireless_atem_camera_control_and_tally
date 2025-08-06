# Enables many compiler warnings
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -Wstrict-prototypes -Wshadow -Wmissing-prototypes
CFLAGS += -pedantic-errors -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual
CFLAGS += -Wdeclaration-after-statement -Wfloat-equal -Wformat=2
CFLAGS += -Wmissing-declarations -Wmissing-include-dirs -Wnested-externs -Wpointer-arith
CFLAGS += -Wredundant-decls -Wsequence-point -Wswitch -Wundef -Wwrite-strings
CFLAGS += -Wunreachable-code -Wno-tautological-constant-out-of-range-compare
CFLAGS += -DDEBUG=0
# CFLAGS += -Werror

# Debug flags
ifeq "$(TARGET)" "debug"
CFLAGS += -UDEBUG -DDEBUG=1 -g -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
endif

# Release flags
ifeq "$(TARGET)" "release"
CFLAGS += -DNDEBUG -O3 -Wno-unused-variable
LDFLAGS += -flto
endif



# Defaults to current platform and development target
PLATFORM = native
TARGET = dev

# Overwritable directories for output files
BUILD_ROOT = ../build/proxy
DIST_ROOT = ../dist/proxy

# Directories and paths for generated build files and output files
BUILD_DIR = $(BUILD_ROOT)/$(PLATFORM)/$(TARGET)
BIN_PATH = $(DIST_ROOT)/$(PLATFORM)/proxy_$(TARGET)
LIB_PATH = $(BUILD_ROOT)/$(PLATFORM)/libatem_$(TARGET).a



# Defaults to build binary
$(BIN_PATH):

# Lists all object files
OBJS += $(BUILD_DIR)/async.o
OBJS += $(BUILD_DIR)/atem_assert.o
OBJS += $(BUILD_DIR)/atem_cache.o
OBJS += $(BUILD_DIR)/atem_debug.o
OBJS += $(BUILD_DIR)/atem_packet.o
OBJS += $(BUILD_DIR)/atem_server.o
OBJS += $(BUILD_DIR)/atem_session.o
OBJS += $(BUILD_DIR)/timeout.o

# Generates object files for all required sources
$(BUILD_DIR)/async.o: ./async.c
$(BUILD_DIR)/atem_assert.o: ./atem_assert.c
$(BUILD_DIR)/atem_cache.o: ./atem_cache.c
$(BUILD_DIR)/atem_debug.o: ./atem_debug.c
$(BUILD_DIR)/atem_packet.o: ./atem_packet.c
$(BUILD_DIR)/atem_server.o: ./atem_server.c
$(BUILD_DIR)/atem_session.o: ./atem_session.c
$(BUILD_DIR)/main.o: ./main.c
$(BUILD_DIR)/timeout.o: ./timeout.c

# Rebuilds everything on makefile changes
$(OBJS) $(BUILD_DIR)/main.o: makefile

# Includes generated dependency files if available
-include $(BUILD_DIR)/*.d

# Builds object files
$(OBJS) $(BUILD_DIR)/main.o:
	mkdir -p $(dir $@)
	$(CC) -c $< -o $@ -MMD $(CFLAGS) $(CPPFLAGS)

# Links generated object files to a single executable
$(BIN_PATH): $(OBJS) $(BUILD_DIR)/main.o
	mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# Builds static library
$(LIB_PATH): $(OBJS)
	$(AR) -rcs $@ $^
