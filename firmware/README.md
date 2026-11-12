# OffGrid Messenger LoRa v1.2 firmware

Target: ESP32-C3 Super Mini / ESP-IDF 6.1 / SX1278 Ra-02.

- The firmware **never** calls `esp_wifi_init`, `esp_wifi_start`, `esp_now_init` or `esp_now_send`.
- USB and NimBLE remain active. ESP32-C3's internal Wi-Fi circuitry is not initialized; BLE still legitimately uses the chip's Bluetooth hardware.
- The same pairing key powers browser-side AES-GCM and a **domain-separated** radio HMAC. The key stays in NVS, and `KEYGET` is available on *physical USB only*.
- One task exclusively owns SX1278 SPI. No Wi-Fi radio is involved.
- Default SX1278 SF7 / BW125kHz / CR4/5 / 433 MHz / 10 dBm. Radio mode switching is **not** included in v1.2.
- BLE advertising uses `OffGridLoRa-XXXX`; the original BLE authentication scheme is retained, and authenticated `CAPS?` reports `FAMILY=LORA`. **The original Android app is not LoRa-ready in v1.2.**

Build:

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Do not connect multiple serial clients to the same port. Do not `erase-flash` unless you intend to wipe both firmware and NVS. Always attach a suitable antenna before LoRa TX.

## Local USB protocol

```text
READY|<MAC>|...|FAMILY=LORA|FW=1.2.0-rc.1|FLOW=2
RADIO|SX1278_READY|433000000|SF7|BW125000|CR4/5|TX_DBM=10
STATUS
CAPS?
KEYSTATUS
KEYGET
KEYSET <64 hex chars>
MSGHEX <1..180 bytes hex>
CTRLHEX <1..180 bytes hex>  # high-priority encrypted delivered/read control
APPACK <sender_mac> <sender_session> <radio_id>
READ <sender_mac> <sender_session> <radio_id>
```

`MSGHEX` and `CTRLHEX` yield `QUEUED|session|id|hex`. `SENT` is emitted after SX1278 TxDone, `RADIOACK` after the peer firmware authenticates the frame. The sender browser advances the fragment after `RADIOACK`, while **only AES-GCM-protected OGM4 delivered/read receipts** change the displayed double/blue ticks. `APPACK` is a local USB diagnostic acknowledgment *after browser decryption*, without an extra per-fragment delivery radio packet. `FAILED|...|NO_RADIO_ACK` indicates an unconfirmed RF acknowledgment, not proof that no one received the data. `TRACE|TX_DONE`, `TRACE|RADIO_ACK` and `TRACE|APP_VALIDATED` provide diagnostic timing. Both boards need this new ACK engine; READY advertises `FW=1.2.0-rc.1|...|ACKENGINE=2|FLOW=2`. `LINK|...` reports counters; a green USB connection is *not* proof of a healthy remote radio link. Do not use `KEYGET` output in support logs.

The full [hardware acceptance matrix](../docs/HARDWARE_ACCEPTANCE.md) is mandatory before any stable/hardware-verified claim.

## v1.2 burst reliability and diagnostics

The SX1278 owner task serializes transmissions and prioritizes authentic radio ACKs. `RegModemStat` is checked before best-effort CAD on new DATA transmissions; only channel-busy DATA is deferred (without consuming a retry). Discovery HELLOs are suppressed during recent peer traffic. `LINK` includes `CAD_BUSY`, `PENDING`, `CTRLQ`, `TXFAIL`, `RADIOACK`, and other counters. The browser provides a high-priority one-fragment-at-a-time scheduler, encrypted delivered/read batching, and a Settings → LoRa speed test. It is an **application goodput** test, not an ESP-NOW speed test. The receiver's browser must be connected. Both firmware and web clients must expose `FLOW=2` for this new receipt format.

Do not confuse a radio ACK with the encrypted delivered/read receipt. Single gray tick still means actual TxDone for the whole message, double gray means authenticated application delivery, and double blue means an authenticated read receipt after local viewing. Missing receipt remains unconfirmed; this is not proof that plaintext was lost.
