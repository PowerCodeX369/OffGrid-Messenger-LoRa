# v1.6.0-rc.1 — Integrated selective-repeat LoRa firmware

**State:** Fully integrated source candidate (ESP-IDF firmware + Web Serial browser + frozen Android sources). Host and simulated two-node tests pass. A complete ESP-IDF 6.1 build, physical radios, hardware profile switching, throughput and battery/power tests are **NOT** performed. Do not claim this is field-reliable or quote projected throughput as measured.

## What changed from v1.5

Previously the browser encrypted an OGM4 application envelope, split it into **160-byte browser-originated frames**, and waited for an authenticated radio ACK after each. The ESP32 radio worker would then request a new frame over USB. Each radio DATA frame had a 22-byte OGM2 header and 16-byte HMAC; radio ACKs caused numerous direction changes and browser round-trips. The last user benchmark at SF7/BW500 was **487.8 application bytes/s, median 2.07-second confirmed RTT**, 8/8 trials. This is the baseline, not the predicted v1.6 result.

In v1.6 the browser **still encrypts the entire OGM4 envelope with AES-256-GCM** but hands the whole ciphertext (up to 7168 bytes) to the ESP32 using bounded, positively acknowledged USB staging. The radio worker owns the RF transfer. Data fragment size is **224 bytes**, radio header **14 bytes**, radio HMAC **16 bytes** = **254-byte maximum SX1278 RF frame** (within the 255-byte PHY FIFO). Four consecutive ciphertext fragments are followed by **one authenticated received-fragment bitmap ACK**. Missing fragments alone are retransmitted in a bounded way (at most five attempts per fragment), and all responses are sent by the single half-duplex radio-owner task. The v1.5 160-byte legacy radio path remains in firmware solely as a fallback/reference; the v1.6 web client uses the new bulk path.

The sender gets `BULKSENT` when every fragment has actually completed SX1278 TxDone (**one gray tick**); `BULKCONFIRMED` means *radio-level* bitmap ACKs accounted for all fragments. **Two gray ticks** still require an independently authenticated encrypted application-delivered receipt, and **two blue ticks** require the independently authenticated encrypted read receipt after viewing. A missing application receipt is still *unconfirmed*, never fabricated. OGM4 AES-GCM application messages and encrypted receipt batching are unchanged.

## Protocol framing and compatibility

- Old HMAC-protected OGM2 control packets retain their wire version and are used for HELLO, profile negotiation, and the new authenticated `OG_TYPE_ARQ_CAPS` capability byte.
- New radio frame begins `0x01` and contains: version, type DATA/bitmap ACK, flags, fragment index, total fragment count, length, two 32-bit transfer-ID halves, up to 224 bytes of encrypted payload or 4-byte received bitmap, and 16-byte truncated HMAC-SHA256. No plaintext is passed to radio firmware.
- The pairing-derived, domain-separated radio HMAC key from v1.5 is reused. Verification is mandatory before changing fragment state. The complete ciphertext is authenticated and decrypted using AES-256-GCM in the receiving browser before the message is accepted.
- Before transmitting v1.6 ARQ DATA, the sender firmware requires the receiving peer's **authenticated OGM2 ARQ capability** announcement. Without it, pending v1.6 ciphertext fails explicitly (`PEER_REQUIRES_V16_ARQ`) instead of being sent to an old v1.5 device. **Flash both boards with v1.6 and load both v1.6 browsers; do not mix versions.**
- USB Web Serial commands are `BULKBEGIN <16hex-id> <bytes> DATA|CTRL`, `BULKCHUNK <id> <sequence> <hex>`, `BULKEND <id> <8hex-CRC32>`. USB chunks are 160 bytes; these are **NOT** per-fragment radio ACKs. CRC32 detects staging corruption, but is not a security MAC. Status includes `BULKREADY`, `BULKSTEP`, `BULKQUEUED`, `BULKSENT`, `BULKPROGRESS`, `BULKCONFIRMED`, `BULKFAILED`; incoming ciphertext uses `RXBSTART`, `RXBCHUNK`, `RXBDONE` with CRC32 and then browser-side AES-GCM verification.
- Maximum single encrypted transfer is **7168 bytes** (32 × 224). It is a bounded-text-oriented transport, not yet a streaming image or voice protocol. Browser chat remains text only, no media features enabled.

## Power profiles and hardware protection

| Manual selector | PHY | Requested output | Conditions |
|---|---|---|---|
| Fast / Low (LOW) | SF7, BW500, CR4/5 | nominal **+18 dBm** | Requires local USB command `POWER18 VERIFIED` after checking supply, 433 MHz antenna and legal duty/power. Explicit SX1278 PA_DAC high-power configuration. |
| Balanced | SF7, BW250, CR4/5 | +10 dBm | Boot default, no high-power opt-in. |
| Max / Range (MAX) | SF9, BW125, CR4/5 | nominal **+18 dBm** | Same local opt-in as Fast; may consume significantly more current and has much lower bitrate. |
| Experimental +22 (EXPERIMENTAL) | **Disabled** | +22 dBm **UNSUPPORTED** | SX1278 PA capability does not provide a documented +22 dBm mode. Selection and firmware command reject it. It is intentionally displayed as disabled rather than misrepresenting measured output. |

