ROOT = ../..
DIST_DIR = $(ROOT)/dist/extract_html
GENERATOR = $(ROOT)/build/html_generate

FILES = index post

OUT_FILES = $(FILES:%=$(DIST_DIR)/%.http) $(FILES:%=$(DIST_DIR)/%.html)

# Generates all files
.PHONY: all
all: $(OUT_FILES)

# Creates output directory
$(DIST_DIR):
	mkdir -p $@

# Compiles executable from generated source file piped to compilers stdin
$(GENERATOR): ./main.c ../../firmware/http_respond.c 
	./generate.sh | $(CC) -xc - -o $@

# Generates .http files
$(DIST_DIR)/%.http: $(GENERATOR) | $(DIST_DIR)
	./$< "$(CASE)" > $@

# Generates .html files from .http
$(DIST_DIR)/%.html: $(DIST_DIR)/%.http
	sed -n '/^.$$/,//p' $< | tail -n +2 > $@

# Defines case state for each output
$(DIST_DIR)/index.http: CASE=0
$(DIST_DIR)/post.http: CASE=1

# Cleans up compiled binary and generated files
.PHONY: clean
clean:
	$(RM) $(GENERATOR) $(OUT_FILES)
	rmdir $(DIST_DIR)
