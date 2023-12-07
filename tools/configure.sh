#!/usr/bin/env bash

if [[ "$1" == "" || "$2" == "" ]]; then
	echo "Usage: $0 <ip-addr> key=value..."
	exit
fi

args=("${@:2}")
curl -X POST $1 ${args[@]/#/--data-urlencode } --max-time 5
printf "\n"
