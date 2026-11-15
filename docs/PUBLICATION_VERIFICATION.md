# Publication preparation — local verification report

**Scope:** documentation, contributor onboarding, `.gitignore`, GitHub Actions workflows/templates and a source-checksum manifest. No production C firmware, native Android Kotlin, JavaScript, browser HTML/CSS or protocol format has been modified by this public-source preparation step.

## Local checks executed

- `bash firmware/tests/run-host-tests.sh` — **PASS**. Covers portable C radio/ARQ tests, browser/crypto source contracts and native Kotlin protocol checks via included runner.
- `bash android/tests/run_native_radio_tests.sh` — **PASS**. Kotlin LoRa radio control parser, authenticated high-power safety contract and original Compose screen preservation checks.
- `python3 tests/test_public_docs.py` — **PASS** for required onboarding files and relative Markdown links (source-checked).
- GitHub workflow and issue-form YAML parsing/required-key validation using Python PyYAML — **PASS locally**. GitHub-hosted action runtime not yet checked.
- `git diff --check` and `git fsck --full --no-reflogs` — completed. Two dangling historical tag objects remain reported; no repository object corruption reported. Existing named release tags retained.
- Current tracked files and **711 reachable historical Git blobs** scanned for private-key blocks, GitHub tokens, AWS key patterns and Slack token patterns — no high-confidence signature matches; **this is not a complete forensic audit of every possible secret or PII format**.
- `sha256sum -c SOURCE_MANIFEST.sha256` — verified after staging all publication files and freezing the source manifest.

## Not performed / not implied

- No full ESP-IDF v6.1 firmware target build or Android SDK 36 Gradle APK build in this preparation environment.
- No GitHub-hosted CI execution; the repo owner must observe all three workflows after pushing.
- No physical two-board range/endurance/security/regulatory qualification in this preparation pass.
- No official signed APK or verified flashable firmware binary is distributed as a formal release asset.
- No blanket claim that every historical commit has a detailed body, or that every source module is audited/comment-perfect.

The maintainer has separately reported automatic switching working on hardware and an urban Max test of about 200–250 m. Those observations are not converted into reproducible qualification results by this repository-maintenance task.
