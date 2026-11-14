# OffGrid Messenger — Android Universal LoRa + preserved ESP-NOW

**Android source release candidate 3.0.0-rc.1**, developed against the working OffGrid LoRa web v1.10.0-rc.1 and radio firmware v1.8.0-rc.4. This is a source project, **not a prebuilt or hardware-qualified APK**. Android SDK / Gradle and an Android test phone were unavailable in the release preparation environment.

## What's in the app

The launcher opens `LoRaActivity`, a secure, offline Android WebView containing byte-for-byte copies of the actual v1.10 web assets (`web/`, with one added Android bridge script in `index.html`). This design retains the *same UI and browser application logic*, rather than reimplementing and diverging:

- Encrypted text with authenticated delivered/read receipts and improved short-message scheduling.
- Photo selection, resizing/compression to 48 KiB, image bubbles/gallery and Android photo export.
- Opus voice recording/playback, max 40 KiB and 120 s, subject to actual browser codec size limits. Clear 2-minute notes **cannot be guaranteed** under 40 KiB.
- Encrypted multipart transfer, bounded retries/progress, media cancellation, text priority between media segments, transfer diagnostics and receive-side replay/duplicate guards.
- Manual and automatic three-profile LoRa modes; temporary authenticated Fast for media, followed by synchronized restoration.
- Identical web settings, themes, conversation UI and local encrypted WebView IndexedDB history. That store is **separate** from a desktop browser's storage and the historical ESP-NOW Android app's database; migration is **not** implemented.

Android-native `navigator.serial` shim is a Web Serial-compatible interface to either **USB OTG** or **Bluetooth LE**, selected from the Connect dialog. Radio content remains encrypted by the original browser-side AES-256-GCM and authenticated over LoRa. Local web assets are loaded only from an HTTPS-identified offline WebView asset origin; INTERNET permission is not requested.

The original `MainActivity` Kotlin/Compose ESP-NOW app has **not been rewritten or deleted**. Android Back offers **Open original ESP-NOW app**. Its existing BLE firmware and independent ESP-NOW radio source remain separate.

## Build with Android Studio

1. Back up your existing installed OffGrid Android app and signing key before making changes. **Do not uninstall it** merely to upgrade; local history/keys may be lost.
2. In Android Studio, **Open `android/`**, choose JDK 17 and install Android SDK 36. The project specifies Android Gradle Plugin 8.13.2, Kotlin 2.4.20 and Gradle 8.14.4. The historical project does not include `gradle-wrapper.jar`; Android Studio or a compatible locally installed Gradle is required.
3. Sync dependencies, select `app` and **Build → Build APK(s)**. For a development build, use a device on Android 8+ (API 26 minimum) with current Android System WebView. APK location is normally `android/app/build/outputs/apk/debug/app-debug.apk` **after a successful local build**.
4. Install the APK on a test device. To install over your previous `com.offgrid.messenger` package, use the **same signing certificate** as before. Otherwise install on a separate test device or deliberately plan a backed-up migration.
5. The web/Android source uses no network permission and does not depend on a localhost server.

**Do not treat a green host test as an Android APK compilation.** Run the app on a physical phone with supported WebView, microphone, USB OTG and Bluetooth.

## Option A — USB OTG, no ESP32 reflash

- Both LoRa boards can remain on the user-confirmed **firmware v1.8.0-rc.4**; no radio or key migration is required.
- Use an Android USB OTG/data cable to attach one ESP32-C3 to the phone, power the other from a separate supply, launch the app and tap Connect → USB OTG.
- The USB adapter currently supports the ESP32-C3's native Espressif USB Serial/JTAG interface (VID `0x303A`). A CP210x/CH340 adapter is **not implemented** in this driver; do not mislabel it as supported.
- Confirm the firmware `READY`, existing pairing/encryption key and a short authenticated text before testing media, receipts or automatic modes.
- Test camera-roll photo selection and microphone permission in the native UI; media should go through the unchanged radio/USB ARQ path.

## Option B — Bluetooth LE, companion radio firmware required

**Important:** `FW=1.8.0-rc.4` intentionally disables BLE bulk media. To send **the same** photo/voice OGM4 traffic wirelessly over BLE, build and flash the optional companion firmware included in this release to **both** ESP32s (after backing up the known-working firmware). Its SX1278, USB, ARQ and browser protocol remain unchanged. Do not erase NVS.

From repository root:

```sh
cd firmware
idf.py set-target esp32c3
idf.py build
# Only after build succeeds, flash each board using its own physical USB port:
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM1 flash
```

- BLE uses the existing `OffGridLoRa-...` authenticated NimBLE GATT service. Enter the existing 256-bit shared key once in Android (64 hex or 43 base64url characters). Android encrypts that local copy using non-exportable Android Keystore AES-GCM.
- Provision/rotate that key over **physical USB**, not BLE. Firmware **never** permits `KEYGET` or `KEYSET` via BLE.
- Following fresh BLE HMAC challenge-response, companion firmware must advertise `CAPS|FAMILY=LORA|...|BULK=1|MEDIA=1` before Android opens a virtual serial port. Older BLE firmware is rejected with an explanatory error rather than allowing broken media.
- Companion BLE uses newline ASCII GATT records, bounded notification queues and negotiated ATT MTU; the binary GATT mode is **not** advertised. BLE performance and active-attack resistance need physical validation. BLE HMAC authentication at connection time is not a substitute for independently authenticated per-command control packets; RF profile changes remain additionally signed by the radio-control protocol.
- Do not switch radio power modes unless the actual two-node antenna, supply and RF regulatory conditions are suitable. 22 dBm is **not supported**.

## Host tests and status

```sh
# From repository root
sh firmware/tests/run-host-tests.sh
sh android/tests/v2/run-tests.sh
node android/tests/test_android_serial.cjs
python3 tests/test_android_v2.py
```

The host suite validates original AES-GCM, 48 KiB/40 KiB media, delivery/read receipt protocol, ARQ windows/retries, three radio modes, Android JS bridge, Kotlin framing/key conversion, BLE firmware command allowlist and unchanged original ESP-NOW Kotlin source. It does **not** substitute for compiling Android source against the Android SDK, compiling the companion ESP-IDF firmware, or two-device field tests.

See [Android feature/transport acceptance](../docs/ANDROID_UNIVERSAL_ACCEPTANCE.md) and [system roadmap](../docs/ROADMAP.md).
