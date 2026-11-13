# v1.7 automatic Fast-first SX1278 mode selection — engineering specification

**Status:** source-level release candidate based on the user's confirmed-working **v1.6.1-rc.2**. The user-measured v1.6.1 Balanced reference is **457.4 B/s goodput, 1.95 s median application RTT, 8/8 confirmed for 1,024-byte probes**. No v1.7 radio/throughput test has been performed. All existing encryption, bulk USB, six-fragment selective-repeat ACK, 25-ms guard, and delivered/read receipt engines are unchanged and verified byte-identical against v1.6.1 sources.

## Why not define a fixed maximum distance?

LoRa receiver sensitivity is a function of SF, bandwidth, interference, antenna quality and front-end calibration; distance alone does not predict packet delivery. Semtech SX1276/77/78/79 datasheet LF port typical sensitivities (representative at 433 MHz) are approximately **−118 dBm at SF7/500 kHz**, **−122 dBm at SF7/250 kHz**, and **−131 dBm at SF9/125 kHz**. These are typical *receiver sensitivity numbers*, not recommended switch points or a guaranteed range. SX1278 packet RSSI uses the LF correction (`-164 + RegPktRssiValue`; add negative packet SNR as appropriate), and per-packet SNR is signed quarter-dB. RSSI can require front-end calibration. Source: Semtech SX1276/77/78/79 datasheet, revision 7 May 2020 (datasheet tables and §5.5.5). For link adaptation reference: The Things Stack ADR describes SNR margin, recent frame statistics and conservative changes; our private two-node protocol is NOT LoRaWAN.

## Three modes

| Mode | PHY | Manual TX | Auto TX | Typical sensitivity |
|---|---|---:|---:|---:|
| Fast (`LOW`) | SF7/500 kHz, CR4/5 | nominal +18 dBm | nominal +18 dBm | −118 dBm |
| Balanced | SF7/250 kHz, CR4/5 | **+10 dBm** | **nominal +18 dBm** | −122 dBm |
| Range (`MAX`) | SF9/125 kHz, CR4/5 | nominal +18 dBm | nominal +18 dBm | −131 dBm |

**Why boost Auto Balanced?** Changing from Fast +18 dBm to Balanced +10 dBm would lose 8 dB of TX power while improving receiver sensitivity by only about 4 dB, so *Balanced at +10 dBm is not necessarily longer range than Fast at +18 dBm*. Auto Balanced instead keeps the same requested 18 dBm and uses a narrower bandwidth for margin. Manual Balanced remains unchanged as a lower-power option.

18 dBm is a **module nominal request**, not verified antenna-port output. The SX1278 driver uses a PA_DAC setting for this and requires correctly rated regulator, matched antenna and compliance with local 433 MHz/channel-bandwidth/EIRP rules. The unsupported +22 dBm control is absent. No auto/PA18 setting is saved across reboot; reset returns to manual Balanced at +10 dBm.

## Initial policy and thresholds (need real field tuning)

The controller takes **the worse of recent authenticated local and remote packet RSSI and SNR readings**. Each node publishes its own averaged metrics on an authenticated control frame approximately every 3.5 seconds during RF idle. The coordinator is the lexicographically lower 6-byte MAC. One node initiates the same signed profile negotiation used by v1.6.1-rc.2. The other node follows; manual mode buttons win over Auto. A switch requires the radio to be idle and the peer's metrics to be fresh. Samples are EWMA-smoothed and never mapped into metres.

| Condition in Auto | Threshold on the worse direction | Action |
|---|---|---|
| Fast weakening | RSSI ≤ **−106 dBm** **or** SNR ≤ **−1 dB**, or repeated ARQ losses | Move to Balanced |
| Balanced weakening | RSSI ≤ **−113 dBm** **or** SNR ≤ **−3 dB**, or repeated ARQ losses | Move to Range |
| Balanced recovered | RSSI ≥ **−97 dBm** **and** SNR ≥ **+3 dB** | Promote to Fast |
| Range recovered | RSSI ≥ **−104 dBm** **and** SNR ≥ **+2 dB** | Promote to Balanced, then Fast if further improvement |
| Emergency severe fading | Fast RSSI ≤ −113 or SNR ≤ −4; Balanced RSSI ≤ −119 or SNR ≤ −5 | Permit an earlier downgrade within cooldown |

These are **conservative engineering defaults** with RSSI hysteresis, **two consecutive downgrade decisions**, **four consecutive upgrade decisions**, roughly 3.5-second decision spacing, 45-second nominal cooldown and 22-second general promotion stabilization. Emergency downgrade still requires fresh telemetry from both ends and repeated decisions. Distances in metres cannot be derived from these thresholds without field measurements. The policy does **not** scan another PHY when a link is already completely silent: it is impossible to guarantee simultaneous switching between two isolated independent radios without a shared rendezvous channel or time synchronization. A reboot of both nodes restores Balanced.

