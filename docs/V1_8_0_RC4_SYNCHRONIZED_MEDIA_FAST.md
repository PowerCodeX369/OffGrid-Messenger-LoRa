# v1.8.0-rc.4 — temporary, synchronized Fast for photo and voice transfers

**Status: firmware/web release candidate; host verified, not yet compiled using ESP-IDF 6.1 or tested on two physical ESP32/SX1278 boards.** The user confirmed v1.8.0-rc.3 photo and voice transfers at ~1 metre. That working media/crypto/ARQ baseline is preserved; the only feature change is radio profile orchestration around a media transfer.

## Expected behavior

1. Before the first media transfer from a normal Balanced or Range setting, the initiating browser asks the operator to confirm **both** radio boards can safely operate at nominal +18 dBm, with correct 433 MHz antennas, stable 3.3 V supply and permitted RF operation. The authorization is local to that browser/USB session. Declining leaves the transfer on its existing mode.
2. The sender sends USB `POWER18 VERIFIED` (when needed), then `MEDIA ON`. The ESP32 checks radio availability and, in Auto mode with enough fresh telemetry, refuses an unsafe Fast promotion. Manual mode may attempt Fast; on failure, authenticated rollback retains the original link.
3. A temporary HMAC-authenticated OGM2 `PROFILE_OFFER`/`PROFILE_AGREE` uses `OG_PROFILE_MEDIA_FLAG=0x10` (separate from AUTO=0x40 and the +18 dBm acknowledgement `0xA5`). Both radios switch on the previous profile using the existing timed two-device handshake. **No media fragment is sent before the initiator reports `MEDIA|READY|MODE=LOW`, which is emitted only after the new profile receives authenticated peer traffic.** On a failed attempt, `MEDIA|BYPASS|<reason>` preserves the previous mode.
4. The original encrypted media part format (5,800 bytes), AES-GCM, 48 KiB photo cap, 40 KiB voice cap, authenticated bitmap ARQ, retry and delivery/read/played receipts are unchanged. Automatic RF downgrade still operates for a deteriorating connection.
5. After the last radio-confirmed encrypted segment, or on cancel/failure, the browser issues `MEDIA OFF`. Firmware waits for outstanding ARQ traffic and encrypted receipts before negotiating the **saved previous** radio mode with the same HMAC handshake. Browser waits for `MEDIA|RESTORED|MODE=...` before processing another queued message. Already-Fast sessions remain Fast. The Auto-enabled state (if any) is not cleared by media switches, and manual buttons are unchanged.
6. A crashed/disconnected browser cannot force an instantaneous restoration. A 10-minute hint expiry provides best-effort bounded recovery, with a deterministic coordinator. A completely broken link cannot synchronize mode changes and requires manual recovery (both boards reboot to Balanced) if rollback fails.

## Safety and compatibility

- **Both boards must use firmware `FW=1.8.0-rc.4` and both browser sessions use `1.8.0-rc4-lora`.** Do not mix with rc.1 radio firmware: old firmware does not support the authenticated media intent and browser-side READY barrier. `READY` advertises `MEDIAFAST=1`.
- Initial boot profile remains manual Balanced SF7/BW250/+10 dBm. The Fast session is **temporary**; user-approved +18 dBm and existing antenna/power requirements still apply. No +22 dBm mode is offered.
- At long range, forcing Fast can break a working link; preflight can bypass on weak Auto telemetry, the signed radio handover has timed rollback, and media remains retryable. Success is never inferred merely from USB staging or profile command emission.
- The new flag is authenticated by the existing SX1278 HMAC; it does not add a new public/unauthenticated RF command. No ESP-NOW or Wi-Fi initialization.
- **Performance caveat:** Fast can reduce airtime for longer photos/voice clips, but two coordinated profile changes introduce seconds of overhead. Very short clips may transfer faster staying Balanced. No speed improvement is claimed without physical measurement.

## Two-board acceptance checklist

1. Save your working rc.3 Git ZIP. Build from new `firmware/` with `idf.py set-target esp32c3 && idf.py build` and flash **both boards** without NVS erase. Replace **both browsers' entire `web/` folder**, hard refresh and reconnect. Confirm `READY` advertises `FW=1.8.0-rc.4|MEDIAFAST=1`.
2. With both in manual Balanced, send text A→B and B→A; confirm decrypted delivery/read ticks work exactly as before.
3. Send a small photo from A. Approve +18 dBm for both devices. Observe `MEDIA|BOOST_REQUEST|LOW|RETURN=BALANCED`, `PROFILE|ACTIVE|LOW`, `MEDIA|READY|MODE=LOW` **before `BULKBEGIN`**, then `MEDIA|RESTORE_PENDING|BALANCED`, `PROFILE|ACTIVE|BALANCED`, `MEDIA|RESTORED|MODE=BALANCED` **after the last segment**. Confirm B shows the original photo and A has genuine double ticks. Repeat B→A and voice notes in both directions.
4. Test a second photo without a repeated power confirmation in the same USB session. Confirm both radios are again Balanced after completion, and no new text message is blocked.
5. Enable Auto on one browser (both peers synchronized). From Auto-Balanced, send media and verify the temporary Fast mode, then return to the same Auto state. In weak Auto telemetry, a `MEDIA|BYPASS|FAST_LINK_MARGIN_INSUFFICIENT` is expected and delivery stays on the original mode. Do **not** claim long-range success from one-metre tests.
6. Cancel an active media transfer or inject a lost segment and verify mode restoration or explicit `MEDIA|RESTORE_FAILED` diagnostics; no false delivery/read ticks.
7. Test manual Range only when both nodes can communicate at that distance. If a Fast switch fails to verify, both should time out/rollback; never assume the pair is synchronized if the diagnostics disagree.

## Automated checks

`bash firmware/tests/run-host-tests.sh`, `bash firmware/tests/run-syntax-checks.sh`, `python3 tests/test_v18_media_fast_browser.py` (Playwright+Chromium). Source tests verify the previous rc.3 photo/voice codec, encrypted protocol, ARQ/radio driver, CSS/UI, receipt and Android source files remain byte-identical.
