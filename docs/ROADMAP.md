# Web-first LoRa roadmap

The original ESP-NOW repo remains untouched. The Android app in this repo stays frozen throughout v1.x.

- **v1.0.0**: independent Wi-Fi-free LoRa firmware; one conservative fixed profile, NimBLE, USB, authenticated two-way encrypted text, basic receipts, web client.
- **v1.1.0**: field-proven radio reliability, channel-busy handling, ACK latency instrumentation, collision testing and repair.
- **v1.2.0**: selective-repeat fragmentation, sliding window, bitmap ACK, resume after losses, text priority during large media.
- **v1.3.0**: Fast / Balanced / Ultra-range SX1278 profiles, initially manual then evidence-based adaptation.
- **v1.4.0**: adaptive progressive photos, compression driven by measured link capacity.
- **v1.5.0**: speech-optimized voice encoding, delivery and playback.
- **v1.6.0**: LoRa RX/TX/sleep scheduling and measured battery optimization.
- **v1.7.0**: web app polish and packet-level diagnostics.
- **v1.8.0**: cryptographic/replay audit, interference tests, endurance, field validation.
- **v2.0.0**: last phase—update existing Android app to auto-detect ESP-NOW original firmware versus LoRa firmware over authenticated BLE and render family-specific profiles/compression without degrading legacy support.

Each milestone must pass its own source checks, an ESP-IDF compile, and physical validation before being called hardware-stable.
