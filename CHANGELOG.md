# Changelog

## v1.8.0-rc.3 — Voice budget web-only correction (2026-11-13)

- Increase encoded voice-note file cap from 20 KiB to **40 KiB** in the recorder, OGM4 codec and encrypted media receiver. A 40 KiB voice file can be split and reassembled with the existing media format; photos remain capped at 48 KiB in the browser (50 KiB protocol limit).
- Raise recording time ceiling from 18 seconds to **120 seconds**; the first reached limit (recording duration or hard byte budget) stops capture. Use mono mic capture with echo/noise suppression and request ~8 kbps Opus; actual bytes depend on the browser's encoder and content. Short notes are not padded or transmitted at fixed 40 KiB.
- Update microphone/tooltips, prevent stale JS in cache, add 40 KiB/40 KiB+1 boundary tests and Chromium media tests. **No firmware, RF, media transport, photo encoder, receipt or Android changes; existing rc.2 ESP-NOW-style UI retained.**
- **Not a 2-minute quality guarantee:** 40 KiB/120 s is ~2.73 kbps including container overhead; ordinary natural-sounding Opus speech cannot be assumed at this bitrate. The app stops before overflow and never silently sends an oversized voice note.


## [v1.8.0-rc.2] — 2026-11-13 · **web-only media UI repair**

- Restore **visible and operable ESP-NOW-parity photo and voice composer controls** on the LoRa page. Root cause: obsolete v1.0 CSS set `#attachments, #voiceButton { display:none!important }` even after v1.8 added functional media handlers. Remove that override rather than rewriting working media code.
- Preserve original ESP-NOW stylesheet and HTML media classes (`composer-attach`, `voice-button`, `chat-photo`, `voice-clip`, gallery), plus local image compression, browser Opus recorder and playback. Only the radio backend and honest size limits differ.
- Bump browser JS version to `1.8.0-rc2-lora` and HTML asset cache keys to `lora180rc2` so browser reload fetches the corrected stylesheet. **Firmware version stays `1.8.0-rc.1` and requires no reflash.**
- Add Chromium regression that asserts computed styles and real button visibility, opens the native photo chooser, exercises microphone handler and verifies received photo/audio elements at 1440/820/390px. The original test only checked that DOM elements existed and missed the CSS hide.
- Keep the v1.8.0-rc.1 firmware, encrypted media codec, ARQ transport, Auto/manual power profiles, chat receipts and frozen Android source byte-identical. Host tests pass; physical two-radio media tests remain unverified.

## [v1.8.0-rc.1] — 2026-11-13

- Add encrypted photo sending with size-targeted WebP/JPEG recompression and natural-detail-preserving resolution floor (48 KiB maximum). Existing gallery/zoom/download retained.
- Add short compressed Opus voice notes with playback/played receipt; hard 20 KiB encoded-file cap, browser-enforced early recording stop. Do **not** claim 2-minute clear Opus speech at 20 KiB.
- Add `OGM4 mediaPart` per-file browser-layer segmentation (5,800-byte parts), independent fresh AES-GCM envelopes and 16-byte truncated SHA-256 whole-file integrity. Bounded reassembly, duplicate protection, partial retry and application-level delivery only after the complete authenticated message.
- Add local `MEDIA ON/OFF` AUTO-radio hints. The sender can request authenticated Fast only with two-way strong signal margins, and original Auto-RF fallback remains active between ARQ pieces. Manual settings untouched; no unsafe automatic high-power change.
- Preserve v1.7 ARQ worker and SX1278 driver byte-for-byte. Photos, short voice, download and pause/retry are browser-managed; no ESP32 image/audio decoding.
- Add Node loss/duplicate/auth media tests and Chromium compression/Opus smoke. Actual ESP-IDF 6.1 build and physical media/RF tests **not performed**, so RC only. See `docs/V1_8_0_ENCRYPTED_MEDIA.md`.


## [1.7.0-rc.1] — 2026-11-13

