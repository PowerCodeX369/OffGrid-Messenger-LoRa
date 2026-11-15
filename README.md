> **CURRENT RELEASE: OffGrid Messenger LoRa v3.0.5-rc.1 — Final Weak-Link Stabilization Candidate.** Preserves SF7 Fast/Balanced, SF9 Max Range, SF12 Ultra (text only), original native Android UI, AES-256-GCM media and signed two-node switching. Adds **encrypted reconnect-safe text outboxes in Android/web**, acknowledgement-aware backoff with per-message jitter, text/control traffic priority, SF12 CR4/8 FEC and airtime-aware Ultra ARQ deadlines, and explicit RF compatibility checks (`RF_POLICY=ULTRA12_FEC8`, authenticated ARQ_CAPS=3). Both ESP32s + the matching Android/web clients need updating. **Source and host tests passed; ESP-IDF target, Android APK and real radio tests are NOT verified.** Complete [installation, acceptance and rollback guide](docs/V3_0_5_FINAL_RELIABLE_TEXT.md). Earlier release notes below are historical; do not follow their outdated installation versions for v3.0.5.

> **NEW v3.0.4-rc.1: Ultra Long Range SF12/BW125 (encrypted TEXT + receipts ONLY).** SF9 Max Range remains unchanged, as do Fast, Balanced, the original Android native UI, AES-256-GCM, and non-Ultra media. Ultra adds HMAC-covered short ARQ fragments, SF12 airtime-aware ACK/retry deadlines, authenticated two-board capability negotiation and Auto Ultra entry/recovery hysteresis. **Both radios, the Android app and full web client must use the new coordinated v3.0.4 sources.** No flash-ready binary or phone-tested APK is claimed. Read [integration, field test and rollback guide](docs/V3_0_4_ULTRA_LONG_RANGE.md).

> **Android/RF companion hotfix v3.0.3-rc.1:** early synchronized Range adaptation, fresh LoRa signal indicators and Android mode-command error reporting. The original Android UI and browser web client are preserved. Both radios need the companion firmware; the Android source now declares versionCode 303. See [hardware test plan](docs/ANDROID_RF_RANGE_V3_0_3.md). This is source-tested, NOT a phone/radio-qualified APK.

> **Android native LoRa v3.0.2-rc.1 (source candidate):** The original Android Compose chat screen remains intact. The existing radio dropdown now correctly names **SX1278 LoRa** and provides only **Auto, Fast SF7/BW500, Balanced SF7/BW250, and Max Range SF9/BW125**—the obsolete ESP-NOW LR 250/500 kbps choices are gone. Android now explicitly confirms both boards' +18 dBm RF readiness, waits for firmware `POWER18|ENABLED`, then requests `MEDIA ON` and waits for synchronized `MEDIA|READY|MODE=LOW` before photo/voice bulk sending. After the transfer it sends `MEDIA OFF` and waits for restoration. A bypass visibly uses the current mode rather than pretending Fast is active. All other original UI, the web app, BLE pairing/GATT client, firmware, SX1278 driver and ARQ engine are unchanged. **No APK or physical two-board validation was possible here.** See [Android native v3.0.2 acceptance](docs/ANDROID_NATIVE_LORA_V3_0_2.md).

> **Android native LoRa v3.0.1-rc.1 (source candidate):** The original ESP-NOW **native Compose user interface is restored byte-for-byte**; the experimental WebView launcher, bundled duplicate web UI and USB-OTG Android screen from v3.0.0 have been removed. The original authenticated BLE GATT/Keystore connection now carries LoRa-compatible encrypted bulk text, 48 KiB photos, 40 KiB voice notes, receipts, media retries and existing radio controls through native Kotlin code. **Web messenger files remain completely unchanged.** The v1.8.0-rc.4 firmware still advertises authenticated BLE but **cannot send LoRa bulk media over BLE without the included companion firmware flashed onto both radios**, preserving NVS. Neither an APK nor an ESP-IDF binary was built here. See [native Android setup](android/README.md) and [hardware acceptance checklist](docs/ANDROID_NATIVE_LORA_ACCEPTANCE.md). Battery optimization remains last, after Android.

