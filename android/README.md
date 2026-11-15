# Android app — native OffGrid Messenger v3.0.6 source

This is the **native Kotlin / Jetpack Compose** Android client, not the discarded WebView experiment. It communicates with the nearby ESP32-C3 through authenticated **BLE GATT**, while the radio firmware communicates with the remote node by LoRa. The original chat, media and pairing UI are preserved.

## Prerequisites

- Android Studio with an installed Android SDK **API 36**, JDK **17**, and a compatible Gradle build (Android Gradle Plugin **8.13.2**, Kotlin **2.4.20**, minimum Gradle **8.13**). See `build.gradle.kts` and `app/build.gradle.kts` for the pinned versions.
- A phone running Android **8.0+ (API 26+)**, Bluetooth enabled and the permissions requested by the application granted.
- Both ESP32 boards flashed with compatible current firmware. **BLE requires the firmware's authenticated bulk companion**, not an earlier BLE-advertising-only build.
- The same 32-byte secret used by both LoRa nodes; securely provision it using the documented USB/paired-app flow. Do not share it in issues or diagnostics.

## Build and install

1. Open the repository's `android/` directory (not the repository root) as a Gradle project in Android Studio; install API 36 if prompted.
2. Let Gradle synchronize, select the **app** module and build/run a **debug** APK on a connected device. No APK is bundled or signed as an official release.
3. Command-line alternative, after installing JDK/Android SDK/Gradle **8.13 or newer compatible version**:

   ```bash
   cd android
   gradle :app:assembleDebug --no-daemon
   # Debug APK is normally created under app/build/outputs/apk/debug/.
   ```

4. **Do not uninstall an existing deployment to upgrade it**. Android's Keystore and encrypted history can be lost. To update in place, the new APK must have the same package ID (`com.offgrid.messenger`) and **matching signing identity** as your installed app. Debug builds signed with a different key will not be a safe upgrade; back up appropriately before changing keys.

## Pair and use

1. Power the local ESP32 with antenna connected; launch OffGrid Messenger and grant BLE permissions.
2. Use the **Pair** device chooser to select the intended `OffGrid-...` ESP32. Bluetooth device identity is not the same as the LoRa/ESP-NOW Wi-Fi MAC; select the device the system chooser reports.
3. Provision/unlock the shared key using the existing app flow, then confirm **connected and authenticated** before sending.
4. Verify a short encrypted text and receipt in both directions using a second radio/client.
5. In the radio menu, the default state is **manual Balanced**. To use automatic LoRa range switching, select **Auto**, approve the explicit safety prompt for both radio boards and nominal +18 dBm, then verify both devices' active profiles.

**Auto bands:** RSSI > −100 dBm → Balanced; −130 through −100 inclusive → Max; below −130 → Ultra. **Ultra is text/receipt-only.** The device reports the actual firmware profile; BLE signal strength is not the LoRa RSSI. Fast remains an available manual/temporary-media mode. Auto intentionally does **not** persist across MCU reboot.

## Developer map and tests

- `app/src/main/java/com/offgrid/messenger/MainActivity.kt`, `PremiumUi.kt`: native screen and UI integration.
- `BleNodeClient.kt` and `BleSessionHolder.kt`: BLE link and session ownership.
- `RadioLink.kt`: LoRa controls and telemetry mapping.
- `OgmCodec.kt`, `EncryptedHistory.kt`, `SecureKeyStore.kt`: encrypted messages, history and key handling.

From repository root:

```bash
bash android/tests/run_native_radio_tests.sh
bash firmware/tests/run-host-tests.sh
```

These are **JVM/source tests**, not instrumented Android tests or actual phone BLE validation. See [testing](../docs/TESTING.md), [Android field acceptance](../docs/ANDROID_NATIVE_LORA_ACCEPTANCE.md), [troubleshooting](../docs/TROUBLESHOOTING.md) and [security boundaries](../SECURITY.md).

Past Android transition documentation is preserved in `android/PHASE_*` and [`docs/history/ANDROID_README_BEFORE_PUBLICATION.md`](../docs/history/ANDROID_README_BEFORE_PUBLICATION.md); do not mistake older version labels for the current APK source.
