> v1.6.1-rc.1 is not hardware validated. See [regression test and measured benchmark checklist](V1_6_1_RC1_BOTTLENECK_FIX.md).

> v1.6.0-rc.1 hardware acceptance **NOT YET PASSED**. See [full v1.6 integration and acceptance matrix](V1_6_0_ARQ_INTEGRATION.md). Original historical checklist follows.

# v1.2.0-rc.2 hardware acceptance: NOT YET PASSED

**Current evidence:** Existing Ra-02 modules passed earlier standalone SPI/GPIO and two-way RF tests using other firmware. That evidence does **not** certify the v1.2 radio engine or its browser ACK chain.

Perform these tests before publishing a stable hardware-validated release:

1. **ESP-IDF 6.1 full build.** Verify `idf.py --version`, run `idf.py set-target esp32c3 && idf.py build`, and save any first failure with its build log. No SDK/source checks here substitute for this step.
2. **Power + antenna.** Both Ra-02 radios must use matched antennas and stable 3.3 V at the module during TX. Confirm no GPIO9 boot strap issue. Do not wire/unwire while powered.
3. **Radio init.** Flash both. Both must print `RADIO|SX1278_READY` and then `READY|...|FAMILY=LORA|FW=1.2.0-rc.1|...|ACKENGINE=2|FLOW=2`. Wrong `RegVersion` or missing SPI wire must cause a `FATAL` message, not a pretend healthy connection.
4. **Verify no Wi-Fi.** Confirm there are no `wifi:init`, `espnow` runtime logs or ESP-NOW RF frames. Bluetooth advertising `OffGridLoRa-XXXX` may be active; Android interoperability is not validated yet.
5. **Pairing.** Set a matching 32-byte key on both. Wrong-key frames should increase `BADTAG` and not generate chat bubbles. Never share actual key values in exported logs.
6. **Low-volume exchange.** In both directions send 20 short unique texts, check each appears once, then validate single gray, double gray and blue ticks. Log **send to RX**, **radio ACK**, **authenticated radio ACK and browser APPACK after decryption**, **encrypted delivered/read** as distinct milestones.
7. **Loss recovery.** With one node temporarily unable to receive (without touching antenna during TX), confirm bounded retries and honest `UNCONFIRMED`; after recovery, resume messaging without reboot. Never label missing ACK as proof of no delivery.
8. **Simultaneous sends.** Send text from both browsers nearly simultaneously at least 20 times; look for half-duplex collisions, duplicates and any radio task stalls. Compare retry/ACK counters.
9. **USB reconnect.** Refresh browser, reconnect manually; verify no ghost transactions and that NVS key survives. Disconnect browser on receiver and verify MCU can still radio-ACK frames without false browser delivered/read ticks.
10. **BLE smoke test, future final app.** Confirm NimBLE advertising and authenticated `CAPS?` / GATT works before v2.0 Android integration (not yet verified here).
11. **Longer duration.** Run at least 30 minutes of intermittent text traffic under actual power conditions; log crashes, RSSI/SNR, CRC, bad tags, timeouts, and percent of messages with authenticated application delivery.

Do not claim specific range, throughput, latency percentiles, power savings or zero packet loss without timestamped measurements from this exact build and boards.

**v1.2 acceptance matrix (including 20-message bursts and authenticated speed tests):** [V1_2_0_BURST_RELIABILITY.md](V1_2_0_BURST_RELIABILITY.md). Historical v1.1 notes: [V1_1_0_RELIABILITY.md](V1_1_0_RELIABILITY.md).