# OffGrid Messenger LoRa

**v1.10.0-rc.1 — WEB-ONLY SECURITY/STABILITY HARDENING on the user-confirmed-working rc.6 release.**

The browser now avoids concurrent duplicate decryption/application handling of the same authenticated radio transfer, while failed authentication remains retryable. A bounded receive guard, 100,000-operation replay stress test, 40,000 mutated-frame/parser cases, and Chromium duplicate-delivery tests are included. **Keep both ESP32s on FW=1.8.0-rc.4** and deploy the new complete `web/` folder to both clients; no flashing. Existing text/media code, 48 KiB photos, 40 KiB voice, manual/Auto modes and media Fast handover are preserved. This is not a persistent RF control-frame anti-replay implementation or a security certification. See [v1.10 limitations and acceptance](docs/V1_10_0_SECURITY_STABILITY.md).

---


**v1.8.0-rc.6 — WEB-ONLY SHORT-TEXT LATENCY FIX, on top of the working v1.8.0-rc.4 radio firmware.**
Short encrypted texts/receipts now stage their BEGIN/CHUNK/END over one USB write, and text sent during a long photo/voice transfer may transmit between completed media segments. The rc.5 7,040-byte media pieces and eight-line USB pipeline are preserved. Diagnostics: `TEXTPERF` and `TEXTRECEIPT`. See `docs/V1_8_0_RC6_TEXT_LATENCY.md`.

**Previous rc.5 baseline — MEDIA EFFICIENCY & MEASUREMENTS.** Keep **both ESP32s on `FW=1.8.0-rc.4`**; install the complete rc.5 `web/` directory on **both browsers** and hard-refresh. The browser now uses 7,040-byte media segments (48 KiB photo: 7 instead of 9), 8-line USB upload batches (firmware continues to acknowledge every four), and reports `MEDIASEG` / `MEDIAPERF` timings in USB diagnostics. The encrypted media header format, radio ARQ/driver, photo/voice limits, UI, receipts and automatic Fast synchronization stay unchanged. **No physical speed gain has been measured yet.** See [web-only optimization and acceptance](docs/V1_8_0_RC5_MEDIA_EFFICIENCY.md).

---

**v1.8.0-rc.4 — temporary Fast mode while sending photos and voice notes, then synchronized return to the previous setting.** Built directly from the user-confirmed-working v1.8.0-rc.3 photo/voice release.

**Both ESP32-C3 boards must be rebuilt/flashed with `FW=1.8.0-rc.4`, and both browsers must use `1.8.0-rc4-lora`.** This is **not a web-only hotfix**. Do not erase NVS. Manual/Auto modes, AES-256-GCM, receipt ticks, image compression, 40 KiB voice budget, UI, ARQ and SX1278 driver are retained. The browser waits for an authenticated two-node Fast-mode confirmation **before** sending a single media segment, then waits for restoration of the saved mode after transmission. Fast may be bypassed on an unsafe or busy link; no media is knowingly sent during an unconfirmed mode transition. The first media use from Balanced/Range prompts for approval that **both** boards can safely transmit at nominal +18 dBm.

**Release candidate:** C/JS host tests and browser simulation have passed; real ESP-IDF 6.1 build and two-radio media Fast/restore tests have not been run here. Keep your verified rc.3 firmware/web ZIP for rollback. See [media RF handover and hardware acceptance checklist](docs/V1_8_0_RC4_SYNCHRONIZED_MEDIA_FAST.md).

---

**v1.8.0-rc.3 — voice-only size update (40 KiB max / 120 seconds maximum recording), layered on the v1.8.0-rc.2 web UI.**

**No reflash:** Both ESP32s remain on `FW=1.8.0-rc.1`. The browser voice limit, mono Opus request and voice tooltips are the only functional changes; photos, UI controls, transport, encryption, manual/Auto RF and firmware are identical to rc.2. Shorter voice clips have proportionately smaller encoded files in general, but browser Opus cannot guarantee one-minute voice at 20 KiB or two-minute continuous natural speech at 40 KiB. The byte limit is absolute and recording stops when the codec approaches it. See [voice budget note](docs/V1_8_0_RC3_VOICE_BUDGET.md).

