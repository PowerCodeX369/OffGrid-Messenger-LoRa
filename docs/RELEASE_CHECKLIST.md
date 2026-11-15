# GitHub publication and release qualification checklist

## Publication readiness (source repository)

- [x] Preserve full Git ancestry from the original LoRa `v1.0.0` tag; keep historical release tags immutable.
- [x] Add current root/firmware/Android/web installation documentation, architecture, hardware, troubleshooting and version index.
- [x] Keep historical notes separate from current setup instructions and document upstream ESP-NOW provenance.
- [x] Include GPL-3.0 license, security/support policy, contributing guide, code of conduct, issue/PR templates.
- [x] Add dedicated host, ESP-IDF target and Android APK build workflows. **Their success on GitHub still must be observed.**
- [x] Generate source-manifest checksums for the packaged tracked files and perform Git integrity checks.
- [ ] Create the GitHub repository under your own account; choose public/private visibility and review any repo metadata and files before pushing.
- [ ] Push `main` and historical tags; check default branch protection and GitHub Actions results on the hosted repository.
- [ ] Create GitHub release notes with clear **field-beta / not certified** status; avoid claiming shipped binaries unless built and verified.

## Qualification before a stable/production claim

- [ ] ESP-IDF 6.1 `esp32c3` target firmware builds on CI and target hardware.
- [ ] Android Gradle build succeeds on CI and physical phone, with signature-safe upgrades.
- [ ] Two-board synchronized Auto transition verification at each RSSI band and reverse transitions.
- [ ] Complete loss/recovery scans and real two-direction receipts tested repeatedly under weak RF.
- [ ] Android↔Android, Android↔web, web↔web messaging and media checks.
- [ ] Restart, NVS preservation, replay/rekey, queue persistence and endurance qualification.
- [ ] Regional RF legal assessment including the configured 433.000 MHz center and +18 dBm modes; implement any required airtime/duty controls.
- [ ] Threat-model review, private vulnerability reporting process, and disclosure of remaining security limits.
- [ ] Reproducible field metrics: reliability, data rate, range constraints, recovery latency and incident log.

**The final Git tag `v3.0.6` marks feature completion, not regulatory or hardware certification.** The separate publication-preparation tag `v3.0.6-public.1` is documentation/workflow-only; no source firmware, web runtime or Android Kotlin files were changed for this preparation step.

## Safe GitHub publication from Linux

```bash
# In the extracted repository root (verify first):
git status --short
git log -1 --oneline
git tag --list | tail
sha256sum -c SOURCE_MANIFEST.sha256

# Before making a public repository, review tracked content for real keys and private data.
# If no remote is configured, create your own repository and push from this exact directory:
gh repo create OffGrid-Messenger-LoRa --private --source=. --remote=origin --push
git push origin --tags
# When ready for public visibility (after private-data review):
# gh repo edit --visibility public --accept-visibility-change-consequences
```

The `gh` commands must be run by the GitHub account owner, on their machine. Avoid creating an empty README/license on GitHub during repo creation, which can introduce an unrelated initial commit. Never force push or change old tag targets to make a release look cleaner.
