# Desktop web client — v3.0.6

This is the current complete browser messenger (`index.html`, `app.js`, `protocol.js`, `lora_bulk.js`, supporting scripts and CSS). It uses **Web Serial** for a physically attached ESP32-C3. It is **not a hosted Wi-Fi service** and does not require internet for messaging after the page has loaded locally.

## Requirements

- Desktop **Chrome or Edge** with the Web Serial API, access to a physical USB data cable and permission to use the serial port.
- Current companion firmware flashed onto **both** ESP32s and the **same 32-byte secret key** provisioned for both nodes. Do not paste the key into bug reports or terminal logs.
- A secure browser context: `http://localhost` / `http://127.0.0.1` is suitable. Web Serial is not supported by every browser and must not be expected to work on arbitrary mobile browsers.

## Launch

From the repository root:

```bash
python3 -m http.server 8765 --bind 127.0.0.1 --directory web
```

Visit `http://127.0.0.1:8765` in Chrome/Edge. On the first computer, click **Connect ESP32**, select its USB serial device, then use the application's security/pairing UI to provision or unlock the matching secret. Repeat on the other computer/node. The supplied firmware supports USB key administration; protect the key against disclosure. Confirm both peers authenticate before attempting text or radio changes.

Use the **entire matching `web/` directory** on both computers. Hard-refresh after upgrades to avoid browser caching older scripts. Every peer must run a compatible firmware; the `READY` output's old `FW=1.8.0-rc.4` marker is intentional. Verify `AUTO_SCAN=TRIBAND`, `ARQ_CAP=4`, `PROFILES=4` and `ACK_COMPACT=16` instead.

## Send text, media and receipts

1. Connect and confirm link status; test one short message in **both directions** at close range.
2. A local RF acknowledgment is **not** the same as authenticated decrypted application delivery. Check the delivered/read state displayed by the app.
3. Use the photo/voice controls for non-Ultra modes within the app's size limits (compressed photos up to 48 KiB, voice notes up to 40 KiB). **Ultra does not offer media**; it is text and receipts only.
4. If a message remains unconfirmed, keep the browser open and consult diagnostics/retry status; do not assume the receiver got it, and do not delete queued ciphertext casually.
5. Avoid connecting two browser instances to the **same physical serial port** simultaneously.

## Radio settings

- **Manual Balanced** is the default after reboot.
- **Fast** is still available manually and for negotiated temporary media handover.
- Enable **Auto** once, confirm both radios are supplied/antennas fitted and locally authorized for nominal +18 dBm. Auto is deliberately opt-in, not an automatically saved preference.
- Auto mapping: stronger than −100 dBm → Balanced; −100 through −130 inclusive → Max; below −130 → Ultra. Switching still requires signed peer coordination. Radio silence is handled by best-effort scan/rendezvous, not an imaginary weak-RSSI reading.

Use Settings / diagnostics for LoRa signal, `LINK`, retries and progress. The [rc.3 field procedure](../docs/V3_0_6_RC3_RSSI_BANDS_AND_AUTORECOVERY.md) explains successful mode confirmations.

## Developer orientation

- `app.js`: UI/state/pairing and message lifecycle.
- `protocol.js`: application encryption/packet envelopes.
- `lora_bulk.js`, `lora_scheduler.js`: USB staging and queue sequencing.
- `lora_media.js`, `lora_receipts.js`, `weak_text_queue.js`: media, receipts, retry handling.
- `lora_rx_guard.js`: bounded incoming deduplication.

From root run `bash firmware/tests/run-host-tests.sh`, including JS and browser contract checks. See [development](../docs/DEVELOPMENT.md) and [troubleshooting](../docs/TROUBLESHOOTING.md). Historical instructions for v1.7–v1.8 are archived at [`docs/history/WEB_README_BEFORE_PUBLICATION.md`](../docs/history/WEB_README_BEFORE_PUBLICATION.md), **not** current upgrade guidance.
