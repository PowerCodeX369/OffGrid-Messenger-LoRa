#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
if ! command -v kotlinc >/dev/null 2>&1; then
 echo 'ERROR: kotlinc required for native Android protocol host tests' >&2
 exit 1
fi
out=$(mktemp -d);trap 'rm -rf "$out"' EXIT
kotlinc android/app/src/main/java/com/offgrid/messenger/OgmCodec.kt \
  android/app/src/main/java/com/offgrid/messenger/MediaPayload.kt \
  android/app/src/main/java/com/offgrid/messenger/LoRaNativeProtocol.kt \
  android/app/src/main/java/com/offgrid/messenger/LoRaNativeBulk.kt \
  android/tests/NativeLoRaProtocolTest.kt -include-runtime -d "$out/native-lora.jar"
java -jar "$out/native-lora.jar"
kotlinc android/app/src/main/java/com/offgrid/messenger/WeakTextRetry.kt \
  android/tests/WeakTextRetryTest.kt -include-runtime -d "$out/retry.jar"
java -jar "$out/retry.jar"
kotlinc android/app/src/main/java/com/offgrid/messenger/ReceiptStates.kt \
  android/tests/WeakReceiptTest.kt -include-runtime -d "$out/receipt.jar"
java -jar "$out/receipt.jar"
python3 android/tests/test_native_lora_ui_preservation.py