**Earlier release:** v1.8.0-rc.2 restored ESP-NOW-parity media controls in the LoRa web client.

**Compatibility:** **Keep both ESP32s on the existing `FW=1.8.0-rc.1`**. Only the browser assets change in this release: the original ESP-NOW-style `+` photo picker and microphone are visible again, and received media retains the original photo gallery and native audio player presentation. Do **not** flash or erase NVS for this hotfix; use the included `web/` directory on both browsers, hard refresh and reconnect USB. The browser version is `1.8.0-rc3-lora`, firmware remains `1.8.0-rc.1`.

**Root cause:** an inherited v1.0 LoRa stylesheet explicitly had `#attachments, #voiceButton { display:none!important }`, even though v1.8 had implemented the photo and voice handlers. That rule has been removed. The original ESP-NOW stylesheet is still the literal CSS foundation of this client. Regression tests now require visible, enabled media buttons and actual photo-picker/mic click wiring in Chromium across desktop, split and mobile layouts; the earlier v1.8 tests checked only whether these elements existed, so they missed the regression.

**Source constraints:** v1.8.0-rc.1 ESP-IDF firmware, SX1278 driver, ARQ engine, browser media/crypto logic, receipt code and the Android tree are unchanged. No new throughput claim or two-board field-validation claim is made. See `docs/V1_8_0_RC2_WEB_UI_REPAIR.md`.

Previously: v1.8.0-rc.1 introduced encrypted photos and short Opus voice notes on the working v1.7 Auto-RF platform.

This is a **source release candidate**. All browser C/JS/Node media tests and Chromium compression/short-Opus tests passed; **ESP-IDF 6.1 compilation and real two-board media transfers were not possible in the container**. The confirmed-working v1.7 ZIP remains the rollback.

## New in v1.8

| Feature | Enforced limit | Implementation |
|---|---|---|
| Photos | **48 KiB** (≤49,152 bytes) | WebP preferred/JPEG fallback, quality-adaptive 1600–640 px long edge, EXIF stripped via Canvas; error rather than unreadable extreme downscale |
| Voice notes | **40 KiB** (≤40,960 bytes) | Browser mono Opus ~8 kbps requested; early stop near 37 KiB, actual encoded file strictly capped; maximum recording duration 120 seconds if byte budget permits |
| Large media transport | 7,040 bytes/segment in rc.5; 7,168 bytes/encrypted bulk transfer | Fresh AES-256-GCM per piece, authenticated radio HMAC and six-fragment selective ARQ, whole-media SHA-256 digest, retry only unconfirmed piece |
| Media-aware RF | Opt-in AUTO only | Fast priority with strong **two-way** link evidence; signed downgrade when weak, manual profile override preserved; cannot switch safely during a completely silent link |

**Important audio limitation:** 120 seconds within 40 KiB = only ~2.73 kbps including metadata, below the standard Opus codec’s 6 kbps minimum specification. The browser **cannot guarantee** a natural-sounding two-minute note inside 40 KiB, nor an exact 20 KiB one-minute note. Voice size scales with real recorded audio but encoding overhead varies; the recorder stops at the 40 KiB cap. For longer natural-sounding speech the format/size budget will need a future revision.

**First tests:** flash both boards with v1.8 and open **both v1.8 browsers**. Start in manual Balanced, send text both ways, then try the `+` photo button and the microphone. To allow a media transfer to opportunistically use Fast then adapt down, enable Auto after checking the normal high-power safety prompts. Everything remains offline with no Wi-Fi or ESP-NOW firmware use. The Android source stays frozen.

Read **[v1.8 media architecture and hardware acceptance](docs/V1_8_0_ENCRYPTED_MEDIA.md)** for packet format, recovery, field tests and limitations.

---

## Previous baseline details — v1.7


**v1.7.0-rc.1 — optional automatic three-mode LoRa link adaptation with Fast priority, authenticated peer synchronization, and manual override**

