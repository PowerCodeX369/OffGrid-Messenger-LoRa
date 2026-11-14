#!/bin/sh
set -eu
cd "$(dirname "$0")/../../.."
sh android/tests/run_native_lora_tests.sh
