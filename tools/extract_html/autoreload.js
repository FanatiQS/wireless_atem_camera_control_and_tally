const http = require("http");
const { readFile } = require("fs/promises");
const { watch } = require("fs");
const { exec } = require("child_process");
const { promisify } = require("util");
const run = promisify(exec);

// Gets path to configuration file
const configFilePath = process.argv[2] || "./config.json";
console.log("Using configuration file at:", configFilePath);

// Reloads all connected clients when config file or source file changes
const reloadingResponses = new Set();
[
	configFilePath,
	"../../firmware/http_respond.c",
	"../../firmware/version.h"
].forEach((path) => {
	watch(path).on("change", () => {
		console.log(`Reloading ${reloadingResponses.size} client(s)`);
		reloadingResponses.forEach((res) => res.writeHead(307, { Location: "/" }).end());
	});
});

// Launches HTTP server
http.createServer(async function (req, res) {
	// Responds to POST request
	if (req.method === "POST") {
		req.socket.end((await run(`./generate.sh POST_ROOT --http`)).stdout);
	}
	// Only accepts GET method
	else if (req.method !== "GET") {
		console.error("Rejecting invalid method:", req.method);
		req.socket.end();
	}
	// Responds to request when a file is changed
	else if (req.url == "/reload") {
		reloadingResponses.add(res);
		req.on("close", () => reloadingResponses.delete(res));
	}
	// Generates and serves configuration page that automatically reloads when changed
	else if (req.url === "/") {
		// Delays fetching after POST to simulate device rebooting
		if (req.headers.referer) {
			await new Promise((resolve) => setTimeout(resolve, 1000));
		}

		// Creates generator script configuration arguments from config file
		let generator_arguments = "";
		try {
			const config = JSON.parse(await readFile(configFilePath));
			generator_arguments = Object.entries(config).map(([ key, value ]) => `--${key}="${value}"`).join(" ");
		}
		catch (err) {
			if (err.code !== "ENOENT") throw err;
			console.log("No config file found:", err.path);
		}

		// Responds with generator response and appends automatic reload script on file change
		req.socket.write((await run(`./generate.sh ROOT --http ${generator_arguments}`)).stdout);
		req.socket.end('<script>fetch("/reload").then(() => location.reload())</script>');
	}
	// Responds with error page from file
	else {
		try {
			const fileName = req.url.slice(1);
			req.socket.end(await readFile(`../../dist/extract_html/${fileName}.http`));
		}
		catch (err) {
			if (err.code != "ENOENT") throw err;
			console.error("Unable to read file:", err.path);
			req.socket.end();
		}
	}
}).listen(80);

console.log("Autoreload server running...");
