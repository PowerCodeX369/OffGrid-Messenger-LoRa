# OffGrid LoRa Android v3.0.3-rc.1 — radio synchronization and weak-link diagnostic fix

## Confirmed source defects

1. The distributed v3.0.2 Android Studio source still declared `versionCode=301` and `versionName=3.0.1-rc.1-native-lora`. An installed APK could not be unambiguously distinguished from the earlier app. This version declares `versionCode=303`, `versionName=3.0.3-rc.1-native-lora-rf`. **This source is not a prebuilt or tested APK.**
2. The LoRa auto controller could defer a downgrade for up to the 45-second cooldown. Its Fast/Balance thresholds (-106/-113 dBm) were later than the user's observed communication failure around -100 dBm; it also required 3 quality samples from each direction and two policy decisions. Both radios have to receive an authenticated control negotiation *before* the link fails.
3. `LINK|RSSI=` retained the last accepted packet's RSSI even after the ESP32-to-ESP32 RF link went silent. Android BLE could remain paired, which is not evidence of a working LoRa link.
4. The Android app logged `PROFILE|ERROR`, `AUTO|ERROR` and timeouts but did not consistently show the cause of failure in the native UI.
5. In Auto mode, MEDIA ON could request temporary Fast with unknown quality or after link quality had already deteriorated.

## What changed

- Retains the original Compose UI and its same three LoRa menu options plus Auto; Android now exposes the *existing* state and an Android Toast when radio-mode control fails.
- The early **field-tuned, conservative** RSSI triggers are Fast -> Balanced near **-84 dBm**, Balanced -> Max Range near **-88 dBm**, and (if already markedly weak) Fast -> Max Range near **-88 dBm**. These are trigger inputs to a smoothed RSSI + SNR + authenticated two-direction, two-observation policy, **not guaranteed switching boundaries**. Normal promotion to Fast requires sustained stronger link metrics, and a cooldown still prevents early promotion/oscillation.
- Only one coordinator proposes profile change, via the existing HMAC-authenticated OFFER/AGREE/new-profile verify/rollback. No unilateral one-radio mode hopping.
- Auto measurements become eligible after 2, not 3, authenticated samples from each side; it never switches using stale peer metrics. The failure of a complete RF link cannot be cured by an impossible two-node handshake.
- Media ON in Auto refuses to force Fast if either direction's quality is inadequate or stale. In manual mode, clearly weak authenticated local quality also blocks temporary Fast.
- Firmware `LINK` now includes `PEER_AGE_MS` and `RFHEARD` (received an authenticated peer frame within 30 seconds, **not a delivery guarantee**). `AUTO|...` diagnostic includes `QUALITY_READY` and measurement ages. Android renders stale peer readings as `LoRa peer silent` instead of stale RSSI.
- BLE `CAPS` and USB `READY` include `RF_POLICY=EARLY88`. The firmware continues advertising `FW=1.8.0-rc.4` because the existing web app **strictly checks that exact value**. The extra policy marker identifies the new companion build without breaking the working web client. Both ESP32s must run this updated companion build for coordinated behavior.
- Web source, original native Compose UI, AES-GCM, encrypted receipts, the 48/40 KiB codecs, SX1278/ARQ driver, radio packet layout, and BLE authentication UUIDs/key flow are unchanged.

## Hardware setup and test (required)

1. Backup the known-good firmware and Android app. Build `firmware/` using ESP-IDF 6.1 on your machine. Flash **both** ESP32-C3s at close range. Do not erase NVS.
2. Build `android/` as an APK in Android Studio using JDK 17/Android SDK 36, retaining your original signing key. Check installed app version *3.0.3-rc.1*, not 3.0.1. No APK is supplied or claimed as tested.
3. Keep both radios together at ~1 m. Check the Android menu says `Auto / Fast / Balanced / Max Range`, not ESP-NOW LR250/500. With confirmed hardware and legal power conditions, tap **Auto** and approve nominal 18 dBm for both boards. This is opt-in; manual selection disables Auto. Verify `AUTO|ON` at the initiator, `AUTO|PEER_ON` at the other radio, and `AUTO|...|QUALITY_READY=1` after enough authenticated samples. Don't attempt to enable Auto after the RF link is already lost.
4. Confirm text, image and voice work in both directions while close. Reconnect the unchanged web client to one board if desired; it should accept the `READY` string and display Auto correctly. Do not connect Android and USB simultaneously to the same physical node while comparing command outcomes.
5. Gradually separate the radios while sending **short encrypted text** periodically. Copy diagnostics from **both** nodes, especially: `LINK|...|RSSI=...|PEER_AGE_MS=...|RFHEARD=...|SNR_Q4=...|PROFILE=...|AUTO=...|ARQ_TIMEOUT=...|ARQ_RETX=...|CRC=...|BADTAG=...`, plus `AUTO|...`, `PROFILE|...`, `BULKSTAT|...`, and `RADIO_DIAG|...`.
6. Look for `AUTO|SELECT|BALANCED` followed by `PROFILE|ACTIVE|BALANCED`, then `AUTO|SELECT|MAX` and `PROFILE|ACTIVE|MAX` as the weak-link threshold is reached. Exact RSSI trigger points vary with smoothing, SNR and authenticated sample age. In Max mode throughput is much lower. It must still transmit *verified* text both ways.
7. If a link becomes totally silent before the switch, **move the radios closer**, restore an authenticated peer link, and retry. Do **not** independently force one board to another SF/BW: the other board will be deaf on its old profile.

## If it still dies near -100 dBm

The SX1278 datasheet typical sensitivity at 433 MHz is approximately -118 dBm (SF7/BW500), -122 dBm (SF7/BW250) and -131 dBm (SF9/BW125), under its stated test conditions. A -100 dBm reported packet should normally leave receiver margin. However `RSSI` is only the last successfully authenticated reception and tells nothing by itself about the *other direction*, link age, channel interference, antenna mismatch, RF front-end damage, supply droop at 18 dBm, or BLE/RF application queues. We need `PEER_AGE_MS`, SNR, CRC/BADTAG, ARQ timeout/retry and profile events **from both nodes** to distinguish these. Do not interpret phone BLE connectivity as LoRa connectivity.

## Validation status

- Host C policy, wire, selective-repeat, BLE allowlist, Kotlin native protocol, and browser encryption/media regressions: PASS.
- ESP-IDF target compile: NOT RUN in this environment.
- Android Gradle APK build: NOT RUN in this environment.
- 2-node physical switching/range test: NOT RUN. Thresholds are informed by the reported failure and are not hardware-qualified.
