#!/usr/bin/env sh

set -e
set -o pipefail



conf_get() {
	local html=$(curl -s $1)
	if [[ "$html" == "" ]]; then
		exit 1
	fi

	conf_ssid=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=ssid\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value="\(.*\)[^"]*".*/\1/p')
	echo "	ssid=$conf_ssid"
	conf_psk=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=psk\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value="\(.*\)[^"]*".*/\1/p')
	echo "	psk=$conf_psk"
	conf_name=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=name\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value="\(.*\)[^"]*".*/\1/p')
	echo "	name=$conf_name"
	conf_dhcp=$(echo "$html" \
		| sed -n 's/.*<input \([^>]*\)name=dhcp\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*\(checked\).*/\1/p')
	echo "	dhcp=$conf_dhcp"
	conf_localip=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=localip\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value=\(.*\).*/\1/p')
	echo "	localip=$conf_localip"
	conf_netmask=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=netmask\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value=\(.*\).*/\1/p')
	echo "	netmask=$conf_netmask"
	conf_gateway=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=gateway\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value=\(.*\).*/\1/p')
	echo "	gateway=$conf_gateway"
	conf_atem=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=atem\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value=\(.*\).*/\1/p')
	echo "	atem=$conf_atem"
	conf_dest=$(echo "$html" \
		| sed -n 's/.*<input \(.*\)name=dest\([^>]*\)>.*/\1\2/p' \
		| sed -n 's/.*value=\(.*\).*/\1/p')
	echo "	dest=$conf_dest"
}

conf_update() {
	ssid=$conf_ssid
	psk=$conf_psk
	name=$conf_name
	dhcp=$conf_dhcp
	localip=$conf_localip
	netmask=$conf_netmask
	gateway=$conf_gateway
	atem=$conf_atem
	dest=$conf_dest
}

conf_validate() {
	conf_get $1

	if [[ $ssid != $conf_ssid ]]; then
		echo "SSID did not match: '$ssid', '$conf_ssid'"
		exit 1
	fi
	if [[ $psk != $conf_psk ]]; then
		echo "PSK did not match: '$psk', '$conf_psk'"
		exit 1
	fi
	if [[ $name != $conf_name ]]; then
		echo "Device name did not match: '$name', '$conf_name'"
		exit 1
	fi
	if [[ $dhcp != $conf_dhcp ]]; then
		echo "DHCP did not match: '$dhcp', '$conf_dhcp'"
		exit 1
	fi
	if [[ $localip != $conf_localip ]]; then
		echo "Local ip did not match: '$localip', '$conf_localip'"
		exit 1
	fi
	if [[ $netmask != $conf_netmask ]]; then
		echo "Netmask did not match: '$netmask', '$conf_netmask'"
		exit 1
	fi
	if [[ $gateway != $conf_gateway ]]; then
		echo "Gateway did not match: '$gateway', '$conf_gateway'"
		exit 1
	fi
	if [[ $atem != $conf_atem ]]; then
		echo "ATEM address did not match: '$atem', '$conf_atem'"
		exit 1
	fi
	if [[ $dest != $conf_dest ]]; then
		echo "Dest did not match: '$dest', '$conf_dest'"
		exit 1
	fi
}



if [[ "$DEVICE_ADDR" == "" ]]; then
	echo "Missing environment variable DEVICE_ADDR"
	exit 1
fi

trap '[ "$?" -eq 0 ] || echo Failed' EXIT

inet=$(ifconfig | grep 'inet ' | grep -v '127.0.0.1')



echo "Get host address"
host_addr=$(echo $inet | cut -d ' ' -f2)
echo "	$host_addr"

echo "Get free static ip address"
while [[ 1 ]]; do
	input_localip=192.168.1.$((($RANDOM % 254) + 1))
	ping -t 1 -c 1 $input_localip > /dev/null || break
done
echo "	$input_localip"

echo "Get network netmask"
input_netmask=$(echo $inet | cut -d ' ' -f4)
if [[ $(echo $input_netmask | grep '0x.*') != "" ]]; then
	hex=$(echo $input_netmask | sed -n 's/0x\(.*\)/\1/p')
	h1=${hex%????}
	h2=${hex#????}
	input_netmask=$(printf "%d.%d.%d.%d\n" 0x${h1%??} 0x${h1#??} 0x${h2%??} 0x${h2#??})
fi
echo "	$input_netmask"

echo "Get network gateway address"
input_gateway=$(netstat -rn | grep '^\(default\|0\.0\.0\.0\)' | sed -n 's/^[^ ]* *\([^ ]*\).*/\1/p' | head -1)
echo "	$input_gateway"



if [[ $host_addr == "" || $input_localip == "" || $input_netmask == "" || $input_gateway == "" ]]; then
	exit 1
fi

echo "Connect to device at DHCP ip"
conf_get $DEVICE_ADDR
conf_update

echo "Configure device to static ip"
dhcp=""
localip=$input_localip
netmask=$input_netmask
gateway=$input_gateway
dest=254
curl -s $DEVICE_ADDR \
	--data-urlencode dhcp=0 \
	--data-urlencode localip=$localip \
	--data-urlencode netmask=$netmask \
	--data-urlencode gateway=$gateway \
	--data-urlencode dest=$dest \
	> /dev/null

echo "Validate device configuration at static ip"
conf_validate $localip

echo "Configure device back to DHCP"
dhcp="checked"
netmask=255.255.255.255
gateway=0.0.0.0
dest=1
atem=$host_addr
curl -s $localip \
	--data-urlencode dhcp=1 \
	--data-urlencode netmask=$netmask \
	--data-urlencode gateway=$gateway \
	--data-urlencode dest=$dest \
	--data-urlencode atem=$atem \
	> /dev/null

echo "Validate device configuration at DHCP ip"
conf_validate $DEVICE_ADDR

echo "Success"
