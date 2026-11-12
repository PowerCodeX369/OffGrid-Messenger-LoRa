# Web-first LoRa roadmap

The original ESP-NOW repo remains untouched. The Android app in this repo stays frozen throughout v1.x.

- **v1.0.0**: independent Wi-Fi-free LoRa firmware; one conservative fixed profile, NimBLE, USB, authenticated two-way encrypted text, basic receipts, web client.
- **v1.1.0**: split real radio TxDone, radio ACK, and authenticated delivery/read receipts; improve the SX1278 FIFO transfer path.
- **v1.2.0**: bounded sequential half-duplex text scheduling, priority encrypted receipt batching, channel contention handling, and a genuine encrypted LoRa speed test. **Selective-repeat/sliding-window/bitmap ACK is deferred** until the one-at-a-time scheduler has passed real two-board burst testing, and will have its own versioned milestone; no claim of selective repeat is made for v1.2.
- **v1.3.0**: Fast / Balanced / Ultra-range SX1278 profiles, initially manual then evidence-based adaptation.
- **v1.4.0**: adaptive progressive photos, compression driven by measured link capacity.
- **v1.5.0**: speech-optimized voice encoding, delivery and playback.
- **v1.6.0**: LoRa RX/TX/sleep scheduling and measured battery optimization.
- **v1.7.0**: web app polish and packet-level diagnostics.
- **v1.8.0**: cryptographic/replay audit, interference tests, endurance, field validation.
- **v2.0.0**: last phase—update existing Android app to auto-detect ESP-NOW original firmware versus LoRa firmware over authenticated BLE and render family-specific profiles/compression without degrading legacy support.

Each milestone must pass its own source checks, an ESP-IDF compile, and physical validation before being called hardware-stable.
