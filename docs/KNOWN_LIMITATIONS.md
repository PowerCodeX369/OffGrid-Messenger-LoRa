# Known limitations and non-goals — v3.0.6

These items are documented so downstream users do not mistake source-level tests or feature completion for a certified consumer device.

## Hardware / RF

- Auto is **off at boot**. The user must select it and authorize nominal +18 dBm locally; the app will initially show manual Balanced. No new default-Auto feature is claimed.
- LoRa RSSI comes from **successfully decoded, authenticated packets**. A completely silent link provides no new RSSI. Lost-peer scanning/rendezvous is best effort and may take time or fail under persistent interference or true out-of-range conditions.
- Exact −100/−130 threshold mapping has **no hysteresis** by explicit design; RSSI fluctuations near boundaries can cause repeated negotiations. Forward/reverse switching requires authentication and RF airtime.
- SX1278 operation is half duplex; SF12 packets have long airtime. A single reported urban range cannot establish legal or reliable operation in another environment.
- Factory profile hardcodes **433.000 MHz**, +10/+18 dBm settings, and does **not implement jurisdiction-specific duty-cycle enforcement**. Spectrum compliance must be established before transmitting.
- Single factory application partition, **no OTA, secure boot or documented signed firmware update/rollback implementation**.
- Not a routing mesh; only direct peers are modeled, not many-to-many multi-hop topology.

## Client and protocol

- Ultra is **text and encrypted receipts only** in official clients; it cannot be relied upon for photo/voice delivery.
- Web Serial requires a supported desktop browser and USB port. It does not work universally in Safari/Firefox/mobile browsers.
- Android build and background BLE operation require real Android SDK/device testing; source/JVM tests do not establish BLE reliability on all devices.
- A local radio ACK is not proof of application delivery. Some unconfirmed messages remain pending; interruptions can require retry/re-pair and may fail permanently out of RF range.
- Durable partial-media transfer resume after complete client loss is not guaranteed; browser storage quota and native whole-snapshot history updates may limit scalability.

## Security / release

- Shared symmetric key and physical USB key provisioning mean physical/endpoint compromise can reveal impersonation capability; no forward-secret per-peer identity exchange is claimed.
- RF authenticated-control replay resistance **across reboot**, transactional key rotation and BLE GATT active-interception hardening remain open for formal review.
- No independent security audit, actual Android APK/ESP-IDF build evidence in this preparation environment, official binaries, hardware endurance/range warranty, or regulatory certification is claimed.
- GPL-3.0 source is offered *as is*, without guarantees for critical, emergency or life-safety use.

See [testing](TESTING.md), [security](../SECURITY.md), [architecture](ARCHITECTURE.md) and [release checklist](RELEASE_CHECKLIST.md).
