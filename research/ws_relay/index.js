const net = require("net");
const ws = require("ws");
const http = require("http");
const fs = require("fs");
const { spawn } = require("child_process");

// Creates a child process for the atem parser
let child = null;
function restartClient() {
	if (child) child.kill();
	child = spawn(
		"./client",
		[
			process.argv[2],
			"--autoReconnect",
			"--printProtocolVersion",
			"--printTally",
			"--tcpRelay",
			"--cameraId",
			process.argv[3]
		],
		{
			cwd: "../../test",
			shell: true,
			stdio: "inherit"
		}
	);
}

// Servers the html
const httpServer = http.createServer((req, res) => {
	fs.readFile("./index.html", (err, data) => {
		if (err) {
			res.end(err);
		}
		else {
			res.end(data);
		}
	});
});
httpServer.listen(8080);

// Creates WebSocket server to relay all ATEM data to
const wss = new ws.Server({ server: httpServer, clientTracking: true });
wss.on("connection", () => {
	console.log("WebSocket connected");
	restartClient();
});

// Creates TCP server for receiving ATEM data
const tcp = net.createServer();

// Binds ATEM TCP socket to all WebSocket clients
tcp.on("connection", (sock) => {
	console.log("UDP socket connected");

	// Relays ATEM data to all WebSocket clients
	sock.on("data", (data) => {
		wss.clients.forEach((ws) => ws.send(data));
	});

	// Logs message if TCP socket to ATEM closes
	sock.on("close", () => {
		console.log("TCP socket closed");
	});

	// Logs error on ATEM TCP socket
	sock.on("error", (err) => {
		console.log("TCP socket error");
		console.error(err);
	});
});

// Logs error on ATEM TCP server
tcp.on("error", (err) => {
	console.log("TCP server error");
	console.error(err);
});

// Logs message if ATEM TCP server closes
tcp.on("close", () => {
	console.log("TCP server closed");
});

// ATEM TCP server listens on port 9910, the same as ATEM UDP port
tcp.listen(9910);
