# OffGrid Messenger v3.0.5-rc.2 — close-range two-node text regression isolation

**Status: web-only diagnostic/hotfix candidate. It has NOT been phone/radio tested and cannot be called a verified repair of the reported no-delivery failure.**

## Reproduction as supplied by the user

Two ESP32-C3 + SX1278 boards, both connected by Web Serial to the same laptop, close to each other, both shown as **Connected**, with encrypted text stuck at the clock/pending indicator. The left board's web UI displayed about **-45 dBm**; the right displayed **RSSI unknown**. The same hardware reportedly delivered text on v3.0.4. This suggests possible asymmetric authenticated RF reception or a transfer/ACK stall, but the screenshot provides no firmware counters or USB bulk status and **does not identify the failing layer**.

## Changes (only web code)

1. Roll back **only** v3.0.5's new job priority reshuffle to **v3.0.4 FIFO dispatch** as a controlled regression-isolation experiment. Other v3.0.5 encrypted deferred text/outbox mechanisms stay.
2. Repair a real receipt-state bug: an old failure or retry callback can no longer demote a cryptographically confirmed `delivered` or `read` message into `waiting`, `sending`, or `failed`.
3. Log privacy-safe `BULK_STAGE` transitions for text and receipts: submitted, USB staged/queued, RF sent, and authenticated bitmap confirmed or failed. All counters and lengths are diagnostic; ciphertext, plaintext and 256-bit keys are **never** logged.
4. Preserve useful tail fields from `LINK`: RSSI, freshness, `PEER_ARQ`, ARQ bitmap ACK RX/TX, retries, timeouts, `ARQ_BADMAC`, USB errors, CRC, BADTAG, active mode.
5. Show a short 48-bit SHA-256 **pairing-key fingerprint** computed locally from the 32-byte random key. Identical keys show the same fingerprint. This is not authentication proof, but **different fingerprints conclusively explain an inability to exchange authenticated packets**. It must never display or persist the raw key. No NVS or pairing changes are necessary.
6. Issue at most **one** extra `STATUS` command if a text job has not completed after ten seconds; do not flood firmware's command queue. Restore the original RF and BLE firmware paths unchanged.

This does not prove the queue reshuffle is the regression. It provides an A/B test against a clean original v3.0.4 web build **on the same v3.0.5 firmware**.

## Installation: NO new firmware flash

1. Back up the original v3.0.5 web folder. **Leave the v3.0.5 firmware flashed on both ESP32s**; leave both boards' NVS and AES keys untouched. This web build requires `RF_POLICY=ULTRA12_FEC8`, exactly as v3.0.5 firmware already advertises.
2. Serve the **whole** patched `web/` folder over localhost/HTTPS, not selected files, on both browser windows. Hard-refresh both pages (Ctrl+Shift+R). Use Chromium/Web Serial as before.
3. Keep both radios nearby in **manual Balanced** mode; do not enable media or change RF settings while testing. Connect each window to a DIFFERENT ESP32 USB serial port.
4. Open **Settings → USB diagnostics** on each window. Compare `PAIRING_KEY_FP|SHA256_48=...`. Two different fingerprints mean the boards **do not share a key**. Don't reset NVS; preserve both keys before any repair. A matching fingerprint is necessary but not enough for connectivity.
5. Send one unique short text A → B. Then one B → A. Wait up to 30 seconds per send. Reopen **USB diagnostics → Copy diagnostics** on both windows; paste logs into separate files or the conversation. The compact `LINK` records now include the **full ARQ counters**.
6. If text still fails, repeat the same manual Balanced one-letter test on both windows with the **pristine v3.0.4 web diagnostic comparator** included separately. It recognizes the current v3.0.5 `READY` at this profile; DO NOT use the comparator for SF12 Ultra (its Ultra display assumes older 4/5 FEC). If the old client works but rc.2 does not, the regression is in the web client. If neither works, isolate firmware/keys/receiver with the counter evidence. Do not claim either outcome until tested.
7. Roll back this web-only candidate by restoring your backed-up complete v3.0.5 web folder; no ESP32 flash is involved.

## Interpret bulk stages and counters

| Diagnostic | Interpretation |
|---|---|
| No `BULK_STAGE` after clicking Send | App-side queue, encryption, storage or JS execution problem. Check browser Console. |
| `BULKQUEUED_SUBMITTED`, no `BULKQUEUED_ACKED` | USB command stage failed/timed out. Look for `ERROR`, `BULKFAILED`, `ARQ_USBERR`. |
| `BULKQUEUED_ACKED`, no `BULKSENT` | Firmware staged ciphertext, but RF ARQ did not transmit. Check `PEER_ARQ`, profile phase, bad HMAC, peer activity and radio counters. |
| `BULKSENT`, no `BULKCONFIRMED` | All fragments were transmitted but RF bitmap ACKs are not confirmed. Compare `ARQ_ACKTX`/`ARQ_ACKRX`, `ARQ_TIMEOUT`, `ARQ_BADMAC`, `CRC` from both boards. |
| `BULKCONFIRMED`, no received chat bubble | Radio ARQ confirmed; investigate the receiving device's USB forwarding, AES-GCM decrypt, `RXBDONE`, `ARQ_USBERR` and browser Console. |
| Receiver displays chat, sender lacks double ticks | End-to-end encrypted application delivery receipt missing or not received. Not an RF send failure by itself. |
| `RFHEARD=0` / `RSSI unknown` on one side | One direction has no *fresh authenticated* peer reception. The browser being connected over USB is not a radio reception test. |

Do not paste `KEYDATA|` or any raw pairing key from ESP-IDF logs. The built-in browser diagnostics deliberately redact raw key values.

## Source QA

The full v3.0.5 host test suite, including C radio policy, Android native tests, browser crypto, media, bulk and new stage-reporting tests, passes. Neither ESP-IDF target compilation nor a physical 2-board test can be run here. This candidate remains a **diagnostic test release** until the user's two-board results identify the failing layer and verify reliable delivery.
