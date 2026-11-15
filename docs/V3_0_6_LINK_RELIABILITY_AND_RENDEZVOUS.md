# OffGrid Messenger LoRa v3.0.6-rc.1 — automatic recovery, link adaptation and field acceptance

## Why the v3.0.5-rc.4 test failed

The user's field report: Auto stayed on Balanced while moving away; the authenticated LoRa peer was lost; the ESP32s did not rediscover one another even after returning to near-zero distance. Existing v3.0.5 diagnostics can distinguish BLE/USB connection from an authenticated RF peer. The exact failed-frame timeline was **not** captured on both radios in this field report, so the source-level root causes below are concrete *risk paths*, not claims of a measured RF failure.

Inspection of rc.4 found:

1. `og_auto_decide()` required fresh authenticated quality data from *both* radios. `AUTO_METRIC` reports were queued at discovery priority and could be discarded when a peer had recently been heard. At a deteriorating link, a missing remote report could prevent downgrades even when the local authenticated measurements were poor.
2. An old pending ciphertext transfer could prevent both sides of the signed profile handshake, causing a prolonged RF adaptation veto.
3. If two radios ended on different modulation profiles after a partial change/reboot/rollback, they had no standardized way to look for each other on the common boot mode. Two different SF/BW profiles cannot receive one another, regardless of physical distance.
4. A crashed client could leave a temporary Fast media boost active for up to ten minutes, suppressing loss recovery.
5. If two radios were already on the common Balanced profile and the SX1278 continuous receiver became wedged, there was no timed and bounded software receive re-arm path.

## What v3.0.6 implements

**Authenticated local-quality safety fallback:** Keep the original two-way RSSI/SNR policy for promotions and ordinary mode decisions. When the peer previously authenticated Auto permission but its quality report becomes missing/stale, recent locally *authenticated* weak readings can still propose a DOWNGRADE. It cannot unilaterally change the peer radio, cannot promote without both directions, and does not enable Auto or nominal +18 dBm without the existing explicit approval. All changes retain the signed OFFER/AGREE/verified new-profile protocol. Authenticated RF loss counters are additional input, not a fabricated RSSI value.

**Recovery rendezvous after prolonged silence:** On a non-Balanced profile, after 45 seconds without an authenticated peer (80 seconds in SF12 Ultra), temporarily listen on the **same** SF7/BW250 Balanced profile used on boot. A 15-second probe window sends authenticated HELLOs approximately every 2.5 seconds; if no valid peer is received, return to the original profile and try again after a 10–14-second dwell. This preserves the possibility of finding a remote node still on SF12, while providing an overlap to rediscover a peer that rebooted to Balanced or rolled back after a failed profile change. If an authenticated peer is received during a Balanced probe, lock to Balanced and advertise `RECOVERY|AUTHENTICATED`; never fake delivery or accept unauthenticated frames. Recovery may still take multiple scan cycles when nodes are unsynchronized. No guarantee is possible beyond radio coverage or with mismatched pairing keys.

**Safe pending-data handling:** Old unconfirmed encrypted transfers are explicitly canceled rather than being marked delivered when an urgent safety downgrade or radio recovery is necessary. The existing Android/web encrypted outbox may retry the same application message ID later, with re-encryption and duplicate suppression. The firmware cannot promise unattended delivery when the client is closed.

**Radio receive re-arm:** On Balanced after 120 seconds of authenticated-peer silence, and with no pending transfer, reapply the same radio profile no more than once per 120 seconds to restart continuous RX. This is a bounded recovery measure, not a hardware reset, and does not erase pairing keys or change frequency.

**Media recovery:** An abandoned temporary Fast session no longer blocks discovery indefinitely. After prolonged silence, its state is cleared and recovery can proceed; if a client was connected during that loss, it should treat unconfirmed media as not delivered.

**Control priority and status:** Periodic Auto metrics are coalesced rather than queued indefinitely, and are not discarded merely because the peer was recently heard. Radio ACKs still take priority. Diagnostic lines identify `RECOVERY|SCANNING`, `RECOVERY|HOME`, `RECOVERY|AUTHENTICATED`, `RECOVERY|RX_REARM`, `AUTO|PREEMPT` and `AUTO|PEER_PREEMPT`. `READY` advertises `LINK_RECOVERY=RENDEZVOUS36|AUTO_FALLBACK=LOCAL_AUTH` alongside the historical web-compatible FW identifier.

