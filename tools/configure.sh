#!/usr/bin/env bash

if [[ -z "$1" ]] || [[ -z "$2" ]]; then
	echo "Usage: $0 <ip-addr> key=value..."
	exit
fi

for arg in "${@:2}"; do
	args+=("--data-urlencode" "$arg")
done
res=$(curl --silent --show-error --max-time 5 $1 "${args[@]}")

if [[ "$res" == "<!DOCTYPE"* ]]; then
	echo "Successfully commited configuration"
elif [[ "$res" != "" ]]; then
	echo $res
fi
