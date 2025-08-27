#include <assert.h> // assert
#include <stdlib.h> // malloc, exit
#include <string.h> // strcmp, strlen, strcpy
#include <stdio.h> // fprintf, stderr, sprintf, printf
#include <stddef.h> // size_t
#include <stdbool.h> // bool, true, false

#include <sys/socket.h> // SOCK_STREAM
#include <arpa/inet.h> // inet_addr

#include "../utils/utils.h"

// Size of string buffers
#define BUF_MAX_SIZE (1024)

// Configuration entry state
struct config_entry {
	const char* key;
	char value[BUF_MAX_SIZE];
};

// Configuration entry chnge
struct config_change {
	const char* key;
	const char* value;
};

// Global configuration state to compare against
static struct config_entry state_config[] = {
	{ .key = "ssid" },
	{ .key = "psk" },
	{ .key = "name" },
	{ .key = "atem" },
	{ .key = "dest" },
	{ .key = "dhcp" },
	{ .key = "localip" },
	{ .key = "netmask" },
	{ .key = "gateway" }
};
#define CONFIG_ENTRIES_COUNT (sizeof(state_config) / sizeof(state_config[0]))



// Flushes HTML data in stream until input tag or end of stream is reached
static bool config_element_next(int sock) {
	const char* const match = "<input ";
	size_t index = 0;
	http_socket_recv_chunking_start();

	char quote = '\0';
	char buf[2];
	while (true) {
		size_t recved = http_socket_recv(sock, buf, sizeof(buf));
		if (recved == 0) {
			http_socket_recv_chunking_end();
			return false;
		}
		assert(recved == 1);
		assert(buf[0] != '\0');

		if (quote) {
			if (buf[0] == quote) {
				quote = '\0';
			}
		}
		else if (buf[0] == '"') {
			quote = '"';
		}
		else if (buf[0] == '\'') {
			quote = '\'';
		}
		else if (buf[0] == match[index]) {
			index++;
			if (match[index] == '\0') {
				return true;
			}
		}
		else {
			index = 0;
		}
	}
}

// Gets input attribute value from HTTP stream and return value indicates if input tag is still open
static bool config_attribute_value(int sock, char buf[BUF_MAX_SIZE]) {
	size_t recved = http_socket_recv(sock, buf, 2);
	assert(recved == 1);
	size_t index = 0;
	if (buf[0] == '"') {
		while (true) {
			assert(index < BUF_MAX_SIZE);
			recved = http_socket_recv(sock, buf + index, 2);
			assert(recved == 1);
			if (buf[index] == '"') {
				break;
			}
			index++;
		}
	}
	else {
		do {
			index++;
			assert(index < BUF_MAX_SIZE);
			recved = http_socket_recv(sock, buf + index, 2);
			assert(recved == 1);
		} while (buf[index] != ' ' && buf[index] != '>');
	}
	bool ret = (buf[index] != '>');
	buf[index] = '\0';
	return ret;
}

// Fills config entry from HTTP stream
static void config_attribute_get(int sock, struct config_entry* entries) {
	char buf[BUF_MAX_SIZE];
	char value[BUF_MAX_SIZE] = {0};
	struct config_entry* entry = NULL;
	bool checked = false;

	while (true) {
		// Gets input attribute key from HTTP stream
		char c;
		size_t index_tag = 0;
		do {
			assert(index_tag < sizeof(buf) - 2);
			size_t recved = http_socket_recv(sock, &buf[index_tag], 2);
			assert(recved == 1);
			c = buf[index_tag];
			index_tag++;
		} while (c != '=' && c != ' ' && c != '>');

		// Gets config entry with the same key/name
		if (strcmp(buf, "name=") == 0) {
			bool tag_closed = !config_attribute_value(sock, buf);
			size_t index_entry = 0;
			while (strcmp(buf, entries[index_entry].key) != 0) {
				index_entry++;
				assert(index_entry < CONFIG_ENTRIES_COUNT);
			}
			entry = &entries[index_entry];
			if (tag_closed) {
				break;
			}
		}
		// Gets config entry key from name attribute
		else if (strcmp(buf, "value=") == 0) {
			if (!config_attribute_value(sock, value)) {
				break;
			}
		}
		// Gets checked attribute
		else if (strcmp(buf, "checked ") == 0) {
			checked = true;
		}
		else if (strcmp(buf, "checked>") == 0) {
			checked = true;
			break;
		}
		// Flushes uninteresting attributes value
		else if (c != ' ') {
			if (!config_attribute_value(sock, buf)) {
				break;
			}
		}
	}

	// Copies buffered value into config entries value buffer
	assert(entry != NULL);
	if (strcmp(entry->key, "dhcp") == 0) {
		sprintf(entry->value, "%d", checked);
	}
	else {
		assert(strlen(value) > 0);
		strcpy(entry->value, value);
	}
}

// Fills all config entries from HTTP stream
static void config_get(int sock, struct config_entry* entries) {
	// Makes HTTP request
	http_socket_send_string(sock, "GET / HTTP/1.0\r\n\r\n");
	http_socket_recv_cmp_status(sock, 200);

	// Extracts configuration
	while (config_element_next(sock)) {
		config_attribute_get(sock, entries);
	}
	http_socket_recv_close(sock);

	for (size_t i = 0; i < CONFIG_ENTRIES_COUNT; i++) {
		printf("\t%s=%s\n", entries[i].key, entries[i].value);
	}
}



