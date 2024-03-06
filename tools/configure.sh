#!/usr/bin/env bash

if [[ "$1" == "" || "$2" == "" ]]; then
	echo "Usage: $0 <ip-addr> key=value..."
	exit
fi

args=("${@:2}")
res=$(curl --silent --max-time 5 $1 ${args[@]/#/--data-urlencode })

if [[ "$res" != "<!DOCTYPE"* ]]; then
	echo $res
else
	echo "Successfully commited configuration"
fi
