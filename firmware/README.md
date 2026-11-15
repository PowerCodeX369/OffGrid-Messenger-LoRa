> **Current ESP32 LoRa companion v3.0.5-rc.1:** Ultra SF12/BW125 uses CR4/8 with airtime-aware ARQ retry; every other profile, including SF9 Max Range, retains its prior modem settings. Requires both ESP32s on `RF_POLICY=ULTRA12_FEC8` / authenticated capability 3. Use ESP-IDF 6.1, do **not** erase NVS. [Integration and field qualification](../docs/V3_0_5_FINAL_RELIABLE_TEXT.md).

> **v3.0.4-rc.1 Ultra companion:** signed synchronized `OG_MODE_ULTRA=4`, SF12/BW125 plus LDRO, text-only tagged transfer policy and Ultra-specific authenticated ARQ/ACK windows, retry and TxDone timing. SF9 remains unchanged. Flash both boards together after backing up NVS/firmware; no erase. The existing browser `FW=1.8.0-rc.4` identifier remains for strict wire compatibility but `PROFILES=4`/`ULTRA=SF12_TEXT_ONLY` signal the new build. Full target ESP-IDF build and hardware tests still required. [Acceptance guide](../docs/V3_0_4_ULTRA_LONG_RANGE.md).

> **RF policy EARLY88 (Android v3.0.3 companion):** AUTO is opt-in, requires explicit legal/hardware authorization for nominal 18 dBm. Aggressive downgrades are authenticated and synchronized; web `READY` base FW identifier remains 1.8.0-rc.4 for strict client compatibility. Verify `RF_POLICY=EARLY88` and read [field test procedure](../docs/ANDROID_RF_RANGE_V3_0_3.md).

# Firmware v1.8.0-rc.1 — LoRa media-capable bridge

The v1.7 SX1278 SPI2 driver, radio frame authentication, 224-byte ARQ fragments, six-packet selective-repeat bitmap window, authenticated profile negotiation, 3 manual power/speed modes, and two-way RSSI/SNR AUTO policy are unchanged. No images or audio are decoded/stored in MCU RAM: the browser compresses and AES-GCM encrypts **5,800-byte media segments**, and firmware just forwards each via the existing `BULK` USB/RF mechanism. The local physical-USB `MEDIA ON` hint may trigger the existing authenticated FAST profile handshake **only if AUTO was enabled** and both sides have safe signal margins; `MEDIA OFF` cancels the hint. A hint automatically expires after 10 minutes. Cannot recover an already-silent RF link without a confirmed handshake.

Build with ESP-IDF 6.1 using `idf.py set-target esp32c3 && idf.py build`; flash **both ESP32s**, preserve NVS pairing. Confirm `FW=1.8.0-rc.1`, `ARQ=1`, `AUTORF=1`, `MEDIA=1`. No Wi-Fi/ESP-NOW, photo codec or voice recorder runs on ESP32. BLE remains present but native LoRa Android media is not implemented. See `../docs/V1_8_0_ENCRYPTED_MEDIA.md` and the root README for wiring and acceptance.

## v1.7 automatic RF mode

This release retains the v1.6.1-rc.2 USB/AES/ARQ transport unchanged. New local USB commands: `AUTO ON`, `AUTO OFF`, and existing `PROFILE LOW/BALANCED/MAX` for manual override. `AUTO ON` requires `POWER18 VERIFIED` on the initiating USB after explicit operator confirmation of **both** radio boards. Authenticated `OG_TYPE_AUTO_CTRL` and `OG_TYPE_AUTO_ACK` synchronize Auto ON/OFF; periodic HMAC-protected `OG_TYPE_AUTO_METRIC` reports supply the peer's measured RSSI/SNR. Only one MAC-elected coordinator changes profiles using the original OFFER/AGREE/verification/rollback. Radio hardware access remains in the single SX1278 task. No Wi-Fi or ESP-NOW runtime.

Auto Balanced uses nominal +18 dBm (different from **manual** Balanced +10 dBm) because otherwise stepping down from Fast +18 dBm would *reduce* the end-to-end link budget. `PROFILE|ACTIVE` includes `AUTO=0/1`, `AUTO|ON`/`AUTO|PEER_ON` mark synchronized enablement; `AUTO|ERROR` records failures. Mode switch attempts require no in-flight ARQ transfer and fresh authenticated bilateral telemetry. See `../docs/V1_7_0_ADAPTIVE_RF.md`.

