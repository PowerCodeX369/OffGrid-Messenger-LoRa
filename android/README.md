# OffGrid Messenger Android — original native app, LoRa transport

**v3.0.1-rc.1 (source candidate)**. The original native Android Compose application is again the **only launcher**. The experimental v3.0.0 WebView UI, alternate connection picker, and bundled HTML/CSS/JS screen have been removed. The original `PremiumUi.kt`, chat layout and theme, icons, native in-app camera, BLE pairing screens, background service, and encrypted local history are unchanged.

This release changes the underlying transport and protocol to interoperate with the **current OffGrid LoRa browser** without adopting its UI.

## Connections and companion ESP32 firmware

The phone still pairs through the **original Android BLE GATT client** (`BleNodeClient`, `BleSessionHolder`) using the same OffGridBLE2 HMAC challenge/response and the existing Keystore-wrapped 256-bit shared key.

**Yes, LoRa firmware still supports BLE:** the existing NimBLE service advertises as `OffGridLoRa-xxxx` with the same GATT UUIDs. HOWEVER, the user's proven **firmware v1.8.0-rc.4** advertises a BLE link but intentionally lacks BLE bulk media support (`CAPS|...|MEDIA=0`, and its command allowlist rejects bulk traffic). This native app requires the **opt-in BLE-bulk companion** from the included `firmware/` folder on both ESP32-C3 radios; the app checks fresh authenticated `CAPS|FAMILY=LORA|PROTO=2|BULK=1|MEDIA=1` before enabling Send. Without that companion it refuses to claim media support. No USB OTG or WebView UI was added.

Keep a backup of the working rc.4 binary, and do not erase NVS when flashing the BLE companion. It contains the same LoRa/SX1278 modulation, ARQ source, USB browser protocol and manual/automatic radio switching. Only the BLE bulk command allowlist, authenticated bulk forwarding and BLE notification queue behavior were extended (already present in the preceding v3.0.0 source release). Physical BLE GATT media behavior has not yet been tested on a phone.

## Original interface: no visual changes

The user still interacts with the native Compose chat application exactly as before: same chat list, composer, + photo picker, camera, push/tap microphone behavior, player, photo viewer, receipt ticks, dialogs, encrypted history and notification preferences. Old radio-rate UI labels have LoRa command mappings **beneath** the same controls:

| Existing button value | LoRa command |
| --- | --- |
| AUTO | `AUTO ON` |
| SPEED or LR500 | `PROFILE LOW` (Fast, SF7/BW500) |
| LR250 | `PROFILE BALANCED` (SF7/BW250) |
| RANGE | `PROFILE MAX` (SF9/BW125) |

This deliberately retains the old visible rate-label strings. The actual applied profile comes from firmware `LINK|...|PROFILE=...` records. Fast and Max at nominal +18 dBm require separate **hardware/power authorization**; this native app does *not* silently send `POWER18 VERIFIED` merely because a mode button is tapped. Physical USB authorization / a verified operator workflow is required. If the firmware refuses high power, the app reports it instead of faking synchronization.

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

The UI is verified byte-for-byte at source level against the original v3.0.0 preserved native Compose UI, not from device screenshots.

## Hardware acceptance test

1. Back up working ESP32 v1.8.0-rc.4 binaries and flash the included BLE-bulk companion to both boards, without NVS erase. Verify USB web chat remains unchanged.
2. Build the native Android APK locally, using the same package/signing identity. Connect one phone to one node by BLE; verify the original pairing screen and `CAPS|...BULK=1|MEDIA=1`.
3. Send text **Android→web** and **web→Android**, check genuine delivery/read ticks, duplicate suppression, wrong-key rejection and reconnection.
4. Send a 48 KiB photo and 40 KiB voice in both directions; verify byte limits, image detail, audio playback, media progress and SHA validation.
5. Test the existing manual/Auto RF control options and media Fast/restore on two boards, paying attention to RF power authorization.
6. Run repeated short texts during a photo, background/foreground, disconnect/reconnect and a 30-minute mixed-media soak. Record failures and BLE GATT diagnostics before treating v3.0.1 as stable.
