# Android native LoRa v3.0.2-rc.1 — radio menu and media Fast synchronization

## Cause established from source and screenshot

The original native Android menu still offered ESP-NOW `SPEED`, `LR500`, `LR250`, `RANGE` and an `ESP-NOW link` heading; those did not describe SX1278/LoRa accurately. It was mapping these old choices internally to LoRa commands, and actual `LINK|...PROFILE=...|AUTO=...` values were not parsed correctly.

For media, the native app issued `MEDIA ON` but never first authorized the firmware's nominal +18 dBm path. Firmware returns `MEDIA|BYPASS|POWER18_NOT_VERIFIED` in that case. The app treated BYPASS as a generic `active` state without indicating that it remained in Balanced, explaining the observed absence of a Fast switch while a photo still sent successfully.

## Scope of the correction

- Original native Compose UI preserved, **except the user-requested radio menu heading, options and mode label**.
- Exactly three manual radio profiles plus Auto: Fast SF7/BW500, Balanced SF7/BW250, Max Range SF9/BW125.
- One-time-per-BLE-connection **explicit** hardware/antenna/RF-legal confirmation; then `POWER18 VERIFIED`; wait for `POWER18|ENABLED` before `PROFILE LOW` / `PROFILE MAX` / `AUTO ON` / `MEDIA ON`.
- `MEDIA ON` requests a signed two-node Fast handover; sender waits for `MEDIA|READY|MODE=LOW` before bulk media data. If firmware explicitly says BYPASS, sender explains current-mode fallback; on ERROR/timeout, it fails safely. After sending, `MEDIA OFF` requests synchronized original-mode restoration.
- Radio status is parsed from `LINK` profile/Auto fields. Selecting manual mode from Auto waits for `AUTO|OFF` before the manual profile command.
- Power consent is *not* persisted; a BLE reconnection requires a fresh confirmation. The phone never requests +22 dBm.
- **No changes whatsoever** to firmware, web messenger, native BLE GATT/Keystore, ARQ, message encryption, photo/voice compression, or chat screens beyond that menu.

## Existing BLE firmware compatibility

This update relies on the **BLE-bulk companion firmware already used by Android v3.0.1**. Its authenticated command allowlist already supports `POWER18 VERIFIED`, `AUTO ON/OFF`, `PROFILE LOW/BALANCED/MAX`, `MEDIA ON/OFF`, `BULKBEGIN/BULKCHUNK/BULKEND`. It reuses the original challenge-response GATT service and peer synchronization. No further ESP32 flashing is required **if both nodes already run that BLE-bulk companion**, as indicated by successful BLE photo sending in the screenshot. The old browser-only rc.4 firmware still requires the BLE-bulk companion for mobile media.

## Field acceptance

1. Build from Android Studio (same app ID/signing as before) and connect BLE. Check `CAPS|FAMILY=LORA|PROTO=2|BULK=1|MEDIA=1` and that LINK shows Balanced/SF7/BW250 rather than LR250.
2. Verify the radio dropdown shows exactly Auto/Fast/Balanced/Max Range; change one node to manual Fast and confirm both show `PROFILE|ACTIVE|LOW`. Repeat Balanced and Max. The second node follows automatically.
3. From Balanced, send a photo. Confirm both-board power dialog once. Expected sender events: `POWER18|ENABLED`, `MEDIA|BOOST_REQUEST|LOW|RETURN=BALANCED`, `PROFILE|ACTIVE|LOW`, `MEDIA|READY|MODE=LOW`, photo BULKCONFIRMED, `MEDIA|RESTORE_PENDING|BALANCED`, `PROFILE|ACTIVE|BALANCED`, `MEDIA|RESTORED|MODE=BALANCED`.
4. Test short voice then another photo in the same BLE connection: no repeated high-PA dialog, Fast switch and restoration for each media transfer.
5. Decline +18 dBm approval: media sends in the current mode and clearly reports this choice; no false claim of Fast.
6. Repeat under weak signal or a busy RF state: Fast may be declined, but encrypted media should remain deliverable without faking mode agreement; record `MEDIA|BYPASS|...` and recovery.
7. Disconnect/reconnect BLE and confirm the power approval is required again. Test encrypted text, receipts, 48 KiB photo, 40 KiB voice, peer-to-peer Auto, and original web app unaffected.

## Testing and limitations

Standalone Kotlin radio-link parser and command mapping tests passed; Android native bulk+media protocol tests and LoRa C/JS host suite passed. The Compose menu was compared byte-for-byte against the original native file after applying only the documented menu edits. **Android APK/Gradle build, ESP-IDF build, physical BLE phone verification, and two-radio mode-change proof are not available in this environment.** Do not replace your known-working installation without retaining rollback/source backups.
