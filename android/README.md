> **Current Android v3.0.5-rc.1 (versionCode 305):** encrypted reconnect-safe outgoing TEXT outbox and authenticated receipt-only delivery confirmation; original native Compose chat UI preserved. Requires **both** ESP32s on companion `RF_POLICY=ULTRA12_FEC8`; existing BLE key pairing retained. Automatic retry runs while connected/Activity active and encrypted history restores after restart—not while the app process is dead. [Full build, test and rollback guide](../docs/V3_0_5_FINAL_RELIABLE_TEXT.md).

> **v3.0.4-rc.1 native Android Ultra mode:** new `Ultra Long Range · SF12 · TEXT only` radio menu entry. Original Compose chat UI, BLE GATT pairing, 48 KiB photos/40 KiB voice in other profiles, receipts and history remain. In Ultra only encrypted text and receipts are supported; media controls are blocked. Both ESP32s need v3.0.4 firmware for authenticated Ultra mode negotiation. Build with Android Studio; host tests do not create a tested APK. See [`V3_0_4_ULTRA_LONG_RANGE.md`](../docs/V3_0_4_ULTRA_LONG_RANGE.md).

> **v3.0.3-rc.1 source:** build the native app with versionCode 303; fixes Auto/manual failure visibility and stale LoRa RSSI. Original Compose layouts remain unchanged. Companion firmware on BOTH ESP32s is needed for early weak-link switching. [Details](../docs/ANDROID_RF_RANGE_V3_0_3.md).

# OffGrid Messenger Android — original native app, LoRa transport

**v3.0.2-rc.1 (source candidate, based on v3.0.1)**. The original native Android Compose application is again the **only launcher**. The experimental v3.0.0 WebView UI, alternate connection picker, and bundled HTML/CSS/JS screen have been removed. The original native chat layout, theme, icons, camera, BLE pairing screens, background service and encrypted history remain unchanged. **Only the radio dropdown text/options are updated to the real LoRa modes**; no alternative chat UI was introduced.

This release changes the underlying transport and protocol to interoperate with the **current OffGrid LoRa browser** without adopting its UI.

## Connections and companion ESP32 firmware

The phone still pairs through the **original Android BLE GATT client** (`BleNodeClient`, `BleSessionHolder`) using the same OffGridBLE2 HMAC challenge/response and the existing Keystore-wrapped 256-bit shared key.

**Yes, LoRa firmware still supports BLE:** the existing NimBLE service advertises as `OffGridLoRa-xxxx` with the same GATT UUIDs. HOWEVER, the user's proven **firmware v1.8.0-rc.4** advertises a BLE link but intentionally lacks BLE bulk media support (`CAPS|...|MEDIA=0`, and its command allowlist rejects bulk traffic). This native app requires the **opt-in BLE-bulk companion** from the included `firmware/` folder on both ESP32-C3 radios; the app checks fresh authenticated `CAPS|FAMILY=LORA|PROTO=2|BULK=1|MEDIA=1` before enabling Send. Without that companion it refuses to claim media support. No USB OTG or WebView UI was added.

Keep a backup of the working rc.4 binary, and do not erase NVS when flashing the BLE companion. It contains the same LoRa/SX1278 modulation, ARQ source, USB browser protocol and manual/automatic radio switching. Only the BLE bulk command allowlist, authenticated bulk forwarding and BLE notification queue behavior were extended (already present in the preceding v3.0.0 source release). Physical BLE GATT media behavior has not yet been tested on a phone.

## Original interface: same chat UI; corrected radio menu only

The original native Compose chat, list, composer, photo picker, microphone, recorder/player, receipts, history and pairing controls remain unchanged. The **existing radio dropdown** is corrected to show exactly the LoRa settings rather than the old ESP-NOW LR250/LR500 labels:

| Menu choice | Authenticated BLE command | Radio setup |
| --- | --- | --- |
| Auto (adaptive) | `AUTO ON` | Dynamically chooses Fast/Balanced/Range |
| Fast | `PROFILE LOW` | SF7/BW500 nominal +18 dBm |
| Balanced | `PROFILE BALANCED` | SF7/BW250 +10 dBm when manual |
| Max Range | `PROFILE MAX` | SF9/BW125 nominal +18 dBm |

The Android screen reads **actual `PROFILE=` and `AUTO=` fields** from the firmware's `LINK` telemetry. In Auto, it displays the applied modulation and highlights Auto. It no longer equates `LR 250/500 kbps` with a LoRa RF mode; those are ESP-NOW PHY labels.

