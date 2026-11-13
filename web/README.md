# Web messenger v1.8.0-rc.5 — media transfer efficiency (web-only)

Keep **both radios on `FW=1.8.0-rc.4`**; update **both** browser windows to `1.8.0-rc5-lora`. The media codec now uses 7,040-byte payload segments (48 KiB photo: 7 encrypted transfer sessions rather than 9; 40 KiB voice: 6 rather than 8). The browser sends up to 8 × 160-byte USB chunks per write, below the firmware 4 KiB USB input buffer; the firmware still validates each chunk and issues its existing every-four-chunk checkpoints. No firmware change or flash is needed. The two browsers must match because media segment count/length validation changed; do not mix rc.4 and rc.5 web clients for media.

After sending a photo, **Settings → USB diagnostics → Copy diagnostics** includes `MEDIASEG|...` per-part timings and a final `MEDIAPERF|...` line. The latter separates Fast profile handover, USB staging, RF confirmation, overhead and restoration. See `../docs/V1_8_0_RC5_MEDIA_EFFICIENCY.md`. No physical throughput improvement is claimed from host tests.

---

# Web messenger v1.8.0-rc.4 — synchronized temporary media Fast

**New ESP32 firmware required on both boards (`FW=1.8.0-rc.4`)**, and both browsers must use this web directory. The photo/voice UI and audio/image codec are unchanged from rc.3. Before sending media, the browser asks (once per physical USB session) to approve the nominal +18 dBm Fast profile for BOTH boards, then waits for firmware `MEDIA|READY|MODE=LOW` after an authenticated peer switch. On completion/failure/cancel it sends `MEDIA OFF`, waits for `MEDIA|RESTORED` and returns to the saved manual/Auto profile. Unavailable Fast is explicitly bypassed on weak/busy links. Run the two-node acceptance checklist in `../docs/V1_8_0_RC4_SYNCHRONIZED_MEDIA_FAST.md`.

---

# Web messenger v1.8.0-rc.3 — Voice-only 40 KiB cap

**Web-only update on rc.2.** Keep the existing `FW=1.8.0-rc.1` on BOTH ESP32 boards; do not flash. The browser changes are limited to the voice-note recorder, receive limits, microphone tooltips, asset cache version and voice-specific tests/docs. Photo settings, SX1278/ARQ, auto/manual radio modes, crypto, ticks and the ESP-NOW-style buttons are unchanged.

Voice notes use browser-native mono Opus when supported, with noise/echo suppression, a requested speech-oriented **8 kbps** bitrate, a maximum **40 KiB (40,960 bytes)** encoded audio file, and a **120-second** duration ceiling. Recording stops automatically when nearing the byte limit. Short notes remain small because audio is encoded on demand; it is **not padded** to 40 KiB. WebM framing, actual speech complexity and browser encoder behavior mean there is no guaranteed exact proportional size (such as 1 min = 20 KiB), and clear continuous two-minute speech cannot be promised at 40 KiB. No uploaded file exceeds the hard cap; failed final encodes are rejected rather than sending corrupt or oversized content. See `../docs/V1_8_0_RC3_VOICE_BUDGET.md`.

# Web messenger v1.8.0-rc.2 — original ESP-NOW media UI restored

**Web-only fix; no ESP32 reflash.** Keep firmware `FW=1.8.0-rc.1` on BOTH ESP32s. Use this entire `web/` folder on both browser sessions and hard-refresh so the updated `styles.css?v=lora180rc2` is loaded. The original ESP-NOW `+` photo-picker, microphone recording button, in-message image thumbnails/gallery and native voice-note controls now render with the same UI styling and interaction. They are disabled until the browser is connected and paired with the correct AES key.

The original ESP-NOW CSS remains the base stylesheet. The old text-only LoRa CSS rule `#attachments, #voiceButton { display: none !important; }` caused the missing media controls; it has been removed. No RF/ARQ or media transfer protocol change is needed. Use `python3 tests/test_v18_ui_parity.py` (Chromium/Playwright optional) for rendered desktop, split and mobile checks. More details: `../docs/V1_8_0_RC2_WEB_UI_REPAIR.md`.

# Web messenger v1.8.0-rc.1 — compressed and encrypted media

Serve the complete `web/` directory over localhost/HTTPS and use **v1.8 firmware on both boards**. Photo `+` and microphone are enabled only after encryption pairing. Photos are recompressed as WebP/JPEG with an absolute 48 KiB budget (photo quality varies), voice records Opus at a suggested 16 kbps, with an absolute **20 KiB** budget (typically 7–12 seconds, NOT two minutes). Every 5,800-byte media part is separately AES-256-GCM encrypted, passed to the existing 7,168-byte radio bulk transport, and reassembled only after whole-media SHA verification. Delivered/read ticks require the usual encrypted application receipts.

