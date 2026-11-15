# OffGrid LoRa v3.0.6-rc.2 — Max/Ultra ACK efficiency and weak-RF collision repair

## User-reported problems

1. On two nearby ESP32-C3/SX1278 nodes, SF9 Max Range text was unacceptably slow.
2. SF12 Ultra Long Range sometimes did not deliver any encrypted text despite a visible peer.
3. Both profiles used a 34-byte radio bitmap ACK: 14-byte header + 4-byte bitmap + 16-byte authentication tag. User requested a 16-byte maximum radio ACK and smaller long-range text fragments.

## Root cause and scope

The firmware used a **350-ms incomplete-burst ACK-silence timer in Ultra**, although the next SF12/125 kHz CR4/8 data fragment can be on air for multiple seconds. An ACK could therefore start while another DATA packet was being received, creating a collision. Max's incomplete-burst timing was likewise not derived from the active packet length. A normal last-fragment END_BURST ACK remains quick (25–80 ms turnaround); only **partial** bursts need an airtime-aware holdoff.

During a Max/Ultra ARQ transfer, the radio owner also defers opportunistic HELLO, CAPS and Auto metric transmissions so they cannot steal the half-duplex receive window while a 16-byte ACK is on air. Mandatory signed profile replies and legacy ACKs retain their priority.

The original 34-byte ACK transmitted 18 bytes of *authenticated metadata* plus a 16-byte HMAC-SHA256-128. We did **not** truncate the tag or remove authentication to reach 16 bytes. Instead, **Max/Ultra transmit only the full 16-byte HMAC tag** of the original 18-byte ACK preimage. The currently active sender knows session, transfer ID, fragment count and its unacknowledged bitset. With at most three outstanding fragments, it tests at most seven candidate nonempty bitmap subsets against that HMAC. Wrong-transfer, tampered and stale ACKs cannot confirm the current burst. The radio byte count is exactly **16**, including the full 128-bit authentication tag. This is a new authenticated ARQ subformat.

### Transport changes

| Property | Fast / Balanced | Max SF9/BW125 | Ultra SF12/BW125 CR4/8 |
|---|---|---|---|
| ARQ ACK on air | Original 34 B | **16 B, implicit authenticated bitmap** | **16 B, implicit authenticated bitmap** |
| Text RF fragment data capacity | 224 B | **128 B** (was 224 B) | 48/64/80/96 B, unchanged (already small) |
| Photo/voice | Original | Existing 224-B media fragment capacity retained | Disabled, unchanged |
| Send window | Original 6 | 3 | 1–2 |
| Max attempts per fragment | 5 | 7 for 128-B text / 5 for 224-B media | 12 |
| Partial ACK RX silence | Existing | Next SF9 DATA airtime + 180 ms | Next SF12 DATA airtime + 180 ms |
| ACK deadline | Existing | 16-B SF9 ACK airtime + margin, **measured after TxDone** | 16-B SF12/CR4/8 ACK airtime + margin, **measured after TxDone** |

For 125 kHz, 8-symbol preamble, typical calculated ACK airtimes are SF9/CR4/5 **247 ms → 165 ms** and SF12/CR4/8 **2,499 ms → 1,713 ms**, comparing 34 vs. 16 bytes. These are airtime calculations, not physical latency benchmarks. Real round trips also include DATA airtime, task scheduling, processing, retransmissions and link availability. Smaller fragments make less data subject to loss but can be slower for clean, long transfers; there is no promised speedup for every packet length.

### Authenticated compatibility guard

Both new radios announce signed `ARQ_CAPS=4`; values 1/2/3 are still parsed for basic interoperability, but a Max or Ultra profile change requires the peer to prove capability 4 first. Receiver also rejects incompatible range-fragment flags in other profiles. Both ESP32s **must use rc.2** to communicate using the new ACK subformat. Mixed rc.1/rc.2 Max/Ultra operation is deliberately blocked rather than silently losing messages.

