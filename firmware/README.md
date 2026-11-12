# OffGrid Messenger LoRa v1.0 firmware

Target: ESP32-C3 Super Mini / ESP-IDF 6.1 / SX1278 Ra-02.

- The firmware **never** calls `esp_wifi_init`, `esp_wifi_start`, `esp_now_init` or `esp_now_send`.
- USB and NimBLE remain active. ESP32-C3's internal Wi-Fi circuitry is not initialized; BLE still legitimately uses the chip's Bluetooth hardware.
- The same pairing key powers browser-side AES-GCM and a **domain-separated** radio HMAC. The key stays in NVS, and `KEYGET` is available on *physical USB only*.
- One task exclusively owns SX1278 SPI. No Wi-Fi radio is involved.
- Default SX1278 SF7 / BW125kHz / CR4/5 / 433 MHz / 10 dBm. Radio mode switching is **not** included in v1.0.
- BLE advertising uses `OffGridLoRa-XXXX`; the original BLE authentication scheme is retained, and authenticated `CAPS?` reports `FAMILY=LORA`. **The original Android app is not LoRa-ready in v1.0.**

Build:

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Do not connect multiple serial clients to the same port. Do not `erase-flash` unless you intend to wipe both firmware and NVS. Always attach a suitable antenna before LoRa TX.

## Local USB protocol

```text
READY|<MAC>|...|FAMILY=LORA|FW=1.0.0
RADIO|SX1278_READY|433000000|SF7|BW125000|CR4/5|TX_DBM=10
STATUS
CAPS?
KEYSTATUS
KEYGET
KEYSET <64 hex chars>
MSGHEX <1..180 bytes hex>
APPACK <sender_mac> <sender_session> <radio_id>
READ <sender_mac> <sender_session> <radio_id>
```

`MSGHEX` yields `QUEUED|session|id|hex`; the firmware sends authenticated packets and outputs `SENT` after a radio ACK, `DELIVERED` after a remote browser fragment `APPACK`, and `FAILED|...|UNCONFIRMED` if retries expire. `LINK|...` reports counters; a green USB connection is *not* proof of a healthy remote radio link. Do not use `KEYGET` output in support logs.

The full [hardware acceptance matrix](../docs/HARDWARE_ACCEPTANCE.md) is mandatory before any stable/hardware-verified claim.
