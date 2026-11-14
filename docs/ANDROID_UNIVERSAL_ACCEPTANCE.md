# Android universal feature parity and hardware acceptance — source candidate

**Android**: `3.0.0-rc.1-universal-lora` · **Web content**: exact `1.10.0-rc.1` · **Existing LoRa firmware**: `1.8.0-rc.4` (USB) · **Optional BLE companion**: bundled `firmware/` tree; requires new flash on both boards.

## Protected original baseline

- The original ESP-NOW Kotlin/Compose client (`MainActivity`, `BleNodeClient`, existing camera/media/database code) is not rewritten. Open it via Android Back menu.
- The LoRa launcher is `LoRaActivity`; its HTML/CSS/scripts match the verified web UI, with only Android's `android_serial.js` inserted before other scripts.
- LoRa SX1278 RF driver, selective-repeat data engine, adaptive policy, radio profile values, media codecs, and existing USB command protocol are unchanged. Companion firmware additions affect authenticated BLE transport commands and forwarding only.
- No Wi-Fi or ESP-NOW radio initialization is introduced into LoRa firmware. Original ESP-NOW project remains a separate build.

## Required physical-phone tests — NOT COMPLETE

| Test | Steps / success criterion | Tested here? |
|---|---|---|
| Android compilation | Android Studio JDK17, SDK36; debug APK builds and launches on API26+ | **No** |
| Old ESP-NOW | Android Back → original app, pair with old ESP-NOW device and exchange texts/media | **No** |
| USB old firmware | Phone USB OTG → v1.8.0-rc.4 ESP32-C3; receive READY and pair without reflashing | **No** |
| BLE companion build | `idf.py build` ESP-IDF 6.1; clean flash on two boards, NVS preserved | **No** |
| BLE auth and caps | Enter key, pass HMAC challenge, see `BULK=1` and `MEDIA=1`, fail closed on old BLE firmware | **No** |
| Text and receipts | Two phones/radios, 10 small texts each direction, gray→double-gray→double-blue only after GCM validation | **No** |
| Photos | Pick 1 MB+ image, compress to ≤48 KiB, send and view verified image, retry packet loss | **No** |
| Voice | Microphone capture and Opus playback, variable length ≤40 KiB, 120 s time limit but size may stop early | **No** |
| Scheduling | Send text during media; text passes at segment boundary without losing media | **No** |
| Manual modes | One phone changes Fast/Balanced/Range and other board securely follows | **No** |
| Auto RF | Strong/weak link simulated/field-tested, synchronized transitions and hysteresis | **No** |
| Media Fast/restore | Send photo, synchronized Fast before media, restore old mode when complete/cancelled | **No** |
| Reconnects | Disconnect USB/BLE mid-photo, no false delivered tick or corrupted plaintext, retry supported | **No** |
| UI parity | Verify identical photo button, microphone controls, bubbles, gallery, audio player, settings in Android WebView | **No** |
| Long-running operation | 2+ hours messaging/media, permission denial, background/foreground and Android Doze | **No** |

## Host-only validation performed

C11 policy and ARQ tests, Node JavaScript Web Serial bridge simulation, Kotlin/JVM utility codec tests, browser crypto/media regressions, source byte-parity checks. Platform-dependent Android APIs require a real Android build/phone. Host syntax checks use stubs and are **not** ESP-IDF or Android toolchain builds.

## Security boundaries

No INTERNET permission; WebView remote navigation blocked, message bridge origin-scoped. AES-GCM application crypto still runs in the local copied browser client. Device pairing key is encrypted with AndroidKeyStore for BLE; app needs that key to decrypt (browser receives a local copy after authenticated BLE connection), so a compromised phone or WebView remains in threat scope. BLE key-export commands are denied in firmware. The initial BLE challenge authenticates the client, but per-command GATT MACs and persistent RF replay protection across power loss remain future security work. Do not describe those as audited or complete.

## Rollback

Keep copies of the **original v1.10 web and v1.8.0-rc.4 firmware** and the old Android app signing key. Do not erase radio NVS. USB mode can use the original firmware; reverting from BLE companion firmware restores the old authenticated text-only BLE behavior. Old ESP-NOW Android source is unchanged; app installation must preserve signing identity to retain user data.
