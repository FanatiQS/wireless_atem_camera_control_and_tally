#!/usr/bin/env sh

# Aborts on errors
set -e

# Gets state as first argument
state=$1
shift

# Parses command line arguments
while [ $# -gt 0 ]; do
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
		--no_animation=*)
			no_animation="${i#*=}"
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
output=$(printf '%b' "$({
	# Extracts only addr/value/str
	echo "#define http_write_value_addr(http, ptr, addr) addr"
	echo "#define http_write_value_uint8(http, ptr, value) value"
	echo "#define http_write_value_string(http, ptr, str, size) str"
	echo "#define http_write_value_bool(http, ptr, checked) checked"

	{
		echo "#include \"./firmware/version.h\" // FIRMWARE_VERSION_STRING"

		# Defines macros to replace configurations
		echo "#define http_write_wifi(http) \"${wifi_status-"-50 dBm"}\""
		echo "#define atem_state \"${atem_status-"Connected"}\""
		echo "#define http_write_uptime(http) \"${uptime-"0h 13m 37s"}\""
		echo "#define http_write_local_addr(http) \"${current_addr-"192.168.1.69"}\""
		echo "#define ssid , \"${ssid-"myWiFi"}\""
		echo "#define psk , \"${psk-"password123"}\""
		echo "#define name , \"${name-"waccat0"}\""
		echo "#define dest , \"${dest-4}\""
		echo "#define atem_addr , \"${atem_addr-"192.168.1.240"}\""
		if [ "$dhcp" = 0 ]; then
			echo "#define CONF_FLAG_DHCP ,"
		else
			echo "#define CONF_FLAG_DHCP , \" checked\""
		fi
		echo "#define localip , \"${localip-"192.168.1.69"}\""
		echo "#define netmask , \"${netmask-"255.255.255.0"}\""
		echo "#define gateway , \"${gateway-"192.168.1.1"}\""
		echo "#define err_code \"${err_code-"400 Bad Request"}\""
		echo "#define err_body \"${err_body-$err_code}\""

		# Defines compile time configuration options
		echo "#define NO_HTML_ENTRY_ANIMATION ${no_animation-"0"}"

		# Defines macros to strip out implementations
		echo "#define HTTP_RESPONSE_CALL(fn) fn"
		echo "#define HTTP_RESPONSE_START(label)"
		echo "#define HTTP_RESPONSE_END"
		echo "#define HTTP_RESPONSE_WRITE"

		sed -n "/(HTTP_RESPONSE_STATE_$state)/,/HTTP_RESPONSE_END/p" ./firmware/http_respond.c
	} | cc -E -P -xc - | sed 's/http->//'
} | cc -E -P -xc - | sed -n 's/ *"\(.*\)" */\1/pg' | sed 's/" "//g' | sed 's/\\"/"/g' | tr -d '\n')")

# Outputs HTTP or HTML without trailing blank line
if [ -n "$http" ]; then
	printf '%s' "$output"
else
	carriage_return=$(printf "\r")
	printf '%s' "$output" | sed "1,/^$carriage_return$/d" | tr -d '\n'
fi
