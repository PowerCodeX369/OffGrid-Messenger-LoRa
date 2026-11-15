## v3.0.4-rc.1 Ultra security and reliability limitations

- **SF12 Ultra permits encrypted text and encrypted delivered/read receipts only.** The supplied native/web clients reject media at enqueue and before RF stage; firmware rejects correctly tagged MEDIA bulk commands and `MEDIA ON`. Because OGM4 payloads are ciphertext to the ESP32, deliberately mislabelled DATA from an untrusted custom client cannot be classified at the radio. This is an official-client feature restriction, not a cryptographic payload-type firewall.
- New Ultra RF data/bitmap frames and fragment-length metadata remain HMAC-SHA256-128 authenticated; application text/receipts still use AES-256-GCM. Both peers must advertise an **authenticated** Ultra ARQ capability before the signed two-board mode negotiation. Old peers cannot silently join an incompatible SF12 mode.
- Short packets and longer retries improve tolerance of RF loss but **do not guarantee delivery** without a decodable two-way radio link. SF12 greatly increases airtime; obey local band, output power, antenna and duty-cycle rules. No new +22 dBm mode.
- Persistent replay protection across firmware reboots, independent crypto audit, physical range and full Android/ESP-IDF target build remain outstanding.

## Android native LoRa v3.0.1-rc.1 security boundaries (current)

- The **original native Compose app** is the launcher again. There is **no WebView UI, web-to-native JavaScript bridge, or USB-OTG connection screen** in v3.0.1. The original BLE HMAC challenge/response, Android Keystore-wrapped 32-byte pairing key, and native encrypted history remain.
- Application text, media segments, and receipts use LoRa browser-compatible **OGM4 AES-256-GCM** with authenticated transfer IDs. Each BLE bulk session requires fresh verified `CAPS|FAMILY=LORA|PROTO=2|BULK=1|MEDIA=1` after successful BLE authentication; the app fails closed on the old firmware's BLE media-disabled service.
- The original radio firmware still supports BLE pairing, but the included **opt-in BLE bulk companion** must be flashed onto **both** nodes for current LoRa media. The BLE GATT authentication alone does not provide link-layer confidentiality for all metadata/control commands; device-level BLE pairing and traffic interception need real-hardware threat evaluation.
- This native source has host interoperability simulations, **not** a verified Android APK, ESP-IDF target build, or physical BLE/radio security audit. Persistent authenticated RF control replay defense across MCU resets is still pending.

**Historical section below:** the Android 3.0.0 WebView threat model applies only to the previous, superseded release; its WebView-specific claims do not describe the new native launcher.

---

## Android 3.0.0-rc.1 source candidate: security boundaries

- Local `WebViewAssetLoader` origin, origin-checked `WebViewCompat` native bridge, no Android `INTERNET` permission, and external WebView navigation blocked. This is a protection layer, **not** a substitute for Android on-device testing.
- Radio contents stay encrypted with existing web AES-256-GCM and authenticated by existing OGM2 HMAC. Android BLE key is additionally encrypted at rest using AndroidKeyStore; GATT `KEYGET`/`KEYSET` remain denied and physical USB key provisioning is required.
- For BLE bulk media the **new companion firmware is mandatory** and is not ESP-IDF-compiled or RF-tested here. The initial BLE HMAC challenge authenticates the connection only: active interception of unencrypted GATT control traffic and persistent RF-command replay across resets remain unqualified threats. Secure BLE pairing/transport-layer enforcement and explicit per-command authorization need a further audit.
- Android and LoRa firmware sources have only undergone host simulations and source review; do not deploy as security-qualified without platform builds and physical phone/radio tests.

# v1.10 browser receive hardening

The browser now serializes authenticated work by peer/transfer ID before
performing asynchronous AES-GCM decryption and IndexedDB writes. Concurrent
copies of a received transfer are not processed twice. Legacy radio packet
IDs that arrive during validation receive an APPACK only **after successful
authenticated application handling**. Bad tags and invalid envelopes cannot
poison the success cache; short-term replay memory is bounded and cleared
on disconnect. These properties are covered by deterministic stress and
Chromium integration tests.

**This is not persistent RF control-frame anti-replay across firmware reboots.**
The signed mode-switch/control protocol and its existing session nonce checks
are unchanged. The pairing key remains exportable to a trusted local USB
operator; key rotation between NVS and IndexedDB is not atomic across power
loss. No independent security audit has been completed. See
`docs/V1_10_0_SECURITY_STABILITY.md` for explicit remaining work.

# v1.8 security note — media transport

Image and voice contents are **not plaintext on USB or RF**: files are compressed locally, then segmented into 7,040-byte chunks with a whole-file truncated SHA-256 digest; every segment is independently AES-256-GCM encrypted with a fresh nonce/transfer ID, and the existing LoRa frame HMAC is retained. Partial files never cause delivery/read receipts; only complete verified media is displayed and stored encrypted locally. Untrusted image/audio decoders remain within the browser sandbox. Receiver keeps at most four partial media files in RAM and expires them after 20 minutes (this is not persistent resume). `MEDIA ON` is permitted only via physical local USB and cannot bypass the user-enabled AUTO setting or authenticated profile handshake.

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

## v1.7 automatic radio-control security

Auto is **opt-in** and is not persisted after reboot. `AUTO ON` requires the initiating operator to confirm safe nominal +18 dBm operation of BOTH radios and sends a key-authenticated control frame to the peer. A shared pairing key is authorization, **not a physical measurement or device attestation**. Over-the-air link metrics (packet RSSI/SNR, profile and retry count) are HMAC-authenticated but not encrypted and could reveal changing radio conditions to a local observer. Automatic profile OFFER/AGREE frames carry the same radio HMAC, nonce, peer validation and rollback as manual switching. An attacker with the pairing key can forge controls; compromised devices cannot be distinguished cryptographically without replacing the trust model. A last-seen per-session control ID limits replay during a running session, but persistent replay protection and anti-rollback across resets require further review. This version has no independent security audit or field safety certification. Do not use as an emergency-reliability guarantee.
