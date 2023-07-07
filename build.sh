#!/bin/bash
board=${1:-esp32:esp32:esp32}
# esp8266:esp8266:nodemcuv2
echo "Building for $board"
../arduino-cli.exe compile \
    --fqbn ${board} \
    --build-property build.partitions=min_spiffs \
    --build-property upload.maximum_size=1966080 \
    -e
