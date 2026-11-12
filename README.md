# OffGrid Messenger LoRa

**v1.6.1-rc.2 — restored working v1.6.1 transport, one-click synchronized manual profiles, three modes**

Independent SX1278/Ra-02 + ESP32-C3 Super Mini firmware, browser messenger, source/test history, and preserved original Android application. Maintained under PowerCodeX. This is **LoRa-only** firmware: no Wi-Fi or ESP-NOW initialization; the original ESP-NOW repository is separate and unchanged.

> **Measured working baseline:** The user's v1.6.1-rc.1 **Balanced** benchmark transferred eight 1,024-byte encrypted tests at **457.4 bytes/s, 1.95 s median RTT, 8/8 confirmed**. Seven trials reported zero retransmissions, while one trial required two and took 3.39 seconds. This v1.6.1-rc.2 update **does not modify the working ARQ, SX1278 driver, USB staging, AES encryption or receipt engines**. It fixes manual profile synchronization only; there is **no claimed throughput increase**.

> **Release status: SOURCE CANDIDATE.** Host C/JavaScript tests, simulated ARQ loss injection and browser UI checks pass. **No ESP-IDF build or live two-radio rc.2 synchronization test was completed here.** Keep the v1.6.1-rc.1 firmware/web archive as your known-working rollback, not v1.6.2/v1.6.3. See [v1.6.1-rc.2 synchronization repair and acceptance](docs/V1_6_1_RC2_ONE_CLICK_SYNC.md).

## What works in source

- AES-256-GCM end-to-end encrypted text, local encrypted message history, authenticated delivered/read receipts and correct single/double/blue ticks. The radio only sees encrypted OGM4 ciphertext plus authenticated but visible RF metadata.
- Dedicated ESP32-C3 SX1278 LoRa worker and 433 MHz manual three-profile system. Manual, authenticated peer offer/agree and timed rollback remain; reboot returns to Balanced.
- **Integrated v1.6 selective-repeat engine:** transfer encrypted ciphertext in one bounded USB bulk transaction; send up to six **224-byte ciphertext RF fragments** per SF7 burst (three in slow Max Range), then one HMAC-authenticated bitmap ACK; retransmit only missing fragments. RF frames are max **254 bytes** including header and 16-byte radio tag, within SX1278 FIFO's 255-byte limit.
- **Real encrypted LoRa speed test** in browser Settings (64–1024-byte probes, multiple trials, application goodput, median/p95 RTT, confirmed/unconfirmed). No fake raw-PHY throughput claims.
- USB Web Serial and existing NimBLE service preserved in firmware. BLE/Android app is **not connected to the new v1.6 bulk transport**, and voice/images/video remain disabled pending further reliability and throughput testing.
- New v1.6 authenticated peer-capability handshake: both radios must be updated; old v1.5 peers do not silently accept the incompatible new ARQ data frames.

## Manual LoRa modes and safety

| Mode | SX1278 settings | Output power | Status |
|---|---|---:|---|
| **Fast / Low** | SF7 · BW500 kHz · CR4/5 | nominal **+18 dBm** | Initiating user explicitly confirms **both** boards are suitable |
| **Balanced** (boot default) | SF7 · BW250 kHz · CR4/5 | **+10 dBm** | Safe starting mode for RF testing |
| **Max / Range** | SF9 · BW125 kHz · CR4/5 | nominal **+18 dBm** | Same two-board confirmation; deliberately slower but more link budget |

The Ai-Thinker Ra-02 may be sold as an 18 dBm nominal-output module, but that is **not proof of achievable or safe RF power on every board**. v1.6 uses the SX1278 high PA_DAC register path for an 18 dBm *requested* level. Before using Fast/Max, check power supply under TX load, correct 433 MHz antenna, module variant and local radio rules. For Fast or Max, select the mode on **one browser only**. The prompt asks the initiating operator to check **both** radios/antennas/power supplies and local RF regulations. The browser sends `POWER18 VERIFIED` locally when needed, then `PROFILE LOW` or `PROFILE MAX`; a 6-byte **HMAC-authenticated** control request carries that two-board confirmation to the peer. The receiver follows automatically, with no second local USB click. Authorization is a user declaration, **not measured RF hardware attestation**. Power authorization resets on reboot; `PROFILE BALANCED` returns to the safe default. The 22 dBm mode has been **removed**, not merely disabled.

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

**Do not run `erase-flash`** unless you intend to reset your stored shared pairing key. The firmware and web clients must all be **v1.6.1-rc.2**; the profile OFFER/AGREE frame is now 6 bytes and is incompatible with the older 5-byte profile-control format. Older v1.5 is not wire-compatible with this release's ARQ payloads.

## Run the encrypted web messenger

From the repository root, serve **this archive's `web/`** directory:

```bash
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open <http://127.0.0.1:8765> in a current desktop Chrome/Edge with Web Serial. Open a separate browser session/tab for each connected ESP32, grant USB access, and use the same 32-byte pairing key on both nodes. Check `READY` contains `FW=1.6.1-rc.2|ARQ=1|BULK=1|PROFILES=3|SYNC=ONE_CLICK` and, after discovery, `LINK` shows `PEER_ARQ=1`. Make a short text test before running bursts or switching modes.

The web application uses `web/lora_bulk.js` to stage AES-GCM ciphertext in bounded **four-command USB batches**, acknowledging each batch instead of each chunk; the ESP32 then owns RF 224-byte fragments. Normal radio ACKs mean RF reception only. The receiver browser must **decrypt and authenticate** the OGM4 envelope before issuing a delivered receipt. Read receipts depend on actual browser viewing. Missing application confirmations are shown as unconfirmed, never falsely delivered.

For comparisons, run Settings → **LoRa speed test**, ideally 8 × 1024-byte encrypted probes, in Balanced first. Record profile, goodput, p50/p95 RTT, confirmed/lost, `ARQ_RETX`, `ARQ_ACKTX`, `ARQ_TIMEOUT`, `ARQ_BADMAC`. The user measured **457.4 B/s, 1.95 s median RTT, 8/8 confirmed** on v1.6.1-rc.1 Balanced. v1.6.1-rc.2 retains the exact transport settings; measure again to confirm no regression.

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
| **v1.6.1-rc.2** | Restore one-browser synchronized Fast/Balanced/Max switching, signed two-board power confirmation, peer-busy response; preserve rc.1 transport unchanged |

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
