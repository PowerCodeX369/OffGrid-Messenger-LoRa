# OffGrid Messenger LoRa v1.8.0-rc.1 — encrypted media and automatic RF

**Status: source candidate, not certified on the two physical boards.** v1.7.0-rc.1 is the user-confirmed working baseline. The SX1278 driver and v1.6.1-derived selective-repeat ARQ transport are **byte-for-byte unchanged**. A full ESP-IDF 6.1 build and media transfer on real hardware remain required. Do not delete the v1.7 backup.

## Photo messages

- Click **+** in the web chat and choose an image. Maximum input 25 MB / 40 megapixels to protect the browser.
- A canvas decodes, resizes and re-encodes as WebP (JPEG fallback) with quality steps from 0.91 down to 0.67 and long edge from 1600 down to **no less than 640 pixels**. The code only accepts a candidate at **48 KiB or less**; if a complex picture cannot fit at a reasonable quality/resolution, it reports an error rather than secretly reducing to a tiny blur.
- Canvas recompression strips typical EXIF metadata and location data. Output file is **not guaranteed identical** to the original; alpha is composited against white when JPEG fallback is required. In-app encrypted gallery, zoom, download and local IndexedDB storage are retained. Lossless PNG is permitted only when it naturally fits the same 48 KiB cap.
- WebP/quality presets are browser capabilities, not a guaranteed perceptual-quality metric. Detailed foliage, noise, text and screenshots compress differently; size and resolution are controlled, visual quality must be reviewed.

## Voice notes — the hard bitrate limit

- Click the microphone, grant browser microphone permission, then click again to send. Capture is **local only** and uses MediaRecorder + **Opus** (preferred WebM, optional Ogg; MP4 only if available). Suggested bitrate **16,000 bits/s**, mono-like speech capture with echo cancellation / noise suppression.
- The **encoded audio file** is strictly **no larger than 20 KiB**. The browser stops early as the encoded chunks approach 15 KiB, allowing some headroom for the encoder/container trailer. A time cap of 18 seconds is a secondary safety limit. If the resulting codec output still exceeds 20 KiB it is rejected rather than transmitted.
- **Two minutes of natural-sounding voice in 20 KiB is physically incompatible with ordinary Opus voice quality.** 20,480 bytes / 120 s = 1,365 bits/s including headers. RFC 6716 specifies Opus >=6,000 b/s and typically 16–20 kbps for wideband speech. At 16 kbps, 20 KiB holds about **10 seconds** of audio before container overhead. A 120 s note would require ~240 KB plus framing. v1.8 prioritizes understandable, natural speech and the hard 20 KiB limit: expect **~7–12 second clips**, not 120-second notes. The mic UI states that constraint. Multi-part long audio could be added later, but would be **more than 20 KiB total**.
- The voice message can be played back with browser controls. Playback receipts only occur after the receiver's audio starts playing. Media data remains encrypted in IndexedDB at rest.

## Wire format, recovery and tick integrity

The existing radio USB path (`BULKBEGIN` / `BULKCHUNK` / `BULKEND`) accepts **up to 7,168 ciphertext bytes** per RF transfer. v1.8 does **not** increase the SX1278 frame size, alter bitmap ACK engine or introduce another radio protocol. Instead the sending browser:

1. Encodes a file with the limits above and assigns one 64-bit **message ID**.
2. Splits it into **5,800-byte** media pieces, at most **9 image pieces / 4 voice pieces** for the configured limits. Each has a 28-byte authenticated metadata header: version, media type, MIME code, fragment index/total, full file size and **128-bit truncated SHA-256 digest** of the whole compressed file. Width/height or duration are in the existing authenticated OGM4 envelope metadata.
3. Encrypts each piece as a **fresh AES-256-GCM OGM4 envelope** with its own new transfer ID and nonce. The radio adds its existing HMAC-SHA256 authentication. No unencrypted file bytes reach ESP32 USB or the SX1278.
4. Sends one piece at a time through the existing **six-fragment ARQ window**, retrying a radio-unconfirmed piece up to **three attempts** with bounded delay. The user sees cumulative sender progress and can pause at a piece boundary. The receiver deduplicates repeated pieces and reassembles out of order, retaining up to four partial files in RAM for at most twenty minutes.
5. Only after every piece is decrypted and the full media SHA-256 matches does the recipient create a visible message, store the encrypted local history and send the existing **encrypted delivered/read** receipts. **Radio ACK is not application delivery.** A failed/abandoned piece does not fake a double tick.
6. Sender retry (tap failed message) restages the file from the beginning. Receiver partial pieces may be reused if its browser remains open and it has not expired. **There is no persistent resume state across browser/reboot loss yet.** Do not mistake transport-level ARQ resumption for durable upload checkpointing.

