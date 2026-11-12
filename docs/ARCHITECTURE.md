# v1.0 LoRa-only architecture

## Runtime ownership

```text
Chromium (Web Serial) --USB--> USB command task ----+  
                                                    | -> LoRa task -> SX1278 SPI2 -> 433 MHz P2P
Original NimBLE GATT ----- BLE command task --------+  
                      BLE notify <--- LoRa task -----+
```

1. ESP32-C3 uses the eFuse-derived base MAC for a stable identity **without initializing Wi-Fi**. The firmware includes no Wi-Fi state machine, ESP-NOW MAC callback, hybrid fallback or PHY adaptation.
2. Radio task configures SX1278, then publishes `RADIO|SX1278_READY` and `READY|...|FAMILY=LORA`. USB and BLE are independently initialized; the LoRa task owns the SPI bus and TX/RX modes.
3. Browser encrypts OGM4 payload using AES-256-GCM under the shared 32-byte node key. The LoRa task derives a separate HMAC-SHA256 key via `HMAC(pairing_key, "OffGrid-LoRa-HMAC-v1")`, then authenticates the full OGM2 packet with the first 16 HMAC bytes. The radio never receives plaintext chat content.
4. A keyed HELLO is sent periodically. Peer discovery occurs only after successful tag validation. No NVS peer MAC needed; a new board can be paired without source modifications.
5. Upon receiving authenticated DATA, the peer MCU schedules a short randomized radio ACK; this confirms a radio-level reception, not browser decryption. It forwards a fragment via `RX` and requires an independent `APPACK` from the browser.
6. `APPACK` triggers a radio DELIVERED receipt. Sender firmware reports `DELIVERED` to the sender browser when received, resolving the current fragment. After full AES-GCM authentication the receiver browser queues an encrypted delivered receipt, and read receipts follow on user-visible viewing. These aren't faked by radio ACK.
7. The radio task sends high-priority control frames before queued DATA. Waiting ACKs reserve the half-duplex channel until their random turnaround time. Data attempts are bounded (4); once exhausted, the sender receives `FAILED|...|UNCONFIRMED`, which does *not* assert the receiver never got it.
8. All app content is direct peer-to-peer. There is no network server, no Wi-Fi bootstrap and no hosted web interface on the microcontroller.

## Important v1.0 limitations

This is a first integrated source baseline. It uses only one fixed LoRa profile and a stop-and-wait browser fragment stream. Large encrypted payloads may take multiple radio packets and are not performance-optimized. Recent packet deduplication is in volatile RAM; an extended persistent replay window, selective-repeat fragmented media, automatic RF adaptation, and power/sleep optimization belong to later releases. BLE GATT carries the existing original authentication interface but the Android universal dual-firmware application **has not yet been implemented**.

Validated by unit tests and static integration checks only; physical battery/radio range and real full ESP-IDF build remain to be measured.
