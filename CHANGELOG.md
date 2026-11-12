# Changelog

## [1.1.0-rc.1] — 2026-11-12

**LoRa text reliability/performance source candidate; hardware acceptance pending.**

- `SENT` now means actual SX1278 TxDone (single gray tick when all fragments transmitted); `RADIOACK` is a separate authenticated RF-fragment confirmation.
- Only an authenticated encrypted OGM4 delivered/read receipt produces double-gray/blue ticks; a lost receipt stays unconfirmed.
- Remove redundant firmware-level fragment-delivered radio packets and unnecessary application-ACK-driven data retransmissions.
- Priority `CTRLHEX` for encrypted receipts; up to three browser-prepared text fragments, but a single outstanding unacknowledged LoRa RF frame.
- SX1278 FIFO packet burst over SPI DMA; 218-byte TX/RX emulation regression test.
- Shorter bounded radio ACK wait with randomized backoff, reduced idle HELLO traffic, persistent PSA HMAC key handle, and per-stage timing diagnostics.
- Reject older firmware without `ACKENGINE=2` to prevent silent v1.0/v1.1 mixing.
- Photo/voice/video remain disabled, Android files unchanged, and no Wi-Fi or ESP-NOW runtime calls.

**Important:** No real ESP-IDF build, benchmark, or two-board acceptance test was possible in the source authoring environment. No guaranteed text latency or packet-loss rate is claimed.


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