- Add **operator-enabled Fast-priority automatic link adaptation**. Coordinator chosen deterministically by device MAC; authenticated per-node RSSI/SNR and recent ARQ retry/CRC reports establish link conditions in both directions. Fixed metres are not used.
- Add HMAC-authenticated AUTO control/ACK/quality frames, bounded retries, replay-id checks, stale-metric protection and activity-aware reporting. Mode changes reuse the v1.6.1-rc.2 signed one-click OFFER/AGREE and timed verification/rollback.
- Add per-profile RSSI/SNR hysteresis, two-sample downgrade, four-sample upgrade, 45-second nominal cooldown and emergency downgrade for sharply failing links. Operator may override Auto with any manual profile. Reboot remains manual Balanced.
- **Safety:** nominal +18 dBm requires explicit two-board confirmation. Auto Balanced uses nominal +18 dBm for correct link-budget progression; manual Balanced remains +10 dBm. Unsupported +22 dBm remains absent.
- **No ARQ rewrite:** keep the v1.6.1-rc.2 radio driver, six-frame window, AES-256-GCM/receipt protocol, USB bulk and Android files identical; add policy, packet and Chromium UI tests.
- **Qualification:** pure C/JS host suite and UI tests passed, but complete ESP-IDF 6.1 build, live two-radio automatic switch, power/current and field-range tests are pending. Previous user-confirmed v1.6.1-rc.2 is the rollback. See `docs/V1_7_0_ADAPTIVE_RF.md`.


## [1.6.1-rc.2] — 2026-11-13

**Rebuild directly from the last measured working v1.6.1-rc.1 transport. Not based on v1.6.2 or v1.6.3.**

- Repair one-browser manual synchronization: a signed 6-byte OGM2 profile offer/agree contains the initiating operator's confirmation that BOTH ESP32 power supplies and antennas were inspected. The other node follows the authenticated request, without an independent USB `POWER18 VERIFIED` click.
- Keep the local physical-USB opt-in to initiate Fast/Max, and never overdrive an SX1278. The unsupported fourth +22 dBm mode is removed from firmware and UI.
- Add bounded response timeout, deduplicated profile responses, and explicit authenticated peer-busy rejection rather than silent waiting. Pause mode changes when the browser still has pending user content; keep rollback and Balanced-on-reboot.
- **Deliberately do not change** the measured-working v1.6.1 ARQ bridge, six-fragment window, 224-byte payload, 25-ms guard, grouped USB staging, AES-256-GCM, HMAC radio packets, or delivered/read receipt logic. Source-equality checks compare these directly against the original v1.6.1 tag.
- Regression tests cover frame payload/nonce/consent validation, peer rejection, three profile registers, browser command order and active/rollback states.
- **Measured reference:** v1.6.1-rc.1 Balanced 457.4 B/s, 1.95 s median RTT, 8/8 confirmed (user). No v1.6.1-rc.2 RF measurement or ESP-IDF 6.1 build is available in the authoring environment.


## [1.6.1-rc.1] — 2026-11-13

**Response to measured v1.6 Fast-mode regression: 358.2 B/s versus v1.5 Fast 487.8 B/s.**

- Batch 160-byte encrypted USB stage commands in bounded four-line writes; checkpoint after every group rather than after every chunk, preserving sequence/CRC32 checks. A 1-KB ciphertext requires 4 USB stage round trips rather than 9.
- Use six-frame bitmap window for SF7 500/250 kHz (three for slower Max); 1-KB AES-GCM probes fit one 5-fragment window and one bitmap ACK rather than two bursts.
- Add a receiver-rearm guard of 25 ms after TxDone instead of 3 ms to reduce very-short-preamble missed frames. This is a conservative hypothesis pending per-transfer retry data, not a measured fix.
- Forward completed encrypted RX over USB in bounded groups of four, reducing radio-loop polls.
- Add per-transfer `BULKSTAT` RF duration, frames/retransmissions/ACK timeout counts and benchmark separation of USB staging vs RF confirmation vs encrypted end-to-end RTT.
- Maintain AES-256-GCM chat/receipt, HMAC-SHA256 radio data/ACK, manual modes, hardware-safe PA policy, disabled +22 dBm, BLE, and frozen Android. No automatic switching or media enablement.
- Host regression tests and browser UI checks pass; **ESP-IDF 6.1 full build and physical v1.6.1 benchmark still pending**. Do not claim field speed improvement.
- [Engineering review / hardware acceptance](docs/V1_6_1_RC1_BOTTLENECK_FIX.md).


