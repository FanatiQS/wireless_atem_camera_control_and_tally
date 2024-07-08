#include <string.h> // strstr, memcmp, strlen, strchr
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <stdio.h> // fopen, fread, FILE, fprintf, stderr
#include <stddef.h> // NULL, size_t

#include "../../firmware/version.h" // FIRMWARE_VERSION_STRING

#define CHANGELOG_HEAD "# Changelog\n\n## Version "
#define CHANGELOG_MATCH CHANGELOG_HEAD FIRMWARE_VERSION_STRING "\n"

// Ensures everything is ready for release
int main(void) {
	// Ensures firmware header version is not a development version
	if (strstr(FIRMWARE_VERSION_STRING, "-dev") != NULL) {
		fprintf(stderr, "Version is \"" FIRMWARE_VERSION_STRING "\"\n");
		return EXIT_FAILURE;
	}

	// Reads content from changelog file
	char changelog_content[1024];
	FILE* changelog_file = fopen("../changelog.md", "r");
	size_t changelog_len = fread(changelog_content, sizeof(char), sizeof(changelog_content), changelog_file);
	if (changelog_len < strlen(CHANGELOG_MATCH)) {
		fprintf(stderr, "Content from changelog file too short: %zu %zu\n", changelog_len, strlen(CHANGELOG_MATCH));
		return EXIT_FAILURE;
	}
	changelog_content[changelog_len - 1] = '\0';

	// Compares changelog file content with expected content
	if (memcmp(changelog_content, CHANGELOG_MATCH, strlen(CHANGELOG_MATCH)) != 0) {
		char* version = &changelog_content[strlen(CHANGELOG_HEAD)];
		char* lf = strchr(version, '\n');
		if (lf != NULL) {
			*lf = '\0';
		}
		fprintf(stderr, "Changelog is \"%s\", expected \"%s\"\n", version, FIRMWARE_VERSION_STRING);
		return EXIT_FAILURE;
	}

	printf("Ready for release\n");
	return EXIT_SUCCESS;
}