**Do not deploy Auto before an actual ESP-IDF 6.1 build and two-board field tests.** The v1.6.1-rc.2 tag remains the confirmed rollback.

# OffGrid Messenger LoRa — ESP-IDF firmware v1.7.0-rc.1

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

Balanced SF7/BW250/+10 dBm is the boot profile. Fast SF7/BW500/+18 dBm (opt-in) and Max SF9/BW125/+18 dBm (opt-in) use the module-rated nominal 18 dBm high PA_DAC policy after verifying the radio's power supply/antenna/legal limits. For high-power profiles, the initiating user confirms BOTH devices in the v1.7.0-rc.1 browser. One USB device receives `POWER18 VERIFIED`, then `PROFILE LOW` or `PROFILE MAX`, and forwards the authenticated request containing that confirmation to the other device. The receiving ESP32 follows automatically without its own USB command. `POWER18 OFF` disables future high-power switching once already back in Balanced. There is no Experimental +22 dBm mode; it was removed as unsupported by SX1278. Original manual profile OFFER/AGREE and rollback are preserved.

## New v1.6 bulk protocol

`BULKBEGIN <id16> <size> DATA|CTRL`, `BULKCHUNK <id16> <seq> <hex>`, `BULKEND <id16> <crc8>` accept AES-256-GCM ciphertext from the current `web/` client. USB staging replies `BULKREADY`, `BULKSTEP`, and `BULKQUEUED`; radio worker emits `BULKSENT`, `BULKPROGRESS`, `BULKCONFIRMED`, or `BULKFAILED`. Receiver returns `RXBSTART`, `RXBCHUNK`, `RXBDONE` for encrypted browser-side reassembly and AES-GCM authentication. CRC32 is for USB corruption detection, not security. 224-byte ciphertext RF data, 6-fragment SF7 window (3 in SF9 Max), authenticated bitmap ACK, missing-only retransmission and a 7168-byte transfer maximum.

Both firmware endpoints must announce a valid HMAC-authenticated `OG_TYPE_ARQ_CAPS` before using the new wire data framing. Do not mix v1.5 and v1.6. See [full integration/acceptance notes](../docs/V1_6_0_ARQ_INTEGRATION.md).

## Development tests

Run `bash firmware/tests/run-host-tests.sh` from repo root; it includes full C ARQ bridge simulations, intentionally lost frames/bitmap ACKs, web protocol tests, power checks and source checks. Optional `python3 tests/test_v15_web_smoke.py` requires Chromium and Playwright. **No full ESP-IDF build or real-board benchmark was performed in the authoring environment.** Keep the user-confirmed v1.6.1-rc.2 as a recovery baseline until both hardware devices pass acceptance.

## v1.6.1 measured-regression repair

`BULKCHUNK` still validates every 160-byte sequence and CRC32, but `BULKSTEP` is emitted once per bounded group of four USB commands (or final partial group). The included browser now pipelines four chunk lines per write. `BULKSTAT|<id>|RF_MS=...|FRAMES=...|RETX=...|TIMEOUT=...` describes *this* transfer, not a cumulative LINK counter. Six SF7 224-byte chunks fit in one radio bitmap window for a 1-KB encrypted probe; SF9 Max stays three. A 25-ms post-TxDone guard provides receiver FIFO/HMAC/IRQ rearm time before the next packet. These are source-level optimizations requiring actual two-board validation; no throughput gain is guaranteed.

**Update both boards and both browser clients.** New READY advertises `FW=1.7.0-rc.1|USBPIPE=4|WINDOW=6|PROFILES=3|SYNC=ONE_CLICK|AUTORF=1`. Do not mix the old per-chunk acknowledgment web client with this grouped acknowledgment firmware. See `../docs/V1_6_1_RC1_BOTTLENECK_FIX.md`.

## v1.6.1-rc.2 — one-click synchronized control only

No changes to `components/offgrid_arq/`, the SX1278 SPI driver, RX/TX loop, USB ciphertext staging or AES/read-receipt engine. Profile OFFER/AGREE packets include one extra HMAC-covered byte declaring the initiating operator inspected both boards, so a peer follows without separate `POWER18 VERIFIED`. Peers busy with ARQ transfers return an authenticated reject. `PROFILE|ERROR` reports a failed handshake; timed rollback and Balanced-on-reset remain. **Both ESP32s must be upgraded together**, as older v1.6.1-rc.1 control packets are rejected. No new performance result has been measured.
