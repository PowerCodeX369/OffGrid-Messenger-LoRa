# v1.2.0-rc.2 TX recovery candidate

## Trigger

Hardware report: v1.1.0-rc.1 exchanged LoRa messages, whereas the subsequently
installed v1.2.0-rc.1 did not reliably send even short text.

## Scope and cause status

The v1.2.0-rc.1 firmware inserted an SX1278 modem-status/CAD gate immediately
before every data transmission. It could repeatedly defer a pending frame,
preventing `SENT`/`TRACE|TX_DONE` even with a healthy physical link. This is a
**code-level regression risk**, not a verified root cause on the user's boards.

rc.2 **removes that pre-TX CAD gate and reverts to v1.1 TX/retry behavior**,
while retaining v1.2 browser-side scheduling, authenticated receipt batching,
benchmark tooling and firmware ACK priority. The SX1278 CAD driver remains
available for future isolated testing but is not used by the message scheduler.

This hotfix does not change Wi-Fi (still disabled), BLE, USB, GPIO mapping,
pairing keys, AES-GCM, radio HMAC, RF parameters, message/tick semantics,
Android, or stored conversations.

## Acceptance and fallback

1. Back up v1.1.0-rc.1, the last observed messaging build.
2. Build rc.2 with ESP-IDF 6.1; do not erase flash/NVS.
3. Flash **both** boards; verify `FW=1.2.0-rc.2`, `ACKENGINE=2`, `FLOW=2`.
4. Use the bundled `web/` client in *both* Chrome/Edge tabs. Hard reload to
   avoid a stale script. Confirm both browsers load the same build.
5. Send one short message each way, and record `QUEUED`, `SENT`, `RADIOACK`,
   `RX`, `TRACE|TX_DONE`, `BADTAG`, and `LINK` from both USB diagnostics.
6. If successful, test five sequential messages, then a five-message burst;
   compare receipt confirmation, latency and failure counts.
7. If no send/receive after this change, restore v1.1.0-rc.1 (both firmware and
   web). Do not assume the CAD theory is the only cause. Supply diagnostics.

`CAD_BUSY` should remain zero in rc.2, because its active TX path no longer
consults CAD. The `LINK` line still exposes this counter for compatibility.

No full ESP-IDF build or physical two-board validation was available at
packaging time. This is **not** a stable release.
