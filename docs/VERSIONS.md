# Release history, compatibility and reproducibility

**No earlier Git commits or tags have been rewritten.** The LoRa repository contains the `v1.0.0` history and subsequent versions up to feature-complete `v3.0.6`. Its pre-LoRa ESP-NOW ancestry is documented in [`PROVENANCE.md`](PROVENANCE.md), not inserted into the LoRa Git graph.

## Current baseline

- `v3.0.6`: finalized source milestone at the same code as `v3.0.6-rc.3-rssi-auto` (`e840802`).
- `v3.0.6-public.1`: current public-source documentation/CI preparation; **no firmware, web runtime, Android app or protocol changes**.
- **Both radios and both client source folders must match the intended compatibility family.** For Max/Ultra the peer must authenticate `ARQ_CAP=4` and understand the compact ACK format. `FW=1.8.0-rc.4` is deliberately retained as a USB/BLE *wire compatibility* identifier; don't equate it to the current source tag.
- **Auto is opt-in** with operator power approval. Historical releases may have different thresholds, capabilities and client interfaces.

## Historical tags found in the Git archive

The following rows are based on actual Git tag targets, not reconstructed from filenames or version marketing descriptions. Use [`CHANGELOG.md`](../CHANGELOG.md) and the individual `docs/V*` notes to understand each tagged change. You can see each exact commit with `git show <tag>`.

| Tag | Commit target |
| --- | --- |
| `v1.0.0` | `d160b16` |
| `v1.1.0-rc.1` | `357ac73` |
| `v1.2.0-rc.1` | `f46ba78` |
| `v1.2.0-rc.2` | `1842342` |
| `v1.5.0-rc.1` | `c4a794d` |
| `v1.6.0-rc.1` | `7e4aeda` |
| `v1.6.1-rc.1` | `e866ff4` |
| `v1.6.1-rc.2` | `eac6181` |
| `v1.7.0-rc.1` | `9545e85` |
| `v1.8.0-rc.1` | `686347b` |
| `v1.8.0-rc.2` | `e658185` |
| `v1.8.0-rc.3` | `197f9fd` |
| `v1.8.0-rc.4` | `a5c1a08` |
| `v1.8.0-rc.5` | `de71d44` |
| `v1.8.0-rc.6` | `f477b2d` |
| `v1.10.0-rc.1` | `b8dfe12` |
| `v3.0.0-rc.1-android` | `24a7fae` |
| `v3.0.1-rc.1-android-native` | `9c8cf35` |
| `v3.0.2-rc.1-android-native` | `6583722` |
| `v3.0.3-rc.1-android-native-rf` | `778b130` |
| `v3.0.4-rc.1-ultra` | `8801b1d` |
| `v3.0.5-rc.1-final-candidate` | `0592bfe` |
| `v3.0.5-rc.2-web-diagnostics` | `6f6faed` |
| `v3.0.5-rc.3-arq-caps-fix` | `b615e7c` |
| `v3.0.5-rc.4-ultra-sync` | `43e3e54` |
| `v3.0.6` | `e840802` |
| `v3.0.6-rc.1-link-recovery` | `c8e3571` |
| `v3.0.6-rc.2-compact-arq` | `1f5abf3` |
| `v3.0.6-rc.3-rssi-auto` | `e840802` |

## Safely examine or build an old version

```bash
git log --oneline --decorate --all --graph -25
git tag --list
# Prefer a second worktree rather than overwriting an in-use checkout:
git worktree add --detach ../offgrid-previous v3.0.6-rc.2-compact-arq
# When finished: git worktree remove ../offgrid-previous
```

Never install mixed, protocol-incompatible firmware versions just to compare UI screens. Two-node rollback means reverting **both boards** and associated clients as one coordinated operation while preserving the pairing keys and NVS. Do not change old tag targets or rebase published commits.

[Current setup](../README.md) · [Architecture](ARCHITECTURE.md) · [Known limitations](KNOWN_LIMITATIONS.md) · [Release checklist](RELEASE_CHECKLIST.md)
