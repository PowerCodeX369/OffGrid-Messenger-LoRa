# OffGrid LoRa roadmap (updated for v1.8)

- **v1.6.1-rc.2 — user-confirmed stable baseline:** reliable text, 224-byte ARQ data chunks, batched authenticated ACKs, one-click synchronized Fast/Balanced/Range manual modes.
- **v1.7.0-rc.1 — user-confirmed locally functional:** opt-in two-way RSSI/SNR automatic Fast-first transitions, with safe fallback/rollback and manual override. Longer range not yet field-tested.
- **v1.8.0-rc.1 — this release candidate:** compressed and fully encrypted photos at ≤48 KiB, short Opus voice notes ≤20 KiB, media segmentation/retry, and media-aware AUTO RF hints. A two-minute clear voice note cannot fit the 20 KiB cap with Opus.
- **v1.8 hardware qualification (next):** real radio media testing, 1-minute sustained transfers, frame loss/duplicate injection, photo rendering, Opus microphone/playback, power profiling, long-range RF quality thresholds and complete ESP-IDF build.
- **v1.9 (planned):** durable, reconnect-safe media checkpoints in encrypted IndexedDB, receiving progress and cancellation across reboot, measured bitrate/latency, optional higher-budget long voice notes if the user accepts the physics of clear audio.
- **v1.10 (planned):** battery/low-power modes, sleep/wake power draw measurements, regulatory duty-cycle handling and safe +18 dBm supply validation.
- **v1.11 (planned):** replay resistance across reboot, independent security audit, fuzzing and soak tests.
- **v2.0 (final Android integration):** update the frozen Android BLE app to detect LoRa/ESP-NOW family, support authenticated radio/media transfers and preserve the original ESP-NOW behavior.

Every milestone must pass host tests and a clean ESP-IDF build, followed by physical two-node acceptance, before being called a hardware-stable release. No real-world distance is inferred from RSSI alone.
