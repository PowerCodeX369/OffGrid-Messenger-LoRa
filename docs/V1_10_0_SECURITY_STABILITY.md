# v1.10.0-rc.1 — security / stability validation

## Compatibility and rollback

This release is **web-only in runtime behavior**: install the bundled `web/`
directory in both browser environments; keep both ESP32-C3/SX1278 radios on the
existing **FW=1.8.0-rc.4**. Do not erase NVS or change pairing keys. The full
ESP-IDF firmware project and its original Git history remain in the ZIP for
reproducibility. `v1.8.0-rc.6` is the exact rollback tag. If there is a problem,
restore *both* browsers' rc.6 `web/` folders; no reflashing is necessary.

## Specific hardening implemented

* A bounded **authenticated receive guard** locks one browser transfer ID from
  the start of asynchronous AES-GCM validation until the **whole application
  envelope** has been processed. Concurrent duplicate SX1278 bulk/legacy
  transfers cannot run the handler twice. A duplicate legacy packet received
  during the wait is added to that transfer's eventual authenticated APPACK
  set. The same transaction is not trusted before GCM succeeds.
* A transfer with an invalid GCM tag or malformed envelope **is never cached
  as accepted** and can be retransmitted. Reception after a disconnected
  browser or replaced in-memory key is not accepted, so a stale asynchronous
  decryption cannot silently add an accepted ID to a new connection.
* In-flight validation is bounded to 16 IDs, and the recent success cache to
  512 IDs with 12-minute expiry. This is **process-local anti-duplicate state**
  only; the original encrypted message-ID history still provides durable
  duplicate suppression for received chat messages.
* Identical AES-GCM, protocol framing, image/voice codecs, read and delivered
  receipt semantics, radio HMAC, ARQ, authenticated profile synchronization,
  temporary Fast mode and USB staging to rc.6. No invented success ticks.

## Automated tests

* Existing host C radio/ARQ, JS crypto/media/USB, loss injection, source
  boundaries and device-mode tests.
* 100,000 deterministic receive-guard operations including replay attempts,
  invalid/expired tokens, failed validation and bounded storage.
* 40,000 malformed/mutated legacy USB frame and media-segment parser inputs;
  AES-GCM ciphertext tamper at *every* byte and swapped transfer-ID AAD;
  full 40 KiB out-of-order voice reassembly.
* Headless Chromium in-memory app test for concurrent duplicates on both
  legacy and bulk receive paths, failed-GCM retry and authenticated APPACK
  accumulation. Desktop/split-screen/mobile photo and voice UI checks,
  media preflight/restore tests and rc.6 text interleaving regressions.

## Hardware acceptance (must complete before stable release)

1. Check both browser versions report `1.10.0-rc1-lora` and both radios still
   report `FW=1.8.0-rc.4`.
2. Exchange 50 short encrypted texts in both directions: no duplicate chat
   bubbles; real delivered and read receipts required.
3. Exchange one 48 KiB encrypted photo and one 40 KiB encoded voice note in
   each direction. Verify exact content integrity and playback.
4. Disconnect/reconnect the USB cable during a transfer. Confirm there is no
   forged delivery tick, then retry the failed message normally.
5. Check manual Fast/Balanced/Range, opt-in Auto, and temporary Fast/restore
   for media; do not alter the power approval rules.
6. Run 1-hour soak tests, simultaneous traffic, packet-loss and range tests,
   and a full ESP-IDF 6.1 build in the target environment.

## Outstanding security limitations

* **No persistent anti-replay counter for authenticated RF control frames
  across ESP32 reboots.** Current per-session HMAC/nonce checks do not prove
  persistent freshness, so do not describe the controls as replay-proof.
  Implementing persistent counters and compatibility negotiation requires a
  coordinated firmware change and physical two-board acceptance; it is
  deliberately **not** introduced into this working radio release.
* **Key rotation is not crash-atomic across ESP32 NVS and browser IndexedDB.**
  Export the old key securely and back up encrypted local history before
  rotating keys. Changing the board key during a failed migration can make
  previously encrypted history inaccessible.
* There is no independent cryptographic or penetration audit, no secure
  element, and no RF field-security certification. Physical USB access via
  `KEYGET` can expose the shared pairing key.
* Media reassembly remains in-memory and does not resume after browser
  restart. This is the deferred v1.9 checkpoint feature, not a v1.10 claim.
* The 433 MHz transmission legality, real +18 dBm output/power supply and
  regional duty-cycle compliance require physical validation. Battery and
  sleep optimizations remain **last, after Android**.

No passwords, pairing keys, plaintext chats, audio or pictures are recorded in
these test logs or validation reports. Do not paste `KEYDATA` into bug reports.