The USB `READY` marker includes `ACK_COMPACT=16|ARQ_CAP=4|MAX_TEXT_FRAG=128` in addition to existing `RF_POLICY=ULTRA12_FEC8`, `FW=1.8.0-rc.4`, and other strict browser compatibility fields. The historical FW label remains unchanged because the existing web UI expects it; **inspect ACK_COMPACT**, not FW, to distinguish the new firmware.

### Scope of changes

- `firmware/main/main.c`: select new ARQ framing by profile, cap-4 handling, size/time policy, 16-byte ACK dispatch.
- `firmware/main/lora_arq_timing.h`: testable next-packet-airtime policy.
- `firmware/main/lora_wire.h`: authenticated cap-4 allowlist.
- `firmware/components/offgrid_arq`: 16-byte implicit HMAC ACK, smaller SF9 text frame flag, bounded bitmap verification, bridge integration.
- **Not modified:** SX1278 registers/radio driver, power settings, native Android Compose interface, BLE service, crypto key storage, Web UI, AES-GCM, media codecs, application receipts and SF7 radio behavior. Max remains SF9, Ultra remains SF12, both at their previously configured nominal 18 dBm.

## Installation & hardware acceptance

1. Keep an immutable backup of the confirmed working v3.0.6-rc.1 project, firmware and app. Do **not** erase NVS or encryption keys.
2. With ESP-IDF 6.1 configured, build the supplied **firmware/** (`idf.py set-target esp32c3`, then `idf.py build`). Flash **both ESP32-C3 boards** (`idf.py -p PORT flash` for each). Install the matching known-good Android/web application from the same repo if it is not already installed; client code is unchanged in rc.2.
3. Connect both nodes at close range; verify both READY records include `ACK_COMPACT=16|ARQ_CAP=4`, and LINK has `PEER_ARQ=1` after authenticated capability exchange. Use identical AES pairing keys and same RF profile.
4. In **Balanced**, send short text, photos and voice in both directions; confirm true RF `BULKCONFIRMED` and authenticated application receipts.
5. Select **Max**, wait for `PROFILE|ACTIVE|MAX` on **both**, send a 1-character text, a ~500-character text, and a photo. Look for `ARQ|ACK_BYTES=16|HMAC=SHA256_128|MAX_WINDOW=3`, rising `ARQ_ACKRX`, and `BULKCONFIRMED`. Copy `BULKSTAT` latencies. Max photo/voice should still fit.
6. Select **Ultra**, wait for both `PROFILE|ACTIVE|ULTRA`, send one short encrypted text at close range before any range testing. Expect 16-byte ACKs, an actual `BULKCONFIRMED`, and an authenticated app receipt. Photos/voice must still be unavailable in Ultra.
7. Deliberately block or remove one node temporarily, then restore it; observe retries and subsequent successful ACK confirmation, with no false double ticks or duplicate chat messages. Compare both directions.
8. Only after near-range verification should you do gradual distance measurements in Ultra and Max. Capture both nodes' `LINK`, `ARQ_TX`, `ARQ_ACKTX`, `ARQ_ACKRX`, `ARQ_TIMEOUT`, `ARQ_RETX`, CRC/BADTAG, `PROFILE` and `BULKSTAT` records.

**Rollback:** flash **both** boards back to v3.0.6-rc.1 if necessary. Never flash one rc.1 and leave the other in rc.2 Max/Ultra; they use incompatible ACK formats. Never assume a single-tick `SENT` means the recipient received or decrypted the message. Encryption keys stay on the ESP32 NVS if you do not erase it.

## Qualification and limitations

Host C simulation includes lost DATA and ACKs, authenticated 16-byte messages, malformed/faked and replayed ACK rejection, full Max media capacity, Ultra 48–96-byte reassembly, and testable SF9/SF12 airtime. Full C/JavaScript/Kotlin regressions are run when release is packaged. This environment does **not** have an ESP-IDF target toolchain or Android SDK; this is source and host validation, not a built flashable binary, Android APK or physical throughput measurement. Extreme low-SNR reception and legal airtime limits remain physical constraints.
