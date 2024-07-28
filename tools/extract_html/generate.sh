#!/usr/bin/env bash

# Aborts on errors
set -e

# Gets state as first argument
state=$1
shift

# Parses command line arguments
while [[ $# -gt 0 ]]; do
	i=$1
	case "$i" in
		--http)
			http=1
			;;
		--err_code=*)
			err_code="${i#*=}"
			;;
		--err_body=*)
			err_body="${i#*=}"
			;;
		--wifi_status=*)
			wifi_status="${i#*=}"
			;;
		--atem_status=*)
			atem_status="${i#*=}"
			;;
		--uptime=*)
			uptime="${i#*=}"
			;;
		--current_addr=*)
			current_addr="${i#*=}"
			;;
		--name=*)
			name="${i#*=}"
			;;
		--ssid=*)
			ssid="${i#*=}"
			;;
		--psk=*)
			psk="${i#*=}"
			;;
		--dest=*)
			dest="${i#*=}"
			;;
		--atem_addr=*)
			atem_addr="${i#*=}"
			;;
		--dhcp=*)
			dhcp="${i#*=}"
			;;
		--localip=*)
			localip="${i#*=}"
			;;
		--netmask=*)
			netmask="${i#*=}"
			;;
		--gateway=*)
			gateway="${i#*=}"
			;;
		-h|--help)
			echo "Usage: $0 state [...opts]"
			echo "	Read source code to see what arguments are available"
			exit 0
			;;
		*)
			echo "Invalid argument: $1"
			exit 1
	esac
	shift
done

# Sets workding directory to project root
cd "$(dirname $0)/../.."

# Generates HTTP response
output=$(printf '%b ' $({
	# Extracts only addr/value/str
	echo "#define http_write_value_addr(http, ptr, addr) addr"
	echo "#define http_write_value_uint8(http, ptr, value) value"
	echo "#define http_write_value_string(http, ptr, str, size) str"
	
	echo "#define STRIP_ME(...)" # Defines stripping macro
	echo "STRIP_ME(" # Strips all data up to first macro function call to keep content for
	{
		echo "#include \"./firmware/version.h\" // FIRMWARE_VERSION_STRING"

		# Defines macros to replace configurations
		echo "#define http_write_wifi(http) \"${wifi_status-"-50 dBm"}\""
		echo "#define atem_state \"${atem_status-"Connected"}\""
		echo "#define http_write_uptime(http) \"${uptime-"0h 13m 37s"}\""
		echo "#define http_write_local_addr(http) \"${current_addr-"192.168.1.69"}\""
		echo "#define CACHE_SSID , \"${ssid-"myWiFi"}\""
		echo "#define CACHE_PSK , \"${psk-"password123"}\""
		echo "#define CACHE_NAME , \"${name-"waccat0"}\""
		echo "#define dest , \"${dest-4}\""
		echo "#define atem_addr , \"${atem_addr-"192.168.1.240"}\""
		echo "#define localip , \"${localip-"192.168.1.69"}\""
		echo "#define netmask , \"${netmask-"255.255.255.0"}\""
		echo "#define gateway , \"${gateway-"192.168.1.1"}\""
		echo "#define err_code \"${err_code-"400 Bad Request"}\""
		echo "#define err_body \"${err_body-$err_code}\""

		# HTTP_RESPONSE_CASE macro functions should close stripper, dumps its data and starts new stripper
		echo "#define HTTP_RESPONSE_CASE_STR(http, str) ) str STRIP_ME("
		echo "#define HTTP_RESPONSE_CASE(fn) ) fn STRIP_ME("

		# Strips out C conditional for DHCP if it is not enabled
		[ "$dhcp" == 0 ] && echo "#define if #define DELETE_THIS_ROW"

		sed -n "/case HTTP_RESPONSE_STATE_$state:/,/break;/p" ./firmware/http_respond.c | sed 's/http->//'
	} | gcc -E -P -xc -
	echo ")" # Closes last stripper
} | gcc -E -P -xc - | sed 's/" "//g') | sed 's/\\"/"/g' | sed 's/" *$//' | sed 's/^"//')

# Outputs HTTP or HTML
if [[ "$http" == 1 ]]; then 
	printf '%s' "$output"
else
	printf '%s' "${output#*$'\r\n\r\n'}"
fi
