# Hardware setup, wiring and regulatory checks

**Reference design:** two ESP32-C3 Super Mini microcontrollers with two SX1278/Ra-02-style 433 MHz transceivers, suitable antennas and 3.3 V supplies. These are two **direct** peer radios, not a multi-hop mesh network.

## Parts and safety

- 2 × compatible ESP32-C3 Super Mini devices, 2 × SX1278/Ra-02 433 MHz modules, 2 × antennas matched to the operating band, 2 × USB data cables.
- Stable **regulated 3.3 V** for each SX1278 and **common ground** with its ESP32. Never feed 5 V to SX1278 VCC. Ensure the regulator can supply the radio during high-power bursts; don't join two independent regulated 3.3 V outputs together.
- Fit antennas before transmitting. Use short clean RF wiring and suitable antenna connectors. Verify module pinout against **your actual board revision**; clones can differ.
- Avoid powering questionable wiring from an unprotected laptop USB port. Check for shorts, polarity, voltage sag and heat before enabling nominal +18 dBm modes.

## Firmware's configured connection map

Confirmed in `firmware/components/offgrid_sx1278/offgrid_sx1278.c`:

| Radio pin | ESP32-C3 signal |
| --- | --- |
| VCC | Regulated 3.3 V |
| GND | Common ground |
| NSS / CS | GPIO10 |
| SCK | GPIO6 |
| MOSI | GPIO7 |
| MISO | GPIO5 |
| RESET | GPIO3 |
| DIO0 | GPIO4 |
| DIO1 | GPIO0 |
| DIO2–DIO5 | Not connected by this driver |

The SX1278 connection and ESP32 native USB wiring are independent. Some development boards expose special boot or USB functions on particular pins; **confirm the actual board schematic** before assembly.

## RF configuration in current source

| Manual profile | SF | Bandwidth | Nominal TX setting | Official client media support |
| --- | --- | --- | --- | --- |
| Fast / LOW | SF7 | 500 kHz | +18 dBm (operator approval) | Yes |
| Balanced | SF7 | 250 kHz | +10 dBm | Yes |
| Max | SF9 | 125 kHz | +18 dBm (operator approval) | Yes |
| Ultra | SF12 | 125 kHz, CR4/8 | +18 dBm (operator approval) | **Text/receipts only** |

`firmware/main/lora_modes.h` currently sets center **433000000 Hz** across profiles. Before using **any transmitter**, check the current rules for the actual jurisdiction: spectrum allocation, center frequency and occupied bandwidth, antenna gain, output EIRP, duty-cycle/channel access restrictions, device certification and any licensing requirements. **This repository does not establish legal permission for 433.000 MHz or +18 dBm in your region**; application safety prompts are not a radio compliance mechanism.

## First boot and two-device preparation

1. Photograph/document wiring and record board versions and the two USB ports.
2. Compile current firmware from [`firmware/README.md`](../firmware/README.md) and flash both devices. **Do not erase NVS** if the existing key must be retained. No prebuilt firmware image is supplied in source archives.
3. Check each device's USB diagnostics for `RADIO|SX1278_READY` and `READY|...FAMILY=LORA`, current capability markers and a healthy key state; do not paste secrets into public logs.
4. Connect one Android or desktop client to each board and provision/unlock the **same** pairing key.
5. With boards close and antennas installed, verify short text and end-to-end receipts *both directions* on Balanced.
6. Only then enable Auto after its explicit power/supply/legal confirmation. Check **both** boards report the same applied mode.
7. Record radio test results and limitations using [`TESTING.md`](TESTING.md).

## Upgrade precautions

Older Max/Ultra ACK formats are incompatible with current authenticated compact ACK capability (`ARQ_CAP=4`). Update both boards together and keep an earlier working image plus Git tag for rollback. An Android upgrade requires the same package and signing identity to preserve app data. More detail: [`VERSIONS.md`](VERSIONS.md).
