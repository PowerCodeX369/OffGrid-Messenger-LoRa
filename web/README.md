# OffGrid Messenger LoRa — web v1.5.0-rc.1

Browser-based encrypted text messenger, using USB Web Serial to a dedicated SX1278 ESP32-C3 node. Serve via `python3 -m http.server 8765 --bind 127.0.0.1 --directory web` from the repository root. Use the same 32-byte pairing key on both nodes. Photos, voice and video are intentionally disabled.

**Manual profile switching:** Settings → **Manual LoRa profiles**. One user action negotiates with the peer through authenticated control packets on the *existing* LoRa profile. Both nodes must run v1.5 firmware; v1.2 and v1.5 have incompatible default bandwidths. The UI waits for `PROFILE|ACTIVE`; firmware rolls back if new-mode confirmation fails. Beware pending message/receipt jobs; a forced switch marks them unconfirmed, never delivered.

**Speed test:** Settings → LoRa speed test. Sends encrypted random probes to the paired browser and records message RTT, goodput and failures. Compare 8 × 1024-byte benchmarks in each profile. These are not raw PHY bitrates.

Encryption and tick meanings are unchanged: AES-256-GCM OGM4 application content; radio HMAC-SHA256; one tick after TxDone, double gray after authenticated delivered receipt, blue after authenticated read receipt. Wi-Fi and ESP-NOW are absent from the firmware. The original Android app remains frozen until the final universal-app milestone.
