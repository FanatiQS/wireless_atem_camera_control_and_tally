// @ts-check

const http = require("http");
const { readFile } = require("fs/promises");
const { watch } = require("fs");
const { exec } = require("child_process");
const { promisify } = require("util");
const run = promisify(exec);

// Reloads all connected clients when source files change
const reloadResponses = new Set();
[
	"../../firmware/http_respond.c",
	"../../firmware/version.h"
].forEach((path) => {
	watch(path).on("change", () => {
		console.log(`Reloading ${reloadResponses.size} client(s)`);
		reloadResponses.forEach((res) => res.writeHead(307, { Location: "/" }).end());
	});
});

// Stores posted data after POST for all further reloaded GET requests (resets on clean connect)
/** @type {Iterable} */
let postedConfigEntries = [];

// Launches HTTP server
http.createServer(async function (req, res) {
	// Gets request url and queryParams
	const { pathname, searchParams } = new URL(req.url || "", "http://localhost");

	// Responds to POST request
	if (req.method === "POST") {
		// Saves posted data to for GET request
		let body = "";
		req.on("data", (chunk) => body += chunk);
		await new Promise((resolve) => req.on("end", resolve));
		postedConfigEntries = new URLSearchParams(body);

		console.log("Serving response for POST request");
		req.socket.end((await run(`./generate.sh POST_ROOT --http`)).stdout);
	}
	// Only accepts GET method
	else if (req.method !== "GET") {
		console.error("Rejecting invalid method:", req.method);
		req.socket.end();
	}
	// Responds to request when a file is changed
	else if (pathname == "/reload") {
		console.log("Registering reload client:", req.socket.remoteAddress);
		reloadResponses.add(res);
		req.on("close", () => reloadResponses.delete(res));
	}
	// Generates and serves configuration page that automatically reloads when changed
	else if (pathname === "/") {
		console.log("Serving response for root page");

		// Creates generator script configuration arguments from posted config and query params
		const configEntries = [
			...postedConfigEntries,
			...searchParams
		];
		const generator_args = configEntries.map(([ key, value ]) => `--${key}='${value}'`).join(" ");

		// Resets config on clean connection without referer (no reload)
		if (!req.headers.referer) {
			postedConfigEntries = [];
		}

		// Responds with generator response and appends automatic reload script on file change
		try {
			req.socket.write((await run(`./generate.sh ROOT --http ${generator_args}`)).stdout);
			req.socket.end('<script>fetch("/reload").then(() => location.reload())</script>');
		}
		catch (err) {
			console.error(err);
			res.end(err.message);
		}
	}
	// Responds with error page from file
	else {
		try {
			const fileName = pathname.slice(1);
			console.log("Serving static page:", `${fileName}.http`);
			req.socket.end(await readFile(`../../dist/extract_html/${fileName}.http`));
		}
		catch (err) {
			if (err.code != "ENOENT") throw err;
			console.error("Unable to read file:", err.path);
			req.socket.end();
		}
	}
}).listen(80);

console.log("Auto-reload server running...");
