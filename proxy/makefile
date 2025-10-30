# Defaults to development build and native platform
BUILD ?= dev
PLATFORM ?= native

# Sets build flags shared by all builds
CFLAGS += -DDEBUG=0 -MMD -g

# Enables many compiler warnings
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -Wstrict-prototypes -Wshadow -Wmissing-prototypes
CFLAGS += -pedantic-errors -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual
CFLAGS += -Wfloat-equal -Wformat=2
CFLAGS += -Wmissing-declarations -Wmissing-include-dirs -Wnested-externs -Wpointer-arith
CFLAGS += -Wredundant-decls -Wsequence-point -Wswitch -Wundef -Wwrite-strings
CFLAGS += -Wunreachable-code -Wno-tautological-constant-out-of-range-compare

# Configuration specific flags
ifeq "$(BUILD)" "release"
CFLAGS += -DNDEBUG -g0 -O3 -Werror -Wno-unused-variable
LDFLAGS += -flto
else ifeq "$(BUILD)" "debug"
CFLAGS += -UDEBUG -DDEBUG=1 -fsanitize=address,undefined
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

# Cleans generated files
.PHONY: clean
clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(BIN_PATH) $(LIB_PATH) $(BIN_PATH).dSYM $(BIN_PATH).pdb
	-rmdir $(BUILD_DIR) $(BUILD_ROOT)/proxy $(DIST_ROOT)/proxy $(BUILD_ROOT) $(DIST_ROOT)



# Sets target triple for cross compilation
ifneq "$(PLATFORM)" "native"
CFLAGS += --target=$(PLATFORM)
LDFLAGS += --target=$(PLATFORM)
endif

# All cross compilation platforms
PLATFORMS += aarch64-macos-none x86_64-macos-none
PLATFORMS += aarch64-windows-gnu x86-windows-gnu x86_64-windows-gnu
PLATFORMS += aarch64-linux-gnu x86-linux-gnu x86_64-linux-gnu

# Allows defining variables through rules by peeling off each layer
all_platforms/%:; $(MAKE) $(addsuffix /$*,$(PLATFORMS))
all_builds/%:; $(MAKE) $(addsuffix /$*,release dev debug)
$(foreach p,$(PLATFORMS), $(eval $(p)/%:; $(MAKE) $$* PLATFORM=$(p)))
$(foreach p,release dev debug, $(eval $(p)/%:; $(MAKE) $$* BUILD=$(p)))

# Cleans all platforms and builds
.PHONY: clean_all
clean_all:
	$(MAKE) all_platforms/all_builds/clean
	$(MAKE) all_builds/clean
