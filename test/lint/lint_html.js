// @ts-check

import { readFileSync, readdirSync } from "node:fs";
import { HTMLHint, HTMLRules } from "htmlhint";
import { HtmlValidate } from "html-validate";
import { normalize, relative } from "node:path";
import ignore from "ignore";

// Initializes HTML validator rules
const htmlvalidate = new HtmlValidate({
	extends: ["html-validate:recommended"],
	rules: {
		"no-inline-style": "off",
		"no-implicit-close": "off",
		"no-implicit-button-type": "off",
		"no-implicit-input-type": "off",
		"meta-refresh": "off",
		"close-order": "off",
		"element-permitted-content": "off",
		"prefer-tbody": "off"
	}
});

// Initial linting rules with everything enabled
const rules = Object.fromEntries(Object.values(HTMLRules).map((rule) => [ rule.id, true ]));

// Attribute rules
rules["attr-value-single-quotes"] = false;
rules["attr-value-double-quotes"] = false;
rules["attr-value-not-empty"] = false;
rules["attr-sorted"] = false;

// Script and style rules
rules["script-disabled"] = false;
rules["inline-script-disabled"] = false;
rules["style-disabled"] = false;
rules["inline-style-disabled"] = false;

// Tag rules
rules["input-requires-label"] = false;
rules["tag-pair"] = false;
rules["tag-self-close"] = false;

// Lints files not ignored by gitignore
let errors = 0;
const rootDir = `${import.meta.dirname}/../..`;
const gitignore = ignore().add(readFileSync("../../.gitignore").toString());
for (const dirent of readdirSync(rootDir, { withFileTypes: true, recursive: true })) {
	// Reads file content
	const path = `${dirent.path}/${dirent.name}`;
	if (!path.endsWith(".html")) continue;
	if (gitignore.ignores(relative(rootDir, path))) continue;
	console.log(path);

	// Examples does not have to include a title
	const examplesPath = path.startsWith(normalize(`${rootDir}/tools/html_config_module/examples/`));

	// Lints HTML using html-validate
	const report = htmlvalidate.validateFileSync(path);
	if (!report.valid) {
		for (const result of report.results) {
			for (const message of result.messages) {
				if (message.ruleId === "attr-quotes" && message.context.error === "unquoted") continue;
				if (message.ruleId === "element-required-content" && examplesPath) continue;
				console.error(message);
				errors++;
			}
		}
	}

	// Lints HTML using HTMLHint
	const results = HTMLHint.verify(readFileSync(path).toString(), rules);
	for (const result of results) {
		if ([ '<', '>' ].some((c) => result.message === `Special characters must be escaped : [ ${c} ].`)) {
			continue;
		}
		if (result.rule.id === "title-require" && examplesPath) continue;
		if (result.rule.id === "head-script-disabled") continue;
		console.error(result);
		errors++;
	}
}

// Exits with error code corresponding to number of errors encountered
console.log(`Number of errors for ${import.meta.url.slice(import.meta.url.lastIndexOf("/") + 1)}:`, errors);
process.exitCode = errors;
