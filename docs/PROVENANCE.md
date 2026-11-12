# Provenance

Original (ESP-NOW-only) project: <https://github.com/PowerCodeX369/OffGrid-Messenger>.
Original source snapshot: repaired commit **`263502f`** (PowerCodeX).

- `android/`: byte-for-byte original source snapshot (97 files), see `ANDROID_FROZEN_SHA256.txt`. **Not LoRa-ready in v1.0.**
- `web/protocol.js`, `web/adaptive.js`, UI assets: derived from the original encrypted browser messenger.
- `web/app.js`, `web/index.html`, `web/styles.css`: adapted from original; ESP-NOW radio controls and outgoing photo/voice sends disabled in v1.0.
- `firmware/main/ble_bridge.c`, BLE helper headers: inherited NimBLE GATT design with LoRa-capability identification and disabled unsupported binary-path advertisement.
- `firmware/components/offgrid_sx1278/`: adapted from preliminary Hybrid experimental SPI driver code; redesigned pure-LoRa framing and ACK engine is entirely new in `firmware/main/`.
- **Not imported:** the old Hybrid ESP-NOW/LR rate implementation, dual-radio switch, LoRa bridge queues and transport retry logic.

The original repository and its tags are not modified by this LoRa source preparation. The optional README link to this new project must be added by the owner to the original repository only if desired.