Independent SX1278/Ra-02 + ESP32-C3 Super Mini firmware, browser messenger, source/test history, and preserved original Android application. Maintained under PowerCodeX. This is **LoRa-only** firmware: no Wi-Fi or ESP-NOW initialization; the original ESP-NOW repository is separate and unchanged.

> **Measured working baseline:** The user's v1.6.1 Balanced test confirmed **8/8 encrypted 1,024-byte messages, 457.4 B/s and 1.95 s median RTT**. The user subsequently confirmed **v1.6.1-rc.2 messaging and one-click manual synchronization work perfectly**. **v1.7 preserves that version's ARQ transport, SX1278 driver, USB bulk, AES-GCM, read/delivered receipts, and Android files byte-for-byte.** Only radio control/quality telemetry, automatic policy and the web UI were added. No new field throughput or range figures are claimed.

> **Release status: SOURCE CANDIDATE.** Host C/JavaScript tests, simulated ARQ loss injection and Chromium UI checks pass. **No full ESP-IDF 6.1 build or physical two-node automatic switch has been performed in this environment.** Keep the **user-confirmed working v1.6.1-rc.2 ZIP** as your rollback. A matched v1.7 firmware/web client is required on both nodes. See [automatic link adaptation and hardware acceptance](docs/V1_7_0_ADAPTIVE_RF.md).

## What works in source

- AES-256-GCM end-to-end encrypted text, local encrypted message history, authenticated delivered/read receipts and correct single/double/blue ticks. The radio only sees encrypted OGM4 ciphertext plus authenticated but visible RF metadata.
- Dedicated ESP32-C3 SX1278 LoRa worker and 433 MHz three-profile system. **Automatic adaptation is opt-in**, prefers Fast, uses both directions' authenticated RSSI/SNR and recent RF retries, and coordinates mode changes using the **proven authenticated one-click OFFER/AGREE + rollback**. Manual override still works; reboot recovers in manual Balanced.
- **Integrated v1.6 selective-repeat engine:** transfer encrypted ciphertext in one bounded USB bulk transaction; send up to six **224-byte ciphertext RF fragments** per SF7 burst (three in slow Max Range), then one HMAC-authenticated bitmap ACK; retransmit only missing fragments. RF frames are max **254 bytes** including header and 16-byte radio tag, within SX1278 FIFO's 255-byte limit.
- **Real encrypted LoRa speed test** in browser Settings (64–1024-byte probes, multiple trials, application goodput, median/p95 RTT, confirmed/unconfirmed). No fake raw-PHY throughput claims.
- USB Web Serial and existing NimBLE service preserved in firmware. BLE/Android app is **not connected to the new v1.6 bulk transport**, and voice/images/video remain disabled pending further reliability and throughput testing.
- New v1.6 authenticated peer-capability handshake: both radios must be updated; old v1.5 peers do not silently accept the incompatible new ARQ data frames.

## Automatic Fast-first LoRa modes, manual override and safety

| Mode | SX1278 settings | Output power | Status |
|---|---|---:|---|
| **Fast / Low** | SF7 · BW500 kHz · CR4/5 | nominal **+18 dBm** | Initiating user explicitly confirms **both** boards are suitable |
| **Balanced** (boot default) | SF7 · BW250 kHz · CR4/5 | **+10 dBm manual / nominal +18 dBm in Auto** | Extra link margin in Auto, safe manual default |
| **Max / Range** | SF9 · BW125 kHz · CR4/5 | nominal **+18 dBm** | Same two-board confirmation; deliberately slower but more link budget |

