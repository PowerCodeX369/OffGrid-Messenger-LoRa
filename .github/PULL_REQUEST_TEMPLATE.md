## Summary and motivation

Describe the problem and scope (firmware / native Android / web / tests / docs).

## Changes and compatibility

- [ ] Source changes are focused and documented; no unnecessary refactor.
- [ ] No breaking protocol/key/NVS change, **or** I documented a coordinated two-board/client upgrade and rollback.
- [ ] App encryption, genuine delivered/read receipts and RF authentication are preserved.
- [ ] Historical Git tags/commits were not rewritten.
- [ ] No actual private keys, personal IDs, private messages or NVS dumps were committed.
- [ ] `CHANGELOG.md`, relevant README(s) and any version/compatibility docs are updated when needed.

## Tests performed (only check what actually ran)

- [ ] `bash firmware/tests/run-host-tests.sh`
- [ ] `bash android/tests/run_native_radio_tests.sh`
- [ ] ESP-IDF 6.1 `idf.py build` for `esp32c3`
- [ ] Android SDK 36 Gradle `:app:assembleDebug`
- [ ] Two physical ESP32 boards (RF/Auto/ARQ)
- [ ] Native Android BLE device
- [ ] Desktop browser + Web Serial
- [ ] Both client directions and real authenticated receipts
- [ ] Long-range or soak testing, with conditions and measurements

**Exact commands, outputs, and tests not performed:**

## RF / security impact

Any implications for legality, frequency, output power, duty cycle, keys, replay, wire compatibility or failure/reconnection state?

## Evidence

Sanitized logs, issue references and screenshots without personal/device secrets.
