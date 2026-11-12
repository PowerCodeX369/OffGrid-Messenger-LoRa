# v1.6.1-rc.2 — preserve measured-good v1.6.1, fix only manual synchronization

## Source provenance and measured baseline

This candidate branches **directly from** Git tag `v1.6.1-rc.1`; it does not merge or cherry-pick the later v1.6.2/1.6.3 application/transport changes. The user measured the original v1.6.1 in **Balanced** at 457.4 B/s goodput (8/8 confirmed), median 1.95 s and p95 3.39 s for 1,024-byte encrypted probes. Clean trials reported USB 0.12–0.13 s, RF confirm ~1.31–1.35 s; one retrying trial took 3.39 s with two retransmissions. These are application timings, not theoretical PHY rates. No new speed gain is claimed.

## Fault and focused repair

The previous 5-byte OGM2 authenticated profile OFFER/AGREE required **each** receiver to have previously executed `POWER18 VERIFIED` locally when receiving a Fast or Max request. Thus the first browser could not synchronize the other node automatically and silent refusal looked like a stuck switch. This version uses 6 bytes: one byte of mode (1 LOW, 2 BALANCED, 3 MAX), four nonce bytes mirroring the frame ID, one byte `0xA5` if and only if high-PA (otherwise `0x00`). These six bytes are HMAC-authenticated with the rest of the LoRa frame. The initiator prompts to inspect both boards, sends `POWER18 VERIFIED` over its physical USB (if not already authorized), then `PROFILE <MODE>`. The remote node validates the signed request and joins automatically, without waiting for any browser command.

A busy remote node returns a signed PROFILE_REJECT; invalid/unrecognized/single-byte-tampered authorizations are rejected. The old 5-byte control packet is deliberately incompatible with this new version, so **reflash both boards and hard-refresh both browser clients**. Bounded response timeouts and previous timed rollback remain. A reboot restores Balanced even if a profile switch becomes mismatched. Both nodes must use matching encryption keys.

The three modes are: Fast (SF7, 500 kHz, nominal +18 dBm), Balanced (SF7, 250 kHz, 10 dBm, boot default), and Max Range (SF9, 125 kHz, nominal +18 dBm). **No Experimental 22 dBm option exists.** This power level is requested, not hardware-measured. Check suitable 3.3 V supply and antennas before allowing high-PA transmission, and obey applicable local RF rules.

## Source safety scope

A regression script compares all original firmware ARQ engine sources, SX1278 SPI driver, BLE bridge, web bulk handler, encrypted protocol, receipt/scheduler and benchmark scripts **byte-for-byte** to tag `v1.6.1-rc.1`. The radio loop, data queue and radio HMAC portions of `firmware/main/main.c` are also compared. **No attempts to improve bitrate by decreasing ACK timeouts or changing packet scheduling** are made in this release; that requires measured hardware evidence. The web client likewise retains the v1.6.1 encrypted message pipeline.

## Acceptance on both physical devices — required before stable

1. Flash `FW=1.6.1-rc.2` on **both** ESP32s (do not erase NVS); use v1.6.1-rc.2 web files in **both** browser tabs. READY must include `PROFILES=3|SYNC=ONE_CLICK|ARQ=1|BULK=1|USBPIPE=4`.
2. Both boot in Balanced. Send a short encrypted text **both directions**. Verify it arrived and double-gray ticks only follow the actual authenticated delivered receipt; double-blue only after read.
3. With no queued messages or benchmark running, choose Fast on **one** browser only. After confirming inspection of **both** boards, expect `PROFILE|NEGOTIATING` on the initiator, `PROFILE|PEER_REQUEST` on the receiver, `PROFILE|AGREED`/`PROFILE|VERIFYING`, then `PROFILE|ACTIVE|LOW` on both. No second click.
4. Choose Balanced on one browser, then Max, then Balanced again. Each must show `ACTIVE` on both sides. If it times out or rolls back, preserve both USB diagnostic logs; reboot both to restore Balanced.
5. Send 20+ messages alternating direction and a 20-message burst. Observe no unearned ticks, missing messages or false positive delivery. Avoid **FORCE** switches during messages.
6. Run 8 × 1024-byte encrypted benchmark in Balanced and Fast. Compare with **457.4 B/s, 1.95 s Balanced median** on v1.6.1-rc.1. Watch outlier `RETX`/`TIMEOUT`, not only averages.
7. Temporarily prevent the receiver from accepting a mode while it has unfinished data; initiator should receive `PROFILE|ERROR|PEER_BUSY_...`, not hang forever. Test unplugging/rebooting one node during negotiation; both can reboot to Balanced.
8. Keep the original v1.6.1-rc.1 ZIP as rollback; never mix 5-byte and 6-byte profile negotiation firmware.

## Build instructions

From a folder without parentheses/spaces with ESP-IDF 6.1 initialized:

```bash
cd firmware
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash
idf.py -p /dev/ttyACM1 flash
```

Serve the root `web/` via `python3 -m http.server 8765 --bind 127.0.0.1 --directory web`. No real ESP-IDF or two-board profile exchange was available in the source environment. Passing host tests and a mocked-browser smoke test **does not prove RF reliability**.