## Authentication and synchronization

1. The operator connects both ESP32s with v1.7 firmware and the matching v1.7 web app. Both use the same 256-bit pairing key. The authenticated peer must be alive.
2. In Settings → Automatic / manual LoRa modes, choose **Enable automatic mode** on ONE browser; the operator confirms both boards are suited for nominal 18 dBm. USB sends `POWER18 VERIFIED` then `AUTO ON`.
3. The initiating board sends a signed `OG_TYPE_AUTO_CTRL` with nonce, receives `OG_TYPE_AUTO_ACK`, and enables Auto. The peer authenticates the control before enabling and can retry duplicate ACKs. The change is not accepted merely because a browser displays a selected mode.
4. Both boards begin HMAC-protected periodic `OG_TYPE_AUTO_METRIC` reports containing each board's locally measured RSSI, SNR, sample freshness and recent RF loss indicator. Peer quality reports with stale IDs or mismatching mode are ignored.
5. Only the coordinator proposes a `PROFILE_OFFER` flagged as Auto; the receiving peer must already be Auto-enabled and free of pending transfers. The original `PROFILE_AGREE` response, verified NEW-profile HELLO, bounded timeout and rollback remain in force. No normal user message/receipt is marked delivered by a mode-change ACK.
6. When the user selects a **manual** mode, the browser disables Auto and invokes the manual signed profile change. The peer follows without a second browser click. `AUTO OFF` can also be issued alone via USB. `STATUS` includes `AUTO` and `PROFILE` diagnostics.
7. If Auto negotiation fails, the initiating browser displays an error; unsupported legacy peers cannot silently enable Auto. If a profile switch loses connectivity, the original timed rollback applies. If a radio becomes completely unreachable, reboot **both** boards to restore manual Balanced.

## Physical acceptance checklist

- [ ] Compile actual ESP-IDF 6.1 firmware (`idf.py set-target esp32c3 && idf.py build`) from a clean folder; both boards report `FW=1.7.0-rc.1`, `AUTORF=1`, `ARQ=1`, `BULK=1`, `PROFILES=3`.
- [ ] With Auto **OFF**, repeat the previous 8×1,024 B Balanced benchmark and 30-message two-way burst. Delivery/read ticks and no lost/duplicated content must match the known-working v1.6.1-rc.2.
- [ ] On one browser, enable Auto after confirming both stable supplies/antennas and regulatory permissions. Verify `AUTO|ON` locally and `AUTO|PEER_ON` remotely; only one coordinator reports leader status.
- [ ] In a strong, quiet link, automatic Fast is selected on both nodes. Verify a short text in each direction and do the 8×1,024 B encrypted benchmark. Don't infer speed from reported raw PHY bitrate.
- [ ] Introduce controlled attenuation/obstruction while recording **both** nodes' `AUTO|...` diagnostics. Verify Fast→Balanced then Balanced→Range before the link loses packets. Do not use radiated power above allowed limits or disconnect antennas under TX.
- [ ] Restore conditions and check hysteresis/cooldown: Range→Balanced→Fast without oscillation. A 45-second dwell can be intentional; urgent downgrade may be faster.
- [ ] During a long encrypted transfer, verify no Auto mode switch interrupts 224-byte ARQ data, its bitmap ACKs or encrypted delivered/read receipts.
- [ ] Manually select any profile while Auto is active, verify Auto is OFF on both nodes and manual one-click synchronization still works.
- [ ] Power cycle one radio during negotiation and capture rollback/errors. Power cycle **both** to recover to manual Balanced if profiles diverge.
- [ ] Compare application goodput B/s, confirmed/unconfirmed, p50/p95, ARQ_RETX/ARQ_TIMEOUT, RSSI and SNR. Test stable distance and dense/interfering environments; repeat at multiple distances. The thresholds will be revised based on measured packet error rates and both nodes' link margins, never advertised as a fixed physical range.
- [ ] Check Tx voltage/current when Balanced Auto and Fast Auto use nominal +18 dBm. The manually verified operator consent is **not** electrical certification.

**Primary sources:** Semtech, *SX1276/77/78/79 datasheet* (Rev. 7, May 2020), sensitivity tables and §5.5.5 RSSI/SNR; The Things Industries, *Adaptive Data Rate*, https://www.thethingsindustries.com/docs/concepts/features/lorawan/adr/ ; Semtech/LoRa modulation description, https://www.thethingsnetwork.org/docs/lorawan/modulation-data-rate/ .
