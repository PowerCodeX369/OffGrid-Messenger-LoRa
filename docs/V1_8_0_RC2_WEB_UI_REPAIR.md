# v1.8.0-rc.2: ESP-NOW media UI parity hotfix

## Reproduced root cause

v1.8.0-rc.1 shipped correct media UI DOM (`#attachments`, `#photoPicker`, `#voiceButton`) and JavaScript handlers for `sendPhoto`, `toggleVoiceRecording`, in-chat image gallery and `<audio controls>` voice playback. But its `web/styles.css` inherited an old text-only LoRa rule:

```css
#attachments, #voiceButton { display: none !important; }
```

This CSS overrides the composer and hides both media buttons even after USB pairing enables them. Earlier automated media tests validated only DOM presence and image/Opus encoding, not computed CSS visibility. **v1.8.0-rc.2 deletes the hide rule.** The underlying 2026-11-09 reconstructed original ESP-NOW `styles.css` remains byte-for-byte the prefix of our LoRa stylesheet and provides the same image, audio, composer and gallery presentation.

## What's identical and what's intentionally different

- **Same media UI:** plus-button photo picker, microphone button replacing send when input is empty, recording red state, received/sent photo thumbnails, in-app gallery/zoom/download, native audio playback and duration indicators.
- **Different radio implementation:** original ESP-NOW PHY is never copied into LoRa firmware. SX1278, USB bulk selective ARQ, AES-256-GCM, HMAC, packet limits, delivered/read receipts, Auto/manual RF remain exactly v1.8.0-rc.1.
- **Different media limits:** LoRa 48 KiB images and 20 KiB voice notes versus the original ESP-NOW limits; display and controls are matched, but it is not truthful to advertise the original two-minute recording limit on this browser.
- **No firmware changed:** `READY|...|FW=1.8.0-rc.1` is the expected response while `APP_VERSION` reads `1.8.0-rc2-lora`.

## Apply without flashing

1. **Leave both ESP32s flashed with LoRa v1.8.0-rc.1.** Don't erase NVS or change pairing keys.
2. Replace the old `web/` folder in its entirety with the v1.8.0-rc.2 web folder, or extract the full rc2 repository and serve that `web/` directory.
3. Restart the local HTTP server if necessary; in **both** browsers force-refresh `Ctrl+Shift+R`, reconnect USB, and confirm both clients use the v1.8.0-rc.2 web files. No network/cloud is involved.
4. After pairing, the `+` attachment and microphone buttons are visible. When text is typed, the send arrow replaces the mic as on the original ESP-NOW UI.
5. First test one small image and a ~3-second voice note in each direction. Verify receiving thumbnail, photo viewer, playable voice clip and authentic delivered/read/played receipts. Actual two-radio media transfer is not verified by this release.

## Validation and rollback

- Run `sh firmware/tests/run-host-tests.sh` (C/JS protocol regression).
- Run `python3 tests/test_v18_browser_smoke.py` (photo WebP/JPEG and synthetic Opus browser test).
- Run `python3 tests/test_v18_ui_parity.py` (real rendered CSS/button/picker/received media at desktop, split and mobile widths).
- Roll back the `web/` folder to the prior version only if needed; **no device firmware change** was made. Keep v1.7/v1.6.1 working archives.
