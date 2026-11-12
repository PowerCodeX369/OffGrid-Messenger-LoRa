# Security

- Chat encryption: original OGM4 AES-256-GCM in the browser, before USB transmission.
- Radio-frame authentication: HMAC-SHA256 truncated to 128 bits, derived with an independent domain label; covers all LoRa headers, HELLO and ACKs.
- Pairing key: 32 random bytes stored in ESP32 NVS. `KEYGET` is a deliberate **physical USB** export path (not BLE). Protect the USB port and browser environment.
- BLE GATT: original challenge/HMAC scheme. The v1.0 BLE capability handshake exists for later universal Android work; LoRa binary GATT transfer is NOT advertised.
- Delivery and read: receiver-browser confirmation is distinct from physical radio ACK; do not treat `SENT` as cryptographic decryption success.
- Limitations: recent replay deduplication only, no secure element, no fully audited persistent replay window, no independent RF security review, no physical hardware test evidence on this release. Do not rely on v1.0 for life-safety or emergency communication.
- Never paste your encryption key into public bug reports. Use private disclosure channels when reporting vulnerabilities.
