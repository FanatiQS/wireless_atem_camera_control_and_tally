// @ts-check

import { execSync } from "node:child_process";

/**
 * Lists all files matching one of listed file extensions and is not ignored by any ignore files
 * @param {string[]} suffixes
 * @returns {string[]}
 */
export function listFiles(suffixes) {
	const flags = "--cached --others --exclude-standard";
	const cmd = `git ls-files ${flags} ${suffixes.map((suffix) => `'*${suffix}'`).join(" ")}`;
	return execSync(cmd).toString().split("\n").slice(0, -1).map((path) => {
		return `${import.meta.dirname}/../../${path}`;
	});
}
