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
auto_bin=/tmp/offgrid-lora-auto-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main \
   -Ifirmware/components/offgrid_sx1278/include \
   firmware/tests/test_lora_auto.c -o "$auto_bin"
"$auto_bin"
rm -f "$auto_bin"
recovery_bin=/tmp/offgrid-lora-recovery-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main \
    -Ifirmware/components/offgrid_sx1278/include \
    firmware/tests/test_lora_link_recovery.c -o "$recovery_bin"
"$recovery_bin"
rm -f "$recovery_bin"
profile_bin=/tmp/offgrid-lora-profile-switch-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main \
   -Ifirmware/components/offgrid_sx1278/include \
   firmware/tests/test_profile_switch.c -o "$profile_bin"
"$profile_bin"
rm -f "$profile_bin"
verify_bin=/tmp/offgrid-ultra-profile-verify-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main -Ifirmware/main/ultra \
    firmware/tests/test_ultra_profile_verify.c firmware/main/ultra/og_ultra_policy.c \
    -o "$verify_bin"
"$verify_bin"
rm -f "$verify_bin"
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
policy_bin=/tmp/offgrid-ultra-policy-$$
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/main/ultra firmware/tests/test_ultra_policy.c \
    firmware/main/ultra/og_ultra_policy.c -o "$policy_bin"
"$policy_bin"
rm -f "$policy_bin"
ultra_bin=/tmp/offgrid-ultra-arq-$$
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/components/offgrid_arq/include -Ifirmware/main/ultra \
    firmware/tests/test_ultra_arq.c \
    firmware/components/offgrid_arq/og_arq_bridge.c \
    firmware/components/offgrid_arq/og_sr.c \
    firmware/components/offgrid_arq/og_sr_usb.c \
    firmware/main/ultra/og_ultra_policy.c -o "$ultra_bin"
"$ultra_bin"
rm -f "$ultra_bin"
compact_bin=/tmp/offgrid-compact-arq-$$
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/components/offgrid_arq/include firmware/tests/test_compact_range_arq.c \
    firmware/components/offgrid_arq/og_arq_bridge.c firmware/components/offgrid_arq/og_sr.c \
    firmware/components/offgrid_arq/og_sr_usb.c -o "$compact_bin"
"$compact_bin"
rm -f "$compact_bin"
timing_bin=/tmp/offgrid-range-airtime-$$
cc -std=c11 -O2 -Wall -Wextra -Werror \
    -Ifirmware/main -Ifirmware/components/offgrid_arq/include \
    -Ifirmware/components/offgrid_sx1278/include \
    firmware/tests/test_range_ack_timing.c firmware/main/ultra/og_ultra_policy.c \
    firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c -o "$timing_bin"
"$timing_bin"
rm -f "$timing_bin"
ble_mobile_bin=/tmp/offgrid-ble-mobile-policy-$$
cc -std=c11 -Wall -Wextra -Werror -Ifirmware/main firmware/tests/test_ble_mobile_bulk.c -o "$ble_mobile_bin"
"$ble_mobile_bin"
rm -f "$ble_mobile_bin"
# v3.0.1 restores the original native Compose app. The v3.0.0 WebView
# adapter and its tests are intentionally retired; test native BLE/OGM4 instead.
sh android/tests/run_native_lora_tests.sh
node --check web/lora_bulk.js
node firmware/tests/test-lora-bulk.cjs
node --check web/lora_scheduler.js
node --check web/lora_benchmark.js
node --check web/lora_receipts.js
node --check web/app.js
node --check web/protocol.js
node --check web/lora_media.js
node --check web/lora_rx_guard.js
node firmware/tests/test-lora-rx-guard.cjs
node firmware/tests/test-lora-security-fuzz.cjs
node firmware/tests/test-lora-media.cjs
node firmware/tests/test-lora-voice-40k.cjs
node firmware/tests/test-protocol.cjs
node firmware/tests/test-adaptive.cjs
node firmware/tests/test-diagnostic-protocol.cjs
node firmware/tests/test-lora-delivery.cjs
node firmware/tests/test-weak-text-retry.cjs
node firmware/tests/test-web-rc3052-diagnostics.cjs
node firmware/tests/test-rc4-text-submission.cjs
node firmware/tests/test-lora-scheduler.cjs
node firmware/tests/test-lora-receipts.cjs
python3 tests/test_rc2_send_path.py
python3 tests/test_boundaries.py
python3 tests/test_ultra_end_to_end_contract.py
python3 tests/test_weak_link_release.py
python3 tests/test_v15_profile.py
python3 tests/test_v161_rc2_regression.py
python3 tests/test_v17_adaptive.py
if command -v clang >/dev/null 2>&1; then sh firmware/tests/run-syntax-checks.sh; fi

python3 tests/test_v18_media.py
python3 tests/test_v18_ui_source.py

python3 tests/test_v306_recovery_contract.py
python3 tests/test_v306_rc2_compact_ack_contract.py
