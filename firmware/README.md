# OffGrid Messenger LoRa — ESP-IDF firmware v1.6.0-rc.1

The full ESP32-C3 **LoRa-only** firmware is here, including the main radio owner (`main/main.c`), SX1278 SPI driver (`components/offgrid_sx1278`), authenticated selective-repeat transport (`components/offgrid_arq`), NimBLE bridge, USB Web Serial, and tests. It is **not** a standalone C prototype.

## Compile / flash

Use **ESP-IDF 6.1** in an initialized shell, from an extracted project path without spaces or parentheses:

```sh
cd firmware
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash
# Switch port for the second board
idf.py -p /dev/ttyACM1 flash
```

**Do not `erase-flash`**, as that erases the stored AES pairing key. No Wi-Fi or ESP-NOW startup. The radio is a dedicated one-owner task; USB and BLE/NimBLE are present. The app/Android BLE transport is not yet integrated with v1.6 ARQ ciphertext staging.

## Power and radio modes

Balanced SF7/BW250/+10 dBm is the boot profile. Fast SF7/BW500/+18 dBm (opt-in) and Max SF9/BW125/+18 dBm (opt-in) use the module-rated nominal 18 dBm high PA_DAC policy after verifying the radio's power supply/antenna/legal limits. **Each board** must explicitly receive `POWER18 VERIFIED` over its own physical USB session before either high-power profile can be selected. `POWER18 OFF` disables future high-power switching once already back in Balanced. The fourth Experimental +22 dBm option is **disabled and rejected** as unsupported by SX1278. Original manual profile OFFER/AGREE and rollback are preserved.

## New v1.6 bulk protocol

`BULKBEGIN <id16> <size> DATA|CTRL`, `BULKCHUNK <id16> <seq> <hex>`, `BULKEND <id16> <crc8>` accept AES-256-GCM ciphertext from the current `web/` client. USB staging replies `BULKREADY`, `BULKSTEP`, and `BULKQUEUED`; radio worker emits `BULKSENT`, `BULKPROGRESS`, `BULKCONFIRMED`, or `BULKFAILED`. Receiver returns `RXBSTART`, `RXBCHUNK`, `RXBDONE` for encrypted browser-side reassembly and AES-GCM authentication. CRC32 is for USB corruption detection, not security. 224-byte ciphertext RF data, 4-fragment window, authenticated bitmap ACK, missing-only retransmission and a 7168-byte transfer maximum.

Both firmware endpoints must announce a valid HMAC-authenticated `OG_TYPE_ARQ_CAPS` before using the new wire data framing. Do not mix v1.5 and v1.6. See [full integration/acceptance notes](../docs/V1_6_0_ARQ_INTEGRATION.md).

## Development tests

Run `bash firmware/tests/run-host-tests.sh` from repo root; it includes full C ARQ bridge simulations, intentionally lost frames/bitmap ACKs, web protocol tests, power checks and source checks. Optional `python3 tests/test_v15_web_smoke.py` requires Chromium and Playwright. **No full ESP-IDF build or real-board benchmark was performed in the authoring environment.** Keep v1.5 as a recovery baseline until both hardware devices pass acceptance.
