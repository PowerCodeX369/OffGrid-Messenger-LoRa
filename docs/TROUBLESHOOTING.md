# Troubleshooting — current v3.0.6

Always start with **both** devices' `STATUS` or `LINK` diagnostics; remove pairing keys, absolute addresses and private message content before posting logs. The old `FW=1.8.0-rc.4` marker is a deliberate compatibility constant; use feature-capability markers to recognize the newer firmware.

| Symptom | Investigate | Recovery without destroying data |
| --- | --- | --- |
| USB browser cannot see a port | Supported Chromium/Edge? USB data cable? Serial port busy? OS permissions? | Close serial monitors/other browsers, reconnect cable, re-grant Web Serial, check `ls /dev/ttyACM* /dev/ttyUSB*` |
| Browser shows old modes or wrong status | Cached scripts, mixed `web/` versions | Serve latest complete web directory and hard-refresh; don't mix JS from old releases |
| Android sees node but won't authenticate | BLE permissions, chooser identity, pairing key and matching firmware capabilities | Re-pair device without deleting saved key; use BLE diagnostics; ensure both boards have BLE bulk companion |
| Boards show connected but text never confirms | `PEER_ARQ`, `ARQ_CAP`, `ACK_COMPACT`, key and GCM compatibility | Update both boards and both client copies together; send one short Balanced text and inspect sender/receiver receipts |
| Android initially shows Balanced instead of Auto | **Expected**: Auto intentionally disabled at boot | Select Auto and complete operator high-power authorization on one client |
| No Max/Ultra change at weak signal | Auto not enabled, wrong power authorization, one peer stale, mode verification pending | Check **both** boards for `AUTO=ON` and `PROFILE|ACTIVE`; RSSI requires decoded authenticated RF packets |
| Auto stopped switching after complete radio loss | Missing RF means no fresh RSSI; search windows may not overlap | Wait for recovery scanning; bring radios temporarily closer, check antennas and profiles; don't assume a manual command crosses a broken link |
| Ultra works but photos/voice don't send | By design: Ultra official clients support text/receipts only | Move to Max/Balanced/Fast when the radio link and operator power authorization allow |
| Incomplete photos, duplicate texts or missing receipts | Check `BULK` statuses, media size, browser retry queue and ARQ timeout | Preserve encrypted outbox, collect diagnostics, retry with a smaller payload at close range |
| Firmware build fails or exceeds partition | Wrong ESP-IDF, wrong target, stale build, partition size | Use ESP-IDF 6.1, `idf.py set-target esp32c3`, `idf.py build`; collect build log, don't erase working NVS as a "fix" |
| Android update fails installation | Different package/signing key or Android SDK mismatch | Rebuild with same signing identity or plan a documented migration; don't uninstall blindly if history matters |
| Radio symptoms after wiring | Missing/common ground, 5 V wiring risk, disconnected antenna, weak 3.3 V | Disconnect power, verify board wiring and supply before retesting |

## Suggested diagnostic capture

1. Git commit/tag, device type, client version, Android OS/browser version and firmware `READY` feature flags.
2. Both peers' sanitized `LINK|`, `PROFILE|`, `AUTO|`, `RECOVERY|`, `ARQ_` diagnostics for a repeatable failure.
3. Precise user action, time, selected profile, expected and actual result, number and approximate size of test messages, whether two-way authenticated receipts occurred.
4. If applicable: `idf.py build` error, Gradle build error or a short BLE log filtered to the application tag. Do not attach full private histories or pairing keys.

See [issues](../.github/ISSUE_TEMPLATE/), [support](../SUPPORT.md), [hardware setup](HARDWARE_SETUP.md), and the [rc.3 acceptance guide](V3_0_6_RC3_RSSI_BANDS_AND_AUTORECOVERY.md).