The Ai-Thinker Ra-02 may be sold as an 18 dBm nominal-output module, but that is **not proof of achievable or safe RF power on every board**. v1.6 uses the SX1278 high PA_DAC register path for an 18 dBm *requested* level. Before using Fast/Max, check power supply under TX load, correct 433 MHz antenna, module variant and local radio rules. For Fast or Max, select the mode on **one browser only**. The prompt asks the initiating operator to check **both** radios/antennas/power supplies and local RF regulations. The browser sends `POWER18 VERIFIED` locally when needed, then `PROFILE LOW` or `PROFILE MAX`; a 6-byte **HMAC-authenticated** control request carries that two-board confirmation to the peer. The receiver follows automatically, with no second local USB click. Authorization is a user declaration, **not measured RF hardware attestation**. Power authorization resets on reboot; `PROFILE BALANCED` returns to the safe default. The 22 dBm mode has been **removed**, not merely disabled.

**Using Auto:** after both v1.7 nodes discover one another, open **Settings → Automatic / manual LoRa modes → Enable automatic mode** on **one browser**. Confirm both antenna/power/regulatory checks. The signed `AUTO ON` command propagates to the peer, and only the elected coordinator (lower MAC) proposes subsequent changes. It compares fresh measurements from **both** receiving radios, lowers speed proactively when signal quality approaches the sensitivity limits, and uses hysteresis/cooldown to avoid rapid oscillation. **Manual profile buttons immediately override Auto**, and an `AUTO OFF` command disables automation on both peers. The optional automation is **not enabled automatically at boot**, because +18 dBm requires an operator check; both nodes recover in manual Balanced after a restart. In Auto, *Balanced* uses nominal +18 dBm to avoid losing 8 dB transmitter margin relative to Fast; manual Balanced stays +10 dBm. Read [thresholds, failure modes and acceptance test](docs/V1_7_0_ADAPTIVE_RF.md). There is no guaranteed distance boundary in metres.

A *lower* power setting may be preferable for short distances and to reduce front-end overload; the profile names reflect speed/range, not an ability to transmit faster by increasing output power.

## Hardware and wiring

Two ESP32-C3 Super Mini boards, two SX1278 Ra-02 433 MHz modules, two appropriate 433 MHz antennas and **stable regulated 3.3 V**. Never connect SX1278 VCC to 5 V. Check regulator capacity carefully before selecting +18 dBm; do not parallel two independent 3.3 V regulator outputs.

| Ra-02 | ESP32-C3 Super Mini |
|---|---|
| VCC (3.3 V) | Stable 3.3 V supply |
| GND | Common GND |
| NSS / CS | GPIO10 |
| SCK | GPIO6 |
| MOSI | GPIO7 |
| MISO | GPIO5 |
| RESET | GPIO3 |
| DIO0 | GPIO4 |
| DIO1 | GPIO0 |
| DIO2–DIO5 | Not connected |

## Build firmware and flash both ESP32s

Requires Linux or a supported desktop with **ESP-IDF 6.1** installed and the ESP-IDF environment initialized. Extract the full ZIP to a path **without spaces or parentheses** (some previous ESP-IDF generated build commands broke under paths with parentheses). In the repository root:

```bash
cd firmware
idf.py set-target esp32c3
idf.py build
# After build succeeds, flash both physical boards (ports are examples):
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM1 flash
```

**Do not run `erase-flash`** unless you intend to reset your stored shared pairing key. The firmware and web clients must all be **v1.7.0-rc.1**; the profile OFFER/AGREE frame is now 6 bytes and is incompatible with the older 5-byte profile-control format. Older v1.5 is not wire-compatible with this release's ARQ payloads.

## Run the encrypted web messenger

From the repository root, serve **this archive's `web/`** directory:

