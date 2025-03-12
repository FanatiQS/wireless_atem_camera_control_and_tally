# Enables many compiler warnings
CFLAGS := $(CFLAGS)
CFLAGS += -Wall -Wextra -Wpedantic -std=c11 -Wstrict-prototypes -Wshadow -Wmissing-prototypes
CFLAGS += -pedantic-errors -Waggregate-return -Wbad-function-cast -Wcast-align -Wcast-qual
CFLAGS += -Wdeclaration-after-statement -Wfloat-equal -Wformat=2
CFLAGS += -Wmissing-declarations -Wmissing-include-dirs -Wnested-externs -Wpointer-arith
CFLAGS += -Wredundant-decls -Wsequence-point -Wswitch -Wundef -Wwrite-strings
CFLAGS += -Wunreachable-code -Wno-tautological-constant-out-of-range-compare
# CFLAGS += -Werror

# Debug flags
CFLAGS += -DDEBUG=1 -g -fsanitize=address,undefined 
LDFLAGS += -fsanitize=address,undefined

# Release flags
# CFLAGS += -DDEBUG=0 -DNDEBUG -O3 -Wno-unused-variable
# LDFLAGS += -flto



# Directory for all generated build files and binary
BUILD_DIR = ../build/proxy
BIN_PATH = ../dist/proxy

# Defaults to build binary
all: $(BIN_PATH)

# Generates object files for all required sources
$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS) -MMD $(CPPFLAGS)

# Links together all generated object files to a single executable
$(BIN_PATH): $(BUILD_DIR)/main.o
$(BIN_PATH): $(BUILD_DIR)/atem_server.o
$(BIN_PATH): $(BUILD_DIR)/atem_session.o
$(BIN_PATH): $(BUILD_DIR)/atem_packet.o
$(BIN_PATH): $(BUILD_DIR)/atem_cache.o
$(BIN_PATH): $(BUILD_DIR)/atem_assert.o
$(BIN_PATH): $(BUILD_DIR)/atem_debug.o
$(BIN_PATH): $(BUILD_DIR)/timeout.o
$(BIN_PATH):
	$(CC) $^ -o $@ $(LDFLAGS)

# Includes generated dependency files if available
-include $(BUILD_DIR)/*.d
