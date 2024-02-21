#include <string.h> // strstr, memcmp, strlen, strchr
#include <stdlib.h> // EXIT_FAILURE, EXIT_SUCCESS
#include <stdio.h> // fopen, fread, FILE, fprintf, stderr
#include <stddef.h> // NULL, size_t

#include "../../firmware/init.h" // FIRMWARE_VERSION_STRING

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
	char changelogContent[1024];
	FILE* changelogFile = fopen("../changelog.md", "r");
	size_t changelogLen = fread(changelogContent, sizeof(char), sizeof(changelogContent), changelogFile);
	if (changelogLen < strlen(CHANGELOG_MATCH)) {
		fprintf(stderr, "Content from changelog file too short: %zu %zu\n", changelogLen, strlen(CHANGELOG_MATCH));
		return EXIT_FAILURE;
	}
	changelogContent[changelogLen - 1] = '\0';

	// Compares changelog file content with expected content
	if (memcmp(changelogContent, CHANGELOG_MATCH, strlen(CHANGELOG_MATCH))) {
		char* version = &changelogContent[strlen(CHANGELOG_HEAD)];
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
