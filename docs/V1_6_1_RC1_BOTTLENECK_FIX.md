# v1.6.1-rc.1 — Measured-regression response: USB staging and ARQ-window pacing

**Status: source candidate, hardware not yet validated.** Do not call this a measured performance improvement until the same two SX1278 modules complete the benchmark. **No full ESP-IDF 6.1 build was possible here.** The user's v1.6.0-rc.1 Fast-mode result was 358.2 bytes/s at 2.63 s median RTT (8/8 confirmed), versus the v1.5 Fast baseline 487.8 B/s and 2.07 s median (8/8). That's ~26.6% reduced goodput. Both are small samples. Fast-mode RF output also changed between those releases (+2 dBm to nominal +18 dBm); too much near-field signal and supply sag may contribute and must be tested, not assumed.

## Source-level analysis

Three specific v1.6.0 sources of avoidable overhead were found:

1. The browser's `BulkLink.send()` waited for **every** 160-byte `BULKSTEP` USB reply before sending the next line. A 1-KB ciphertext therefore needed seven sequential chunk handshakes plus `BULKBEGIN` and `BULKEND`.
2. `OG_SR_MAX_WINDOW=4` split a typical AES-GCM-encrypted 1,024-byte application probe into a **4+1** data burst with *two* bitmap ACK transactions. Five 224-byte RF fragments fit a six-frame window, so one bitmap ACK suffices when none is lost.
3. `og_arq_bridge_service()` allowed a new packet **3 ms** after TxDone. In SF7/BW500, a LoRa preamble lasts about 2 ms. The receiver's FIFO/IRQ/HMAC/FreeRTOS work can exceed the gap. The resulting possible dropped fragment costs a full 420-ms ACK timeout plus resend/backoff. This is a code-level **risk**, not a verified root cause: the user's new v1.6 USB `ARQ_RETX`/`ARQ_TIMEOUT` counters were not supplied.

## Corrections made

- Browser-to-firmware USB upload is **pipelined in four sequential 160-byte commands per Web Serial write**. It waits for a positive firmware checkpoint after each group (or the last partial group). Firmware still validates **every** sequence and CRC32; `BULKFAILED` aborts immediately. Maximum in-flight stage = four; there is no unbounded USB flood. For a 1-KB object, the browser waits on **four** stages (begin, first four chunks, remaining three, end) instead of nine.
- On receiving a completed encrypted RF transfer, firmware forwards up to four 160-byte encrypted USB chunks per radio-loop turn, rather than one, reducing repeated 24-ms radio receive polls. This is bounded to avoid arbitrarily long work during active radio scheduling. The receiver's browser still authenticates AES-GCM before showing a message.
- Increase `OG_SR_MAX_WINDOW` to six for SF7 Fast/Balanced and select three for slow SF9 Max Range. An encrypted 1-KB probe becomes one 5-fragment RF window and one bitmap ACK instead of two windows and two ACKs, when none is missing. Full 7,168-byte objects still use bounded selective-repeat windows and missing-only retransmission.
- Introduce **25 ms** receiver-rearm guard between successful TxDone events. This deliberately adds a small fixed delay to reduce potential losses caused by very short SF7/BW500 preambles. Timing is conservative and should be tuned using hardware counters; it is not a measured optimum.
- Add `BULKSTAT|id|RF_MS=...|FRAMES=...|RETX=...|TIMEOUT=...` before `BULKCONFIRMED`. The browser records **local USB staging time**, **local RF-confirmation wait**, and ARQ retries per test. Full encrypted roundtrip remains a separate metric. This lets us distinguish USB, radio ARQ, and remote reply time rather than infer root cause from end-to-end RTT alone.
- Require `READY ... FW=1.6.1-rc.1|USBPIPE=4|WINDOW=6` on the web app to avoid using incompatible v1.6 USB acknowledgment behavior. **Update both boards and both web sessions.** The on-air ARQ framing is unchanged (the bitmap window limit is a sender policy), but older web/firmware clients must not be paired with this pipelined USB stage contract.

## Encryption and tick semantics preserved

- The browser still encrypts OGM4 payloads using AES-256-GCM before USB and radio. ARQ data/bitmap frames remain HMAC-SHA256-128 authenticated, and `BULKSTAT` is local USB diagnostics only.
- One gray tick: all local SX1278 TxDone completions. Two gray ticks: separate authenticated encrypted application delivered receipt. Blue ticks: authenticated encrypted read receipt *after viewing*. Missing encrypted receipt stays unconfirmed.
- Manual modes, BLE, paired NVS key, hardware pinout, radio PA safety, experimental **22 dBm blocked**, and frozen Android source are unchanged. Photos/voice/video stay disabled.

## Mandatory testing on the two boards

1. Keep a copy of v1.5 and v1.6. Extract v1.6.1 to a fresh no-spaces/no-parentheses path; build with ESP-IDF 6.1 and flash both without erasing NVS. Use the included web app with a hard reload on both sides.
2. Confirm the *complete* `READY` lines advertise `FW=1.6.1-rc.1`, `ARQ=1`, `BULK=1`, `USBPIPE=4`, and `WINDOW=6`. Confirm `LINK|...|PEER_ARQ=1` on both sides.
3. First send a short text in both directions; then a 500-character text and 20 short messages in a burst. Validate all decryption, duplicates, and tick semantics. If a message is unconfirmed, report it as unconfirmed, not delivered.
4. Run **8 × 1024-byte** encrypted tests in Balanced, then manually enter Fast after both high-PA safety checks. Record goodput, median/p95 RTT, `local USB`, `RF confirm`, `ARQ retransmissions`, `RF_MS`, and `LINK ARQ_RETX/ARQ_TIMEOUT/ARQ_BADMAC` *delta* between the start/end of every run. A high `RETX` or `TIMEOUT` count means avoid shrinking ACK timers blindly; inspect antenna spacing, supply and receiver rearming.
5. Compare *identical* modes, RF power and distance across releases where possible. v1.5 Fast and v1.6 Fast **differ in TX power** (+2 vs nominal +18); do not conclude every speed difference is the transport. In close-range tests use the proper matched antenna and avoid overdriving the receiver.
6. Force missing RX or bitmap ACK while both send concurrently. Confirm only missing fragments retry, no counterfeit double/blue ticks, no hang, and no unbounded queues. Test refresh/reconnection and manual mode rollback.
7. Repeat the benchmark 3 times after hardware warmup. Accept stable release only if reliability is unaffected and median goodput improves *measurably*. Revert to v1.5 if this candidate fails physical acceptance.

## Available tests

- Original C/JS crypto, host SX1278 SPI, profile, ARQ loss recovery, 7,168-byte reassembly, browser envelope, delivery/receipt, and per-fragment simulator checks.
- New 1-KB one-window/one-ACK regression and browser four-line bounded staging assertions; `BULKSTAT` parsing.
- Chromium profile-modal smoke test, C header syntax stubs, Git archive and source manifest checks.
- **Not run:** complete ESP-IDF cross-compile and physical two-device USB/RF test. The code changes aim to remove *known software overhead*, but no guaranteed throughput or latency is claimed.
