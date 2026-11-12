# Contributing to OffGrid Messenger LoRa

The `main` Git branch is the authoritative integrated repository, with historical annotated tags from v1.0.0 onward. This is a **separate LoRa project**, not the working ESP-NOW repository. Use focused commits, meaningful commit subjects and a changelog entry. Git attribution should reflect actual human contributors. Maintainer: PowerCodeX.

Do not add Wi-Fi/ESP-NOW startup calls, change the frozen `android/` tree ahead of the final universal-app release, bypass AES-256-GCM, disable mandatory HMAC or substitute a radio ACK for a verified delivered/read receipt. Keep a bounded ciphertext buffer and a single SX1278 SPI owner; every profile change must remain manual and peer authenticated. **+22 dBm must remain unavailable on SX1278.** Tests cannot replace hardware validation.

Run `bash firmware/tests/run-host-tests.sh` before proposing a change. Firmware changes additionally need a full ESP-IDF 6.1 `idf.py build` on an ESP32-C3 environment; browser changes need the included Chromium UI tests when possible. Provide structured diagnostics including profile, retries, ACK counters and exact test payload size. Mark what was actually tested in the PR: host simulator, real board build, two-board RF test, BLE, or range test.

For pull requests explain the protocol compatibility/rollback plan, especially for changes to `og_sr.h`, `og_arq_bridge.*`, `main.c` and `web/lora_bulk.js`. Attach no real pairing keys (`KEYDATA`), private NVS dumps or exact device identifiers. Do not publish a performance claim without reproducible real hardware measurements. See [v1.6 acceptance](docs/V1_6_0_ARQ_INTEGRATION.md).
