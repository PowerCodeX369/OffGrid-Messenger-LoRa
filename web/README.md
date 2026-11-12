# OffGrid Messenger LoRa v1.1 web client

Original OffGrid Messenger browser UI and OGM4 AES-GCM codec adapted for **SX1278 LoRa text-only** messaging.

From the root: `python3 -m http.server 8765 --bind 127.0.0.1 --directory web` and open <http://127.0.0.1:8765> in Chrome or Edge. Each connected ESP32 requires its own Web Serial session. Pair both using the same 32-byte key. No web content is hosted on the ESP32 and no internet is required.

The app rejects firmware without `FAMILY=LORA`. No `RATE` or `TRANSPORT` commands are sent. Unsupported photo and voice controls are hidden/disabled until they are specifically implemented and tested in later LoRa releases. The original photo/voice source has been retained as a **reference**, not an enabled capability. Inherited browser cryptography tests cover its codecs but are **not proof of LoRa media throughput**.

The UI starts no radio until SX1278 initialization has succeeded; USB `READY` is emitted only after that step. During communication, authenticated delivery and read controls have an independent web queue and bypass ongoing user text transfers.

**Tip:** Browser refresh closes Web Serial; reconnect using Connect ESP32. The pairing key remains in the ESP32 NVS across ordinary flashes. The interface reads it only through physically authorized USB.

## v1.1 delivery engine

Use the v1.1 browser with **both** boards running v1.1 firmware (`ACKENGINE=2`). A local `SENT`/SX1278 TxDone event produces the single tick when all fragments were transmitted; `RADIOACK` advances an authenticated fragment; only AES-GCM delivered/read envelopes produce double/blue ticks. Missing final receipt becomes explicitly unconfirmed, never false success. An encrypted delivered/read control enters priority `CTRLHEX`. Browser prepares up to three text fragments but the firmware sends only one RF-unacknowledged frame at a time. A 500-character Unicode text may still take multiple seconds due to physical airtime; photos/voice/video remain disabled.
