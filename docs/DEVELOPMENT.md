# Development and contributor setup

**Branch policy:** `main` is the integrated history. Develop changes in a new branch; preserve existing tags and commit ancestry. Historical source is reproduced by Git tags, not by editing archived release notes.

## Linux prerequisites

- Git, Python 3, Bash, Node.js 22, a C compiler (`cc`), GNU coreutils including `sha256sum`, and JDK 17 / Kotlin compiler for applicable tests.
- For actual ESP32 images: ESP-IDF **v6.1** configured for `esp32c3` (and the Espressif cross-toolchain).
- For the Android APK: Android SDK **36**, JDK **17**, Gradle **8.13+** compatible with the pinned AGP **8.13.2** and Kotlin **2.4.20**. The repository does **not** currently vendor a Gradle wrapper; use Android Studio or install Gradle yourself.
- For browser end-to-end UI/USB: Chrome/Edge with Web Serial and physical ESP32s, or the optional existing browser test dependencies.

## First checks

```bash
git switch -c fix/describe-your-change
bash firmware/tests/run-host-tests.sh
bash android/tests/run_native_radio_tests.sh
sha256sum -c SOURCE_MANIFEST.sha256
git diff --check
```

Build commands:

```bash
(cd firmware && idf.py set-target esp32c3 && idf.py build)
(cd android && gradle :app:assembleDebug --no-daemon)
```

Do not silently claim builds passed if the required SDK is not installed. CI defines separate host, ESP-IDF and Android jobs and should report each independently.

## Where to make a change

- **RF/power/timing:** `firmware/main/` plus driver/ARQ components; write portable C boundary tests. Every profile change needs authenticated peer agreement, an explicit rollback story and hardware testing.
- **Android BLE/UI:** `android/app/src/main/java/com/offgrid/messenger/`; preserve original native UI and Keystore identity; add JVM source tests and instrumented tests where applicable.
- **Web client:** `web/`; test with actual browser/Web Serial and verify it interoperates with Android rather than assuming JS-only test success.
- **Crypto/wire:** describe protocol version negotiation and how older clients fail safely. Do not weaken AES-256-GCM, HMAC, receipt authentication or deduplication.
- **Documentation/CI:** use focused commits; keep all latest installation guides accurate, archive prior context instead of erasing historical records.

## Changes and review checklist

1. Open an issue for substantial protocol/security changes; state whether firmware, Android, web or multiple layers must upgrade together.
2. Make a focused patch with clear commit subject and body if the change is nontrivial. Add regression tests to the suite before marking it fixed.
3. Update [`CHANGELOG.md`](../CHANGELOG.md), relevant component guide, and compatibility/release notes when behavior changes.
4. Confirm no pairing keys, NVS dumps, device addresses, personal data, logs with secrets, generated binaries or dependencies are accidentally added to Git.
5. In the pull request record **exact test commands and outputs**; differentiate host tests, target builds, hardware pairing, two-board RF, range/throughput and any untested areas.
6. Keep old tags immutable; don't force-push/rewrite the published history. For a source release regenerate `SOURCE_MANIFEST.sha256` after staging changed files (never include the manifest itself) and check it.

## State-machine invariants to protect

- Only authenticated control traffic can cause peer-accepted radio profile changes; both boards must eventually agree on the active mode or expose failure.
- Only authenticated/decrypted application acknowledgments can indicate delivered/read; a radio ACK is insufficient.
- A message displayed twice after a retransmission is a regression; persist stable message IDs and deduplicate on receive.
- Preserve stored keys/NVS and encrypted history when upgrading; never add an automatic destructive reset.
- +18 dBm authorization is a real operator safety gate, not a background optimization toggle.

References: [architecture](ARCHITECTURE.md), [test plan](TESTING.md), [release checklist](RELEASE_CHECKLIST.md), [contributor rules](../CONTRIBUTING.md).
