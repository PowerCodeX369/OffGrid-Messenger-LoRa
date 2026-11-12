# v1.2.0-rc.1 — LoRa burst reliability and speed-test engineering review

Date: 2026-11-12. Status: **source candidate**, pending ESP-IDF 6.1 build, browser/Web Serial acceptance, and physical two-node benchmark. This note documents source behavior, not a field test result.

## Reported problem

On v1.1.0-rc.1, individual short encrypted messages were fast and regularly showed double ticks, but a rapid burst left many outbound messages pending or failed. Delivery/read receipts became unreliable when both ends were busy. The web client also displayed a LoRa diagnostic placeholder rather than an actual speed test.

## Confirmed source bottlenecks

1. **Multiple outstanding browser fragments:** `chosenWindow()` was 3 for text while the SX1278 firmware is deliberately stop-and-wait (one frame awaiting radio ACK). Multiple `MSGHEX` requests accumulated on the MCU and each browser fragment had its own 9-second wait.
2. **Two independent browser send paths:** normal text (`runTransfers`) and encrypted delivery/read receipts (`runUrgentReceipts`) could both call `oneFragment` concurrently. Firmware priority alone did not prevent serial-buffer contention and apparent queue overload.
3. **Receipt amplification:** every rapid incoming text could create *two* separate encrypted LoRa transfers, one delivered and one read, even where both applied to the same fast-open chat.
4. **Control queue exhaustion:** a full firmware control queue did not displace stale HELLO packets or coalesce duplicate ACKs. With repeated RX, essential radio ACKs could be lost to background controls.
5. **Half-duplex collisions:** simultaneous active nodes need channel contention deferral, but an unconditional CAD poll can itself interrupt a currently demodulating receive packet.
6. **No real benchmark:** the web client's speed diagnostics existed only as a placeholder. Legacy `RBENCH_*` parser branches did not implement an authenticated LoRa test.

## Design changes

### Firmware

- Keeps the fixed, conservative 433 MHz, SF7/BW125, CR4/5, 10 dBm profile, no Wi-Fi/ESP-NOW initialization, and the existing BLE/USB services.
- Keeps one SX1278 radio-owner task and **one unacknowledged RF DATA packet globally**.
- Before sending new DATA, reads `RegModemStat` without interrupting RX. If a packet is being detected or received, it defers. If clear, performs **best-effort** short LoRa CAD and randomized delay when busy. CAD is a preamble detector, not a guarantee against RF collisions.
- Defers do **not** consume packet retry attempts. Retry timing contains randomness and a MAC-derived skew; the existing authenticated radio-ACK validation is retained.
- Coalesces queued duplicate radio ACKs; may evict an idle HELLO to admit a critical radio ACK. Suppresses HELLO when recent authenticated peer traffic is present.
- Adds `CAD_BUSY`, `PENDING`, `CTRLQ` and actual `TXFAIL` counters to `LINK`, and limits unnecessary USB `READY` beacons while traffic is active.
- Advertises `FW=1.2.0-rc.1|ACKENGINE=2|FLOW=2|BENCH=APP`. It **does not** introduce a new unencrypted air protocol or assume a radio ACK means an encrypted chat delivery.

### Browser

- One shared `FragmentScheduler` serializes **all outgoing LoRa fragments**, including high-priority encrypted controls. A read/delivery receipt can preempt normal chat **between** frames but never interrupt an in-progress SX1278 transmission.
- Fairness rule allows one normal frame after four priority controls when both queues are nonempty, avoiding data starvation. Bounded memory: 64 queued scheduler operations and 24 unsent text messages.
- Coalesces **up to eight encrypted delivered/read receipts** into a 73-byte receipt body, which fits **one** 160-byte OGM4 fragment after the existing 24-byte header and 28-byte AES-GCM overhead. Each entry carries its own 8-byte target message ID and `delivered`/`read` type. Authenticated receipt parsing rejects invalid lengths, duplicate entries, unknown types, or tampered ciphertext. Read still means the receiver has actually viewed the conversation.
- Encrypted receipt batches get one bounded retry upon an unconfirmed radio acknowledgment. Retries have fresh nonces and transfer IDs; the receiving application applies duplicate message statuses idempotently.
- The new web client requires `FLOW=2`, so an old v1.1 firmware cannot silently interoperate with the new encrypted receipt-batch type. **Update both firmware images and serve both v1.2 web clients.**
- Refresh/session shutdown cancels queued browser jobs and in-flight Web Serial promises rather than letting old queued writes cross into a new session.

