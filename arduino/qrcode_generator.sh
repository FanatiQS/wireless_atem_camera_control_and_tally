#! /usr/bin/env bash
if [ $# -lt 2 ]; then
    echo "Usage $0 <SSID> <PSK> <optional output path>"
    exit 1;
fi
CHROME="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
URL="https://fanatiqs.github.io/wireless_atem_camera_control/arduino/qrcode_generator.html"
"$CHROME" --headless --disable-gpu --no-margins --print-to-pdf=$3 $URL?ssid=$1\&psk=$2
