ROOT = ../..
OUT_DIR = $(ROOT)/dist/extract_html

.PHONY: all
all: $(OUT_DIR)

# Generates all output files
$(OUT_DIR): $(ROOT)/firmware/http_respond.c $(ROOT)/firmware/http.h $(ROOT)/firmware/init.h ./generate.sh
	-mkdir -p $@

	echo "Generating index.html"
	./generate.sh ROOT > $(OUT_DIR)/index.html
	./generate.sh ROOT --http > $(OUT_DIR)/index.http

	echo "Generating post.html"
	./generate.sh POST_ROOT > $(OUT_DIR)/post.html
	./generate.sh POST_ROOT --http > $(OUT_DIR)/post.http

	echo "Generating error pages for specific error codes"
	sed -n 's/.*http_err(http, \"\(.*\)\").*/\1/p' $(ROOT)/firmware/http.c | while IFT= read -r code; do \
		filename=$$(echo $$code | cut -d " " -f1);\
		./generate.sh ERR --err_code="$$code" > $(OUT_DIR)/$$filename.html;\
		./generate.sh ERR --http --err_code="$$code" > $(OUT_DIR)/$$filename.http;\
	done

	echo "Generating error pages for implementation specific errors"
	sed -n 's/.*http_post_err(http, \"\(.*\)\").*/\1/p' ../../firmware/http.c | while IFT= read -r msg; do \
		filename=$$(echo $$msg | tr '[:upper:]' '[:lower:]' | sed 's/ /_/g');\
		./generate.sh ERR --http --err_body="$$msg" > $(OUT_DIR)/$$filename.http;\
		./generate.sh ERR --err_body="$msg" > $(OUT_DIR)/$$filename.html;\
	done

# Cleans up generated files and directory
.PHONY: clean
clean:
	$(RM) -r $(OUT_DIR)
