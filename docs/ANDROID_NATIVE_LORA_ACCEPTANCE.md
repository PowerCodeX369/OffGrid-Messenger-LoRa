# Native Android LoRa acceptance — v3.0.1-rc.1

See `android/README.md` for the exact supported BLE firmware and phone qualification procedure.

The old native Compose UI is unchanged. This is not the v3.0.0 WebView experiment. Bluetooth is still authenticated GATT; both radios require the BLE-bulk companion because rc.4 phone BLE data mode alone cannot carry 7,168-byte ciphertext sessions.

## Compatibility / constraints

- Android ↔ LoRa web: OGM4 and AES-256-GCM, 7,168-byte encrypted bulk, photo/voice mediaPart 12, receiptBatch 11, 48 KiB photo and 40 KiB voice.
- BLE authentication: original OffGridBLE2 challenge-response and original Android Keystore identity.
- Firmware/SX1278/ARQ: unchanged compared with preceding v3.0.0 companion. No Wi-Fi or ESP-NOW initialization.
- Original UI source: PremiumUi.kt, icon resources, camera presentation and navigation preserved. Android app activity is again the launcher.
- No hardware target build, radio-range, BLE GATT or Android APK execution proof in this package.
- Power/battery optimization remains last, **after Android hardware qualification**, per project roadmap.
