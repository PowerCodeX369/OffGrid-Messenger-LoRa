# Contributing to OffGrid Messenger LoRa

Contributions are welcome, especially **reproducible RF/Android integration results, tests, documentation and bug fixes**. This is a feature-complete field-beta source project, **not** yet safety-certified. Maintainer attribution and upstream ESP-NOW provenance: [`docs/PROVENANCE.md`](docs/PROVENANCE.md).

1. Read the [development guide](docs/DEVELOPMENT.md), [architecture](docs/ARCHITECTURE.md), [test matrix](docs/TESTING.md), and [known limitations](docs/KNOWN_LIMITATIONS.md). If you're new, start with documentation, host tests or a labeled issue rather than changing live RF power or cryptographic framing.
2. Search existing GitHub issues before reporting a new bug. File a bug with the issue template, exact tag/commit, reproducible steps, **sanitized** logs and a clear statement of whether you tested on real hardware.
3. Fork and branch from `main` (`fix/...`, `docs/...` or `feat/...`). Submit focused pull requests with meaningful commit subjects and, for nontrivial patches, explain *why* in the commit body. Do not rebase/rewrite already-published project history or retarget historical tags.
4. Run `bash firmware/tests/run-host-tests.sh` and `bash android/tests/run_native_radio_tests.sh` from the repository root. Firmware changes also need an ESP-IDF 6.1 `esp32c3` build; Android changes need a real Android SDK build. For on-air RF changes, record the two-node test environment and results. State clearly what wasn't tested.
5. Keep the radio worker as the single SX1278 SPI owner, preserve authenticated HMAC controls, AES-256-GCM application messages, real delivered/read receipts, original native Android UI and key/NVS compatibility. Changes to RF profile switching must coordinate *both* radios and include an explicit backward-compatibility/rollback plan.
6. Do not silently weaken power authorization, assume that 433.000 MHz/+18 dBm is legal everywhere, replace Ultra's text-only media policy without design review, or claim that host tests prove field reliability.
7. Update relevant current docs and [`CHANGELOG.md`](CHANGELOG.md). Review the [PR template](.github/PULL_REQUEST_TEMPLATE.md) and explain upgrades of firmware, Android and web together when required.

**Never submit:** real pairing keys, `KEYDATA`, NVS dumps, decrypted chats, exact home/device addresses, unredacted GATT traces or credentials. For vulnerabilities, use [private reporting instructions](SECURITY.md) rather than a public exploit issue. Please follow our [code of conduct](CODE_OF_CONDUCT.md).

Your first PR can simply repair documentation or add a deterministic regression test. You do not need two radio boards for every code/doc contribution, but you must not label untested hardware claims as verified.
