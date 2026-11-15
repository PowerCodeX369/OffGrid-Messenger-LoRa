## v3.0.6-rc.3 — Exact RSSI bands and disconnected-peer Auto search (2026-11-15)

- Replace Fast-first automatic policy with the explicit operator requested bands: Balanced for RSSI above −100 dBm; Max for −130 to −100 dBm inclusive; Ultra below −130 dBm. Promotion and downgrade both map directly to the band on the freshest authenticated LoRa RSSI observation. An unavailable packet is not interpreted as −131 dBm. Reject stale/unconsented samples; use the worse of both directions when the peer metric is recent.
- Preserve authenticated coordinator OFFER/AGREE/verify, 16-byte Max/Ultra ACK, AES-GCM application content, original UI and SX1278 driver. Failed offers are backed off; ongoing unconfirmed ciphertext can be explicitly requeued on a degrading link; no synthetic delivered/read ticks.
- Add bounded, provisional Auto profile scanning after true radio silence, including **Balanced→Max→Ultra** (rather than only scanning Balanced from Max/Ultra). Retain legacy manual rendezvous. A scan becomes active only after authenticated peer reception; otherwise home profile is restored and rescanned later. Discovery is best-effort, not guaranteed over an absent physical RF path.
- Update Android native LoRa mode labels/telemetry and web radio-mode UI; show exact thresholds and preserve manual override/high-power safety confirmation. Increment Android versionCode to 307. Keep the web-compatible `FW=1.8.0-rc.4` wire marker; advertise `AUTO_SCAN=TRIBAND|AUTO_RSSI=-100,-130` to distinguish updated firmware.
- Add deterministic host boundary tests, lost-link state-machine tests, Android parsing tests and source integration contracts. Source/host validated; ESP-IDF 6.1 full target build, Android APK build and physical field acceptance are still outstanding. [Deployment and tests](docs/V3_0_6_RC3_RSSI_BANDS_AND_AUTORECOVERY.md).

## v3.0.6-rc.2 — SF9/SF12 compact ACK & airtime-guarded transfers (2026-11-15)

- Fix the reported long SF9 text latency and failed SF12 text transmissions by replacing 34-byte bitmap ACKs with **16-byte full authenticated HMAC-SHA256-128 tags**. Active transfer IDs and at most three outstanding bitmap candidates reconstruct metadata without shortening the cryptographic tag.
- Defer opportunistic HELLO/CAPS/Auto metric control traffic during a slow-profile ARQ transfer so a half-duplex ACK receive window is not interrupted.
- Fix SF12 ACK/DATA collisions caused by the previous 350-ms partial-ACK silence timer, now computed from next-fragment airtime (CR4/8 included). SF9 uses its own airtime-aware silence instead of a fixed potentially inappropriate delay; final-burst ACKs remain fast.
- Use 128-byte text fragments in SF9 Max, preserve original 224-byte SF9 media capacity, and keep Ultra's already-small 48/64/80/96-byte text fragmentation. Preserve existing Fast/Balanced ACK layout and all radio modulation/power configurations.
- Require authenticated `ARQ_CAPS=4` on both peers before enabling Max/Ultra compact ACK format; reject mixed versions safely. Add `ACK_COMPACT=16|ARQ_CAP=4|MAX_TEXT_FRAG=128` to READY. Existing Android/web code and BLE pairing remain unchanged.
- Add deterministic host tests for lost DATA/ACK, partial bitmap, stale/wrong-session/forged compact ACK, and correct SF9/SF12 wait windows. **No ESP-IDF target APK or actual RF hardware test was performed**. See `docs/V3_0_6_RC2_COMPACT_ACK_RANGE_ULTRA.md`.

## v3.0.6-rc.1 — Peer rediscovery and reliable automatic RF downgrade (2026-11-15)

