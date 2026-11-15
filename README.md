# OffGrid Messenger LoRa

**A two-node, offline, end-to-end encrypted LoRa messenger with a native Android app and desktop web client.** No internet connection or server is required to exchange messages between the paired nodes.

> **Project status — v3.0.6, feature complete / field beta.** The software is available for community use and contribution, **not certified for emergency use or commercially qualified**. The maintainer reports working automatic range switching and roughly **200–250 m in one urban Max Range test**; this is a user observation, not a guaranteed range. ESP-IDF target builds, Android APK builds, security review, multi-device endurance, radio compliance and reproducible performance tests remain release qualification items. See [known limitations](docs/KNOWN_LIMITATIONS.md) and [testing](docs/TESTING.md).
>
> **The first operation for an existing installation is to back up the pairing keys, NVS and current app data.** Do not `erase-flash`; upgrade **both ESP32s and both client copies** to the same compatibility family. **Auto does not start by default**: enable it on one client after the explicit RF-power safety confirmation.

## At a glance

| Area | Current capability |
| --- | --- |
| Hardware | Two ESP32-C3 Super Mini boards and SX1278/Ra-02 433 MHz radios |
| Radio | Direct LoRa peer-to-peer; **not** a multi-hop mesh router |
| Clients | Native Android (BLE GATT) and desktop Chromium/Edge (USB Web Serial) |
| Messaging | Application AES-256-GCM, authenticated RF packets, encrypted receipts, retryable text |
| Media | Encrypted compressed photos and voice in supported non-Ultra modes |
| Modes | Fast (SF7/BW500), Balanced (SF7/BW250), Max (SF9/BW125), Ultra (SF12/BW125, text/receipts only) |
| Automatic RF | Authenticated two-peer synchronized switching driven by received LoRa RSSI; recovery scan after silence |
| Power and updates | Operator-authorized nominal +18 dBm modes; single-factory firmware partition without OTA |

### Automatic range policy

Only when **Auto is enabled** and the required power authorization is given:

| Fresh authenticated radio RSSI | Target profile |
| --- | --- |
| Greater than **−100 dBm** | **Balanced** |
| **−130 to −100 dBm**, inclusive | **Max** |
| Less than **−130 dBm** | **Ultra** |

The same mapping applies as the signal improves. **Fast is not removed**: it remains a manual option and is used as a temporary media accelerator when negotiated. The radios must still authenticate and synchronize a profile transition. When packets stop decoding, there is **no new RSSI**; recovery uses provisional profile scanning and is not guaranteed if the peer is out of range. See the [latest policy and field procedure](docs/V3_0_6_RC3_RSSI_BANDS_AND_AUTORECOVERY.md).

## Start here

1. Read [hardware, wiring and RF safety](docs/HARDWARE_SETUP.md) before powering a radio. Confirm the chosen **frequency, bandwidth, antenna, transmit power and airtime comply with local rules**; the default center frequency is **433.000 MHz**, which must **not** be assumed legal in every country.
2. Follow [firmware installation](firmware/README.md) to compile and flash the **same current source** on both ESP32-C3 devices. Do **not** erase NVS or assume a binary is included.
3. Choose the [web client](web/README.md) for desktop USB/Web Serial, or [native Android client](android/README.md) for BLE pairing. Two clients must share the same provisioned 32-byte pairing key; keep it private.
4. Establish a close-range link and confirm **authenticated, decrypted short text and delivered receipts in both directions** before attempting media or long-range switching.
5. To enable automatic switching, select **Auto** on one app, accept the antenna/power/legal confirmation, then verify that **both devices** report the same active profile. Auto is deliberately **off after restart** and is not a saved default.

**Fast reference (Linux / existing ESP-IDF 6.1 installation):**

```bash
cd firmware
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor   # example first device; stop monitor before next command
idf.py -p /dev/ttyACM1 flash monitor   # example second device
```

**Desktop client (from repository root in another terminal):**

```bash
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
# Visit http://127.0.0.1:8765 in Chrome/Edge and choose Connect ESP32.
```

The Linux device names are examples. Check with `ls /dev/ttyACM* /dev/ttyUSB*` and confirm which port belongs to each device. See the component guides before flashing or provisioning keys.

## Repository map

| Location | Purpose |
| --- | --- |
| [`firmware/`](firmware/) | ESP-IDF 6.1 ESP32-C3 application, SX1278 driver, BLE bridge, ARQ and RF profile controller |
| [`android/`](android/) | Native Kotlin/Jetpack Compose client with BLE, encrypted history, text and media |
| [`web/`](web/) | Browser UI, encryption, USB transport, diagnostics and IndexedDB store |
| [`tests/`](tests/), [`firmware/tests/`](firmware/tests/), [`android/tests/`](android/tests/) | Host regression and protocol tests |
| [`docs/`](docs/) | Current guides, diagrams, compatibility, security and version history |
| [`CHANGELOG.md`](CHANGELOG.md) | Chronological changes |
| [`.github/`](.github/) | Automated checks, issues and pull-request templates |

## Builds and checks

```bash
bash firmware/tests/run-host-tests.sh
bash android/tests/run_native_radio_tests.sh
sha256sum -c SOURCE_MANIFEST.sha256
git diff --check
git fsck --no-reflogs
```

Host checks **do not equal hardware acceptance**. GitHub Actions also attempts ESP-IDF and Android target builds; those checks must actually pass on GitHub before claiming build-qualified binaries. See [developer setup](docs/DEVELOPMENT.md) and [test plan](docs/TESTING.md).

## Documentation and support

- [Documentation index](docs/README.md), [architecture](docs/ARCHITECTURE.md), [versions and compatibility](docs/VERSIONS.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md), [known limitations](docs/KNOWN_LIMITATIONS.md), [release checklist](docs/RELEASE_CHECKLIST.md)
- [Contributing](CONTRIBUTING.md), [security disclosure](SECURITY.md), [support](SUPPORT.md), [code of conduct](CODE_OF_CONDUCT.md)
- [Original ESP-NOW repository / attribution](docs/PROVENANCE.md)

Historical documents are intentionally preserved in `docs/` and `docs/history/` for studying past versions. **Do not use old version-specific instructions to install the current release.** To inspect any historical source version, check out its Git tag (prefer a separate working tree; see [versions](docs/VERSIONS.md)).

## License

[GPL-3.0](LICENSE). Follow its terms when redistributing modified source or binaries. The bundled source is provided without warranty; compliance with local radio regulations and safe hardware assembly remain the operator's responsibility.
