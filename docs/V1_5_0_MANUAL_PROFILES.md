# v1.5.0-rc.1 — manual profiles, negotiation and acceptance

**Status:** host regression candidate. No real ESP-IDF 6.1 build or physical-radio profile-change test has been completed here. The user's previous reference goodput at 1 metre was ~227.8 B/s for 1024-byte authenticated probes, 4.45 s median RTT using v1.2 SF7/BW125; do not extrapolate this to the new modes without measurement.

## Why these three modes?

- **LOW** SF7/BW500/+2 dBm: large RF bandwidth means short symbol time and fast data at the cost of worse sensitivity and lower output power. Best first mode for two nearby nodes.
- **BALANCED** SF7/BW250/+10 dBm: startup mode. Tests require **both** boards reflashed to this version; earlier v1.2 boots SF7/BW125 and cannot hear Balanced.
- **MAX** SF9/BW125/+17 dBm: increased spreading factor and output power improve sensitivity/link budget, but increase airtime, so image-sized transfers would be slower. This is the maximum normal PA_BOOST setting configured in our driver—not the Semtech +20 dBm special high-power path. Never claim maximum legal power without checking local rules or verified Ra-02 module/power hardware.

SX1278 datasheet specifies BW up to 500 kHz and an additional +20 dBm mode with special PA_DAC/OCP/restrictions. The present design intentionally leaves PA_DAC at 0x84 and max output at +17 dBm. ESP-IDF SPI DMA supports the driver’s >64-byte FIFO transaction and remains enabled.

## Manual coordinated transition protocol

1. Both nodes boot **Balanced**; connect both USB clients and configure matching AES pairing keys. Wait for `LINK` to show a recently authenticated peer.
2. A user chooses a new mode in Settings on **one** browser. There is **no automatic RSSI/distance-based switching**.
3. The initiating firmware refuses a normal request while its radio DATA frames are outstanding; a deliberate `FORCE` command cancels them as **unconfirmed**. The peer must also be free of pending data frames to participate.
4. Initiator sends a fresh nonce-tagged `OG_TYPE_PROFILE_OFFER` on the **old** profile; it is authenticated by the existing HMAC frame key and retransmitted a bounded number of times if necessary.
5. Receiver validates the current authenticated peer, the mode and nonce, queues authenticated `OG_TYPE_PROFILE_AGREE`, and both nodes schedule transition after the reply. The radio owner task remains the sole SPI owner.
6. Both change SX1278 registers in sleep/standby, then broadcast authenticated HELLOs on the **new** profile. Normal data traffic is held during negotiation/verification. `PROFILE|ACTIVE` is emitted only after receiving a valid peer packet on the new profile.
7. If agreement never arrives, the initiating node stays in its previous mode (`PROFILE|TIMEOUT`). If no peer is heard on the new profile within 6.5 seconds, each unconfirmed node rolls back (`PROFILE|ROLLED_BACK`). A node confirmed early continues extra HELLOs briefly to give the other one a chance to confirm.
8. Every reboot returns to Balanced. A profile stays volatile; no stale NVS radio setting can permanently isolate a node.

**Limitations:** This is a practical authenticated best-effort two-node mode change, not an atomic two-phase database commit. RF interference can cause asymmetric confirmation; a power loss mid-change can leave nodes temporarily mismatched. Reset both to restore Balanced and retry if needed. Persistent replay protection across reboot, channel duty-cycle management, and formal state-machine model checking remain future security/validation work. Only the connected authenticated peer can request a mode change; a radio ACK itself does not certify application delivery.

## Two-device acceptance matrix — MUST PASS on hardware

- [ ] Actual ESP-IDF 6.1 firmware builds on Linux for ESP32-C3; no warnings promoted to errors.
- [ ] Both boards show `FW=1.5.0-rc.1`, `PROFILES=3`, `ACKENGINE=2`, `FLOW=2` and `PROFILE|ACTIVE|BALANCED`.
- [ ] Initial Balanced: 20 text messages each direction and correct one/double/blue ticks. No lost or duplicate user-visible content.
- [ ] Using just one UI selector, Balanced→Low succeeds on both boards; active mode/counters agree. Run 8 × 1024-byte benchmark in Low.
- [ ] Low→Balanced succeeds; repeat benchmark and 20-message burst.
- [ ] Balanced→Max succeeds only with stable 3.3 V supply, matched antennas, and permitted RF operation. Repeat 20-message/8-probe checks at a suitable, non-overloading distance.
- [ ] Max→Balanced returns to baseline; without power cycling, messages and read receipts still work.
- [ ] Force a mode change with a deliberately stuck transfer. It should not fake delivered/read ticks. Application should classify it unconfirmed.
- [ ] Test both nodes trying a mode change concurrently; ensure either safe error/timeout or successful common mode, never irreversible lockout.
- [ ] Temporarily disable the peer during negotiation and verify rollback or explicit timeout. Press reset to restore Balanced if necessary.
- [ ] Confirm BLE advertising survives repeated mode changes; Android functionality is not yet supported/tested.
- [ ] Compare application goodput B/s, median/p95 RTT, retry/fail counts and TX current in each mode. Never label theoretical bitrate as throughput.

**One metre is sufficient for repeatable close-range throughput comparisons**, but range and maximum-power settings require responsible field testing away from overload conditions. The module is not safe to transmit without its antenna, and an inadequate 3.3 V regulator can cause brownouts or erratic behavior. Keep the known-working v1.2 firmware and web backup until this candidate passes physical acceptance.
