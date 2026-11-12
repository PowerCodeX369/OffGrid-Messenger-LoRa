#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main firmware/tests/test_lora_wire.c -o /tmp/offgrid-lora-wire-test-$$
/tmp/offgrid-lora-wire-test-$$
rm -f /tmp/offgrid-lora-wire-test-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/components/offgrid_sx1278/include \
    firmware/tests/test_lora_profile.c \
    firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c -o /tmp/offgrid-lora-profile-$$
/tmp/offgrid-lora-profile-$$
rm -f /tmp/offgrid-lora-profile-$$
mode_bin=/tmp/offgrid-lora-modes-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main \
    -Ifirmware/components/offgrid_sx1278/include \
    firmware/tests/test_lora_modes.c \
    firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c -o "$mode_bin"
"$mode_bin"
rm -f "$mode_bin"
profile_bin=/tmp/offgrid-lora-profile-switch-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main \
   -Ifirmware/components/offgrid_sx1278/include \
   firmware/tests/test_profile_switch.c -o "$profile_bin"
"$profile_bin"
rm -f "$profile_bin"
fifo_bin=/tmp/offgrid-lora-fifo-$$
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-variable \
    -Ifirmware/tests/host_stubs -Ifirmware/components/offgrid_sx1278/include \
    firmware/tests/test_sx1278_fifo_burst.c \
    firmware/components/offgrid_sx1278/offgrid_sx1278.c \
    firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c -o "$fifo_bin"
"$fifo_bin"
rm -f "$fifo_bin"
schedule_bin=/tmp/offgrid-lora-schedule-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main \
    firmware/tests/test_lora_schedule.c -o "$schedule_bin"
"$schedule_bin"
rm -f "$schedule_bin"
bridge_bin=/tmp/offgrid-lora-bridge-$$
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/components/offgrid_arq/include \
    firmware/tests/test_arq_bridge.c \
    firmware/components/offgrid_arq/og_arq_bridge.c \
    firmware/components/offgrid_arq/og_sr.c \
    firmware/components/offgrid_arq/og_sr_usb.c -o "$bridge_bin"
"$bridge_bin"
rm -f "$bridge_bin"
node --check web/lora_bulk.js
node firmware/tests/test-lora-bulk.cjs
node --check web/lora_scheduler.js
node --check web/lora_benchmark.js
node --check web/lora_receipts.js
node --check web/app.js
node --check web/protocol.js
node firmware/tests/test-protocol.cjs
node firmware/tests/test-adaptive.cjs
node firmware/tests/test-diagnostic-protocol.cjs
node firmware/tests/test-lora-delivery.cjs
node firmware/tests/test-lora-scheduler.cjs
node firmware/tests/test-lora-receipts.cjs
python3 tests/test_rc2_send_path.py
python3 tests/test_boundaries.py
python3 tests/test_v15_profile.py
python3 tests/test_v161_rc2_regression.py
if command -v clang >/dev/null 2>&1; then sh firmware/tests/run-syntax-checks.sh; fi
