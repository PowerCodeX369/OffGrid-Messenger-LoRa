# OffGrid Messenger LoRa — v3.0.5-rc.1 Final Stabilization Candidate

> **Release status:** Source-tested release candidate, **not yet hardware-qualified** or designated stable. Must build ESP-IDF 6.1 and Android SDK app on a developer machine, then complete the two-board acceptance tests. We cannot guarantee delivery beyond the RF link budget or without a return path for receipts.

## Purpose and version compatibility

This is the integrated, final-feature stabilization branch based directly on the user's field-tested v3.0.4-rc.1 source. Its aim is to make **short encrypted text recover automatically after weak-link outages** while preserving radio pairing, UI, media and profile switching.

- **Firmware:** both ESP32-C3 / SX1278 boards must run the v3.0.5 companion source built with ESP-IDF 6.1. The compatibility `READY|FW=1.8.0-rc.4` identifier is retained deliberately for historical Web Serial clients, but `RF_POLICY=ULTRA12_FEC8` is added. The BLE `CAPS` includes the same marker. Do not rely on FW alone to identify this update.
- **Android:** v3.0.5-rc.1-native-lora-final, `versionCode=305`; original native Compose UI untouched beyond the previously implemented Ultra radio menu. BLE authentication and secure local history are preserved.
- **Browser:** use the **entire** matching `web/` directory, with `APP_VERSION=1.10.0-rc2-final-weaklink`, not earlier cached JS. Serve over `http://localhost` or HTTPS for Web Serial and encryption APIs. Hard refresh both browsers.
- **Ultra interoperability:** authenticated radio ARQ capability value **3** is required on both radios before SF12/CR4/8 is permitted. A v3.0.4 ARQ capability 2 peer must not trigger a unilateral Ultra switch. Other profiles retain the same SF/BW, coding and packet size rules as v3.0.4. Do not mix v3.0.5 and v3.0.4 for Ultra.

## What the code actually does

### Encrypted deferred text outbox

- Messages already have a persistent **application message ID** distinct from each encrypted **radio transfer ID**. The ID remains constant across retries; ciphertext is freshly sealed with a new AES-GCM nonce/transfer ID. Receiver application duplicate suppression uses the unchanged application ID.
- Android rehydrates its existing encrypted-at-rest message history. An outgoing text that was sending or failed is restored as `waiting`. The scheduler only sends when BLE is authenticated and firmware LINK telemetry reports an **authenticated RF peer frame heard within 30 seconds**. The Android Activity timer runs every 12 seconds while connected and its history is loaded. Android has **no independent background retransmission service** when the app process is not running; the waiting record persists for the next active session.
- Web rehydrates AES-GCM-protected IndexedDB messages. Unconfirmed outgoing text enters `waiting`; the scheduler similarly requires an authenticated, recently heard LoRa peer. When the browser or USB port is closed, stored text remains pending but cannot transmit until the page is open and reconnected.
- Backoff after attempts grows 15s, 30s, 60s, 120s, 240s, then max 300s, plus a 0–5s message-ID-derived pseudorandom jitter. A successful radio transfer with no **application delivered receipt** is held for at least 90s (non-Ultra) or 240s (Ultra) before another try. Backoff bounds **transmission frequency**, not total eventual delivery attempts.
- `sent` is RF transfer acknowledgment, **not** end-to-end delivery. Only a verified encrypted `DELIVERED`, `READ` or `PLAYED` application receipt upgrades display state. Re-delivered message IDs are de-duplicated and acknowledged again. `waiting` cannot overwrite an already authenticated delivered/read status.
- Control packets and short texts preempt queued large media at a complete media segment boundary. RF packets are **never** interrupted in flight. Ultra explicitly blocks image/voice transmission and temporary Fast media boost.

### Reliable Ultra RF

- Max Range **stays SF9/BW125**. Ultra Long Range **stays SF12/BW125 with low-data-rate optimization**. This revision uses Ultra **CR 4/8**, replacing v3.0.4 CR4/5; its additional airtime can help some interference conditions but is not a universal range guarantee.
- Ultra ARQ uses the existing short authenticated fragment formats and adaptive packet size/window policy; raises the per-fragment **bounded** attempts from 5 to 12 and increases ACK deadlines after repeated missed ACKs based on SF12/CR4/8 airtime. Non-Ultra ARQ and existing image/voice codecs remain unchanged.
- Peer/Mode transitions remain authenticated: no switching into a profile the peer hasn't signaled it can use. Auto and manual modes continue to preserve the existing rollback behavior; firmware still requires the existing explicit legal/hardware +18 dBm opt-in.
- `LINK` includes `ARQ_RXDUP` and `ARQ_USBERR` diagnostics. Observe additional existing counters: RSSI and SNR, RFHEARD, PEER_AGE_MS, RX, CRC, BADTAG, ARQ retransmission count, TX errors, and active profile. Capture on **both boards**, not only the app.
- The browser's startup STATUS retry is bounded (max six attempts, spaced five seconds apart) and reports EVENT_QUEUE_FULL instead of hammering a stalled radio worker indefinitely.

## Safety and limitations