## [1.6.0-rc.1] — 2026-11-13

**Complete firmware + web integration of selective-repeat authenticated LoRa transport. Hardware acceptance pending.**

- Replace stop-and-wait 160-byte radio fragments with **224-byte ciphertext fragments** (up to 254-byte on-air packets including frame header and 16-byte radio HMAC), four-fragment burst window and authenticated 32-bit received-fragment bitmap ACK. Retransmit only missing fragments; bound per-fragment retries and transfer size (7168 bytes). The existing OGM4 AES-256-GCM message and receipt layers remain intact.
- Add `offgrid_arq` as an ESP-IDF component with full `og_arq_bridge.*` radio-task integration, USB bulk ciphertext staging and an independent v1.6 web client (`lora_bulk.js`). No longer require a browser/USB round trip after each RF fragment. Introduce authenticated OGM2 ARQ capability announcement to avoid silent mismatches with v1.5 firmware.
- Preserve manual three-mode negotiation and rollback. Fast SF7/BW500 and Max SF9/BW125 now request **nominal +18 dBm** via the SX1278 high PA_DAC path, **only after each physical USB operator explicitly verifies hardware and powers on this capability**; Balanced remains +10 dBm. Fourth **Experimental +22 dBm** selector is shown but disabled and firmware rejects it as unsupported. No automatic switching.
- Keep `BULKSENT` TxDone single tick distinct from `BULKCONFIRMED` authenticated RF bitmap and encrypted OGM4 delivered/read double-gray/blue ticks. Retain full browser chat, BLE firmware, AES-256-GCM local history, priority encrypted receipts and speed benchmark. Photo/voice/video remain disabled and Android files unchanged.
- Add host bridge two-node loss-injection tests (1KB, 4KB, 7168-byte transfers, lost last fragments and ACKs), browser bulk staging/CRC tests, profile/power register tests, sanitizers and source-boundary tests. Known BLE transport and physical-radio limitations documented.
- **Source tested only:** A full ESP-IDF 6.1 compile and actual two-board speed/field reliability tests have **not** been performed. Do not treat theoretical airtime estimates as observed throughput. See [v1.6 integration/acceptance](docs/V1_6_0_ARQ_INTEGRATION.md).


## [1.5.0-rc.1] — 2026-11-13

**Manual three-mode LoRa radio candidate; v1.3/v1.4 were not published.**

- Add Low Power / Fast (SF7, BW500, +2 dBm), Balanced (SF7, BW250, +10 dBm), and Max Power / Range (SF9, BW125, +17 dBm) at 433 MHz, all 4/5 coding. Highest *standard* PA_BOOST output is +17 dBm; do not enable PA_DAC +20 dBm on unverified Ra-02/supply hardware.
- Add new authenticated profile OFFER/AGREE OGM2 control frames, manual USB `PROFILE` command, user-facing web profile chooser, negotiated RF reconfiguration, HELLO verification and timed rollback. No RSSI-driven automatic switching; restart always restores Balanced.
- Add profile-dependent radio-ACK retry deadlines, keeping the previous known-working direct-SX1278 TX path and the v1.2 browser receipt-priority scheduler. No CAD transmit gate.
- Add pure C tests for all three SX1278 registers and PA bounds, authenticated frame layout, source checks for manual-only control, and Chromium web modal smoke test.
- Keep AES-256-GCM chat/receipt protocol, HMAC radio framing, BLE and web USB, 500-character text and speed benchmark. Photos, voice/video and Android LoRa support remain disabled; Android tree is untouched.
- **Not hardware-verified**: actual ESP-IDF 6.1 compilation, two-node mode transitions, rollback, BLE coexistence, range, measured throughput and maximum power supply stability must be tested before stable release. See [acceptance checklist](docs/V1_5_0_MANUAL_PROFILES.md).


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
