#!/usr/bin/env bash

if [[ -z "$1" ]] || [[ -z "$2" ]]; then
	echo "Usage: $0 <ip-addr> key=value..."
	exit
fi

for arg in "${@:2}"; do
	args+=("--data-urlencode" "$arg")
done
curl --silent --show-error --max-time 5 $1 "${args[@]}" | sed 's/.*<p>\([^<]*\).*/\1/'
