#!/usr/bin/env bash

if [[ "$1" == "" || "$2" == "" ]]; then
	echo "Usage: $0 <ip-addr> key=value..."
	exit
fi

args=("${@:2}")
curl --max-time 5 $1 ${args[@]/#/--data-urlencode }
printf "\n"