**Performance reality:** the last measured v1.6.1 Balanced rate was ~457 B/s for 1 KiB encrypted probes. At that *reference goodput*, 48 KiB raw media alone could take ~108 seconds; framing, receipts, retries and RF profile changes increase the elapsed time. Fast may be better, Range will be much slower. These media capabilities are experimental until actual tests confirm their usability.

## Media-aware AUTO RF behavior

- When the operator has enabled **Auto** (and authorized nominal +18 dBm on both boards), the sending browser sends a **local `MEDIA ON`** hint before a media transfer and **`MEDIA OFF`** afterwards.
- If **both authenticated directions have recent strong RSSI/SNR**, Auto may propose **Fast (SF7/BW500)** using the existing signed, one-sided synchronized profile offer/agree/verify/rollback handshake. **The hint never changes manual selection** and never forces Fast if the link is already weak.
- v1.7 Auto continues considering **two-way RSSI/SNR, retries, cooldown and link sensitivity** between media pieces. If the link weakens, the normal synchronized downgrade can select Balanced or Max Range. The sender retries the same encrypted media piece after an ARQ failure; no plaintext is sent, no receipt is fabricated.
- **A completely silent link cannot negotiate a safe change on both ends.** Both must hear the authenticated offer; without connectivity, the system can only report an unconfirmed transfer. Both devices return to manual Balanced on reboot. Auto is opt-in; its thresholds remain *field unvalidated* beyond the user's one-metre tests.
- The `MEDIA ON` hint expires after ten minutes in firmware if a browser crashes. It changes only coordinator scheduling eligibility; **not** PA configuration, power limits, packet crypto, or modulation without the existing authenticated peer negotiation.

## Hardware acceptance checklist

- [ ] Clean ESP-IDF 6.1 build using ESP32-C3 target. No changes to the pinout, NVS pairing key or USB/BLE path.
- [ ] Flash **both nodes** with `FW=1.8.0-rc.1|MEDIA=1`; hard-refresh and reconnect **both v1.8 browsers**.
- [ ] Re-run 20 short text messages each direction, delivered/read receipts and three manual profiles. These remain priority over media.
- [ ] Send 1 photo, verify it arrives with accurate dimensions, rendering, image viewer and no decrypted content in USB logs.
- [ ] Send a 48 KiB synthetic high-entropy file in a test harness; deliberately corrupt one piece, verify that no partial image is displayed and no delivered/read receipt is forged.
- [ ] Send short Opus note and verify accurate duration, playable audio, real played receipt and 20 KiB cap.
- [ ] Test cancel/pause, retry, disconnected USB, temporarily disable one ESP32, and recover by retransmitting the same media message.
- [ ] Enable AUTO, send a photo at close range, verify both devices coordinate a safe Fast switch. Emulate lower received quality (when possible) and verify fallback at piece boundaries, without forcing Fast on a weak link.
- [ ] Compare actual media transfer duration, `ARQ_RETX`, `BULKSTAT`, RSSI, SNR and FW mode from both devices; test after a fresh reboot and after two successive transfers.
- [ ] Test voice encoder in Chromium on the actual operating system (real microphone; not only synthetic audio). Browsers vary in Opus bitrate/format support.

**Known limitations:** voice clip length below user's aspirational 120 s, possible image encoder rejection of intricate images under 48 KiB, ephemeral receiver partial state, slow radio throughput for 50 KiB files, and no physical RF/latency qualification. Live video/calls are not supported.

**External references:** [Opus RFC 6716](https://www.rfc-editor.org/rfc/rfc6716.html), [MDN canvas toBlob](https://developer.mozilla.org/en-US/docs/Web/API/HTMLCanvasElement/toBlob), [Semtech SX1276/77/78/79 datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1278) (LoRa payload max 255 bytes).
