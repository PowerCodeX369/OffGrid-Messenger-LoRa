# OffGrid Messenger LoRa — security model

- **Application secrecy:** OGM4 AES-256-GCM in the browser before USB transmission; delivered/read receipts are encrypted and authenticated as application messages. Never infer delivery from a radio-frame ACK alone.
- **Radio-frame authentication:** a key derived from the shared 32-byte pairing key with a separate HMAC domain label authenticates OGM2 LoRa DATA, HELLO, ACK, and **manual profile OFFER/AGREE** packets with 128-bit truncated HMAC-SHA256. Control frames are not encrypted and expose radio metadata.
- **Manual profile control:** only a user command initiates negotiation, and the receiving peer validates the HMAC before agreeing. A short-lived nonce identifies each attempt. Duplicate offers within the session are guarded against, but persistent replay state across reboots has **not** been implemented or formally reviewed. If an attacker obtains the shared key, they can impersonate a trusted peer and request a profile switch.
- **Key handling:** the pairing key lives in ESP32 NVS. `KEYGET` permits deliberate physical USB export; it is not exposed to BLE. Protect the physical board, browser session and serial port. Key rotation drops radio peer trust and pending transfers.
- **BLE:** original NimBLE challenge/HMAC capability path remains for the final universal Android app. LoRa binary GATT transfer is intentionally **not yet** advertised, and physical BLE operation in this release is not verified.
- **Reliability vs confidentiality:** missing radio ACK or encrypted delivered/read receipt means unconfirmed, never guaranteed non-delivery. A forged blue/double tick is not an acceptable performance optimization.
- **Limitations:** recent replay deduplication, no fully audited persistent replay window, no secure element, no external security audit, no measured two-board v1.5 RF/profile stress test. The mode-change protocol is best-effort with timed rollback, not an atomic distributed commit or an emergency communications guarantee.

Please do not include shared pairing keys in bug reports. Report security vulnerabilities privately.
