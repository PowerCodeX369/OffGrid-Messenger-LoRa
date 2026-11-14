# OffGrid Messenger LoRa v3.0.4-rc.1 — SF12 Ultra Long Range

**Based on the user's full, working v3.0.3-rc.1 Git archive. This is a fully integrated SOURCE release candidate, not a flashed or field-validated firmware/APK.**

## Profiles and restrictions

| UI selection | SX1278 | Power authorization | Content |
|---|---|---|---|
| Auto | Dynamic authenticated two-way profile negotiation | Operator approves both boards' nominal +18 dBm suitability | Text everywhere; media only while *not* Ultra |
| Fast | SF7/BW500/CR4/5 | Nominal +18 dBm verified | Text, photos, voice |
| Balanced | SF7/BW250/CR4/5 | +10 dBm manually | Text, photos, voice |
| Max Range | **UNCHANGED** SF9/BW125/CR4/5 | Nominal +18 dBm verified | Text, photos, voice |
| **Ultra Long Range** | **NEW** SF12/BW125/CR4/5, CRC + low-data-rate optimize | Nominal +18 dBm verified | **AES-256-GCM text and encrypted receipts only** |

Photos and voice cannot be sent in Ultra. Android/Browser send controls block them, pending media stops at a safe segment boundary if Auto enters Ultra, and the ESP32 rejects `BULKBEGIN ... MEDIA`/`MEDIA ON` in Ultra. The firmware cannot inspect encrypted ciphertext's plaintext type: a custom malicious client can mislabel a media packet as text; this restriction is enforced for the supplied official clients and honest type tag. Do not use Ultra as a file channel.

## Reliability and timing

- SF12/BW125 LoRa symbol is 32.768 ms. Explicit-header and CRC registers are derived by the existing validated SX1278 driver; `RegModemConfig3` low-data-rate optimization is enabled whenever symbol time exceeds 16 ms.
- Existing six-packet/224-byte Fast/Balanced/SF9 selective-repeat behavior is preserved. Ultra has a separate HMAC-covered fragment-size flag and 48/64/80/96-byte ciphertext fragments, with one- or two-frame send windows when links weaken; this bounds the damage of a lost long-airtime packet. Packet contents remain AES-256-GCM encrypted and radio frames + bitmap ACKs HMAC-SHA256-128 authenticated.
- Ultra ACK deadlines use actual SF12 packet airtime **starting after TxDone** rather than treating RF transmit time as a timeout. Extra Ultra retries (8 vs 5) and longer app delivery timeout apply to Ultra only, without falsely creating delivered/read ticks. For context, at SF12/BW125 a nominal 64-byte packet needs ~2.79 s of RF airtime and 38-byte bitmap ACK ~1.97 s before USB/BLE, application, contention, or retry overhead. **Ultra is low-throughput by physics**, not a photo/voice link.
- Auto can request Ultra from SF9 Max Range on **fresh authenticated** worse-direction RF metrics around -113 dBm or SNR < -5 dB and packet loss, before complete RF disappearance. To return to SF9 the link must improve substantially for ~120 seconds; 2-direction authenticated negotiation, HMAC control, peer capability checks, phase validation and rollback remain required. Manual Ultra also uses the same synchronized OFFER/AGREE/HELLO process, and the phone permits more time for this exchange.
- The new ARQ_CAPS=2 is authenticated and required from the **other board** before entering Ultra. Mixed v3.0.3/4 boards will reject Ultra rather than end up on different spreading factors. **Both ESP32 boards must run v3.0.4 companion firmware.**

## What is kept intact

- The original SF9/BW125 profile, all three existing profiles, Android native Compose chat appearance except one new radio-menu option, local encrypted history, BLE pairing/Keystore, 48 KiB photo and 40 KiB voice codecs, temporary Fast/restore outside Ultra, and existing USB/Web Serial behavior.
- No Wi-Fi/ESP-NOW initialization. Browser OGM4 AES-GCM encryption, delivery and read acknowledgments stay the same. Android remains the native BLE app, not a WebView clone.
- Base firmware `FW=1.8.0-rc.4` remains as an existing strict browser compatibility marker. The new firmware identifies its distinct capabilities via `PROFILES=4`, `RF_POLICY=ULTRA12`, and `ULTRA=SF12_TEXT_ONLY`. Keep source-archive version/tag clear; don't mistake the `FW` wire marker for the actual Git release.

## Upgrade with rollback

1. Back up your confirmed working **v3.0.3** Android project/APK, its signing key, and **both** existing ESP32 firmware images plus NVS keys. Never erase NVS for this upgrade. Existing encrypted local app history stays tied to the original Android signing identity.
2. On your ESP-IDF 6.1 machine, open included `firmware/`, build target ESP32-C3 and flash both boards (with antennas attached, valid supplies). Flash first while they are **near each other**; avoid enabling Auto/high-power until physical supply, antenna, regulatory band/duty limits have been checked.
3. Build/install included `android/` in Android Studio with original package ID/signing key. The source declares **`versionCode=304`, `versionName=3.0.4-rc.1`**. Do not uninstall the old app without backing up history/keys.
4. Deploy included entire `web/` directory to both browser clients and hard-refresh; the new web client expects the new `READY` capability marker. Don't mix one old web UI with new firmware to diagnose mode buttons.
5. Confirm `CAPS|...|ULTRA=SF12_TEXT_ONLY` and `READY|...|PROFILES=4` before using Ultra. Verify existing SF9 Max Range transfers and automatic handover still work close-range, then try Ultra text in **both directions** and verify delivered/read receipt ticks.
6. In Ultra, photo and voice controls must be disabled. Send a short text and allow several seconds (potentially tens of seconds under retries) for each message/ACK. Do not spam multiple messages while verifying the link.
7. Gradually increase range while logging `RSSI`, SNR, `PEER_AGE_MS`, `RFHEARD`, CRC errors, ARQ retries and timeout counts **from both radios**, plus `AUTO|SELECT|ULTRA`, `PROFILE|ACTIVE|ULTRA` and encrypted receipts. Do not infer delivery from a stale RSSI or BLE connection.
8. If the link becomes completely silent, bring the radios closer while preserving one synchronized profile; a handshake is impossible if neither can receive the other. Revert **both** boards, the Android build and web client to v3.0.3 together if testing exposes a regression.

### Build commands

From `firmware/` in the ESP-IDF 6.1 environment:

```sh
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM1 flash
```

Use the actual port assignments shown by your operating system and validate each completed flash. No `erase-flash` or NVS wipe is requested. Use Android Studio for the Android APK; a prebuilt APK is **not included** in this source release.

### Security, compliance and limitations

A radio that is physically receiving weak periodic frames may still fail to decode encrypted data or return ACKs; no algorithm can guarantee delivery through a lost/interfered link. SF12 slow airtime and legal duty-cycle / band-output limitations are significant. **This change does not implement persistent RF-control anti-replay across ESP32 reboots** or atomic key migration. No independent security audit, full ESP-IDF 6.1 target build, actual Android SDK/APK build, or real two-board field range/throughput test has occurred in this execution environment. Host C/Kotlin/Node and source tests are not hardware certification.

See `docs/V3_0_4_ULTRA_TEST_RESULTS.md` for host coverage and pending acceptance gates.
