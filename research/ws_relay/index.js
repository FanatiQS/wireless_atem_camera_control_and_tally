const net = require("net");
const dgram = require("dgram");
const ws = require("ws");
const http = require("http");
const fs = require("fs");
const { spawn } = require("child_process");

// Gets command line arguments
const atemAddr = process.argv[2];
const cameraDest = process.argv[3]

// Creates a child process for the atem parser
let child = null;
function restartClient() {
	if (child) child.kill();
	child = spawn(
		"./client",
		[
			atemAddr,
			"--autoReconnect",
			"--printProtocolVersion",
			"--printTally",
			"--tcpRelay",
			"--cameraId",
			cameraDest
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
wss.on("connection", (ws) => {
	console.log("WebSocket connected");
	restartClient();
	ws.on("close", () => {
		console.log("WebSocket closed");
	});
	ws.on("error", (err) => {
		console.log("WebSocket error");
		console.error(err);
	});
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



// Creates a proxy server to bridge a wireless and wired network
let lastProxyRemote = null;
const proxy = dgram.createSocket('udp4');

// Redirects data
proxy.on("message", (message, rinfo) => {
	lastProxyRemote = rinfo;
	proxy2.send(message, 0, message.length, 9910, atemAddr);
});

// Listens for data on the same port as atem
proxy.bind(9910);

// Creates atem connection for proxy
const proxy2 = dgram.createSocket('udp4');
proxy2.on("message", (message) => {
	proxy.send(message, 0, message.length, lastProxyRemote.port, lastProxyRemote.address);
});
