# Changelog

## [1.2.0-rc.2] — 2026-11-12

**TX recovery candidate; the prior rc.1 was reported to stop sending messages.**

- Roll back the new rc.1 modem-status/CAD gate from the active data-transmit path. Restore the v1.1 radio send/retry path without changing radio parameters, browser framing, authentication, priorities, or pairing keys.
- Add a source regression guarding against reintroducing blocking CAD before the send call.
- Retain the v1.2 browser scheduling, encrypted receipt batching and speed test.
- **No hardware validation performed.** If TX remains broken, revert both firmware and web to v1.1.0-rc.1, the user's last observed working release.
- Details: [TX recovery / acceptance](docs/V1_2_0_RC2_TX_RECOVERY.md).


## [1.2.0-rc.1] — 2026-11-12

**LoRa burst-scheduling and encrypted speed-test release candidate.**

- One global browser-to-radio fragment scheduler (including encrypted receipts); bounded backlog, high-priority control preemption and fairness after four receipts. Eliminates accidental browser-side competing SX1278 transmissions.
- Authenticated delivery/read receipts compacted into batches of up to eight items (one LoRa fragment), with correct per-message ID/type, idempotent status processing and a bounded retransmission if the receipt radio ACK is missing.
- Firmware keeps one outstanding RF DATA, checks RX modem status without resetting an in-progress packet, then uses best-effort CAD and randomized channel-busy deferral; ACKs never wait for CAD. Suppresses discovery traffic when recently heard peer traffic is ongoing.
- Deduplicates queued radio ACKs and allows critical ACKs to displace stale HELLO commands; exposes real `TXFAIL`, `CAD_BUSY`, `PENDING` and `CTRLQ` diagnostics.
- **Real LoRa speed test:** 64/256/512/1024-byte encrypted application probes, 4/8/12 trials, remote encrypted confirmation, loss rate, median/p95 RTT and payload goodput (not PHY bitrate). Accessible in Settings; does not populate chat history.
- Firmware `FW=1.2.0-rc.1|ACKENGINE=2|FLOW=2`. Updated web clients require `FLOW=2`; both devices and both browsers must use v1.2 together.
- Added 50-job serialized burst, bounded-retry/fairness, 8-receipt AES-GCM, and benchmark probe tests; prior AES-GCM, Unicode and SPI DMA checks remain green.
- Fixed 433 MHz SF7/125 kHz profile unchanged; no ESP-NOW or Wi-Fi runtime, media still disabled, and Android unchanged.

**Validation limitation:** the ESP-IDF 6.1 hardware build, browser Web Serial interaction, and actual two-board burst/loss/throughput performance remain unverified. See [v1.2 engineering and acceptance notes](docs/V1_2_0_BURST_RELIABILITY.md).


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
