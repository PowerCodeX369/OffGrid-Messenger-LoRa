# Security policy and disclosure — v3.0.6

**Status:** source feature-complete, hardware/security qualification **not** complete. OffGrid Messenger uses application AES-256-GCM and HMAC-authenticated RF frames, but neither these primitives nor passing host tests constitutes a complete security audit or a guarantee of private communications in all threat models.

## Reporting a vulnerability

Please **do not disclose actionable exploits, private pairing keys, personal chats, device identifiers or NVS dumps in public issues**. If the GitHub repository has **Private vulnerability reporting** enabled, use its **Security → Report a vulnerability** flow. Otherwise contact the maintainer through an available private channel from the GitHub project/profile and request a secure disclosure method before sharing sensitive material. No guaranteed response timeframe is promised.

Include affected commit/tag, component, expected/actual security behavior, minimum sanitized reproduction and potential impact. The maintainer can investigate, document a fix, create a new release tag and coordinate disclosure; do not assume public reports are confidential.

## Trust model

- Two paired ESP32 LoRa nodes exchange authenticated frames; native Android uses authenticated BLE GATT, and the desktop browser uses a locally connected USB serial port.
- Clients encrypt application messages/media with **AES-256-GCM**, while the RF link authenticates protocol frames using a derived HMAC key. Receipt validity is distinct from RF fragment ACKs.
- Users administer a **shared symmetric key**. Physical access/USB provisioning and a compromised endpoint can defeat the confidentiality/identity assumptions; no forward-secret public-key identity protocol is claimed.
- Android's secure key storage and encrypted history protect local stored material, but process/device compromise and unsafe backups remain relevant.

## Disclosed limitations

1. **Persistent RF-control replay protection across board reboots** is not fully established; volatile deduplication is not an audited persistent anti-replay system.
2. **BLE control/metadata confidentiality and active-interception resistance** are not fully established merely by a connection-time HMAC challenge.
3. **Interrupted shared-key rotation** is not implemented as an atomic two-node transaction. Keep an explicit recovery/rollback procedure.
4. Mixed-version Max/Ultra radio peers can be incompatible. Both must use authenticated `ARQ_CAP=4` and `ACK_COMPACT=16` when running the latest release.
5. Ultra's media prohibition is an **official-client policy**, not a cryptographic proof of media type inside opaque encrypted text labels.
6. No independent security assessment, commercial RF compliance certification, signed OTA release chain or emergency-service assurance is claimed.

Do not bypass the explicit +18 dBm operator verification. Never transmit illegally in a local band; radio compliance and software encryption are separate issues. See [architecture](docs/ARCHITECTURE.md), [known limitations](docs/KNOWN_LIMITATIONS.md), [tests](docs/TESTING.md) and archived [historical security notes](docs/history/SECURITY_NOTES_BEFORE_PUBLICATION.md).

## Supported branch/release expectations

The maintained integration branch is `main`, and the current public source milestone is `v3.0.6` plus the documentation-only `v3.0.6-public.1`. Historical tags are preserved for reproducibility **not** necessarily actively supported security releases. When reporting, identify the exact commit rather than relying on the USB `FW=1.8.0-rc.4` compatibility field.
