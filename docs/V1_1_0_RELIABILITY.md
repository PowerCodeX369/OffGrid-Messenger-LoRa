# v1.1.0 LoRa text transport stabilization — engineering record

**State:** Source and host-test candidate. Two-board ESP-IDF 6.1 build, Web Serial integration, and physical latency measurements are required before hardware acceptance. This file is not proof of real-world performance.

## Requirements / truthfulness of chat receipts

The browser must not conflate distinct events:

| Event | Trigger | Visible state |
|---|---|---|
| Local queue | The browser has prepared an encrypted transfer | Clock / sending |
| `SENT|session|id` | SX1278 TxDone for each fragment; *all* fragments must be sent once | **One gray tick** |
| `RADIOACK|session|id` | Destination firmware authenticated a complete LoRa RF fragment | No new tick; releases this fragment's RF send window |
| Encrypted OGM4 `delivered` | Destination web client decrypted and accepted the full application message, then its encrypted receipt was decrypted by sender | **Two gray ticks** |
| Encrypted OGM4 `read` | Destination web client viewed the conversation and sender authenticated the read receipt | **Two blue ticks** |
| No valid authenticated delivery receipt by bounded deadline | Receiver might have the text, but sender cannot certify it | **Unconfirmed/failed**, not a fabricated double tick |

One gray tick **does not** mean receipt by the other radio. Two gray/blue ticks **never** come from a bare RF acknowledgment or unauthenticated serial line.

## What v1.0 did wrong

- The browser treated a per-fragment application ACK as the condition to advance to the next fragment. Its 14-second timer multiplied across a 500-character or Unicode message.
- The firmware performed radio ACK and then an additional unencrypted (but HMAC-authenticated) fragment `DELIVERED` radio frame, before the independent encrypted OGM4 delivered receipt. That duplicated confirmations and wasted airtime.
- The firmware retransmitted entire data frames while waiting for the browser's APPACK. Missing an application receipt caused unnecessary, expensive repeat airtime.
- Each 218-byte FIFO load/unload required hundreds of individual SPI transactions.
- Regular HELLO traffic competed with user packets and ACKs; control frames could stall behind an outgoing message.
- `SENT` was emitted after radio ACK rather than when the SX1278 TxDone interrupt completed.

## v1.1 source changes

1. Radio ACK is the *only* firmware-level RF fragment confirmation. It authenticates the frame using the pairing-derived HMAC key and releases the pending TX slot. This is not an application read/delivered receipt.
2. Sender emits `SENT` from SX1278 TxDone (once per fragment). Browser displays a single tick when **every** fragment has reached TxDone. A peer RF ACK (`RADIOACK`) advances up to three prepared text fragments; firmware transmits only one un-ACKed frame at a time.
3. Receiver browser writes `APPACK` locally **after** complete OGM4 AES-GCM decryption and application processing. This updates diagnostics only; it does not inject a second per-fragment delivery radio packet. An encrypted delivered/read OGM4 envelope, sent at priority, is the sole authority for double-gray / blue ticks.
4. AES-GCM delivered/read controls use `CTRLHEX` to enter a reserved high-priority firmware queue. The firmware still serializes actual half-duplex SX1278 transmissions and gives RF ACKs first priority.
5. FIFO transfers use one SPI transaction per packet, supported by the SX1278 FIFO interface, with ESP-IDF DMA enabled for the 218-byte maximum frame. A host SPI simulator checks maximum-size TX and RX.
6. A longer idle HELLO cadence reduces collisions and airtime. Retry intervals begin only after TxDone and use bounded random backoff to reduce repeated two-node collisions.
7. A bounded end-to-end receipt watchdog is separate from RF retransmission. It marks a message **unconfirmed** if no encrypted delivered receipt appears. Late authenticated receipts are still allowed to upgrade state.
8. Persistent PSA HMAC key import avoids repeatedly importing/destroying the same HMAC key for each packet; the pairing key and separate domain-separated radio key are unchanged.
9. Web rejects incompatible firmware without `ACKENGINE=2`, so an old v1.0 board cannot silently use the new event semantics. **Flash both boards and serve this version's `web/`.**

## Airtime: what software cannot eliminate

At fixed **SF7 / 125 kHz / CR4/5**, Semtech's formula yields approximately **82 ms** RF airtime for a 38-byte ACK and **343 ms** for a 218-byte maximum packet (8-symbol preamble, explicit header, CRC). This **excludes** USB, encryption, timing margins, opposite-direction radio turns, and retries. A 500-character ASCII message requires multiple encrypted fragments; 500 four-byte Unicode characters use many more. The fixed configuration's raw LoRa PHY bitrate is roughly **5.47 kbit/s**, so it cannot match ESP-NOW's 12–14 KB/s application throughput. BW250 and other fast/range profiles are deferred until mode interoperability is deliberately designed and tested.

References:
- Semtech, *SX1276/77/78/79 Datasheet*, Sections 4.1.1.7 and 4.3 (LoRa time-on-air and SPI FIFO access): https://cdn.sparkfun.com/assets/a/a/0/b/5/DS_SX1276-7-8-9_W_APP_V5.pdf
- Espressif, *SPI Master Driver*, on DMA transfer limits and alignment: https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/spi_master.html

## Measurement protocol (two ESP32-C3 + two Ra-02)

1. Build both boards with ESP-IDF 6.1; confirm `FW=1.1.0` and `ACKENGINE=2` on each. Test on native USB with independent browser windows; same pairing key; radios with correctly matched antennas and stable 3.3 V. No `erase-flash` unless intentionally re-pairing. Keep antennas separated enough to avoid possible near-field overload.
2. Send 50 unique one-character texts in each direction, no simultaneously active media. Record time from user send to receiver bubble, first TxDone tick, encrypted delivered tick, and read tick on focus. Report p50/p95/max and unconfirmed counts, not just the fastest message.
3. Repeat with 50 distinct 500-character ASCII messages and 10 long Unicode messages (including 4-byte characters). Confirm no application queue-full errors, duplicate bubbles, missing data, or false double ticks.
4. Send simultaneously from both browsers 20 times. Check `RADIOACK`, `RETRY`, `BADTAG`, `CRC`, and `TRACE|...` counters; record the maximum retry delay. Persistent collisions or unacknowledged messages are a release blocker.
5. With destination browser disconnected (radio still powered), confirm RF `RADIOACK` but **no double-gray/blue tick**. When no encrypted delivery receipt returns within the bounded deadline, display unconfirmed. With a wrong key, ensure no message is accepted and no forged receipt is shown.
6. Disconnect/reconnect USB and refresh the page. Check no stale transfer can fake successful delivery, and pairing NVS survives ordinary flashing.
7. After measuring clean-link performance, vary distance and interference carefully; record antenna details, power, and environment. Results depend on RF conditions and cannot be assumed from host tests.

## Known limitations

- v1.1.0 is a text-only RF reliability improvement, **not** the future selective-repeat missing-fragment bitmap engine. Up to three fragments can be prepared concurrently, but the firmware still transmits one unacknowledged LoRa packet at a time.
- Half-duplex collision avoidance is probabilistic backoff, not deterministic slot scheduling or CAD-protected TX. Simultaneous RF sends can still collide.
- The original inherited BLE service is not yet tested against the final universal Android application. Android source remains untouched.
- No automatically selected LoRa Fast/Balanced/Ultra-range profiles, media transfer, or Wi-Fi/ESP-NOW are enabled.
- Host tests validate code paths and emulated SPI, not real radio range, link speed, BLE coexistence, or actual ESP-IDF compilation. The two-device acceptance report is a release gate.