The special PA_DAC register path is necessary for nominal +18 dBm; this is **not** an unqualified guarantee of module RF output. The opt-in is volatile, requires physical USB on **each board separately**, and resets on reboot. A stable and appropriately rated 3.3 V supply, antenna, and lawful transmit conditions must be verified before enabling it. While both boards start in Balanced, an authenticated manual profile OFFER/AGREE remains required to switch; the original verification and rollback path is retained. The previous CAD-before-TX lockout was **not reintroduced**. At low distances, do not assume +18 is desirable: start with Balanced and appropriate antenna separation.

Sources: [Semtech SX1276/77/78/79 datasheet](https://cdn.sparkfun.com/assets/a/a/0/b/5/DS_SX1276-7-8-9_W_APP_V5.pdf) (PHY FIFO, LoRa bandwidth/airtime, PA_BOOST, RegPaDac); [Ai-Thinker Ra-02 information](https://docs.ai-thinker.com/en/lora/ra-02) (module rating, subject to revision); [Espressif SPI Master documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/peripherals/spi_master.html) (DMA transfer limits).

## Reliability boundaries

- The bridge uses fixed allocation (bounded `usb_bytes`, `tx_bytes`, and `rx_bytes` buffers), one active transmitting transfer and one buffered receiving transfer. All these buffers hold *ciphertext*, not decrypted messages. The radio worker remains the sole SPI and ARQ owner. Loss of a bitmap ACK triggers selective retransmission; duplicate fragments are detected, and a completed bitmap is available for late duplicates.
- A bitmap ACK is RF confirmation, **not** application delivery. If last-fragment or last-ACK transmission is lost, bounded retransmission and duplicate bitmap ACK recovery should prevent false success. The receiver's browser must still authenticate the OGM4 envelope.
- The browser scheduler currently prioritizes receipt *transfers*, not receipt *fragments* within a long active transfer. Under large messages, this can delay read/delivered receipts. Images and voice are deferred until stream-level scheduling and hardware benchmarks justify their use.
- USB staging requires a live desktop Web Serial browser; the existing BLE/NimBLE code is preserved but **does not provide this v1.6 binary ARQ bulk interface**. Android remains byte-identical to v1.5, **not** a LoRa-capable Android app.
- Half-duplex collisions, RF interference, duty-cycle restrictions, rail sag, browser USB blocking, flash/USB refresh during transfers, and asymmetric manual mode-change confirmation are **not simulated by all host tests**. MACs and ciphertext are authenticated, but persistent RF anti-replay and security audit are still required before a field-critical deployment.

## Required two-board acceptance (real hardware)

1. Save the v1.5 Git ZIP and pairing keys securely. Build v1.6 under a path with no spaces/parentheses using ESP-IDF 6.1; use `idf.py set-target esp32c3 && idf.py build`. Do not erase NVS. Record full build output; these authors did not run the physical ESP-IDF toolchain.
2. Flash **both** ESP32-C3 Super Minis with v1.6. Open **both** v1.6 local web clients and pair. Check `READY` includes `FW=1.6.0-rc.1`, `ARQ=1`, `BULK=1`, `PROFILES=4`, `WINDOW=4`, and `PEER_ARQ=1` after discovery.
3. In **Balanced** (no high-power opt-in needed), send 30 different one-character texts each direction, including bursts and simultaneous sends. Confirm true single/double/blue ticks and no duplicate bubbles or indefinite queues.
4. Run 8 × 1024-byte and 8 × 4096-byte encrypted speed-test trials; record **goodput, p50/p95 RTT, retransmits, bitmap ACKs, failures**, RSSI and exact mode. Benchmark must not claim raw PHY bitrate as application throughput.
5. Test **Fast** only after verifying the supply and antennas separately on each radio, issuing local `POWER18 VERIFIED` on both, and manually switching via authenticated offer/agree. Repeat benchmarks. Do not enable at very close antenna spacing where front-end overload is possible.
6. Test Max/Range only when power/antenna/radio regulations are appropriate. Confirm EXPERIMENTAL +22 is visibly disabled and `PROFILE EXPERIMENTAL` is rejected even through USB.
7. Simulate missing peer, intentional simultaneous transmissions and mid-transfer USB disconnect/reconnect. If delivery receipt is missing, sender must show *unconfirmed*, never a fabricated double gray/blue tick.
8. If the new firmware cannot exchange messages, restore v1.5 and its matching web client on BOTH nodes; capture `READY`, `PROFILE`, `LINK|ARQ_*`, `BULKFAILED` and radio errors from the v1.6 diagnostics. **This v1.6 source is not a hardware-certified stable release.**
