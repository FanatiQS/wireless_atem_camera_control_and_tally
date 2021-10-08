/**
 * This script acts as a middle man between an ATEM switcher and client.
 * It is designed to drop packets to test resends
 *
 * CLI arguments:
 *     dropChance: Percentage of how many packets should be dropped
 *     hostAddr: The ip address of the ATEM switcher
 * example: node index.js 50 192.168.0.128
 *    this will drop 50% of the packets to 192.168.1.128
 */

const dgram = require("dgram");

const socket = dgram.createSocket("udp4");

let client = {};
let server = { address: process.argv[3], port: 9910 };

const dropChance = process.argv[2] / 100;

socket.on("message", (msg, rinfo) => {
	if (Math.random() > dropChance) {
		console.log(true);
		return;
	}
	console.log(false);
	if (rinfo.port !== 9910 && client.port !== rinfo.port) client = rinfo;
	const info = (rinfo.port === client.port) ? server : client;
	socket.send(msg, info.port, info.address);
});

socket.bind(9910);
