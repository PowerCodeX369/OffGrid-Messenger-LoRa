# OffGrid Messenger LoRa

**v1.5.0-rc.1 — three manual LoRa profiles and RF-throughput improvements**

Offline, peer-to-peer **end-to-end encrypted text** on two ESP32-C3 Super Mini boards and SX1278 Ra-02 modules. This independent repository is derived from the original [OffGrid Messenger (ESP-NOW)](https://github.com/PowerCodeX369/OffGrid-Messenger). The original ESP-NOW firmware remains separate and untouched.

> **Verification:** Host C/JS tests and Chromium UI checks passed, but a complete ESP-IDF 6.1 build and two-board profile negotiation have **not** been performed here. This is a release candidate, **not** a hardware-certified stable build. Keep your last known-working firmware and web archive until real tests pass. Changes must be flashed to **both ESP32s**; v1.2 and v1.5 use different startup radio settings and are incompatible over the air.

## Features and restrictions

- **LoRa only:** no Wi-Fi, ESP-NOW, network service, cloud or internet; BLE/NimBLE and USB Web Serial remain in firmware.
- The web app retains **AES-256-GCM** message encryption and encrypted local history. Radio DATA, HELLO, radio ACK and profile-control frames carry a **128-bit truncated HMAC-SHA256** computed with a separately derived pairing key. The same 256-bit pairing key must be present on both nodes.
- **Manual mode change** in *Settings → Manual LoRa profiles*. Only a user click initiates a new profile; the peer participates via authenticated control messages. There is **no automatic RSSI-driven mode selection**.
- One SX1278 owner task handles SPI2, RX, TxDone and radio ACKs. Encrypted delivery/read receipts have high priority in the existing browser scheduler.
- 433 MHz prototype configuration (check local operating rules). Do not transmit without a correctly matched antenna. **Power-off before changing wires**.
- **Disabled:** photos, voice/video, Android LoRa integration, mesh routing, adaptive power control, selective-repeat/bitmap ARQ. Android source is frozen for the final universal-app phase.

## Three manually chosen profiles

| Web label | SX1278 configuration | Expected tradeoff |
|---|---|---|
| **Low Power / Fast** | SF7 · 500 kHz · CR 4/5 · **+2 dBm** | Low TX power, shortest airtime, shortest range/sensitivity |
| **Balanced** (startup default) | SF7 · 250 kHz · CR 4/5 · **+10 dBm** | Better margin with roughly half Low's nominal PHY rate |
| **Max Power / Range** | SF9 · 125 kHz · CR 4/5 · **+17 dBm** | Higher sensitivity and output power; **slower** data rate |

At SF7/BW500 the nominal raw PHY bit rate is around **21.9 kbit/s**, versus around **10.9 kbit/s** for SF7/BW250 and **1.76 kbit/s** for SF9/BW125 (4/5). These are modulation estimates, **not measured application throughput**; on-air packet framing, ACKs, encryption overhead and retries reduce usable goodput. High output power does **not** make transmission faster. The +17 dBm limit is the **normal PA_BOOST** limit in this driver; Semtech's separate +20 dBm PA_DAC mode is **not enabled** and is not assumed safe for an unverified Ra-02 breakout/regulator. See `docs/V1_5_0_MANUAL_PROFILES.md`.

Both devices power up in **Balanced**. To change modes, keep both connected and paired, wait for pending messages to finish, then select a mode in one browser. The requesting node sends a **HMAC-authenticated offer** on the current channel. The peer agrees and both switch after a short guard interval. They exchange authenticated HELLO frames on the new profile and return `PROFILE|ACTIVE` only when verified. An unconfirmed change returns to the previous mode after a timeout. For diagnostics a `PROFILE <MODE> FORCE` USB command can mark pending messages **unconfirmed** rather than silently claiming delivery; avoid force during regular use. A restart returns to Balanced, providing a recovery route if peers become desynchronized.

## Wiring

| Ra-02 | ESP32-C3 Super Mini |
|---|---|
| 3.3 V | Clean regulated 3.3 V — never 5 V |
| GND | GND, common ground |
| NSS | GPIO10 |
| SCK | GPIO6 |
| MOSI | GPIO7 |
| MISO | GPIO5 |
| RST | GPIO3 |
| DIO0 | GPIO4 |
| DIO1 | GPIO0 |
| DIO2–DIO5 | Not connected |

The SX1278 **draws more power in high-power transmission**. On some Super Mini variants the board's 3.3 V output may be marginal; verify the regulator and 3.3 V at the radio under load before using Max Power. Never connect two independent 3.3 V regulator outputs together; use common GND and prevent back-power through GPIOs.

## Building on Linux using ESP-IDF 6.1

Keep the project path free of spaces and parentheses; ESP-IDF shell generation failed on paths containing `(1)` in earlier tests.

```sh
cd ~/Projects/OffGrid-Messenger-LoRa/firmware
idf.py set-target esp32c3
idf.py build
```

After a successful build, flash each board (actual ports may differ):

```sh
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM1 flash
```

Do **not** use `erase-flash` unless you intend to lose NVS pairing keys. Pair the two boards with the same encryption key.

From the repository root serve the new web client locally:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open **http://127.0.0.1:8765** in a current desktop Chrome/Edge. Connect each ESP32 in its own browser session. A refreshed page requires a new serial connection if the browser cannot unambiguously recover permission. Use the included v1.5 browser files on **both** nodes.

Check `READY|...|FW=1.5.0-rc.1|PROFILES=3|ACKENGINE=2|FLOW=2` and `PROFILE|ACTIVE|BALANCED` at startup. The USB commands are `STATUS`, `PROFILE LOW`, `PROFILE BALANCED`, `PROFILE MAX`, and optional `PROFILE MAX FORCE`. The firmware sends progress events `PROFILE|NEGOTIATING`, `PROFILE|AGREED`, `PROFILE|VERIFYING`, `PROFILE|ACTIVE`, `PROFILE|ROLLED_BACK` or `PROFILE|ERROR`.

## Delivery semantics and benchmarking

- **One gray tick**: every message fragment completed local SX1278 TxDone.
- **Two gray ticks**: an authenticated, encrypted *delivered* receipt returned from the recipient application.
- **Two blue ticks**: an authenticated, encrypted *read* receipt returned after the recipient viewed the chat.
- A lost receipt means **unconfirmed**, never a false success claim. The radio ACK is not equivalent to application delivery.
- In **Settings → LoRa speed test**, run 4/8/12 encrypted 64–1024 byte probes and compare median/p95 RTT, confirmed fraction and effective goodput **for each profile**. This is *application* throughput, not a PHY benchmark.

The previous user-measured SF7/BW125 result was approximately **227.8 B/s goodput and 4.45 s median confirmed RTT** for 1,024-byte encrypted probes at 1 metre. The new profiles are intended to improve on that reference, but **no actual v1.5 field measurement exists yet**. Compare all three profiles on the same hardware and distance, initially using Low and Balanced rather than maximum TX power at very short range.

## Repository layout

- `firmware/main/` — authenticated OGM2 frame worker, manual profile handshake, native USB, existing BLE bridge
- `firmware/components/offgrid_sx1278/` — SPI2 driver and pure host-testable SX1278 register policy
- `web/` — encrypted chat, receipts, mode selection, throughput test
- `firmware/tests/`, `tests/` — host regression and boundary tests (not hardware certification)
- `android/` — original Android app unchanged; **not LoRa-compatible yet**
- `docs/` — profile protocol, hardware acceptance, provenance and historical diagnostics
- `SOURCE_MANIFEST.sha256` — integrity checksums for tracked repository files (excluding the manifest itself)

Run available host tests with `firmware/tests/run-host-tests.sh`; mock-header C checks are **not a substitute** for an ESP-IDF firmware build. Git history preserves `v1.0.0`, `v1.1.0-rc.1`, and both v1.2 candidates. v1.3 and v1.4 were **not released**; version 1.5 was requested directly and is a prerelease candidate.

This project is licensed under the included [GPL-3.0 license](LICENSE). Review [security notes](SECURITY.md), [changelog](CHANGELOG.md), and [acceptance instructions](docs/V1_5_0_MANUAL_PROFILES.md) before a field deployment.
