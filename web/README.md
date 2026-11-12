# OffGrid Messenger LoRa — browser v1.6.1-rc.1

This is the **actual full web chat client**, not the isolated transport prototype. It retains encrypted text, browser chat history, verified delivered/read ticks, high-priority receipt batches, the three functioning manual profiles plus a disabled fourth experimental +22 dBm slot, and the LoRa speed test. Photos/voice/video remain disabled. AES-256-GCM is applied **before USB and LoRa**; radio HMAC-SHA256 verifies every new RF frame.

## Launch

From repository root, serve **the new `web/` directory**:

```sh
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Open `http://127.0.0.1:8765` in current Chrome/Edge, connect an ESP32-C3 in each of two browser windows/sessions, and pair both with the same shared key. **Both** nodes must have v1.6 firmware (`READY` advertises `BULK=1|ARQ=1|PROFILES=4`). Do not serve cached v1.5 scripts, which use incompatible per-fragment USB and RF framing.

## LoRa transmission and benchmark

The browser encrypts each OGM4 application envelope, stages the complete ciphertext in USB chunks through `lora_bulk.js`, and the ESP32 radio worker sends up to six 224-byte RF fragments per SF7 burst (three in Max Range) with bitmap ACKs and missing-fragment retransmission. USB/bulk progress and RF delivery are separate from the **end-to-end encrypted** delivered/read receipts. A single gray tick means local TxDone for all data fragments, two gray ticks mean authenticated application delivery, blue ticks mean authenticated viewing; missing receipts stay unconfirmed. An encrypted transfer may contain up to 7168 bytes; text still has the existing 500-character limit.

Settings → **LoRa speed test** sends encrypted random 64–1024-byte probes (4/8/12 trials), measures *confirmed application goodput* and end-to-end RTT including overhead. Compare against the measured v1.5 Fast baseline (~487.8 B/s / 2.07s median RTT) rather than theoretical PHY bitrate. Speeds are **unmeasured for v1.6**.

Settings → **Manual LoRa profiles**: Balanced at boot is SF7/BW250/+10 dBm. Fast SF7/BW500/+18 dBm and Max SF9/BW125/+18 dBm require explicit local `POWER18 VERIFIED` opt-in on **both** radios after hardware/supply/legal checks. The fourth experimental +22 dBm selector is **disabled**; the SX1278 does not support +22 dBm. Firmware uses authenticated manual negotiation and timed rollback. Details: [v1.6 hardware requirements and integration](../docs/V1_6_0_ARQ_INTEGRATION.md).

## v1.6.1 upload and timing changes

The browser sends up to four 160-byte `BULKCHUNK` lines in **one** USB write and waits for a single final group checkpoint. Each line is still sequence-validated and CRC-checked by firmware. `BULKSTAT` records authenticated radio-transfer completion time and per-transfer retries; the speed-test screen now distinguishes local USB staging, RF confirmation, and full end-to-end RTT. A lost encrypted delivered/read receipt still stays unconfirmed. Both firmware images must advertise `FW=1.6.1-rc.1` and `USBPIPE=4` to avoid old stage semantics. Hardware performance has not been measured; see `../docs/V1_6_1_RC1_BOTTLENECK_FIX.md`.
