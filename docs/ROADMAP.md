# OffGrid LoRa roadmap (updated for v1.10.0-rc.1)

- **v1.6.1-rc.2 — user-confirmed stable baseline:** reliable text, 224-byte ARQ data chunks, batched authenticated ACKs, one-click synchronized Fast/Balanced/Range manual modes.
- **v1.7.0-rc.1 — user-confirmed locally functional:** opt-in two-way RSSI/SNR automatic Fast-first transitions, with safe fallback/rollback and manual override. Longer range not yet field-tested.
- **v1.8.0-rc.3 — user-confirmed working media:** encrypted photos ≤48 KiB, recorded Opus voice notes ≤40 KiB, matching ESP-NOW web UI, existing verified text receipts and ARQ.
- **v1.8.0-rc.4 — verified user media Fast handover baseline:** temporary authenticated Fast mode during photo/voice transfers, then signed return to previous manual/Auto mode. Not yet hardware-tested; rc.3 remains fallback.
- **v1.8.0-rc.5 — web-only media efficiency candidate:** 7,040-byte segments, 8-line USB upload batches, detailed per-photo timing; no new firmware flash.
- **v1.8 hardware qualification (still needed):** real radio media testing, 1-minute sustained transfers, frame loss/duplicate injection, photo rendering, Opus microphone/playback, regulatory/power-supply acceptance checks, long-range RF quality thresholds and complete ESP-IDF build.
- **v1.9 (deferred; not included in v1.10):** durable, reconnect-safe media checkpoints in encrypted IndexedDB, receiving progress and cancellation across reboot, measured bitrate/latency, optional higher-budget long voice notes if the user accepts the physics of clear audio.
- **v1.10.0-rc.1 (current):** bounded browser-side in-flight duplicate prevention, GCM/replay/error fuzz testing and Chromium regressions. **Still outstanding for a stable v1.10:** persistent RF control anti-replay across board resets, key-rotation crash-safety, independent security review, full ESP-IDF builds and physical soak/range tests.
- **Android native LoRa v3.0.1-rc.1 (source candidate; hardware qualification pending):** restore the original ESP-NOW Compose UI and BLE pairing screen exactly, remove v3.0.0 WebView launcher, and adapt native AES-GCM, LoRa bulk photo/voice, receipts, priority and manual/Auto controls under the old UI. Original web messenger untouched. Both radios require the included BLE-bulk companion firmware; target Android/ESP-IDF builds and physical BLE/media tests still outstanding. See `docs/ANDROID_NATIVE_LORA_ACCEPTANCE.md`.
- **v2.1 (last, AFTER Android):** battery/low-power modes, sleep/wake power measurements, regulatory duty-cycle handling and safe +18 dBm supply validation.

Every milestone must pass host tests and a clean ESP-IDF build, followed by physical two-node acceptance, before being called a hardware-stable release. No real-world distance is inferred from RSSI alone.
