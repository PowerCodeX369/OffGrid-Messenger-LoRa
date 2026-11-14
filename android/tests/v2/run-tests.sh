#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
if ! command -v kotlinc >/dev/null 2>&1; then
 echo 'SKIP: Kotlin compiler not installed; run these tests in Android Studio CI'
 exit 0
fi
out=$(mktemp -d);trap 'rm -rf "$out"' EXIT
kotlinc android/app/src/main/java/com/offgrid/messenger/LoRaMobileCodec.kt android/tests/v2/MobileCodecTest.kt -include-runtime -d "$out/android-codec.jar"
java -jar "$out/android-codec.jar"
