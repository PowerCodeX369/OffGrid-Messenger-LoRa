# v3.0.4 Ultra Long Range — source test and qualification report

**The project was built from the user-supplied v3.0.3-rc.1 Complete Git ZIP**, not from the earlier standalone Ultra integration kit. Original Git history and v3.0.3 rollback tag remain preserved. No APK or board binaries are claimed here.

## Host tests executed

- `sh firmware/tests/run-host-tests.sh`: every registered C, Kotlin/JVM, JS/Node, Python, and Clang host-stub test completed successfully, including the original Fast/Balanced/SF9 regression suite.
- `firmware/tests/test_ultra_policy.c`: Semtech SF12/BW125/CR4/5 airtime, LDRO, 64-byte and 38-byte packet airtime, ACK timeouts, two-way authenticated mode policy and allowable payload kinds.
- `firmware/tests/test_ultra_arq.c`: actual authenticated selective-repeat ARQ, fragmented 48/64/80/96-byte ciphertext, lost DATA, lost ACKs, retransmission and integrity-preserving reassembly.
- `tests/test_ultra_end_to_end_contract.py`: existing firmware mode4, authenticated peer capability, HMAC-covered fragment sizing, native Android & web media rejection, SF12 airtime and profile control expectations.
- Existing `firmware/tests/test_lora_auto.c`, `test_lora_modes.c`, `test_profile_switch.c`, `test_arq_bridge.c`: unchanged SF9 profile plus new Ultra and signed two-node control.
- Existing native Kotlin OGM4 AES-GCM, 48 KiB photo/40 KiB voice, delivered/read receipts, BLE bulk framing and original Compose layout checks; existing browser media/receipt/crypto fuzz/security regression tests.

## Unavailable / not certified

- No ESP-IDF 6.1 target toolchain/`idf.py` for a full ESP32-C3 build and flash; the C host-stub syntax check is NOT that build.
- No Android SDK/Gradle APK compilation, real Android device, or signed/released APK.
- No real-world two-node SF12 Ultra RF tests, long-term recovery, measured throughput, regulatory duty-cycle or nominal 18 dBm supply validation. Host tests cannot guarantee delivery at -133 dBm (or after the RF link has disappeared).
- No formal external security audit or reboot-persistent RF-control anti-replay implementation.

## Required acceptance gates

1. Compile firmware under ESP-IDF 6.1 for ESP32-C3 with the actual linked component versions. Verify flash/NVS retention and UART/USB boot on both nodes.
2. Verify Android original GATT pairing and encrypted text close-range, plus one photo and voice in Fast/Balanced/SF9 Max Range without regression; verify original browser app runs after cache refresh.
3. Manually select Ultra from **one** end, confirm both boards emit `PROFILE|ACTIVE|ULTRA`, `LINK` shows SF12/BW125, and two short encrypted texts in each direction reach the other with real gray/blue receipts.
4. Confirm photos and voice cannot be initiated in Ultra in either native Android or browser; test an in-progress media transfer when Auto enters Ultra. Preserve accurate failure/retry if a transfer cannot complete.
5. Simulate several ARQ DATA and ACK losses in Ultra and check on-hardware `BULKSTAT|...|RETX=...|TIMEOUT=...` as well as no false delivered tick. One-character text delivery may still require many seconds at SF12.
6. Enable Auto while radios are close, walk apart in steps and capture `RSSI`, SNR, `PEER_AGE_MS`, `ARQ_TIMEOUT`, `AUTO|SELECT|ULTRA`, and profile confirmation from **both** radios. Don't assume a phone BLE link is a LoRa data link. Roll back both radios together if switching stalls.
7. Verify stronger-link recovery Ultra -> SF9 after prolonged strong bidirectional readings and signing, with no profile oscillations; validate legal frequency/output/duty cycle and antenna/supply before transmitting nominal +18 dBm.