## Preserved features

- Fast SF7/BW500, Balanced SF7/BW250, Range SF9/BW125, Ultra SF12/BW125 (text-only) radio profiles unchanged. Ultra CR4/8/LDRO and extra-airtime ARQ remain.
- Existing frequency, PA restrictions, BLE GATT/pairing, USB Web Serial transport, SX1278 SPI driver, Web/Compose UI, AES-256-GCM, HMAC frames, text outbox, receipts, 48 KiB photos and 40 KiB voice are untouched except normal failure/retry behavior during recovery.
- No new RF frame type, changed ciphertext envelope, or new key storage. Both nodes need the v3.0.6 **firmware** to use the same rediscovery policy. Android build version is incremented to 306; its UI and Kotlin logic are otherwise preserved.

## Build and install safely

1. Save your currently working v3.0.5-rc.4 firmware binaries, web directory, Android APK and chat export; record pairing keys privately. Do **not** erase NVS. Do not share the `KEYDATA` USB record or actual keys in diagnostics.
2. Build `firmware/` with ESP-IDF 6.1 for `esp32c3` on a supported local installation. Example: `idf.py set-target esp32c3 && idf.py build` from `firmware/`. Flash both ESP32s using `idf.py -p PORT flash` and inspect startup logs.
3. The original Web Serial client and native Android client do not need a functional UI change. They can be built from the included matching sources. An Android update built as versionCode 306 must use the same signing certificate as the installed app.
4. Pair/connect while the radios are 1–3 meters apart. Confirm `LINK_RECOVERY=RENDEZVOUS36`, `PEER_ARQ=1`, matching nonsecret pairing fingerprints, `PROFILE|ACTIVE|BALANCED`, and an encrypted text + authenticated delivery receipt **both directions**.
5. To use Auto, select **Auto (adaptive)** while both nodes can communicate, approve the required nominal +18 dBm radio/antenna/regulatory conditions for both boards, and confirm `AUTO|ON` locally plus peer Auto agreement. **AUTO OFF is the default on boot**; the app will not silently enable higher power or automatic switching merely because the RF peer is visible.

## Controlled verification before walking away

- Short text each direction, then an image and voice in Balanced, followed by the existing Fast handover and restore. No false delivery ticks.
- Manually switch to SF9 then SF12 Ultra, send short text + receipts, and return to Balanced while both radios are near one another. Photo/voice remain disabled in Ultra.
- Simulate a split profile: switch to Ultra, then **reboot only one** node (which boots in Balanced). Keep the other powered and nearby. After its stale-peer threshold and rendezvous scan, look for `RECOVERY|SCANNING|BALANCED`, followed by `RECOVERY|AUTHENTICATED|BALANCED`. Both should settle on Balanced, exchange fresh signed capabilities, and deliver text again. This is the key v3.0.6 regression test.
- Enable Auto again while near and then separate gradually. Record each node's `AUTO|SELECT`, `PROFILE|AGREED`, `PROFILE|VERIFYING`, `PROFILE|ACTIVE`, SNR, `ARQ_TIMEOUT`, `ARQ_RETX`, `PEER_AGE_MS`, and authenticated delivery receipts. A selected Auto setting must be ON, not just an RSSI indicator.
- Walk beyond reliable coverage and return. Reconnection can require at least one 15-second scan after the stale-peer threshold, or more if the radios search at different times. RF rearm on Balanced may take 120 seconds. Do not infer full delivery from a visible RSSI alone.
- Test long text queued while the link fails, restart app/browser, reconnect and verify that encrypted outbox retries eventually deliver only once, with legitimate receipts.

## Limits and qualification

This is a **v3.0.6 release candidate**, not an independently qualified stable firmware. There was no physical long-range experiment, real ESP-IDF target compilation or Android APK build in this authoring environment. The host simulations cover code-level schedules and invariants; only your two-board test can confirm the recovery interval, RF duty cycle, +18 dBm current draw, regulatory compliance and field reliability. Radio silence due to a dead module, wrong wiring/frequency/key, or an undecodable distant signal cannot be fixed by queue scheduling alone. All transmitted messages still obey local band, power and airtime rules; don't use continuous retries to exceed those limits.