- **Recover from split modulation:** after prolonged absence of authenticated peer traffic on a non-Balanced profile, periodically rendezvous on the common SF7/BW250 boot modulation; only remain there after verifying a peer HMAC. Return home and retry if not found. Keep SF9 and SF12 configurations unchanged.
- **Downgrade while diagnostics are missing:** allow recent locally authenticated RSSI/SNR plus previously authenticated peer Auto approval to propose a safer mode even when a remote metric report is stale. Promotions still require both directions, and every switch still requires authenticated peer agreement/verification.
- **Avoid ARQ adaptation deadlocks:** urgent safer-profile offers can cancel unconfirmed encrypted transfers on either radio, sending explicit failures for the client outbox to retry rather than faking delivery; scheduled quality reports coalesce and don't vanish merely because a peer was heard.
- **Restore receiver operation:** bounded continuous-RX re-arm on a silent Balanced peer, fast HELLO cadence while searching, stale-media-boost release, peer-capability refresh after reconnect, and recovery counters/diagnostics.
- Preserve native Android UI, Web Serial messenger, encryption, messages/receipts, photo/voice codecs, SF7/SF9/SF12 modulation, GATT implementation, RF packet format and SX1278 driver. Android versionCode incremented to 306 (no behavioral/UI changes).
- Source-level C, JavaScript and Kotlin tests include authenticated recovery and policy safety regressions; target firmware/Android builds and two-radio field acceptance remain pending. See `docs/V3_0_6_LINK_RELIABILITY_AND_RENDEZVOUS.md`.

## v3.0.5-rc.4 — SF12 verification collision avoidance and browser duplicate-send guard (2026-11-15)

- Stagger authenticated SF12 verification HELLOs by initiator/responder role. Prior implementation sent multiple-second CR4/8 HELLOs simultaneously after mode change; both could collide until rollback. Pure timing helper and airtime-based host tests protect the old Fast/Balanced/Range verification timing.
- Synchronously guard web Send before asynchronous encrypted IndexedDB persistence; prevent a fast double-click/Enter from creating separate message IDs. Subsequent intentional same-text sends still work. Add received-application-ID diagnostics and rate-limit repeated weak-link waiting notifications.
- Add `PROFILE_SYNC=ULTRA_SLOTS4` to READY without changing strict web-compatible firmware token or authenticated RF protocol.
- **No** Android UI, AES/media codec, SX1278 mode register, radio ARQ, BLE/NVS or message encryption changes. Source tested; physical mode-change and close-range delivery still need user validation.
- [Details and acceptance](docs/V3_0_5_RC4_ULTRA_SYNC_AND_DOUBLE_SEND.md).

## v3.0.5-rc.3 — ARQ capability-3 interoperability repair (2026-11-15)

- **Root cause established from two real node logs:** both firmware images sent authenticated `ARQ_CAPS=3`, but `og_wire_valid()` rejected all cap-3 control frames (accepted only 1 and 2). Consequently `PEER_ARQ=0` on both and every encrypted text failed `PEER_REQUIRES_V16_ARQ` before RF ARQ transmission.
- Allow authenticated ARQ capability values 1, 2, 3; continue rejecting out-of-range values and malformed lengths. Add explicit C wire-format tests. No change to ARQ packet format, crypto, UI, media codec, RF profiles, NVS or radio driver.
- Both radios must be rebuilt/flashed from this corrected firmware. RC2 browser diagnostics remain compatible and are useful for checking `PEER_ARQ=1`, `ARQ_TX>0`, and `BULKCONFIRMED`.
- Full host suite passes; ESP-IDF target build and hardware transmission require user validation. [Details](docs/V3_0_5_RC3_ARQ_CAPS_FIX.md).

## v3.0.5-rc.1 — Final weak-link delivery stabilization candidate (2026-11-15)

- **Encrypted persistent text outbox:** Android native encrypted history + Web IndexedDB now retain undelivered outgoing text as `waiting`; active, authenticated clients retry only after fresh LoRa peer traffic. 15–300-second exponential backoff and per-message 0–5-second pseudorandom jitter; no fake delivery receipts. Restore prior failed/in-flight texts as waiting rather than silently discarding them. No cloud service, no unencrypted message spool.
- **Application-delivery correctness:** distinct message ID vs fresh AES-GCM radio transfer ID per attempt; authenticated delivered/read/played receipts alone upgrade ticks; duplicate receiver messages re-ACKed; receipt grace extended for Ultra. Native receipt state machine preserves delivered/read when a retry is pending.
- **Ultra RF improvements:** SF12/BW125 now uses CR4/8 FEC (SF9/BW125 Max Range remains CR4/5 and unchanged), 12 bounded ARQ attempts, re-computed ACK deadlines from SF12/CR4/8 airtime with increasing backoff, existing adaptive small fragment sizes and one-frame weak-link windows. Old firmware Ultra is blocked until authenticated peer advertises capability 3. Retains high-power opt-in and signed profile switching.
- **Priority and diagnostics:** queued short text and receipts take precedence over long media at safe transfer boundaries; additional ARQ counters and bounded Web Serial STATUS retry to avoid amplifying `EVENT_QUEUE_FULL` during a failed radio boot. MEDIA still rejects Ultra.
- **Compatibility and version:** Android versionCode 305, web v1.10.0-rc.2-final-weaklink, matching new firmware marker `RF_POLICY=ULTRA12_FEC8` on USB and BLE. Keeps `FW=1.8.0-rc.4` historical web compatibility identifier, so check RF_POLICY—not FW—to confirm this release.
- **Validation scope:** full native/Node/C/Python host regression suite and isolated SHA256/Git archive checks. No ESP-IDF toolchain or Android SDK available in build container; no physical BLE/RF or low-battery qualification. Release remains `rc.1` until two-board field acceptance. [Details and safe rollback](docs/V3_0_5_FINAL_RELIABLE_TEXT.md).

