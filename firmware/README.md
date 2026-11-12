# OffGrid Messenger LoRa — ESP-IDF firmware v1.6.1-rc.2

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

Balanced SF7/BW250/+10 dBm is the boot profile. Fast SF7/BW500/+18 dBm (opt-in) and Max SF9/BW125/+18 dBm (opt-in) use the module-rated nominal 18 dBm high PA_DAC policy after verifying the radio's power supply/antenna/legal limits. For high-power profiles, the initiating user confirms BOTH devices in the v1.6.1-rc.2 browser. One USB device receives `POWER18 VERIFIED`, then `PROFILE LOW` or `PROFILE MAX`, and forwards the authenticated request containing that confirmation to the other device. The receiving ESP32 follows automatically without its own USB command. `POWER18 OFF` disables future high-power switching once already back in Balanced. There is no Experimental +22 dBm mode; it was removed as unsupported by SX1278. Original manual profile OFFER/AGREE and rollback are preserved.

## New v1.6 bulk protocol

`BULKBEGIN <id16> <size> DATA|CTRL`, `BULKCHUNK <id16> <seq> <hex>`, `BULKEND <id16> <crc8>` accept AES-256-GCM ciphertext from the current `web/` client. USB staging replies `BULKREADY`, `BULKSTEP`, and `BULKQUEUED`; radio worker emits `BULKSENT`, `BULKPROGRESS`, `BULKCONFIRMED`, or `BULKFAILED`. Receiver returns `RXBSTART`, `RXBCHUNK`, `RXBDONE` for encrypted browser-side reassembly and AES-GCM authentication. CRC32 is for USB corruption detection, not security. 224-byte ciphertext RF data, 6-fragment SF7 window (3 in SF9 Max), authenticated bitmap ACK, missing-only retransmission and a 7168-byte transfer maximum.

Both firmware endpoints must announce a valid HMAC-authenticated `OG_TYPE_ARQ_CAPS` before using the new wire data framing. Do not mix v1.5 and v1.6. See [full integration/acceptance notes](../docs/V1_6_0_ARQ_INTEGRATION.md).

## Development tests

Run `bash firmware/tests/run-host-tests.sh` from repo root; it includes full C ARQ bridge simulations, intentionally lost frames/bitmap ACKs, web protocol tests, power checks and source checks. Optional `python3 tests/test_v15_web_smoke.py` requires Chromium and Playwright. **No full ESP-IDF build or real-board benchmark was performed in the authoring environment.** Keep v1.5 as a recovery baseline until both hardware devices pass acceptance.

## v1.6.1 measured-regression repair

`BULKCHUNK` still validates every 160-byte sequence and CRC32, but `BULKSTEP` is emitted once per bounded group of four USB commands (or final partial group). The included browser now pipelines four chunk lines per write. `BULKSTAT|<id>|RF_MS=...|FRAMES=...|RETX=...|TIMEOUT=...` describes *this* transfer, not a cumulative LINK counter. Six SF7 224-byte chunks fit in one radio bitmap window for a 1-KB encrypted probe; SF9 Max stays three. A 25-ms post-TxDone guard provides receiver FIFO/HMAC/IRQ rearm time before the next packet. These are source-level optimizations requiring actual two-board validation; no throughput gain is guaranteed.

**Update both boards and both browser clients.** New READY advertises `FW=1.6.1-rc.2|USBPIPE=4|WINDOW=6|PROFILES=3|SYNC=ONE_CLICK`. Do not mix the old per-chunk acknowledgment web client with this grouped acknowledgment firmware. See `../docs/V1_6_1_RC1_BOTTLENECK_FIX.md`.

## v1.6.1-rc.2 — one-click synchronized control only

No changes to `components/offgrid_arq/`, the SX1278 SPI driver, RX/TX loop, USB ciphertext staging or AES/read-receipt engine. Profile OFFER/AGREE packets include one extra HMAC-covered byte declaring the initiating operator inspected both boards, so a peer follows without separate `POWER18 VERIFIED`. Peers busy with ARQ transfers return an authenticated reject. `PROFILE|ERROR` reports a failed handshake; timed rollback and Balanced-on-reset remain. **Both ESP32s must be upgraded together**, as older v1.6.1-rc.1 control packets are rejected. No new performance result has been measured.
