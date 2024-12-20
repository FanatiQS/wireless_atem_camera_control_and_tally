import { execSync } from "node:child_process";

export function* listFiles(suffixes) {
	const flags = "--cached --others --exclude-standard";
	const cmd = `git ls-files ${flags} ${suffixes.map((suffix) => `'*${suffix}'`).join(" ")}`;
	for (const path of execSync(cmd).toString().split("\n")) {
		if (!suffixes.some((suffix) => path.endsWith(suffix))) continue;
		yield `${import.meta.dirname}/../../${path}`;
	}
}
