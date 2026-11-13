# v1.8.0-rc.5 — media efficiency and bottleneck diagnosis

## Why this release exists

The user measured the **same approximately 46.7 KiB compressed photo** at **85 s Balanced**, **75 s with automatic temporary Fast**, and **75 s with manually selected Fast** at short range. Approximate end-to-end goodput was therefore 563 vs 638 B/s. This **does not prove that LoRa PHY airtime is irrelevant**. A near-12% wall-time improvement despite doubled LoRa bandwidth strongly suggests that protocol scheduling, ACK windows, USB staging, media-session setup, retransmissions, or browser processing also need attention. At 1 m we cannot generalize the results to weak links.

## Scope: web-only, original radio stays intact

- ESP32 firmware remains `FW=1.8.0-rc.4` **on both boards**. No new SX1278/ARQ/power timing or profile changes, no Android changes.
- Web client `1.8.0-rc5-lora` **on both browsers**. Do not mix rc.4 and rc.5 browser media protocols, because both validate the expected segment size and count.
- Media payload per encrypted browser bulk session grows **5,800 → 7,040 B**. Each part adds 28 B metadata, 24 B application envelope, and 28 B AES-GCM nonce/tag, for up to **7,120 B**, within the unchanged **7,168 B** firmware ARQ upper limit (32 × 224 B).
- 48 KiB photo: **9 → 7** sessions. 40 KiB voice: **8 → 6** sessions. Each session still has a fresh AES-GCM nonce, separate radio MAC, bitmap ACKs, per-piece retries and full-file SHA-256 validation. No change to image compressor, photo cap, Opus codec, voice cap, UI, receipts, or temporary Fast/restore behavior.
- Browser writes up to **8 × 160 B** ciphertext chunks as newline-delimited hex lines in one USB batch. The maximum USB write is under 3 KiB, below the firmware's configured **4 KiB USB Serial/JTAG RX buffer**. The old firmware continues acknowledging chunk indices 3,7,... or the final chunk; the browser waits at its final eighth-chunk checkpoint instead of at every fourth.

## New diagnostic logs (sender browser)

After sending a photo or voice note, open **Settings → USB diagnostics → Copy diagnostics**. Look for:

`MEDIASEG|1/7|BYTES=7040|USB_MS=...|RFWAIT_MS=...|RF_MS=...|FRAMES=...|RETX=...|TIMEOUT=...`

and finally:

`MEDIAPERF|OK|FROM=BALANCED|BYTES=...|SEG=7/7|WALL_MS=...|PRE_MS=...|USB_MS=...|RFWAIT_MS=...|RF_MS=...|OTHER_MS=...|RESTORE_MS=...|FRAMES=...|RETX=...|TIMEOUT=...|RETRIES=...`

`PRE_MS` includes waiting for confirmation and signed two-node Fast handover; `RESTORE_MS` includes signed return. `USB_MS` sums browser USB upload staging, `RFWAIT_MS` sums time from successful USB staging to final radio confirmation, and `RF_MS` sums firmware-reported per-session RF-worker time. `OTHER_MS` is residual browser/media/queue time: an **accounting category**, not proof of a specific culprit. `RETX`/`TIMEOUT` are per-session RF counts. The receiver's browser processing and encrypted delivery receipt may continue after this sender-side media-worker timing; use chat ticks separately. The logs never contain plaintext, voice recordings, photos or pairing keys.

## Acceptance and honest performance comparison

1. Keep the known-working v1.8.0-rc.4 complete Git ZIP intact for rollback. No firmware rebuild or flashing for rc.5.
2. Replace **both** web folders with rc.5, hard-refresh both browsers, reconnect and confirm `FW=1.8.0-rc.4` on the two radios and web `1.8.0-rc5-lora` on both clients.
3. Send short text A→B and B→A, one photo, and one short voice note. Require proper authenticated delivered/read ticks and unchanged photo/voice presentation.
4. In manual Fast (after checking approved RF power), send the **same previously compressed photo** once using rc.4 on both browsers, then once using rc.5 on both browsers (do not mix versions). Time wall start-to-completed radio transfer, and compare `BULKSTAT`, `MEDIASEG`, and final `MEDIAPERF` on rc.5.
5. Then repeat on manual Balanced with Fast boost explicitly declined, comparing RF vs non-RF time and retransmission counts. Short recording durations may be dominated by profile handover.
6. Any missing/corrupted media, lost receipt or persistent mismatch is a regression: roll back **both** web clients to rc.4 without flashing. Both binaries continue using the unchanged v1.8.0-rc.4 ESP32 firmware.

### Limitations and further optimization

The **7,168-byte RF-transfer maximum and six-fragment selective bitmap ACK window remain unchanged**. There is still exactly one radio bulk session active at a time. Improving throughput beyond these safe web-only changes may require firmware/receiver co-design (multiple staged ciphertext buffers, radio turnaround scheduling, or sliding-window ACKs). Those require measured `RF_MS`, `RETX` and timeouts plus two-radio hardware qualification; do not blindly lower ACK delays or reintroduce CAD gating. No new speed figure is claimed by the host regression tests.
