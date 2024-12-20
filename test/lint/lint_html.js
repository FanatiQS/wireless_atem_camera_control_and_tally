// @ts-check

import { readFileSync } from "node:fs";
import { HTMLHint } from "htmlhint";
import { HtmlValidate } from "html-validate";
import { listFiles } from "./utils.js";

// Initializes HTML validator rules
const htmlvalidate = new HtmlValidate({
	extends: [ "html-validate:recommended" ],
	rules: {
		"no-inline-style": "off",
		"no-implicit-close": "off",
		"no-implicit-button-type": "off",
		"no-implicit-input-type": "off",
		"meta-refresh": "off",
		"close-order": "off",
		"element-permitted-content": "off",
		"prefer-tbody": "off",
		"attr-quotes": [ "error", { style: "double", unquoted: true } ]
	}
});

// Initial linting rules with everything enabled
const rules = Object.fromEntries(Object.keys(HTMLHint.rules).map((rule) => [ rule, true ]));

// Attribute rules
rules["attr-value-single-quotes"] = false;
rules["attr-value-double-quotes"] = false;
rules["attr-value-not-empty"] = false;
rules["attr-sorted"] = false;

// Script and style rules
rules["script-disabled"] = false;
rules["inline-script-disabled"] = false;
rules["head-script-disabled"] = false;
rules["style-disabled"] = false;
rules["inline-style-disabled"] = false;

// Tag rules
rules["input-requires-label"] = false;
rules["tag-pair"] = false;
rules["tag-self-close"] = false;

// Lints files not ignored by gitignore
let errors = 0;
const files = [ "index.html", "post.html" ].map((name) => `${import.meta.dirname}/../../dist/extract_html/${name}`);
for (const path of [ ...listFiles([ ".html" ]), ...files ]) {
	console.log(path);

	// Examples does not have to include a title
	const examplesPath = path.startsWith(`${import.meta.dirname}/../../tools/html_config_module/examples/`);

	// Lints HTML using html-validate
	const report = htmlvalidate.validateFileSync(path);
	if (!report.valid) {
		for (const result of report.results) {
			for (const message of result.messages) {
				if (message.message === "<head> element must have <title> as content" && examplesPath) continue;
				console.error(message);
				errors++;
			}
		}
	}

	// Lints HTML using HTMLHint
	const results = HTMLHint.verify(readFileSync(path).toString(), rules);
	for (const result of results) {
		if (result.rule.id === "title-require" && examplesPath) continue;
		console.error(result);
		errors++;
	}
}

// Exits with error code corresponding to number of errors encountered
console.log(`Number of errors for ${import.meta.url.slice(import.meta.url.lastIndexOf("/") + 1)}:`, errors);
process.exitCode = errors;
