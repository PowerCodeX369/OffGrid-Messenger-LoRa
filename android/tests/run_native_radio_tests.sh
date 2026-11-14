#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
out=$(mktemp -d);trap 'rm -rf "$out"' EXIT
kotlinc android/app/src/main/java/com/offgrid/messenger/RadioLink.kt android/tests/RadioLinkTest.kt -include-runtime -d "$out/radio.jar"
java -jar "$out/radio.jar"
python3 android/tests/test_lora_native_media_handover.py
python3 android/tests/test_native_lora_ui_preservation.py
