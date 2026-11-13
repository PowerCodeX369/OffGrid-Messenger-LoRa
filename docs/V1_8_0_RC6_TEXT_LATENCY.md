# v1.8.0-rc.6 — Short-text latency and queue fairness

**Compatible firmware:** `FW=1.8.0-rc.4` on BOTH ESP32-C3 boards. **Web:** `1.8.0-rc6-lora` on both browsers. **Release type:** web-only; no flashing, NVS erasure, RF power change or crypto/key migration.

## Why this change

After v1.8.0-rc.5 improved photo transfer speed, the user reported slow simple texts without a precise measured text RTT. Source comparison shows rc.5 left firmware SX1278/ARQ and OGM4 text encoding unchanged. Instead, the browser made even a 1-chunk ciphertext pay three Web Serial request/checkpoint exchanges, and its FIFO application queue could place a text behind all parts of a long media transfer. Neither observation establishes the complete cause of a standalone text delay, so no numerical speed claim is made.

## Scoped changes

1. When **encrypted** ciphertext is <=160 bytes, `BulkLink.send` writes three existing, ordered, newline-separated USB commands (`BULKBEGIN`, `BULKCHUNK ... 0`, `BULKEND ... CRC32`) in **one** Web Serial write. It awaits the firmware's ordinary `BULKQUEUED` acceptance and then its usual `BULKCONFIRMED` / `BULKSTAT` outcome. A `BULKFAILED` at any point rejects the transfer. Firmware stays untouched. Other ciphertext continues to stage with the rc.5 8-line batches.
2. When transmitting a multi-part image/voice note, the browser checks for a waiting ordinary text **only after** one complete encrypted media part's ARQ has been acknowledged. At most one text is sent at each boundary, via the same AES-GCM/ARQ/delivery-receipt path as other text; then the media resumes. During a profile transition it defers the text. It preserves the parent media as `activeTransfer` for cancellation/disconnect.
3. `TEXTPERF` adds local history persistence, queue, USB staging, RF confirmation wait, firmware RF, retry and timeout measurements. `TEXTRECEIPT` records authenticated application delivered/read RTT, never infers double-gray or blue ticks from a radio ACK. These diagnostics include no plaintext, ciphertext, IDs or secrets.

## Validation

Host C/JS tests include deliberate missing/rejected RF fragments and USB stage failure; Chromium tests cover real photo compression, short Opus encoding, original media UI, profile handover, and `MEDIA1 -> TEXT -> MEDIA2` queue ordering. The old SX1278, ARQ engine, media-segment format, encryption, receipts and frozen Android tree are source-identical to rc.5. **Full ESP-IDF 6.1 build and physical throughput are not verified here.**

## Minimal two-board acceptance

1. Keep both boards on rc.4 firmware and put both browsers on rc.6 (hard-refresh both). Test 10 small texts in each direction on **manual Balanced**, with no media active. Check genuine double-gray/read-blue receipts and record `TEXTPERF` + `TEXTRECEIPT` lines.
2. Repeat on manual Fast. No single-run guarantee: inspect actual `RFWAIT_MS`, retransmissions and `RTT_MS`. Text of more than ~120 plaintext bytes may require multiple USB chunks and use the original staging path.
3. During a 40–48 KiB photo, send a small text from the sending browser; it should leave at the next complete media segment rather than after the entire photo. It should not cause media corruption or interfere with the authenticated Fast/restore handshake. Then send voice and check both old media features.
4. Test simultaneous text in both directions and radio/USB disconnect recovery. If any reliability regression appears, restore **both** rc.5 web folders; firmware can stay rc.4.

**Note:** Even a short text still consumes RF airtime and an authenticated application receipt return trip. One USB write eliminates only local request/response staging overhead, not LoRa air time, rearm guard or RF collisions.
