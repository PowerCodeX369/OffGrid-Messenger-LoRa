# Contributing

OffGrid Messenger LoRa is a separate project from the original ESP-NOW implementation. Do not introduce Wi-Fi/ESP-NOW code or change `android/` before v2.0. Keep all P2P traffic authenticated; preserve OGM4 AES-GCM semantics. Every patch should contain a focused source change, a host regression if feasible, a clear changelog entry, and an explicit note whether ESP-IDF and radio hardware were tested.

For local checks: `firmware/tests/run-host-tests.sh`. Test with an antenna and stable regulated 3.3 V. When reporting failures, share the **first `error:` build line and surrounding output**, or USB `READY`, `RADIO`, `LINK`, `SENT`, `RETRY`, `FAILED`, `DELIVERED` lines. Redact `KEYDATA` and all pairing secrets.
