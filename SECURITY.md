# OffGrid Messenger LoRa — security model

- **Application secrecy:** OGM4 AES-256-GCM in the browser before USB transmission; delivered/read receipts are encrypted and authenticated as application messages. Never infer delivery from a radio-frame ACK alone.
- **Radio-frame authentication:** a key derived from the shared 32-byte pairing key with a separate HMAC domain label authenticates OGM2 LoRa DATA, HELLO, ACK, and **manual profile OFFER/AGREE** packets with 128-bit truncated HMAC-SHA256. Control frames are not encrypted and expose radio metadata.
- **Manual profile control:** only a user command initiates negotiation, and the receiving peer validates the HMAC before agreeing. A short-lived nonce identifies each attempt. Duplicate offers within the session are guarded against, but persistent replay state across reboots has **not** been implemented or formally reviewed. If an attacker obtains the shared key, they can impersonate a trusted peer and request a profile switch.
- **Key handling:** the pairing key lives in ESP32 NVS. `KEYGET` permits deliberate physical USB export; it is not exposed to BLE. Protect the physical board, browser session and serial port. Key rotation drops radio peer trust and pending transfers.
- **BLE:** original NimBLE challenge/HMAC capability path remains for the final universal Android app. LoRa binary GATT transfer is intentionally **not yet** advertised, and physical BLE operation in this release is not verified.
- **Reliability vs confidentiality:** missing radio ACK or encrypted delivered/read receipt means unconfirmed, never guaranteed non-delivery. A forged blue/double tick is not an acceptable performance optimization.
- **Limitations:** recent replay deduplication, no fully audited persistent replay window, no secure element, no external security audit, no measured two-board v1.5 RF/profile stress test. The mode-change protocol is best-effort with timed rollback, not an atomic distributed commit or an emergency communications guarantee.

Please do not include shared pairing keys in bug reports. Report security vulnerabilities privately.

## v1.6 integrated selective-repeat security

AES-256-GCM OGM4 application encryption, local history authentication, and encrypted delivered/read receipts remain browser-owned. The new RF DATA and bitmap ACK frame format is authenticated with a **mandatory** truncated 128-bit HMAC-SHA256 from the existing domain-separated LoRa pairing key. Any unauthenticated RF data is rejected before state mutation. A radio bitmap ACK alone **does not** create an application delivered/read tick. USB CRC32 detects staging errors only and provides no secrecy or authenticity. The authenticated v1.6 capability control frame is required to avoid sending incompatible data to v1.5 devices.

The +18 dBm path is explicitly gated by operator action over physical USB on each board; reboot clears it. The experimental +22 dBm request is not available. Future security work includes persistent replay protection, independent crypto review and hardware power/duty-cycle validation; source tests do not establish end-to-end field security. Never upload `KEYDATA` or pairing keys in bug reports.

## v1.6.1-rc.2 signed two-board power confirmation

HMAC-authenticated PROFILE OFFER/AGREE packets now include a sixth payload byte asserting **the initiating operator** checked the power/antenna and regulatory conditions of **both** devices. The receiving radio follows without its own USB click. This is an operator declaration, **not cryptographic proof of actual current, RF output, regulator adequacy or compliance**. Stolen shared keys could authorize a malicious profile switch; keep keys and serial access protected. Replayed historical controls remain a future security-hardening concern; session/nonce checks are not a persistent replay database. The +22 dBm menu/firmware mode is removed.