1. A peer beacon or LoRa RSSI reading can be received while payload DATA or the return ACK is lost. Neither `BLE connected` nor `RFHEARD=1` guarantees end-to-end delivery.
2. Even SF12 cannot deliver if there is no decodable physical path; retries will wait for authenticated peer activity rather than flooding or claiming success. A relay, antenna, power supply, legal RF setting, or better placement may be necessary.
3. CR4/8 and SF12 require substantially more time on air. Comply with local legal frequency, TX power and duty-cycle/airtime rules. This release does not dynamically know local regulations or enforce every applicable airtime ceiling; test responsibly. Ultra intentionally disallows media.
4. Text persistence is **on the phone/browser**, not unattended ESP32 store-and-forward. The sender's client must reconnect and run for retransmission. No cloud service or server is added.
5. A receiver may receive and display text while its delivered receipt is lost. Sender retries the same application ID and receiver should deduplicate. Recipients on an older incompatible app/build or lost encryption keys cannot recover messages by reattempt alone.
6. Encrypted history depends on the existing AES key and local app storage. Do not wipe NVS, uninstall Android or clear browser site data without backing up keys and history. Do not put pairing keys in USB diagnostic exports, reports or Git.
7. No new proprietary protocols or network mesh relaying are claimed. The original project is still a **direct radio messenger**, not a Meshtastic replacement.

## Upgrade and build instructions

### Back up and flash firmware on BOTH ESP32-C3 boards

1. Extract the complete Git ZIP or `ESP32_Firmware.zip`; identify the `firmware/` directory. Keep the working v3.0.4 sources and capture NVS/key backups as your existing workflow allows (do not publish them).
2. Use a clean path without shell-sensitive parentheses or spaces. ESP-IDF **6.1** shell:

   ```sh
   cd firmware
   idf.py set-target esp32c3
   idf.py build
   idf.py -p YOUR_PORT flash monitor
   ```

3. Flash the same source build on the **second** ESP32 board, **without `erase-flash`**. Verify `READY|...RF_POLICY=ULTRA12_FEC8` and `CAPS|...RF_POLICY=ULTRA12_FEC8` (BLE), and that the hardware still initializes SX1278.
4. If your physical SX1278 wiring deviates from the project's hardware notes, preserve your **known-good board pin map**; don't guess pinouts. Check supply regulation, antenna and GPIO mapping before power tests.

### Android

1. Open the `android/` directory as an Android Studio project; use a compatible Android SDK/JDK (the app Gradle source declares its exact SDK versions). Build/Install using your existing signing credentials. Do not uninstall the old app first: that can remove its local encrypted history/key material.
2. Confirm Android app versionCode **305** and connect using the original authenticated BLE pairing workflow. Check `CAPS ... RF_POLICY=ULTRA12_FEC8` and `LINK` with recent authenticated peer activity.
3. Your native Compose chat and existing photo/voice workflow are unchanged; Ultra is text-only. Verify `waiting` and retry on transient RF failure.

### Web

1. Serve the **whole** `web/` directory over HTTPS or localhost; clear old asset cache, hard refresh, choose the correct USB Serial/JTAG port and verify pairing. Do not mix new and older browser JS files.
2. Check `READY|...RF_POLICY=ULTRA12_FEC8`. Do not repeatedly click Connect during the radio's boot. If it reports `EVENT_QUEUE_FULL`, stop retries, capture ESP-IDF USB console and check radio task/SX1278 startup.
3. Confirm `waiting` state appears for an undelivered encrypted text and returns to sending only when a recent authenticated peer has been heard.

## Hardware field acceptance (required before declaring a stable release)

1. **Near range:** Both boards boot to READY. Verify `Fast`, `Balanced`, `Max Range SF9` and `Ultra SF12`, all with manual synchronized switches and rollback on failure. Auto remains opt-in and only coordinates with a radio that advertises cap3. Verify AES key pairing and encrypted text both ways.
2. **Media regression:** Outside Ultra send 48 KiB photos and 40 KiB voice, with one-time approved Fast handover and restore. In Ultra media controls reject sending (Android and web). No unchecked forced media fast boost under weak signal.
3. **Lost ACK, not lost text:** On receiving application check only one displayed bubble for a repeated message ID; sender shows DELIVERED only after authenticated app receipt. Simulate duplicate radio deliveries, reconnect and restart both phone/web clients.
4. **Weak link:** With both radios initially synchronized to Ultra, send numbered short texts (1–100) both ways while separating. Collect two-node `LINK` status and `BULKSTAT`, `ARQ_*`, `PROFILE`, `AUTO`, `CRC`, `BADTAG`, retries and ACKs. Record the true number delivered and time until receipt; don't measure only RSSI.
5. **Outage/resume:** While a text is waiting, temporarily power down the remote ESP32, then bring it back and restart the Android app/browser. Validate the encrypted record remains, retries resume **after** fresh authenticated RF activity, and no duplicate chat bubbles or false blue ticks appear.
6. **Mixed traffic:** While sending a photo in Balanced, queue short texts and receipt controls. Verify texts are not starved and photo/media restore still completes.
7. **Long soak:** Test at least 30–60 minutes with intermittent coverage, two-way messaging, reconnect, resume and mixed profile changes. Record power, board temperature and RF airtime; terminate if supply or PA behavior is unstable.
8. **Rollback:** If communication regresses, flash **both** radios back to the matching v3.0.4 firmware and reinstall compatible v3.0.4 Android/web, **without erasing NVS**. Don't mix CR4/5 and CR4/8 Ultra radios. Preserve pairing and history backups.

## Test scope and release declaration

`sh firmware/tests/run-host-tests.sh` runs the available host C ARQ/loss/FEC/profile tests, Kotlin native protocol/receipt/retry logic, browser Node tests, crypto/media parser fuzzing, and Python source-isolation checks against known baseline files. The release packaging process verifies complete Git archive and source hash manifest separately. These are **not** substitutes for an ESP-IDF target build, Android Gradle build, on-device UI test, actual BLE media transfer, RF duty-cycle qualification, security audit or two-ESP32 range measurement.

**Release naming:** v3.0.5-rc.1 is the final planned feature-complete *candidate*. Promote to stable v3.0.5 only after field acceptance and correction of any remaining hardware-specific issues. Any security or reliability bugs found after shipment warrant an explicit patch rather than falsely freezing defects.
