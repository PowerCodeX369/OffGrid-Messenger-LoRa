# Firmware — current LoRa v3.0.6 source

**ESP-IDF 6.1, target `esp32c3`.** This firmware runs on each ESP32-C3 Super Mini attached to an SX1278/Ra-02 433 MHz radio. Its single LoRa worker owns the SPI radio; USB Serial/JTAG supports the desktop client and NimBLE serves the native Android client. See [architecture](../docs/ARCHITECTURE.md), [hardware connections](../docs/HARDWARE_SETUP.md) and [version compatibility](../docs/VERSIONS.md).

> The current Git tag `v3.0.6` identifies the feature-complete rc.3 source. A companion tag `v3.0.6-public.1` adds **documentation / GitHub workflow preparation only**; it does **not** change firmware. There is no claimed official prebuilt binary or physical certification. Preserve both boards' stored pairing keys and existing NVS.

## Requirements

- Two compatible ESP32-C3 boards, two SX1278 433 MHz modules, correctly matched antennas, stable regulated 3.3 V supplies, USB data cables.
- Installed and activated **ESP-IDF v6.1** (not generic Arduino IDE). Follow the [Espressif installation guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32c3/get-started/).
- Serial access permissions on Linux; identify each board's exact port before writing.
- Compliance checks for the target location: factory firmware is centered on **433.000 MHz** and nominal high-power mode requires user confirmation. This frequency is **not** declared universally permitted.

## Build and flash both boards

From the root directory:

```bash
cd firmware
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor   # example: first node
# Stop monitor (Ctrl+]) and repeat with the second board's actual port.
idf.py -p /dev/ttyACM1 flash monitor
```

Do **not** run `erase-flash`, wipe `nvs`, or use a different partition table for an existing keyed board without planning migration. The configured partition table contains a 1.5 MiB factory app (`partitions.csv`) and **no OTA partition**. ESP-IDF will report if the compiled image does not fit.

### Confirm both nodes use compatible firmware

Read USB diagnostics/`STATUS` or client diagnostics. A current node should report capability markers including:

```text
FAMILY=LORA
PROFILES=4
PROFILE_SYNC=ULTRA_SLOTS4
AUTO_SCAN=TRIBAND
AUTO_RSSI=-100,-130
ACK_COMPACT=16
ARQ_CAP=4
```

**Note:** `FW=1.8.0-rc.4` is deliberately retained as a *legacy wire-compatibility marker*; it is **not** the current Git source version. Identify rc.3 through the markers above. Both boards must support authenticated ARQ capability 4 for Max/Ultra; mixing older builds can make messages fail.

## Operator controls and behavior

Boot mode is **manual Balanced**, not Auto. After confirming the antenna, adequate supply and local legal permissions, choose Auto once on Android or web to send `POWER18 VERIFIED` as appropriate, followed by authenticated `AUTO ON`. This authorization is **not persisted** across restarts. The peer must agree to synchronized changes.

- Fast: SF7/BW500; manual and negotiated temporary media use.
- Balanced: SF7/BW250; normal boot mode.
- Max: SF9/BW125; weak-signal operation and media supported.
- Ultra: SF12/BW125/CR4/8; **encrypted text/receipts only** in official clients.
- Auto: `RSSI > -100` → Balanced, `-130 <= RSSI <= -100` → Max, `RSSI < -130` → Ultra, plus best-effort lost-peer searches. There is no instantaneous RSSI measurement without a decoded RF packet.

Do not infer successful delivery from radio TX acknowledgment; application delivery requires authenticated end-to-end receipt processing.

## Tests and troubleshooting

```bash
cd ..  # if still in firmware/
bash firmware/tests/run-host-tests.sh
bash android/tests/run_native_radio_tests.sh
```

See [hardware acceptance](../docs/TESTING.md), [diagnostic troubleshooting](../docs/TROUBLESHOOTING.md), and the [exact rc.3 transition procedure](../docs/V3_0_6_RC3_RSSI_BANDS_AND_AUTORECOVERY.md). **Host tests cannot validate SX1278 RF timings or flashing**.

Detailed historical firmware release instructions are preserved in [`docs/history/FIRMWARE_README_BEFORE_PUBLICATION.md`](../docs/history/FIRMWARE_README_BEFORE_PUBLICATION.md); use Git tags for corresponding old source.
