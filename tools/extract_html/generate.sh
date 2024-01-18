#!/usr/bin/env sh

set -e

echo "#define UPTIME      \"${UPTIME:-"0h 13m 37s"}\""
echo "#define WIFI_STATUS \"${WIFI_STATUS:-"-50 dBm"}\""
echo "#define ATEM_STATUS \"${ATEM_STATUS:-"Connected"}\""
echo "#define SSID        \"${SSID:-"myWiFi"}\""
echo "#define PSK         \"${PSK:-"password123"}\""
echo "#define DEVICE_NAME \"${DEVICE_NAME:-"waccat0"}\""
echo "#define DEST        ${DEST:-4}"
echo "#define ATEM_ADDR   \"${ATEM_ADDR:-"192.168.1.240"}\""
echo "#define DHCP        ${DHCP:-1}"
echo "#define LOCAL_ADDR  \"${LOCAL_ADDR:-"192.168.1.69"}\""
echo "#define NETMASK     \"${NETMASK:-"255.255.255.0"}\""
echo "#define GATEWAY     \"${GATEWAY:-"192.168.1.1"}\""

echo "#include <$PWD/main.c>"

echo "int main(int argc, char** argv) {"
echo "	switch (atoi(argv[1])) {"
sed -n "/case HTTP_RESPONSE_STATE_ROOT:/,/break;/p" ../../firmware/http_respond.c
sed -n "/case HTTP_RESPONSE_STATE_POST_ROOT:/,/break;/p" ../../firmware/http_respond.c
echo "	}"
echo "}"
