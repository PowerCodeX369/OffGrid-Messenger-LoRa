# Changelog

## [1.0.0] — 2026-11-12

First independent OffGrid Messenger **LoRa-only** source release.

- New ESP32-C3 SX1278 firmware without ESP-NOW or Wi-Fi initialization, preserving NimBLE and USB.
- 433 MHz SF7/BW125 CR4/5 10 dBm fixed profile and existing tested GPIO map.
- Domain-separated HMAC-SHA256-authenticated LoRa headers and control frames around original browser AES-256-GCM OGM4 payloads.
- Dedicated radio ACK vs encrypted delivered/read receipt handling with bounded retry, duplicate suppression and control-priority turnaround.
- Web UI adapted for encrypted text only; photo/voice, adaptive ESP-NOW and speed benchmarks intentionally disabled.
- Android source preserved unchanged for the final universal-app phase.
- Host regression checks, CI workflow, docs, wiring, hardware acceptance plan, provenance and license.

**Release limitation:** physical ESP-IDF 6.1 compilation and integrated two-node verification pending; no validated latency or delivery-rate claims are made.
