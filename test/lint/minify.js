// @ts-check

import htmlMinifier from "html-minifier";
import { execSync } from "child_process";

// Proves that the generated HTML can not be minified further
let errors = 0;
for (const state of [ "ROOT", "POST_ROOT" ]) {
	const cmd = `../../tools/extract_html/generate.sh ${state} --ssid='1 2' --psk='3 4' --name='5 6'`;
	const html1 = execSync(cmd).toString();

	const html2 = htmlMinifier.minify(html1, {
		collapseBooleanAttributes: true,
		collapseInlineTagWhitespace: true,
		collapseWhitespace: true,
		minifyCSS: true,
		minifyJS: true,
		minifyURLs: true,
		removeAttributeQuotes: true,
		removeComments: true,
		removeEmptyAttributes: true,
		removeOptionalTags: true,
		removeRedundantAttributes: true,
		removeScriptTypeAttributes: true,
		removeStyleLinkTypeAttributes: true,
		sortAttributes: true,
		sortClassName: true,
		trimCustomFragments: true,
		useShortDoctype: true
	});

	if (html1.length === html2.length) continue;
	console.error(html1.length, html2.length);
	const [ nodes1, nodes2 ] = [ html1, html2 ].map((html) => html.split(/(\<[^\<]*)/).filter((str) => str));
	for (let i = 0; i < nodes1.length; i++) {
		const node1 = nodes1[i];
		const node2 = nodes2[i];
		if (!node1 || !node2 || node1.length !== node2.length) {
			if (node1 && node2) {
				console.error(node1.length, node2.length);
			}
			if (node1) {
				console.error(node1);
			}
			if (node2) {
				console.error(node2);
			}
			console.error();
		}
	}
}

// Exits with error code corresponding to number of errors encountered
console.log(`Number of errors for ${import.meta.url.slice(import.meta.url.lastIndexOf("/") + 1)}:`, errors);
process.exitCode = errors;
