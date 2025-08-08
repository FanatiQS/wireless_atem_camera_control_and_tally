# Defaults to develompment build and native platform
BUILD ?= dev
PLATFORM ?= native

# Enables many compiler warnings
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -Wstrict-prototypes -Wshadow -Wmissing-prototypes
CFLAGS += -pedantic-errors -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual
CFLAGS += -Wfloat-equal -Wformat=2
CFLAGS += -Wmissing-declarations -Wmissing-include-dirs -Wnested-externs -Wpointer-arith
CFLAGS += -Wredundant-decls -Wsequence-point -Wswitch -Wundef -Wwrite-strings
CFLAGS += -Wunreachable-code -Wno-tautological-constant-out-of-range-compare
CFLAGS += -DDEBUG=0 -MMD
# CFLAGS += -Werror

# Configuration specific flags
ifeq "$(BUILD)" "release"
CFLAGS += -DNDEBUG -O3 -Wno-unused-variable
LDFLAGS += -flto
else ifeq "$(BUILD)" "debug"
CFLAGS += -UDEBUG -DDEBUG=1 -g -fsanitize=address,undefined
LDFLAGS += -fsanitize=address,undefined
else ifneq "$(BUILD)" "dev"
$(error Invalid build mode: $(BUILD))
endif



# Overwritable directories for output files
BUILD_ROOT ?= ../build
DIST_ROOT ?= ../dist

# Directories and paths for generated build files and output files
BUILD_DIR ?= $(BUILD_ROOT)/proxy/$(PLATFORM)_$(BUILD)
BIN_PATH ?= $(DIST_ROOT)/proxy/proxy_$(PLATFORM)_$(BUILD)
LIB_PATH ?= $(BUILD_DIR)/libatem.a



# Defaults to build binary
$(BIN_PATH):

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

# Lists all object files shared between all builds
OBJS += $(BUILD_DIR)/atem_assert.o
OBJS += $(BUILD_DIR)/atem_cache.o
OBJS += $(BUILD_DIR)/atem_debug.o
OBJS += $(BUILD_DIR)/atem_packet.o
OBJS += $(BUILD_DIR)/atem_server.o
OBJS += $(BUILD_DIR)/atem_session.o
OBJS += $(BUILD_DIR)/timeout.o

# Builds executable
$(BIN_PATH): $(OBJS) $(BUILD_DIR)/main.o
	mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

# Builds static library
$(LIB_PATH): $(OBJS) $(BUILD_DIR)/async.o
	mkdir -p $(dir $@)
	$(AR) -rcs $@ $^

# Includes all object files for remaining object file handling
OBJS += $(BUILD_DIR)/async.o
OBJS += $(BUILD_DIR)/main.o

# Rebuilds everything on makefile changes
$(OBJS): makefile

# Includes generated dependency files if available
-include $(OBJS:.o=.d)

# Builds object files
$(OBJS):
	mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

# Aliases for BIN_PATH and LIB_PATH files
.PHONY: all lib exec
all: $(LIB_PATH) $(BIN_PATH)
lib: $(LIB_PATH)
exec: $(BIN_PATH)
