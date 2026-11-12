# OffGrid Messenger LoRa

**Version 1.2.0-rc.1 — LoRa burst reliability and encrypted speed-test candidate**

Offline, direct peer-to-peer **encrypted text** over two SX1278 Ra-02 transceivers, controlled by a local Chromium Web Serial interface. The firmware **does not initialize Wi-Fi or ESP-NOW**. NimBLE and native USB Serial/JTAG are enabled. This project is a separate derivative of the original [OffGrid Messenger (ESP-NOW)](https://github.com/PowerCodeX369/OffGrid-Messenger), not its replacement.

> **Validation status:** Host-side C and JavaScript regression checks passed. The new integrated firmware has **not** been compiled with a real ESP-IDF 6.1 toolchain or run on two physical ESP32s in the authoring environment. **This v1.2.0 candidate is not yet a hardware-validated release.** Do not count links or ACKs as proven until both boards pass the acceptance checklist.

## Version 1.2 scope

| Included | Not included yet |
|---|---|
| ESP32-C3 + Ra-02 fixed 433 MHz/SF7/BW125/CR4/5 profile | ESP-NOW, Wi-Fi, hybrid switching |
| Physical USB web chat, text and encrypted local history | Photo/voice sending (later releases) |
| Browser AES-256-GCM OGM4 envelopes | Adaptive LoRa Fast/Balanced/Ultra profiles |
| Authenticated LoRa OGM2 packets and startup discovery | Multihop mesh, range claims, automatic RF tuning |
| SX1278 radio ACK, application-delivery receipt, read receipt | Android integration (last phase) |
| Bounded retransmissions, duplicate suppression, ACK priority | Full selective-repeat large-media transport |
| BLE service, with authenticated future capability handshake | Fully tested BLE/Android interoperability |

### Hardware connections

**Before powering the prototype:** disconnect USB while changing leads, use **3.3 V only**, share ground, fit a matched **433 MHz antenna before transmitting**, keep leads short, and check supply voltage. A Ra-02 at transmit power can overload a marginal ESP32-C3 Super Mini regulator; using an adequately rated regulated 3.3 V supply for the radio (with shared GND) is recommended if the voltage sags. Do not connect independent 3.3 V regulator outputs together.

| SX1278 pin | ESP32-C3 Super Mini | Purpose |
|---|---|---|
| VCC | 3.3 V regulated | Never 5 V |
| GND | GND | Common ground |
| NSS | GPIO10 | SPI chip-select |
| SCK | GPIO6 | SPI clock |
| MOSI | GPIO7 | SPI write |
| MISO | GPIO5 | SPI read |
| RST | GPIO3 | SX1278 reset |
| DIO0 | GPIO4 | Radio interrupt |
| DIO1 | GPIO0 | RX timeout interrupt |
| DIO2–DIO5 | Not connected | Not needed in v1.2 |

Current build uses SPI2 at 4 MHz and the same previously tested pin map. **433 MHz legality, allowed transmit power, and duty-cycle restrictions depend on your country and usage; verify them before RF tests.** Default transmission is **10 dBm** (not the module's advertised maximum).

## Linux / ESP-IDF build

Use an absolute path without spaces or parentheses, e.g. `~/Projects/OffGrid-Messenger-LoRa` (previous ESP-IDF shell commands failed on an unescaped `(1)` directory).

```bash
# Activate your already installed ESP-IDF 6.1 environment if needed:
source "$HOME/.espressif/v6.1/esp-idf/export.sh"  # adjust to YOUR actual IDF_PATH
cd ~/Projects/OffGrid-Messenger-LoRa/firmware
idf.py set-target esp32c3
idf.py build
```

Check that `idf.py --version` and `echo "$IDF_PATH"` point to the intended environment. If the build succeeds, connect an antenna and **flash the two test boards separately**, using their actual ports:

```bash
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM1 flash
```

Do **not** use `erase-flash` unless you intentionally want to erase NVS including your pairing keys. Close serial monitors before flashing. Inspect output with `idf.py -p /dev/ttyACM0 monitor` or the browser (not both concurrently).

### Start the web client

From the repository root:

```bash
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open **http://127.0.0.1:8765** in Chrome/Edge. Open separate browser windows for the two USB-attached nodes. Connect each device and set the **same 256-bit pairing key** on both. Create the key on one node, copy it locally and install it on the other (key export is restricted to the physical USB connection). The LoRa transmitter will not send user data before a pairing key is configured. The firmware reports `READY|...|FAMILY=LORA|FW=1.2.0-rc.1|FLOW=2`, and the driver reports `RADIO|SX1278_READY` after successful initialization.

**Do not confuse:** USB `Connected` with a working radio peer. `LINK` shows authenticated RX and radio ACK counters. See [hardware acceptance](docs/HARDWARE_ACCEPTANCE.md).

## Protocol and delivery semantics

- The browser uses the original OGM4 **AES-256-GCM** transfer protocol. Chat content is encrypted *before* being sent to the ESP32.
- Over-the-air LoRa frames additionally carry a **truncated 128-bit HMAC-SHA256** tag derived from the NVS pairing key using a domain-separated label. This authenticates the radio header, HELLO, ACK and encrypted data without adding a second plaintext channel.
- **Single gray tick** means every fragment completed SX1278 TxDone. This is not a receiver acknowledgment.
- `RADIOACK` means the destination firmware authenticated a frame. This advances fragment transfer without waiting for the browser's separate application receipt.
- The receiving browser sends its USB-only `APPACK` *after complete AES-GCM decryption*. The firmware does **not** send a redundant fragment-delivered radio packet.
- **Double gray** comes only from an encrypted delivered receipt, **double blue** only from an encrypted read receipt after the recipient actually views the chat. A lost receipt leaves an explicit *unconfirmed* state; it is not proof of non-delivery.
- `CTRLHEX` carries encrypted delivered/read controls at higher radio queue priority; at most one unacknowledged LoRa radio frame is transmitted at a time. A longer end-to-end watchdog is distinct from the radio ACK timeout.
- SX1278 modem-status polling checks for an ongoing reception before best-effort CAD; short randomized deferrals reduce avoidable half-duplex collisions but cannot prevent all collisions.
- Delivered/read receipts are AES-GCM-encrypted priority batches of up to eight receipt entries; each entry still has a distinct message ID and read/delivery meaning.
- The web client has a **real encrypted LoRa speed test** (64–1024-byte probes) under Settings. It reports application goodput, median/p95 RTT, and confirmed versus unconfirmed trials; it does not claim raw RF throughput.
- SPI FIFO bursts require SPI DMA on ESP-IDF. The maximum 218-byte frame has host emulation tests; hardware build verification is pending.
- The LoRa firmware is direct P2P and learns an authenticated peer MAC at runtime; no hardcoded board MAC or saved ESP-NOW peer routing is required.
- Replays are rejected at the recent packet/transfer level, but the LoRa application has **not received an independent security audit**. A comprehensive persistent replay strategy is planned for v1.8.0.

### What about the Android app?

`android/` is intentionally copied **byte-for-byte from original OffGrid Messenger commit `263502f`** and remains an ESP-NOW-generation app in v1.2. It is **not** a LoRa-compatible Android app yet; the required universal firmware-family detection and LoRa capability UI are planned for v2.0.0. See [Android preservation hashes](docs/ANDROID_FROZEN_SHA256.txt).

## Layout

- `firmware/main/`: pure-LoRa task, authenticated packet/control handling, NVS key, native USB and inherited NimBLE service
- `firmware/components/offgrid_sx1278/`: SPI2 transceiver driver and independently testable profile settings
- `web/`: encrypted web UI and OGM4 codec adapted for text-only LoRa
- `android/`: **untouched** original Android app for later universal support
- `firmware/tests/`, `tests/`: host checks; **not** radio hardware certification
- `docs/`: architecture, pinout, RF test acceptance, security limitations, roadmap, provenance

Run host checks: `firmware/tests/run-host-tests.sh`. See [v1.2.0 burst reliability and acceptance plan](docs/V1_2_0_BURST_RELIABILITY.md) and [v1.1.0 engineering notes](docs/V1_1_0_RELIABILITY.md), [release notes](CHANGELOG.md), and [known limitations](docs/HARDWARE_ACCEPTANCE.md).

## Source provenance, licensing, and independence

This repository was freshly created for the LoRa-only firmware. The web OGM4 encryption/UX and NimBLE service are derived from the original PowerCodeX project; the SX1278 register driver is adapted from the earlier Hybrid experimental development, **not** its problematic dual-radio transport. Original project reference commit: `263502f`. The original ESP-NOW repository remains independent and has not been modified or pushed by this work. The new repository is distributed under [GPL-3.0](LICENSE) on the understanding that the original copyright holder authorizes this derivative; contributors must retain applicable notices.

**No GitHub publishing was performed automatically.** After your own build, hardware acceptance, and review, you may create `PowerCodeX369/OffGrid-Messenger-LoRa` and push this repo and the annotated `v1.0.0` and prerelease tags after your field tests.
