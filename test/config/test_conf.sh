#!/usr/bin/env bash

set -e
set -o pipefail

trap '[ "$?" -eq 0 ] || echo Failed' EXIT

inet=$(ifconfig | grep 'inet ' | grep -v '127.0.0.1')

echo "Get host address"
export CONFIG_TEST_ATEM=$(echo $inet | cut -d ' ' -f2)
echo "	$CONFIG_TEST_ATEM"

echo "Get free static ip address"
while [[ 1 ]]; do
	export CONFIG_TEST_LOCALIP=192.168.1.$((($RANDOM % 254) + 1))
	ping -t 1 -c 1 $CONFIG_TEST_LOCALIP > /dev/null || break
done
echo "	$CONFIG_TEST_LOCALIP"

echo "Get network netmask"
export CONFIG_TEST_NETMASK=$(echo $inet | cut -d ' ' -f4)
if [[ $(echo $CONFIG_TEST_NETMASK | grep '0x.*') != "" ]]; then
	hex=$(echo $CONFIG_TEST_NETMASK | sed -n 's/0x\(.*\)/\1/p')
	h1=${hex%????}
	h2=${hex#????}
	CONFIG_TEST_NETMASK=$(printf "%d.%d.%d.%d\n" 0x${h1%??} 0x${h1#??} 0x${h2%??} 0x${h2#??})
fi
echo "	$CONFIG_TEST_NETMASK"

echo "Get network gateway address"
export CONFIG_TEST_GATEWAY=$(netstat -rn | grep '^\(default\|0\.0\.0\.0\)' | sed -n 's/^[^ ]* *\([^ ]*\).*/\1/p' | head -1)
echo "	$CONFIG_TEST_GATEWAY"

cd $(dirname "$0")/..
make http_config
