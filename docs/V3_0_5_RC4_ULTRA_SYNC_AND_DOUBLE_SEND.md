# v3.0.5-rc.4 — Ultra SF12 two-radio verification and duplicate-send recovery

## Evidence

The November 15, 2026 user-provided 88-second recording shows a manual Ultra
request received while both web clients are connected, with one-click approval,
followed by failure to remain in SF12 and repeated waiting-text notifications.
The accompanying screenshot contains two same-text received `hi` bubbles.
This recording **does not contain a full line-by-line profile handshake log**;
we cannot conclusively assign every radio failure or historical duplicate to
one precise event without the corresponding USB diagnostics and message IDs.

## Source-level root causes and narrow corrections

1. The SF12 verification code made BOTH radios transmit authenticated HELLOs
   almost immediately after switching modulation. A 38-byte, CR4/8 SF12
   authenticated HELLO takes approximately **2,761 ms** of on-air time.
   Simultaneous packets collide on a half-duplex channel and no `PROFILE|ACTIVE`
   result is obtained. The new host-tested **role-specific** slots send the
   initiator's first HELLO after 180 ms; the responder listens first and has a
   fallback probe at 4,800 ms. Repeated probes are separated by 7,000 ms after
   each completed transmission; the responder answers a confirmed HELLO after
   380 ms. Verification may wait up to 47 seconds. This does not change the
   authenticated OFFER/AGREE wire packets, encryption, radio register settings,
   or legacy SF7/SF9 handshake timings.
2. The browser could run two `sendText()` calls before its first asynchronous
   IndexedDB `appendRecord()` completed. The composer wasn't cleared until
   **after** that await, allowing two distinct message IDs for one rapid
   double-click/Enter. The browser now takes a synchronous submission lock,
   clears/disables the composer before the first await, and preserves deliberate
   subsequent sends. If a prior pair of `hi` messages has *distinct* IDs, the
   receiver cannot retroactively prove they were not deliberately sent twice.
3. Repeated weak-link text failures no longer produce unlimited identical
   toast notifications; diagnostics keep their detail. `RX_APP_ACCEPTED` and
   `RX_APP_DUPLICATE_ID` log the authenticated application message IDs, never
   plaintext or pairing key, to distinguish a retransmission from a second
   user-initiated message.

## Preserved behavior

- **Balanced:** SF7 / BW250, 10 dBm — unchanged.
- **Fast:** SF7 / BW500, nominal 18 dBm — unchanged.
- **Max Range:** SF9 / BW125, nominal 18 dBm — unchanged.
- **Ultra:** SF12 / BW125 / CR4/8, nominal 18 dBm, encrypted text/receipts only — modem configuration unchanged.
- The AES-256-GCM envelope, RF HMAC, selective-repeat ARQ packet layout, BLE
  services/pairing/NVS, image/voice codecs, Android Compose UI and browser chat
  layout are unchanged.
- The `READY` firmware-compatible `FW=1.8.0-rc.4` token remains because existing
  clients require it; a **new** `PROFILE_SYNC=ULTRA_SLOTS4` token distinguishes
  firmware with this verification fix. Both radios must be upgraded together.

## Build and field acceptance — required before declaring fixed

1. Back up v3.0.5-rc.3 and both boards' NVS/encryption keys. From the **complete
   v3.0.5-rc.4 source**, build firmware with ESP-IDF 6.1:

   ```sh
   cd firmware
   idf.py set-target esp32c3
   idf.py build
   idf.py -p PORT_1 flash
   idf.py -p PORT_2 flash
   ```

   Do **not** use `erase-flash`; don't change pins, antennas, encryption keys,
   or both radios' board configuration. Use the updated rc.4 browser `web/`
   folder in **both** browser windows; hard-refresh. Android firmware may still
   use the existing native Android app because no Android source was changed.
2. With antennas connected and boards close together, verify `STATUS` on each
   reports `PROFILE_SYNC=ULTRA_SLOTS4`, `PEER_ARQ=1`, and the same pairing key
   fingerprint. Send a unique numbered short text in Balanced in both directions.
3. With the required both-board 18 dBm operator confirmation, select **Ultra**
   on ONE browser. On the initiator look for
   `PROFILE|VERIFYING|ULTRA|ROLE=INITIATOR|FIRST_HELLO_MS=180`; on the other
   browser look for
   `PROFILE|VERIFYING|ULTRA|ROLE=RESPONDER|FIRST_HELLO_MS=4800`.
   Both must eventually report `PROFILE|ACTIVE|ULTRA|SF=12|BW=125000`.
   Only after this should any client accept a new text message for RF delivery.
4. Send a unique numbered short text both ways and verify receipt on the other
   device and actual authenticated delivery/read ticks, not just USB queued.
   SF12 airtime is long: don't mistake a delayed ACK for a failure.
5. Return to Balanced using a one-sided manual change; verify both report
   `PROFILE|ACTIVE|BALANCED`; resend short texts. Confirm original photo/voice
   workflows still work in Balanced, and are blocked while Ultra is active.
6. Reproduce rapid Send/Enter clicks once with identical short text. Only one
   outgoing/incoming message should result. Then deliberately submit the same
   text twice after the first save finishes; both intentional messages must
   display. Compare `RX_APP_ACCEPTED|<id>`/`RX_APP_DUPLICATE_ID|<id>` in USB
   diagnostics; don't share encryption keys or unredacted KEYDATA.
7. If still failing, copy diagnostics **from both nodes** including `PROFILE`,
   `RADIO_DIAG`, `LINK`, `PEER_ARQ`, `ARQ_TX`, `ARQ_ACKRX`, `CRC` and `BADTAG`.
   One-sided `PROFILE|ACTIVE` is NOT a successful synchronized transition.

## Testing scope and limitations

Host C tests exercise the real SF12 airtime formula, verify first/repeat slot
separation, and ensure original profile timing is unmodified. A Node regression
simulates a delayed IndexedDB save and simultaneous send attempts. Full baseline
C / Kotlin protocol / JavaScript / source / host-syntax suites pass.

**Not performed:** ESP-IDF target compilation, Android APK build, physical
handshake, actual RF range validation. This is a **diagnostic release
candidate**; retain v3.0.5-rc.3 as the working Balanced-mode rollback.