// Posts configuration changes to the device over HTTP
static void config_post(int sock, struct config_change* change_config, size_t change_len) {
	// Builds HTTP body
	char body_buf[BUF_MAX_SIZE];
	int body_len = 0;
	for (size_t i = 0; i < change_len; i++) {
		body_len += snprintf(
			body_buf + body_len,
			sizeof(body_buf) - (size_t)body_len,
			"%s=%s&",
			change_config[i].key, change_config[i].value
		);
		assert(body_len >= 0 && (size_t)body_len < sizeof(body_buf));
	}
	assert(body_buf[body_len] == '\0');
	body_buf[--body_len] = '\0';

	// Posts HTTP request with body
	char req_buf[BUF_MAX_SIZE];
	int req_len = snprintf(
		req_buf, sizeof(req_buf),
		"POST / HTTP/1.0\r\nContent-Length: %d\r\n\r\n", body_len
	);
	assert(req_len >= 0 && (size_t)req_len < sizeof(req_buf));
	assert(req_buf[req_len] == '\0');
	http_socket_send_string(sock, req_buf);
	http_socket_send_string(sock, body_buf);
	http_socket_recv_cmp_status(sock, 200);
	http_socket_recv_flush(sock);
	http_socket_recv_close(sock);

	// Inserts config changes into global config state
	for (size_t change_index = 0; change_index < change_len; change_index++) {
		size_t state_index = 0;
		do {
			assert(state_index < CONFIG_ENTRIES_COUNT);
		} while (strcmp(state_config[++state_index].key, change_config[change_index].key) != 0);
		strcpy(state_config[state_index].value, change_config[change_index].value);
	}
}

// Ensures remote configuration matches local configuration
static void config_compare(int sock) {
	struct config_entry entries_remote[CONFIG_ENTRIES_COUNT] = {{0}};
	for (size_t i = 0; i < CONFIG_ENTRIES_COUNT; i++) {
		entries_remote[i].key = state_config[i].key;
		entries_remote[i].value[0] = '\0';
	}
	config_get(sock, entries_remote);

	int errors = 0;
	for (size_t i = 0; i < CONFIG_ENTRIES_COUNT; i++) {
		if (strcmp(state_config[i].value, entries_remote[i].value) != 0) {
			fprintf(
				stderr,
				"Configuraion for '%s' did not match: %s, %s\n",
				state_config[i].key, state_config[i].value, entries_remote[i].value
			);
			errors++;
		}
	}
	if (errors > 0) {
		exit(errors);
	}
}



int main(void) {
	// Gets network configurations from environment variables
	const char* arg_localip = getenv("CONFIG_TEST_LOCALIP"); // "192.168.1.59";
	const char* arg_netmask = getenv("CONFIG_TEST_NETMASK"); // "255.255.255.0";
	const char* arg_gateway = getenv("CONFIG_TEST_GATEWAY"); // "192.168.1.1";
	const char* arg_atem = getenv("CONFIG_TEST_ATEM"); // "192.168.1.69";
	if (!arg_localip || !arg_netmask || !arg_gateway || !arg_atem) {
		fprintf(
			stderr,
			"Configuration not fully set:\n"
			"\tCONFIG_TEST_LOCALIP: %s\n"
			"\tCONFIG_TEST_NETMASK: %s\n"
			"\tCONFIG_TEST_GATEWAY: %s\n"
			"\tCONFIG_TEST_ATEM:    %s\n",
			arg_localip, arg_netmask, arg_gateway, arg_atem
		);
		abort();
	}

	printf("Connect to device at DHCP ip\n");
	config_get(http_socket_create(), state_config);

	printf("Configure device to static ip\n");
	struct config_change config_change1[] = {
		{ .key = "dest", .value = "254" },
		{ .key = "dhcp", .value = "0" },
		{ .key = "localip", .value = arg_localip },
		{ .key = "netmask", .value = arg_netmask },
		{ .key = "gateway", .value = arg_gateway }
	};
	config_post(http_socket_create(), config_change1, sizeof(config_change1) / sizeof(config_change1[0]));

	printf("Validate device configuration at static ip\n");
	int sock = simple_socket_create(SOCK_STREAM);
	simple_socket_connect(sock, HTTP_PORT, inet_addr(arg_localip));
	config_compare(sock);

	printf("Configure device back to DHCP\n");
	struct config_change config_change2[] = {
		{ .key = "dest", .value = "1" },
		{ .key = "dhcp", .value = "1" },
		{ .key = "netmask", .value = "255.255.255.255" },
		{ .key = "gateway", .value = "0.0.0.0" },
		{ .key = "atem", .value = arg_atem }
	};
	sock = simple_socket_create(SOCK_STREAM);
	simple_socket_connect(sock, HTTP_PORT, inet_addr(arg_localip));
	config_post(sock, config_change2, sizeof(config_change2) / sizeof(config_change2[0]));

	printf("Validate device configuration at DHCP ip\n");
	config_compare(http_socket_create());

	printf("Success\n");
}