### Speed / reliability test

Settings → **LoRa speed test** sends AES-GCM-encrypted random probe messages with user-selected payloads **64 / 256 / 512 / 1024 bytes** and **4 / 8 / 12** trials. The recipient browser must be connected and automatically replies using an authenticated encrypted probe reply. Nothing is added to chat history.

Reports confirmed/unconfirmed trials, total elapsed time, **application goodput bytes/s (including failed trial time)**, median and p95 confirmed end-to-end roundtrip time. These are NOT SX1278 PHY bitrates or RF-only times; the result includes USB, browser processing, encryption, backoff, retransmissions, and radio airtime. The remote browser must be running and paired. A Cancel button stops further trials; it cannot abort a frame already on-air.

### Honest limits

- The fixed profile has finite airtime. Even a 500-character ASCII message typically spans several radio fragments. This release aims to avoid artificial stalls, not to claim ESP-NOW-like Mbps, 12 KB/s, or guaranteed instantaneous receipts.
- Current radio framing is still **stop-and-wait**, not a multi-packet selective-repeat window or selective ACK bitmap. This is intentional for half-duplex reliability at the user's requested sequential-send stage; sliding-window selective retransmission is a separate, controlled feature.
- A sender's single gray tick means TxDone for all fragments. Double gray requires a verified application delivered receipt, and blue requires a verified read receipt. If the receipt is lost, display remains unconfirmed even when the recipient saw the text.
- The browser enforces a finite queue, not unbounded buffering. If the 24-text-message backlog limit is hit, additional messages show a local failure with retry option; they are not silently dropped.
- The current Android tree remains byte-identical to the original baseline. Photos/voice/video are not enabled.

## Host validation performed

- C wire/profile tests, 218-byte SPI FIFO DMA burst simulation, and non-disruptive modem-status SPI read simulation.
- Pure C contention-boundary tests.
- JavaScript burst scheduler test (50 tasks, maximum concurrent RF writes **1**), priority preemption, starvation guard, bounded queues and cancellation.
- Receipt pack/unpack and AES-GCM tampering tests (8 receipts per fragment), probe payload encode/decode tests (64–1024 bytes).
- Existing 500-Unicode-character fragmentation, encrypted browser protocol, radio-policy and source-boundary checks.
- Git SHA-256 tracked-file manifest and byte-identical Android preservation.

**Not performed:** real ESP-IDF 6.1 build, Chrome physical Web Serial, and two ESP32/SX1278 RF test. Source tests cannot predict field packet loss or real latency.

## Mandatory two-board acceptance gate

1. Keep a backup of v1.1 and NVS; **do not erase flash**. Compile and flash this candidate on both ESP32-C3s; use the v1.2 browser on both sides. Confirm `FAMILY=LORA`, `FLOW=2`, `ACKENGINE=2` and the same pairing key.
2. With matched 433 MHz antennas and a stable regulated 3.3 V supply, send **20 short messages sequentially in both directions**. Record first gray, receive time, double gray and blue separately.
3. Send a **burst of 20 short messages per direction** without waiting between sends, then both directions simultaneously. Confirm all accepted messages either receive a genuine receipt or are explicitly reported unconfirmed; no unbounded queue lockups. Retrying must never duplicate visible messages.
4. Send 500-character messages with ASCII and multibyte Unicode. Confirm exact plaintext, fragment counts and end-to-end receipts.
5. Run 8 × 256-byte and 8 × 1024-byte speed tests on both nodes, with the other browser connected. Compare confirmed goodput, median/p95 RTT, retries and `CAD_BUSY`, `RX`, `ACK`, `BADTAG`, `CRC`, `PENDING`, `CTRLQ` firmware counters against v1.1.
6. Test after a USB page refresh and cable reconnect. Confirm no old transfer is silently replayed into a new session.
7. Repeat with both users sending simultaneously. Retain logs if any message takes unusually long or loses ACKs.

Only after these pass should the release be promoted from `v1.2.0-rc.1` to stable `v1.2.0`. Do not promise media throughput based on text-only tests.

## References

- Semtech [SX1276/77/78/79 datasheet](https://cdn-shop.adafruit.com/product-files/3179/sx1276_77_78_79.pdf) (SX1278 LoRa modem status `RegModemStat`, CAD preamble detection and airtime formulas).
- Espressif [ESP-IDF SPI master driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/spi_master.html) (DMA and SPI transaction sizing).
