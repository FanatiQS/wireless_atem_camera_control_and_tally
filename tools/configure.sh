#!/usr/bin/env sh

if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: $0 <ip-addr> key=value..."
	exit
fi

url=$1
shift
for arg in "$@"; do
	echo "--data-urlencode \"$arg\""
done | curl $url --silent --show-error --max-time 5 --config - | sed 's/.*<p>\([^<]*\).*/\1/'
