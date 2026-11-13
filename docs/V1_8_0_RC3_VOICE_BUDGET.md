# v1.8.0-rc.3 — Voice-only 40 KiB web hotfix

**Do not flash the ESP32.** Both boards continue running `FW=1.8.0-rc.1`. Replace both browsers' `web/` folder from this hotfix, hard-refresh and reconnect. The browser identifies as `1.8.0-rc3-lora`.

## Changes strictly confined to voice

- The encoded **voice** file has an absolute 40,960-byte cap in all three relevant browser gates: `web/app.js` before queueing, `web/protocol.js` voice pack/unpack, and `web/lora_media.js` media segmentation and reassembly.
- The microphone may record up to 120 seconds, but stops **earlier** if the compressed bytes approach the cap. `MediaRecorder` requests 8,000 audio bits/second (mono when supported), noise suppression, echo cancellation and automatic gain. Browser Opus VBR may produce lower or higher actual rates. An unplayable, empty or over-cap final blob is never sent.
- Audio length is *not* padded to 40 KiB: short notes have proportionately less recorded data, except for unavoidable container overhead. A 1-second voice message generally occupies much less than a 30-second one. Fixed linear sizes (1 minute = 20 KiB; 2 minutes = 40 KiB) are **not** guaranteed, and may be infeasible for continuous speech at a natural quality.
- The original media-segment format, AES-GCM authentication, recipient SHA-256 verification, receipts, radio ARQ, auto/manual RF, photo encoder, and Android files are unchanged.

## Why the 2-minute guarantee is impossible with this browser encoder

40 × 1024 × 8 / 120 = approximately **2,731 bits/s** including WebM/Ogg metadata. RFC 6716 defines Opus starting at 6 kbps, with 8–12 kbps typically useful for narrowband speech and 16–20 kbps for wideband speech. Our browser may accept requests below these values, but output bitrate and intelligibility are not guaranteed. At an actual 8 kbps, 40 KiB holds about 41 seconds before container overhead. Long continuous natural-sounding two-minute speech would need a higher byte allowance, a different dedicated speech codec (with a different receiver), or silence-heavy recording that compresses unusually well. The existing 2-minute timer is an upper boundary, **not an assertion that all two-minute clips fit**.

## Smoke and acceptance

1. Keep v1.8.0-rc.1 firmware. Load rc.3 web on both browsers, clear stale caches, confirm the photo and mic buttons remain visible.
2. Check one 1–2 second mic clip is much smaller than 40 KiB and plays on the peer; check one longer clip stays within 40,960 encoded bytes.
3. Try the maximum: the recorder stops at the byte cap without exceeding 40 KiB or silently truncating a transfer. Large-voice reassembly and SHA-256 checking must succeed.
4. Verify photos still cap at 48 KiB, text/chat ticks unchanged and Auto/Manual RF still work.
5. Run `sh firmware/tests/run-host-tests.sh`, `python3 tests/test_v18_browser_smoke.py` and `python3 tests/test_v18_ui_parity.py`. These host tests do **not** replace physical two-board testing.
