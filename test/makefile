# Defaults to running all tests
all:

# Output directory for test executables
DIST_DIR = ../dist

# Creates dist directory if it does not exist
$(DIST_DIR):
	mkdir $@

# Builds test executable
$(DIST_DIR)/test_%: | $(DIST_DIR)
	$(CC) $^ -o $@ -g -Wall -Wextra -Wpedantic

# Runs test
.PHONY: run_%
run_%: $(DIST_DIR)/test_%
	$<



$(DIST_DIR)/test_http_parser: http/http_parser.c utils/http_sock.c utils/runner.c
TESTS_ALL += run_http_parser

$(DIST_DIR)/test_http_connect: http/http_connect.c utils/http_sock.c
TESTS_ALL += run_http_connect



# Runs playground
$(DIST_DIR)/test_playground: playground.c utils/http_sock.c utils/runner.c
.PHONY: playground
playground: $(DIST_DIR)/test_playground

# Runs all tests
.PHONY: all
all: $(TESTS_ALL)

# Removes all compiled tests
.PHONY: clean
clean:
	$(RM) $(TESTS_ALL:run_%=$(DIST_DIR)/test_%) $(DIST_DIR)/test_playground

# Lists all tests except playground
.PHONY: list
list:
	echo $(TESTS_ALL)
