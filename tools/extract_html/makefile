ROOT = ../..
DIST_DIR = $(ROOT)/dist/extract_html

FILES = index post

OUT_FILES = $(FILES:%=$(DIST_DIR)/%.http) $(FILES:%=$(DIST_DIR)/%.html)

# Generates all files
.PHONY: all
all: $(OUT_FILES)

# Creates output directory
$(DIST_DIR):
	mkdir -p $@

# Generates .http files
$(DIST_DIR)/%.http: | $(DIST_DIR)
	./generate.sh "$(CASE)" --http > $@

# Generates .html files
$(DIST_DIR)/%.html: | $(DIST_DIR)
	./generate.sh "$(CASE)" > $@

# Defines case state for each output
$(DIST_DIR)/index.http: CASE=ROOT
$(DIST_DIR)/post.http: CASE=POST_ROOT
$(DIST_DIR)/index.html: CASE=ROOT
$(DIST_DIR)/post.html: CASE=POST_ROOT

# Cleans up compiled binary and generated files
.PHONY: clean
clean:
	$(RM) $(OUT_FILES)
	rmdir $(DIST_DIR)
