// @ts-check

import { STATUS_CODES, validateHeaderName, validateHeaderValue } from "node:http";
import { readFileSync, readdirSync } from "node:fs";
import { basename, relative } from "node:path";

let errors = 0;
const projRoot = `${import.meta.dirname}/../..`;
for (const dirent of readdirSync(projRoot, { withFileTypes: true, recursive: true })) {
	// Only lints .http files
	const path = `${dirent.parentPath}/${dirent.name}`;
	if (!path.endsWith(".http")) continue;
	console.log(relative(projRoot, path));

	// Reads file content
	const content = readFileSync(path).toString();

	// Parses HTTP response
	const headerDelimiter = "\r\n\r\n";
	let headerDelimiterIndex = content.indexOf(headerDelimiter);
	if (headerDelimiterIndex === -1) {
		console.error("Unable to find HTTP delimiter between headers and body");
		errors++;
		continue;
	}
	const headerLines = content.slice(0, headerDelimiterIndex).split("\r\n");
	const statusLine = /** @type {string} */(headerLines.shift());

	// Validates status line version
	const protocolVersionDelimiterIndex = statusLine.indexOf(" ");
	if (protocolVersionDelimiterIndex === -1) {
		console.error("Unable to find status line delimiter after protocol version:", statusLine);
		errors++;
		continue;
	}
	const protocolVersion = statusLine.slice(0, protocolVersionDelimiterIndex); 
	if (protocolVersion !== "HTTP/1.1") {
		console.error("Invalid HTTP protocol version:", protocolVersion);
		errors++;
	}

	// Validates status line code
	const statusCodeDelimiterIndex = statusLine.indexOf(" ", protocolVersionDelimiterIndex + 1);
	if (statusCodeDelimiterIndex === -1) {
		console.error("Unable to find status line delimiter after status code:", statusLine);
		errors++;
		continue;
	}
	const statusCode = statusLine.slice(protocolVersionDelimiterIndex + 1, statusCodeDelimiterIndex);
	if (Number.isNaN(Number(statusCode))) {
		console.error("Status code is not a valid number:", statusCode);
		errors++;
	}

	// Validates status line message
	const statusMessage = statusLine.slice(statusCodeDelimiterIndex + 1);
	const statusMessageExpected = STATUS_CODES[statusCode];
	if (statusMessage !== statusMessageExpected) {
		console.error("Status message did not match:", [ statusCode, statusMessage, statusMessageExpected ]);
		errors++;
	}

	// Parses HTTP headers
	for (const headerLine of headerLines) {
		// Parses header line
		const headerDelimiter = ":";
		const headerDelimiterIndex = headerLine.indexOf(headerDelimiter);
		if (headerDelimiterIndex === -1) {
			console.error("Unable to parse header line:", headerLine);
			errors++;
			continue;
		}
		const key = headerLine.slice(0, headerDelimiterIndex);
		const value = headerLine.slice(headerDelimiterIndex + headerDelimiter.length);

		// Ensures header key and value do not have leading or trailing whitespace
		if (key.length !== key.trim().length) {
			console.error("Header line key contained leading or trailing whitespace:", [ key ]);
			errors++;
		}
		if (value.length !== value.trim().length) {
			console.error("Header line value contained leading or trailing whitespace:", [ key, value ]);
			errors++;
		}

		// Validates header key
		try {
			validateHeaderName(key);
		}
		catch (err) {
			console.error(err);
			errors++;
		}

		// Validates header value
		try {
			validateHeaderValue(key, value);
		}
		catch (err) {
			console.error(err);
			errors++;
		}
	}
}

// Exits with error code corresponding to number of errors encountered
console.log(`Number of errors for ${basename(import.meta.filename)}:`, errors);
process.exitCode = errors;
