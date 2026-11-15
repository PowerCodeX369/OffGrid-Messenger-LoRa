# v3.0.6-rc.3 — Exact RSSI bands and automatic recovery

**Status: source/host-tested release candidate. Not an on-air validated final release.**

## Scope

This release directly addresses a reported failure to leave Balanced automatically when the radios move apart. The previous policy had early -88/-113 dBm threshold triggers, Fast-first promotion, RSSI smoothing and prolonged waits. The previous link-loss rendezvous explored Balanced from Max/Ultra but could not move *out of silent Balanced*. Neither Android nor the browser may safely reconfigure SX1278 by observing BLE/USB status alone: only the single ESP32 radio owner controls hardware.

## Requested exact band mapping (automatic mode only)

| Fresh authenticated LoRa RSSI | Target on both radios |
| --- | --- |
| **> -100 dBm** | **BALANCED** (SF7, 250 kHz) |
| **-100 through -130 dBm, inclusive** | **MAX** (SF9, 125 kHz) |
| **< -130 dBm** | **ULTRA** (SF12, 125 kHz, text/receipts only) |

Crossing either boundary in either direction changes the requested target without a legacy 45-second RSSI cooldown or Fast-first promotion. Use the worse radio direction when a recent authenticated peer metric exists; otherwise a fresh local measurement can trigger. Readings come from authenticated LoRa frames. Missing packets have **no measurable RSSI** and cannot constitute an instantaneous trigger. Radio switching itself takes authenticated OFFER / AGREE / confirm exchanges and physical airtime, especially at SF12. Exact-threshold operation can flap near a boundary; this is the explicitly requested no-hysteresis policy.

## Lost-link behavior

- After a lack of authenticated radio packets while Auto is active, the firmware starts provisional search. Balanced enters Max search (18 seconds), then Ultra (30 seconds); Max probes Ultra then Balanced; Ultra probes Max then Balanced. Scan windows and the 10+ second home interval repeat. These windows are **best effort**; asynchronous clocks, prolonged collision, radio silence or an unreachable peer can prevent discovery.
- Scans use the same already-authenticated RF and hardware driver. On receiving a cryptographically verified peer frame at a candidate modulation, the node adopts that profile locally, re-advertises capabilities and resumes normal Auto decisions. If nothing authentic is heard, the node returns to its former profile. In Ultra recovery scans, discovery HELLOs are spaced by 7.5 seconds to leave SF12 receive windows between transmissions. The other node must eventually search or transmit on a compatible profile for contact to recover. This is not an omniscient way to control a physically out-of-range peer.
- An abandoned temporary Fast media boost no longer blocks automatic lost-link recovery: when its peer becomes unresponsive, it releases the boost and probes Balanced and Max.
- An abandoned unconfirmed encrypted transfer is failed/requeued by the existing browser/native outbox; no false delivery/read acknowledgment is emitted.
- The old manual Balanced fallback remains when Auto is not armed. Auto still requires explicit authorization of nominal +18 dBm and authenticated peer consent, and is deliberately not persisted across reset.

## Deploy both boards and both clients

1. Save the working `v3.0.6-rc.2-compact-arq` Git ZIP or tag. Do not delete its tag/history.
2. In the repository `firmware/` folder, use ESP-IDF 6.1 to `idf.py set-target esp32c3`, `idf.py build`, then `idf.py -p <YOUR_PORT> flash monitor` **for each ESP32**. Do not `erase-flash`, `erase_nvs` or change the existing key. USB and BLE encryption/keys are unchanged.
3. Replace `web/` on **both** browser computers and hard-refresh; open the updated `android/` project in Android Studio and install the new app **as an upgrade**, not by uninstalling and erasing local key/history. The original Compose chat UI is retained.
4. Position the two boards close together with proper 433 MHz antennas. Confirm their same pairing key, `READY` has `AUTO_SCAN=TRIBAND|AUTO_RSSI=-100,-130` and `ARQ_CAP=4|ACK_COMPACT=16`, and `LINK|...|PEER_ARQ=1` on both boards.
5. Prove encrypted Balanced text and delivered/read receipts in **both** directions. On **one** browser or native app, select **Enable Auto**, approve both radios' antenna/supply/+18 dBm and local legal limits, then wait for `AUTO|ON` and peer `AUTO|PEER_ON`. No second click is required on the peer. **AUTO OFF/manual profiles deliberately stop automatic switching.**
6. Observe `AUTO|SELECT|MAX|REASON=RSSI_BAND` around the -100 boundary and `AUTO|SELECT|ULTRA|REASON=RSSI_BAND` only below -130. Both boards must confirm `PROFILE|ACTIVE|MAX` / `PROFILE|ACTIVE|ULTRA`, with matching SF/BW. During recovery look for `RECOVERY|SCANNING|MAX`, `RECOVERY|SCANNING|ULTRA`, then `RECOVERY|AUTHENTICATED|<mode>|PEER_RESTORED` (or the ARQ variant). Scanning is not confirmation.
7. As the link improves, ensure `ULTRA→MAX` occurs at -130 (inclusive), then `MAX→BALANCED` above -100, with authenticated delivery receipts in both directions. Do not use only the local RSSI label as proof of successful mode synchronization. Check both devices' `PROFILE|ACTIVE` statuses.
8. Test stale metrics and a forced connection break separately. If the peer is completely out of RF range, the firmware can search but **cannot force the distant device to receive a command**. Recovering communication requires both radios to physically re-enter usable RF range.

## Diagnostics and hardware constraints

From either app request `STATUS`. The response includes `LINK|...|RSSI=...|PEER_AGE_MS=...|RFHEARD=...|PROFILE=...|AUTO=...` and `AUTO|...|BALANCED_GT=-100|MAX_FROM=-130|ULTRA_LT=-130|RECOVERY=...`. Confirm that the *other* ESP32 is on the same profile, not merely that the local UI predicted a target. Ultra is text/receipts only, not photos or voice. All high-power modes still need suitable antennas, supplies and compliance with local spectrum rules.

## What changed and what did not

- Changed: `firmware/main/lora_auto.h`, authenticated radio-task Auto decision/recovery, `firmware/main/lora_link_recovery.h`, web labels and Android native radio telemetry/status text, tests/release docs.
- Unchanged: OGM2 packet layout, AES-256-GCM application encryption, HMAC keys, ARQ compact ACK and SF9/SF12 modem configuration, SX1278 driver, original Android Compose UI, BLE authentication, NVS data, photo/voice codecs and stored message formats.
- Source-host acceptance: run `sh firmware/tests/run-host-tests.sh`, `sh android/tests/run_native_radio_tests.sh`, `sha256sum -c SOURCE_MANIFEST.sha256`, `git fsck --no-reflogs`, and `git status --short` from repository root. The manifest is a source archive snapshot, not proof of hardware success.
- **Not verified here:** ESP-IDF target compilation, Android Studio APK generation, two actual boards negotiating under field fading, airtime/collision performance, claimed range, or automatic recovery time under real loss. Do not call this stable until those pass.

## Rollback

If a hardware test regresses, revert **both** ESP32s and both clients to the rc.2 release together. Keep existing NVS/key data; do not flash mismatched compact-ACK versions. The release tag and full Git history in the archive are preserved so rollback requires no history rewriting.
