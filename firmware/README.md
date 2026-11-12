# OffGrid Messenger LoRa — firmware v1.5.0-rc.1

ESP32-C3 SX1278 **LoRa-only** firmware. Build using ESP-IDF 6.1: `idf.py set-target esp32c3 && idf.py build`. The firmware does not initialize Wi-Fi or ESP-NOW. USB Serial/JTAG and NimBLE continue to serve the original encrypted browser protocol. The two-radio Hybrid source and ESP-NOW firmware are **not** imported into this runtime.

## Manual SX1278 profiles

| Mode | SF | BW | Power | Use |
|---|---:|---:|---:|---|
| LOW | 7 | 500 kHz | +2 dBm | Fastest and low-power at close range |
| BALANCED (boot) | 7 | 250 kHz | +10 dBm | Middle setting |
| MAX | 9 | 125 kHz | +17 dBm | More RF link budget, slower |

Default is always **BALANCED** after reset, and it is not saved in NVS. Use `PROFILE LOW`, `PROFILE BALANCED`, or `PROFILE MAX` in the local Web Serial UI or through a bidirectional serial terminal. Switching is manually initiated: authenticated OGM2 offer/agree packets are sent using the *old* profile before both ends reconfigure. The protocol verifies authenticated HELLO on the new profile and rolls back if confirmation is missing. `PROFILE <MODE> FORCE` lets diagnostics cancel pending packets, which must be treated as delivery-unconfirmed. See `../docs/V1_5_0_MANUAL_PROFILES.md` for limitations.

`STATUS` prints `READY`, `LINK`, and `PROFILE|ACTIVE`/`TRANSITION`. The normal worker uses only one unacknowledged radio frame at a time and prioritizes radio ACKs and authenticated browser receipts. The speed benchmark remains in the web UI.

Power: a matched antenna must be attached when transmitting, 3.3 V must remain stable under load, and Max uses **+17 dBm standard PA_BOOST**, not unqualified +20 dBm mode. Verify applicable 433 MHz rules. If your ESP32-C3 Super Mini 3.3 V supply sags, use a properly rated regulated 3.3 V supply for the SX1278 with common ground and no tied regulator outputs.

The media and Android integrations remain disabled/frozen. Full ESP-IDF compilation and real two-board validation are pending; header-only host checks do not prove peripheral or RF behavior.