```bash
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open <http://127.0.0.1:8765> in a current desktop Chrome/Edge with Web Serial. Open a separate browser session/tab for each connected ESP32, grant USB access, and use the same 32-byte pairing key on both nodes. Check `READY` contains `FW=1.7.0-rc.1|ARQ=1|BULK=1|PROFILES=3|SYNC=ONE_CLICK|AUTORF=1` and, after discovery, `LINK` shows `PEER_ARQ=1`. Make a short text test before running bursts or switching modes.

The web application uses `web/lora_bulk.js` to stage AES-GCM ciphertext in bounded **four-command USB batches**, acknowledging each batch instead of each chunk; the ESP32 then owns RF 224-byte fragments. Normal radio ACKs mean RF reception only. The receiver browser must **decrypt and authenticate** the OGM4 envelope before issuing a delivered receipt. Read receipts depend on actual browser viewing. Missing application confirmations are shown as unconfirmed, never falsely delivered.

For comparisons, run Settings → **LoRa speed test**, ideally 8 × 1024-byte encrypted probes, in Balanced first. Record profile, goodput, p50/p95 RTT, confirmed/lost, `ARQ_RETX`, `ARQ_ACKTX`, `ARQ_TIMEOUT`, `ARQ_BADMAC`. The user measured **457.4 B/s, 1.95 s median RTT, 8/8 confirmed** on v1.6.1-rc.1 Balanced. v1.7.0-rc.1 retains the exact transport settings; measure manual Balanced again before trying automatic operation and distinguish throughput changes caused by mode switches from ARQ regressions.

## Version history and repository layout

| Milestone | Changes |
|---|---|
| v1.0.0 | New independent LoRa-only ESP32-C3/SX1278 firmware and authenticated encrypted web messenger, Android source frozen |
| v1.1.0-rc.1 | Correct TxDone vs delivery receipts, SPI DMA FIFO bursts, improved ACK and retry semantics |
| v1.2.0-rc.1 | Burst scheduler and LoRa speed test; first firmware channel-busy regression discovered |
| v1.2.0-rc.2 | Revert blocking pre-transmit CAD gate, restore reliable direct TX path |
| v1.5.0-rc.1 | Three manually coordinated modulation/range profiles and measured 1KB benchmarks; no v1.3/v1.4 published |
| **v1.6.0-rc.1** | Integrated bounded 224B selective-repeat ARQ, bitmap ACK, USB ciphertext staging, safe 18 dBm opt-in and disabled experimental +22; field benchmark showed slowdown |
| **v1.6.1-rc.1** | Four-line pipelined USB staging, six-frame SF7 bitmap windows, 25-ms interframe RX guard; user measured 457.4 B/s Balanced |
| **v1.6.1-rc.2** | Restore one-browser synchronized Fast/Balanced/Max switching; confirmed working by user |
| **v1.7.0-rc.1** | Opt-in automatic Fast-first adaptation with two-way authenticated RSSI/SNR, hysteresis, emergency downgrades, leader election and signed synchronized transitions; preserve v1.6.1-rc.2 transport |

`firmware/main/` holds the ESP-IDF startup, LoRa radio worker, USB and NimBLE integration. `firmware/components/offgrid_sx1278/` is the SPI driver. **`firmware/components/offgrid_arq/` is the new real firmware transport component**. `web/` contains the actual browser messenger. `android/` is preserved, **not v1.6 LoRa-compatible yet**. `firmware/tests/`, `tests/`, `docs/`, `.github/` contain host checks and instructions. `SOURCE_MANIFEST.sha256` pins tracked source integrity. The `.git` archive and portable Git bundle include the full LoRa project history.

## Quality, tests and known limits

Run portable regression tests from repository root:

```bash
bash firmware/tests/run-host-tests.sh
# Optional browser smoke test with Python Playwright and Chromium:
python3 tests/test_v15_web_smoke.py
```

These exercise actual ARQ bridge code on two simulated endpoints with deliberately lost DATA and bitmap ACK frames, 1KB/4KB/7168B ciphertext, browser CRC32 handling, message/receipt cryptography and SX1278 register choices. They **do not** certify that the two physical Ra-02 modules communicate, the ESP-IDF project builds, BLE supports v1.6 bulk traffic, or max PA operation is safe. See **[v1.6.1 targeted performance regression review](docs/V1_6_1_RC1_BOTTLENECK_FIX.md)**. Text-only and one transfer at a time is intentional; larger media/voice require streaming, scheduling and physical validation first.

Contributions: [CONTRIBUTING.md](CONTRIBUTING.md). Security: [SECURITY.md](SECURITY.md). Detailed changes: [CHANGELOG.md](CHANGELOG.md). License: [GPL-3.0](LICENSE). No GitHub push has been performed. Never publish raw pairing keys, NVS images or device secrets.
