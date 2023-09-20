#!/bin/sh

# Default values
outfile=/dev/stdout
iterations=1000

# Getting address from first argument
addr="$1"
shift

# Command line parser
while [[ "$1" ]]
do
	case "$1" in
		-o | --out)
			outfile=$2
			shift 2
			;;
		-i | --iterations)
			iterations=$2
			shift 2
			;;
		-h | --help)
			echo "Usage: $0 [options...] <url>"
			echo "	-o, --out FILE					Output file to put HTML into"
			echo "	-i, --iterations ITERATIONS		Number of requests to make (default: 1000)"
			echo "	-h, --help						Displays the help text"
			exit
			;;
		*)
			echo "Invalid argument $1"
			exit
			;;
	esac
done

# Validates address
if ! curl $addr --connect-timeout 1 --output /dev/null -s
then
	echo "Unable to connect to device"
	exit
fi

echo "Found device at $addr"
echo "Iterating $iterations times"
echo "Outputting to $outfile"

# Writes HTML up to array string content to file
cat > $outfile << EOF
<head>
	<script src='https://cdn.plot.ly/plotly-2.26.0.min.js'></script>
</head>

<body>
	<div id='plotDiv'></div>
	<script>
		const arr = \`
EOF

# Writes array string content to file
for i in $(seq 1 $iterations)
do
 	echo $( TIMEFORMAT=%R; { time curl -s --output /dev/null $addr; } 2>&1 ) >> $outfile
done

# Writes HTML after array string content to file
cat >> $outfile << EOF
\`.split('\n').slice(1, -1).map((value) => value * 1000).sort((a, b) => a - b);
		const occurrence = [];
		for (const index of arr) {
			occurrence[index] ||= 0;
			occurrence[index]++;
		}

		function getPercentile(percentile) {
			return arr[Math.round((arr.length - 1) * percentile / 100)];
		}

		Plotly.newPlot("plotDiv", [{ x: occurrence.keys(), y: occurrence, type: "bar" }]);

		const divs = {
			avg: arr.reduce((acc, value) => acc + value, 0) / arr.length,
			median: arr[Math.round(arr.length / 2)],
			highest: arr[arr.length - 1],
			lowest: arr[0],
			"99th": getPercentile(99),
			"95th": getPercentile(95),
			"90th": getPercentile(90),
			"80th": getPercentile(80),
			"70th": getPercentile(70)
		};
		document.body.innerHTML += "<table>" + Object.entries(divs).map(([ key, value ]) => {
			return "<tr><td>" + key + ":</td><td>" + value + "</td></tr>";
		}).join('') + "</tabel>";
</script></body>
EOF
