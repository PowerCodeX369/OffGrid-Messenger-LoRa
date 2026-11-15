# v3.0.5-rc.3 — Verified source-level ARQ capability rejection fix

## Diagnosis from actual two-board diagnostics

Both radios advertise `RF_POLICY=ULTRA12_FEC8`, show authenticated peer discovery at about -49/-52 dBm, and have the same safely redacted pairing-key fingerprint. But both report `PEER_ARQ=0`, `ARQ_TX=0`, and every queued text returns `BULKFAILED|...|PEER_REQUIRES_V16_ARQ`. This is **not** weak range, USB bulk staging, missing BLE, or a browser encryption issue.

The firmware main task sends a signed `OG_TYPE_ARQ_CAPS` OGM2 control frame with `data[0]=3` to indicate the v3.0.5 Ultra CR4/8 protocol. The radio receiver checks signed frames with `og_wire_valid()` in `firmware/main/lora_wire.h`, but that validator allowed **only** capability values `1` and `2`. Therefore both radios silently discarded each other's capability 3 control frames, never set `peer_arq_seen=true`, and the 12-second bulk hold correctly refused to send any ARQ packets. This explains `ARQ_TX=0` and `PEER_ARQ=0` on **both** boards. It also explains why the web-only rc.2 diagnostics exposed the issue but did not fix it.

## Exact code fix

- `firmware/main/lora_wire.h`: authenticated ARQ_CAPS accepts **1, 2 or 3**. It still rejects unsupported `0`, `4`, `255`, malformed lengths and other malformed frames. Radio HMAC authentication and replay handling remain untouched.
- `firmware/tests/test_lora_wire.c`: validate all three supported capabilities and reject invalid/malformed values, preventing the exact regression.
- No on-air packet layout changes, cryptographic changes, radio profile changes, ARQ engine rewrites, Android changes, web changes, or NVS changes. The rc.2 web diagnostic build can be kept; it is compatible with the corrected firmware.

## Installation and expected result

**Build and flash this corrected firmware on BOTH ESP32-C3 radios**, using ESP-IDF 6.1. Preserve NVS and keys (ordinary `idf.py flash`, not `erase-flash`). The `FW=1.8.0-rc.4` wire-compatible identifier and `RF_POLICY=ULTRA12_FEC8` are intentionally unchanged. Distinguish this release through the source tag `v3.0.5-rc.3-arq-caps-fix`, not by the unchanged runtime `FW` label.

1. At close range, power both nodes, wait for the peer, and use the existing rc.2 diagnostic web client on each board.
2. Run `STATUS` and look for `PEER_ARQ=1` on **both** boards. If still zero, check that both boards actually received the new firmware and check authenticated RF control diagnostics.
3. Send one short encrypted text A→B, then B→A. Expected: `BULKQUEUED`, `BULKSENT`, `BULKCONFIRMED`, valid application delivery receipt and `ARQ_TX>0` / `ARQ_ACKRX>0`. Never interpret `BULKQUEUED` or a single tick as delivered.
4. Test the unchanged Fast, Balanced, SF9 Range, Ultra SF12 text-only, Auto synchronization, photo/voice outside Ultra, and message recovery.
5. Keep v3.0.4 as a rollback. Do not mix v3.0.5 capability-3 firmware with pre-v3.0.5 Ultra-CR4/5 builds without deliberate compatibility testing.

## Verification and limitations

Host C regressions, including malformed v1/v2/v3 capability control, and the complete existing Kotlin/Node/Python/C suite pass. An ESP-IDF target build, physical two-radio confirmation, and Android APK compile **have not run here**. This is a source-level fix to a directly observed failing code path, not yet a hardware-qualified stable release.