## v3.0.4-rc.1 — SF12 Ultra Long Range integrated into v3.0.3 (2026-11-14)

- Add **fourth manual RF profile** `ULTRA` SF12/BW125/CR4/5 at verified nominal 18 dBm; Auto can enter Ultra on weak bidirectional metrics. Existing SF9 Max Range remains exactly SF9/BW125 with the same power setting.
- ARQ ultra-specific 48/64/80/96-byte HMAC-signed fragments, small weak-link ACK windows, extended authenticated retry and SF12 airtime-driven timeouts; original non-Ultra 224-byte transfer and codecs unchanged.
- Reject tagged photos/voice in Ultra at native Android UI, web UI, media queue and firmware command path. Continue encrypted text, delivered/read receipts and profile control.
- Negotiate new authenticated peer `ARQ_CAPS=2` before manual/Auto Ultra; maintain signed OFFER/AGREE and rollback, longer Ultra verification and Android profile response patience.
- Host-tested firmware radio mode, ARQ (including lost data and bitmap ACK), policy calculations, native Android codec, browser crypto and media restrictions. Real ESP-IDF/Android target builds, RF field tests and legal duty compliance remain outstanding. See `docs/V3_0_4_ULTRA_LONG_RANGE.md`.

## v3.0.3-rc.1 — Early LoRa Range switching and true RF-link status (Android + firmware)

- Field-tuned proactive two-node Auto RF downgrade ahead of the observed -100 dBm delivery failure; keep signed handshake, rollback and manual control.
- Distinguish BLE connected vs authenticated LoRa peer heard using age-tagged `LINK` telemetry; show native Android rejection reasons.
- Fix Android versionCode/name so the new build can be distinguished from v3.0.1; preserve original Compose UI and web app.
- Protect weak links from inappropriate temporary Fast media boost; preserve encrypted media transport and BLE GATT authentication.
- **Source-level verification only**; real ESP-IDF/Android builds and RF range tests still pending. See `docs/ANDROID_RF_RANGE_V3_0_3.md`.

## v3.0.2-rc.1 — Native Android LoRa menu and authenticated Fast media handover (2026-11-14)

- Preserve the original native Android Compose chat UI, replacing **only** obsolete ESP-NOW radio menu labels/choices with Auto, Fast, Balanced and Max Range. Interpret actual SX1278 `LINK|...|PROFILE=...|AUTO=...` telemetry; no fictional `LR 250/500 kbps` LoRa setting.
- Fix missing nominal +18 dBm authorization that previously made `MEDIA ON` return `MEDIA|BYPASS|POWER18_NOT_VERIFIED` on Android. Require explicit both-board confirmation once per BLE session and wait for `POWER18|ENABLED` before negotiating authenticated Fast handover; never fabricate success.
- Gate media staging on `MEDIA|READY`, visibly report explicit bypass, fail on synchronization error, and send `MEDIA OFF` with restoration wait after completion/failure. Declining confirmation allows an explicitly labeled current-mode transfer; radio safety approval is never automatic.
- When exiting Auto for a manual profile, wait for authenticated `AUTO|OFF` before issuing a manual `PROFILE` command. Correct the old menu labels without introducing a WebView screen or altering the photo/voice presentation.
- Preserve all `web/`, SX1278 firmware, ARQ, BLE companion, native encryption/codec, GATT pairing and UI files other than the requested radio menu. Host RF/security and standalone Kotlin tests pass; no Android SDK, APK or physical phone was available. This is a source release candidate.

