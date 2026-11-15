# OffGrid Messenger v3.0.6 — Public source preparation

**Release classification:** feature-complete **field beta**, source only. No commercial/security/RF certification or official binaries are included.

## Included functionality

- Two-node 433 MHz LoRa messenger on ESP32-C3/SX1278 with native Android BLE and desktop Web Serial clients.
- AES-256-GCM application encrypted text and supported non-Ultra media, application delivery/read receipts, retry/reconnection safeguards.
- Fast, Balanced, Max and Ultra profiles; authenticated automatic RSSI bands and scan-based lost-peer discovery.
- Compact authenticated Max/Ultra ACK and compatible radio capability controls.

## What is new in `v3.0.6-public.1`

- Current README, hardware, firmware, Android and web setup guides; source architecture and tag/version index.
- Historical entry-point documentation preserved in `docs/history/` without rewriting older Git tags.
- Test, security, troubleshooting, known limitations, contributing and publication checklists.
- GitHub issue forms, pull-request template, code of conduct, support policy and editor defaults.
- Host and separate ESP-IDF / Android build CI workflows; source-manifest consistency checking.

**No firmware, Android Kotlin, web runtime code, encryption, NVS layout or RF wire format was altered by this publication-preparation commit.** Existing `v3.0.6` and `v3.0.6-rc.3-rssi-auto` tags still refer to the original code baseline.

## Verification and outstanding work

- The supplied host regression suite and native Android radio tests pass on the prepared tree; documentation links/YAML and checksum/ Git object integrity were checked locally.
- Actual ESP-IDF v6.1 target compilation, Android SDK 36 APK build, GitHub-hosted CI runs and independent two-board/endurance/security/regulatory qualification remain for the repository owner and contributors to execute.
- The maintainer reports a working automatic-mode test and roughly 200–250 m Max performance in one urban environment; that is not a controlled range guarantee.

## Install / rollback

Read [README](../README.md), [hardware](HARDWARE_SETUP.md), [firmware](../firmware/README.md), [Android](../android/README.md) or [web](../web/README.md). Update both nodes and clients together, preserve pairing keys/NVS and retain prior Git tags. `v3.0.6-public.1` is a documentation-only revision; there is no need to reflash already-working v3.0.6 radios *merely* for these documentation changes.
