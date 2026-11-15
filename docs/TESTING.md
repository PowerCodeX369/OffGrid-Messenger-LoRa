# Testing and acceptance — v3.0.6

## Levels of evidence

| Level | How tested | What it can establish |
| --- | --- | --- |
| Static/source tests | Host C, Node, Kotlin, source contract scripts | Deterministic policy, protocol and integration invariants |
| Target builds | ESP-IDF on ESP32-C3, Android Gradle SDK 36 | That source compiles into intended target artifacts |
| Integration hardware | Both programmed radios + native/web clients | Actual BLE/Web Serial, encryption, media and RF timing |
| Controlled field tests | Defined antennas, environment, interference and attenuator | Reconnection time, range, delivery, sensitivity and failure distribution |
| Security/regulatory qualification | Dedicated assessment / local rules | Deployment-risk findings; **not included** in host suite |

## Commands (Linux, from repo root)

```bash
bash firmware/tests/run-host-tests.sh
bash android/tests/run_native_radio_tests.sh
sha256sum -c SOURCE_MANIFEST.sha256
git diff --check
git fsck --no-reflogs
(cd firmware && idf.py set-target esp32c3 && idf.py build)   # needs ESP-IDF 6.1
(cd android && gradle :app:assembleDebug --no-daemon)  # needs Android SDK 36/JDK 17/Gradle
```

Source-manifest checks test packaged **file consistency** only; they do not certify origin or behavior. A Git tag does not prove firmware builds or RF performance.

## Minimum physical acceptance matrix

1. **Compatibility:** use matching `ARQ_CAP=4`, `ACK_COMPACT=16`, `AUTO_SCAN=TRIBAND` on both radios. Ensure your installed native app and web scripts are from the same release family. Verify the old `FW=` wire ID is expected, not evidence of an old source snapshot.
2. **Encrypted short text:** test web↔web, Android↔Android, Android→web and web→Android, both directions. Capture delivered/read receipts, retry count, duplicate displays and incorrect acknowledgments.
3. **RSSI Auto:** after approving `POWER18`, verify both radios agree on Balanced when RSSI >−100, Max for −100 through −130, Ultra below −130. Test exactly −99/−100/−130/−131 via host policy tests; physical attenuator conditions can be used for approximate on-air boundaries. Test reverse changes too.
4. **Radio silence:** interrupt and restore RF without USB/BLE disconnect. Verify repeated provisional profile scans and eventual *authenticated* recovery when RF becomes physically usable; record time and cases where recovery fails.
5. **Media:** confirm photo/voice outside Ultra, Fast temporary boost when negotiated, restore/bypass behavior, CRC and GCM checks, timeout and interruption handling. No media in Ultra on official clients.
6. **Process failure:** restart each board, Android process or browser during ciphertext queueing, transmission, receipts and reconnect; ensure no fabricated delivery or duplicate presentation.
7. **Endurance:** run sustained repeated messages and transitions, with logs; investigate crashes, watchdogs, reconnection delays, queue retention and ARQ timeouts.
8. **Security:** wrong keys, tampered RF frames, replay across reboot, key rotation interruption, unauthorized BLE control, NVS/data retention and responsible vulnerability disclosure.
9. **RF safety:** document antenna, location, transmit power, center frequency, occupied bandwidth and local airtime/duty conditions. Do not publish distance guarantees based on one anecdotal test.

## Recommended field test record

Keep a table with: source commit and tag, firmware image hash, Android build/signature, client revision, both board models, antenna geometry/gain, location **without personal addresses**, center frequency, BW, SF, CR, TX power, RSSI/SNR both directions, number of messages attempted/delivered/late/duplicated, p50/p95 latency, retransmits, recovered/not-recovered sessions and exact test duration.

**Current observations:** maintainer reports Auto mode working and Max Range about **200–250 m in an urban test**. This is encouraging **user-reported** evidence, not an independently reproduced qualification campaign. Firmware/Android target builds, full on-air matrix and compliance are not yet evidenced by the supplied archive.