## v3.0.1-rc.1 — Native Android UI restoration and LoRa BLE protocol port

- Restore original Android `MainActivity` launcher; remove v3.0.0 alternate WebView, bundled HTML/JS/CSS and USB OTG screen. Original `PremiumUi.kt` and BLE GATT/Keystore code stay byte-identical.
- Replace ESP-NOW-style per-fragment `MSGHEX` sending under the original native UI with LoRa AES-GCM OGM4 bulk transfers, authenticated receipts and web-compatible 48/40 KiB multipart media.
- Preserve the prior opt-in LoRa NimBLE bulk companion firmware unchanged. Require authenticated LoRa bulk capability negotiation on Android before enabling send; no false compatibility with old rc.4 BLE firmware.
- Keep manual/Auto modes mapped behind the existing native radio controls, with original RF code untouched.
- Add native Kotlin end-to-end crypto/media/bulk tests and checks that the web, SX1278 firmware and Android Compose visuals were not changed. No target APK or phone test claimed.

## Android universal app 3.0.0-rc.1 (source candidate; pending Android SDK and two-radio verification)

- New Android launcher uses the unchanged v1.10 web HTML/CSS/JS (plus origin-scoped Web Serial bridge), keeping exact text/photo/voice/receipt/media-queue/RF UI parity.
- Native Android USB host implementation works with ESP32-C3 native USB Serial/JTAG and existing v1.8.0-rc.4 firmware without reflashing. Other USB-UART adapters require a driver.
- Authenticated NimBLE transport reuses the preserved Android client; local pairing key is encrypted with AndroidKeyStore; companion BLE firmware opt-in adds LoRa bulk media and profile commands without exporting keys.
- Legacy ESP-NOW Android `MainActivity` remains present; no radio/ARQ/crypto/media transport rewrite.
- Host C, Node, Kotlin/JVM and existing web protocol checks passed. Android/ESP-IDF full builds and phone/radio acceptance tests **have not run**.

# Changelog

## v1.10.0-rc.1 — security / stability receive hardening (2026-11-13)

- Build directly on the user-confirmed working **v1.8.0-rc.6** source and preserve the original ESP32 `FW=1.8.0-rc.4` runtime. **Web-only:** two browsers must use `1.10.0-rc1-lora`; do not flash the radios.
- Prevent overlapping authenticated decryption/application handling for one LoRa transfer ID; collect legacy APPACK packet IDs while decryption is pending, and authenticate before acknowledging. Bad GCM/envelope data never enters the successful replay cache and can retry.
- Bound the receive-in-progress map and successful-transfer cache; clear at browser disconnect and guard against asynchronous decryptions completing after key/session replacement.
- Add deterministic replay stress (100,000 transitions), 40,000 malformed USB/media parser mutations, byte-wise AES-GCM tampering, transfer-ID AAD checks, full voice reassembly and Chromium concurrent-duplicate tests.
- **Unchanged:** radio firmware and power policy, original ARQ/USB staging, OGM4 encryption, message/status semantics, media compression/segment lengths, and ESP-NOW-style web controls.
- No persistent replay prevention for RF control packets across board resets, no independent audit, no hardware proof of security, and no ESP-IDF build in this environment. See `docs/V1_10_0_SECURITY_STABILITY.md`.


## v1.8.0-rc.6 — shorter-text latency and media queue fairness (2026-11-13)

- **Web-only release on rc.5.** Keep both physical ESP32-C3 nodes running `FW=1.8.0-rc.4`; both browsers use `1.8.0-rc6-lora`. No SX1278, ARQ, authenticated control format, encryption, 48 KiB photo codec or 40 KiB voice changes.
- **One-chunk encrypted text/receipt USB fast path:** for a ciphertext of at most 160 bytes, send firmware's already-supported `BULKBEGIN`, `BULKCHUNK`, `BULKEND` commands in one Web Serial write, then await `BULKQUEUED`. The firmware still verifies sequence, length and CRC32; `BULKFAILED` still rejects; RF bitmap ACK and encrypted delivery/read receipts remain authoritative. Longer messages/media keep rc.5's eight-line batching.
- **Queue fairness during media:** send at most one waiting text after a full media segment has finished its authenticated bitmap-ACK transfer, before starting the next media part. Do not interrupt an RF frame, profile transition or media Fast/restore handshake. Media remains the active parent for cancel/disconnect.
- Content-free `TEXTPERF` (LOCAL, QUEUE, USB, RFWAIT, device RF, retries/timeouts) and `TEXTRECEIPT` (authenticated delivered/read RTT) serial diagnostics help isolate delays. Log no plaintext, keys or message IDs.
- Host C/JS, Chromium real media composer/encoder, UI parity, media transfer/restore, short USB success/error and segment-boundary scheduling tests pass. **No physical speed gain has been measured yet.** This targets the reported text delays without modifying the known-working radio firmware; revert both web folders to rc.5 if necessary.