**One-time-per-BLE-session RF safety confirmation:** Selecting Auto, Fast or Max Range, or sending photo/voice from Balanced, asks you to verify **both** ESP32 radio boards' antennas, stable power supplies and local RF permissions before requesting nominal 18 dBm. After confirmation the app sends `POWER18 VERIFIED`, waits for the firmware's `POWER18|ENABLED` response, then requests the authenticated mode switch. The receiving radio follows automatically; a second phone tap is not required. If confirmation is declined during media, the app explicitly sends at the current profile instead of pretending it boosted.

Photo/voice: `MEDIA ON` → wait for `MEDIA|READY|MODE=LOW` → send encrypted bulk data → `MEDIA OFF` → wait for `MEDIA|RESTORED|MODE=...` or an explicit safe bypass/restore error. No in-flight data is intentionally staged during an unconfirmed transition. Firmware may decline Fast when the peer is stale, busy or link margin is insufficient; the app reports that and keeps the working RF mode. No firmware/web runtime source changed in this update.

## Protocol parity with LoRa web v1.10

- 32-byte shared key, AES-256-GCM, random IV and 64-bit transfer ID, OGM4 AAD, same 24-byte authenticated envelope.
- BLE GATT HMAC challenge-response, then `BULKBEGIN`, sequential 160-byte `BULKCHUNK` records, CRC32 `BULKEND`; authenticated radio bitmap ACKs and `BULKCONFIRMED` control progress.
- Incoming `RXBSTART`, `RXBCHUNK`, `RXBDONE` verifies ordered chunks, CRC32, then GCM **before** app state changes.
- 48 KiB maximum photo, 40 KiB maximum voice, 7,040-byte authenticated media parts and truncated whole-file SHA-256 integrity. Android native recorder prefers Opus with AAC fallback and requests 8 kbps mono speech; the actual 2-minute/40 KiB goal is *not guaranteed* by hardware encoders. Clips stop at duration or byte budget.
- Native messages, authenticated `DELIVERED`, `READ`, `PLAYED`, web-compatible encrypted receipt batch, edit/delete and duplicate recovery. No fake delivery ticks.
- Temporarily request Fast at media start and restore the original profile once radio confirms completion; if negotiated Fast cannot be established, the firmware can bypass to the current safe profile. The existing BLE-validated automatic radio policy is otherwise unchanged.
- Bounded retries and fresh encrypted transfer IDs after an unconfirmed stage, text/receipt priority between complete media segments, bounded receiver state.

**Not implemented:** durable partial-media resumption after process death, independently verified phone-to-ESP32 interoperability, persistent RF-control replay protection across power cycles, BLE background resume of an in-flight transfer, and verified Android APK compilation. These are release/field qualification gates.

## Build

Open `android/` in Android Studio with JDK 17, Android SDK 36 and a compatible Gradle installation. Build the `app` module. An Android SDK/Gradle or physical Android device is **not present** in this release-preparation environment, so no APK was produced or claimed.

Do **not** uninstall the existing Android package before backing up Keystore and encrypted chat history. Keep the original signing certificate for an in-place update.

From repository root, host-test the protocol and the existing LoRa firmware:

```sh
sh android/tests/run_native_lora_tests.sh
sh firmware/tests/run-host-tests.sh
```

The native UI is compared against the preserved original at source level: **only the requested LoRa radio dropdown text/choices differ**. This is not an Android-device screenshot or APK validation.

## Hardware acceptance test

1. Back up working ESP32 v1.8.0-rc.4 binaries and flash the included BLE-bulk companion to both boards, without NVS erase. Verify USB web chat remains unchanged.
2. Build the native Android APK locally, using the same package/signing identity. Connect one phone to one node by BLE; verify the original pairing screen and `CAPS|...BULK=1|MEDIA=1`.
3. Send text **Android→web** and **web→Android**, check genuine delivery/read ticks, duplicate suppression, wrong-key rejection and reconnection.
4. Send a 48 KiB photo and 40 KiB voice in both directions; verify byte limits, image detail, audio playback, media progress and SHA validation.
5. Test the existing manual/Auto RF control options and media Fast/restore on two boards, paying attention to RF power authorization.
6. Run repeated short texts during a photo, background/foreground, disconnect/reconnect and a 30-minute mixed-media soak. Record `POWER18`, `MEDIA`, `PROFILE`, `LINK` and BLE GATT diagnostics before treating v3.0.2 as stable.