Auto RF is **opt-in**: it can request Fast when both received signals are strong and downgrade via the existing authenticated handshake if link quality declines. Manual Fast/Balanced/Range are never overridden by sending media. Transfers pause at part boundaries; tap a failed bubble to retry from the beginning. The receiver retains partial pieces only while its browser session stays live. Not physically tested here. Read `../docs/V1_8_0_ENCRYPTED_MEDIA.md`.

## v1.7 automatic Fast-first modes

Open **Settings → Automatic / manual LoRa modes** and choose Enable Auto on ONE browser after confirming both radio boards can handle nominal +18 dBm TX. The peer follows an authenticated radio control message. Auto prefers Fast, steps through Balanced and Range with RSSI/SNR hysteresis, and promotes back after sustained improvement. Manual profile buttons still work and **override** Auto. Reboot both boards to restore safe manual Balanced if they cannot communicate. Only the user-approved initiating browser starts Auto; it is deliberately not persisted across power cycles. See `../docs/V1_7_0_ADAPTIVE_RF.md`.

This is a new radio-control protocol: both boards and both browsers need v1.7. Chat/receipts, history, bulk Web Serial and encrypted benchmark logic remain the original v1.6.1-rc.2 working implementation.

# OffGrid Messenger LoRa — browser v1.7.0-rc.1

This is the **actual full web chat client**, not the isolated transport prototype. It retains encrypted text, browser chat history, verified delivered/read ticks, high-priority receipt batches, the three functioning manual profiles and the new opt-in automatic synchronized mode, and the LoRa speed test. Photos/voice/video remain disabled. AES-256-GCM is applied **before USB and LoRa**; radio HMAC-SHA256 verifies every new RF frame.

## Launch

From repository root, serve **the new `web/` directory**:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open `http://127.0.0.1:8765` in current Chrome/Edge, connect an ESP32-C3 in each of two browser windows/sessions, and pair both with the same shared key. **Both** nodes must have v1.7 firmware (`READY` advertises `BULK=1|ARQ=1|PROFILES=3|SYNC=ONE_CLICK`). Do not serve cached v1.5 scripts, which use incompatible per-fragment USB and RF framing.

## LoRa transmission and benchmark

The browser encrypts each OGM4 application envelope, stages the complete ciphertext in USB chunks through `lora_bulk.js`, and the ESP32 radio worker sends up to six 224-byte RF fragments per SF7 burst (three in Max Range) with bitmap ACKs and missing-fragment retransmission. USB/bulk progress and RF delivery are separate from the **end-to-end encrypted** delivered/read receipts. A single gray tick means local TxDone for all data fragments, two gray ticks mean authenticated application delivery, blue ticks mean authenticated viewing; missing receipts stay unconfirmed. An encrypted transfer may contain up to 7168 bytes; text still has the existing 500-character limit.

Settings → **LoRa speed test** sends encrypted random 64–1024-byte probes (4/8/12 trials), measures *confirmed application goodput* and end-to-end RTT including overhead. Compare against the measured v1.5 Fast baseline (~487.8 B/s / 2.07s median RTT) rather than theoretical PHY bitrate. Speeds are **unmeasured for v1.6**.

Settings → **Automatic / manual LoRa modes**: Balanced at boot is SF7/BW250/+10 dBm. Fast SF7/BW500/+18 dBm and Max SF9/BW125/+18 dBm require one operator on the initiating browser to confirm **both** radios have suitable antennas, supplies and legal permissions. The initiating ESP32 receives `POWER18 VERIFIED` locally and sends the authenticated profile offer; the peer follows **without a second click**. The unsupported +22 dBm selector has been removed. Firmware uses authenticated manual negotiation and timed rollback. Details: [v1.6 hardware requirements and integration](../docs/V1_6_0_ARQ_INTEGRATION.md).

## v1.6.1 upload and timing changes

The browser sends up to four 160-byte `BULKCHUNK` lines in **one** USB write and waits for a single final group checkpoint. Each line is still sequence-validated and CRC-checked by firmware. `BULKSTAT` records authenticated radio-transfer completion time and per-transfer retries; the speed-test screen now distinguishes local USB staging, RF confirmation, and full end-to-end RTT. A lost encrypted delivered/read receipt still stays unconfirmed. Both firmware images must advertise `FW=1.7.0-rc.1` and `USBPIPE=4` to avoid old stage semantics. Hardware performance has not been measured; see `../docs/V1_6_1_RC1_BOTTLENECK_FIX.md`.

## v1.6.1-rc.2 manual switching

Settings → Automatic / manual LoRa modes → Fast/Balanced/Max on **one** browser. Wait for queued messages/receipts to finish; the UI deliberately refuses a forced switch. High-power options prompt for both-board inspection, not a second USB command at the other end. Both firmware endpoints and both web clients must run v1.7.0-rc.1. See `../docs/V1_6_1_RC2_ONE_CLICK_SYNC.md`.