## v1.8.0-rc.5 — larger media pieces, fewer browser USB checkpoints and timing visibility (2026-11-13)

- **Web-only:** keep both ESP32-C3 firmware images on `FW=1.8.0-rc.4`; use web version `1.8.0-rc5-lora` on **both** browsers. The photo/voice segment length changes from 5,800 to **7,040 bytes**; encrypted bulk frames remain at or below 7,168 bytes, and the receiver still requires AES-256-GCM authentication followed by full-file SHA-256 validation.
- **48 KiB photos use 7 browser-mediated ARQ sessions instead of 9**; **40 KiB voice uses 6 instead of 8**. Upload batches grow from 4 to **8 × 160-byte** USB lines per browser write, without changing the existing firmware's 4-line checkpoint output, sequence validation, CRC32 or 4 KiB USB receive buffer.
- Aggregate sender-side **`MEDIAPERF`** and per-segment **`MEDIASEG`** logs report byte count, end-to-end media-worker wall duration, mode handover/restore, browser USB staging, RF confirmation wait, firmware RF stats, retries and residual app/queue time. Logs contain only sizes/times, no user media, pairing keys or message IDs.
- The user's same-photo measurement was **85 s Balanced / 75 s Fast** for a **46.7 KiB** compressed image; that points to substantial non-PHY or retransmission overhead but cannot isolate its cause without `BULKSTAT` metrics. This candidate makes no untested speed claims and preserves a v1.8.0-rc.4 rollback.
- Chromium media preflight, encoder and original UI checks, host C/JS regression suite, byte-identical firmware and Android, and 48/40 KiB authenticated reassembly verified. Full ESP-IDF build and physical two-board benchmark have not run here.

## v1.8.0-rc.4 — synchronized temporary Fast for photo/voice (2026-11-13)

- Initiating browser waits for **authenticated two-node Fast profile confirmation** before any encrypted media radio transfer; after completion/cancellation/failure it sends `MEDIA OFF` and waits for the previous profile to be restored.
- Firmware uses existing signed OFFER/AGREE/HELLO verification/rollback for media overrides; adds a distinct `0x10` media-intent flag that does not overlap the existing `0xA5` high-power confirmation or `0x40` Auto intent. Retains both manual setting and Auto-enabled state. A busy/weak link can bypass the boost safely.
- Restores only when radio/USB/receipt queues are idle; bounded retries and diagnostics, 10-minute stale-hint recovery. Fast requires explicit operator approval for **both** power/antenna configurations. Old firmware has no compatible session barrier: **flash both nodes**, and replace both browsers.
- AES-GCM media codec, 48 KiB photo/40 KiB voice budgets, voice encoding, ESP-NOW-parity media UI, original ARQ/SX1278 driver, text/receipt logic, Android and Git rollback history remain unchanged. No physical or throughput claims; see `docs/V1_8_0_RC4_SYNCHRONIZED_MEDIA_FAST.md`.



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

## v3.0.5-rc.2 — Web-only near-range regression isolation (2026-11-15)

- Source-level recovery candidate in response to reports that two close ESP32 nodes remain connected in v3.0.5 while outgoing text stays pending; v3.0.4 reportedly delivered text on the same hardware.
- Restore v3.0.4 FIFO transfer ordering as a controlled A/B test; retain encrypted deferred text, integrity, existing firmware and Android without modification.
- Fix monotonic authenticated delivery/read receipt state and add precise USB bulk stage, full ARQ `LINK` counters, and privacy-safe local pairing key fingerprint diagnostics.
- Provide a pristine v3.0.4 browser comparator to test against the same v3.0.5 firmware in manual Balanced (not SF12 Ultra). No firmware or NVS erase required.
- Host tests pass; hardware root cause not yet proven. See `docs/V3_0_5_RC2_WEB_CLOSE_RANGE_REGRESSION.md`.
