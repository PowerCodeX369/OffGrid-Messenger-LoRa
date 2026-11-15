# System architecture — current v3.0.6

## Scope

OffGrid Messenger is **two-endpoint, direct peer-to-peer LoRa**. It is not a routing mesh and has no cloud relay or account backend. A client attaches to a nearby ESP32; the ESP32 authenticates the RF peer and exchanges ciphertext with the remote node.

```text
Android native Kotlin/Compose                   Desktop Chrome/Edge
  UI + AES-256-GCM + history                      UI + Web Crypto + IndexedDB
          | BLE GATT                                    | Web Serial USB
          +-----------------------+---------------------+
                                  |
                         LOCAL ESP32-C3
                 Authenticated BLE/USB command bridge
                       Single LoRa worker/task
                  HMAC-authenticated RF control/data
                       SX1278 SPI transceiver
                                  |
                      433 MHz LoRa P2P link
                                  |
                         REMOTE ESP32-C3
                                  |
                      Remote Android/web client
```

Clients do the application encryption and decryption; the firmware forwards authenticated encrypted payloads. There is a **single SX1278 SPI owner**. BLE RSSI and USB connectivity are not LoRa signal measurements.

## Source map

| Source | Responsibility |
| --- | --- |
| `firmware/main/main.c` | Radio task, peer/session state, transport and control commands |
| `firmware/main/lora_modes.h` | Four modem configurations (including Ultra) |
| `firmware/main/lora_auto.h` | Threshold classification and Auto state |
| `firmware/main/lora_link_recovery.h` | Bounded radio silence scans/rendezvous |
| `firmware/main/lora_profile_control.h` | Two-peer authenticated profile protocol |
| `firmware/components/offgrid_sx1278/` | SX1278 SPI/GPIO driver and profiles |
| `firmware/components/offgrid_arq/` | Fragmentation, selective-repeat ARQ, USB staging |
| `firmware/main/ble_bridge.c` | Authenticated GATT bridge and notification transport |
| `web/protocol.js`, `android/.../OgmCodec.kt` | OGM4 AES-GCM application envelopes |
| `web/app.js`, `android/.../MainActivity.kt` | Client presentation, receipts and session lifecycle |
| `web/lora_bulk.js`, `android/.../LoRaNativeBulk.kt` | Client-side bulk framing |

## Message integrity and delivery

1. Each client encrypts application text or compressed media before handing ciphertext to its ESP32. The shared 32-byte pairing key is sensitive. AES-256-GCM protects application content.
2. The radio transport uses HMAC-derived authentication for RF control and data frames. The RF header family is `OGM2`; the application ciphertext envelope is `OGM4`.
3. Sender and receiver firmware use ARQ fragments, selective repeat and authenticated radio acknowledgments. In Max and Ultra, ACKs are 16-byte authenticated tags; both peers need matching `ARQ_CAP=4`.
4. A radio ACK proves a frame-level event, **not** that the receiver application decrypted or displayed a message. The receiving client checks GCM and emits independent authenticated encrypted delivered/read receipts. Missing receipts remain unconfirmed.
5. Browser and Android maintain bounded retry queues for unconfirmed text. Media is limited by client/firmware transfer size and profile.

## Radio mode state

- At startup, firmware uses **manual Balanced**. `AUTO ON` remains opt-in and requires a local high-power safety confirmation; it is not persisted in NVS.
- **Auto** classifies *fresh authenticated* received LoRa RSSI: `> -100` Balanced; `-130 .. -100` Max; `< -130` Ultra. Both peers negotiate and verify transitions; local changes cannot reliably force an out-of-range remote to change immediately.
- On complete peer silence, no new RSSI is available. Firmware probes alternate modulations using bounded scan/rendezvous windows, only accepting recovered contact after authenticating RF frames; recovery has no guaranteed time.
- Fast is still available manually and as a temporary media accelerator; an abandoned session cannot block recovery indefinitely.

## Version/capability handling

The firmware's USB/BLE `FW=1.8.0-rc.4` response remains intentionally pinned to a **historical compatibility identifier** even though the Git source is 3.0.6. Check `PROFILES=4`, `AUTO_SCAN=TRIBAND`, `ARQ_CAP=4` and `ACK_COMPACT=16` to identify modern protocol features. See [`VERSIONS.md`](VERSIONS.md).

## Trust boundaries and known risks

- Symmetric key compromise enables impersonation; device-level identities and post-compromise forward secrecy are not guaranteed.
- BLE HMAC authentication does not independently guarantee confidentiality of all GATT metadata/control commands.
- RF control replay resilience across resets, safe interrupted key rotation and formal protocol review remain open.
- LoRa is half duplex, airtime-intensive at SF12, sensitive to antenna choice and interference, and region-dependent in legality.
- This is not a safety-critical or professionally certified emergency communications system.

See [`SECURITY.md`](../SECURITY.md), [`KNOWN_LIMITATIONS.md`](KNOWN_LIMITATIONS.md), and [`TESTING.md`](TESTING.md).
