# SX1278 Ra-02 SPI driver

This component is an independent SPI2/GPIO driver used by OffGrid Messenger LoRa v1.0. It knows **nothing** about AES-GCM or the web protocol and does not call any Wi-Fi/ESP-NOW APIs.

- SPI2 NSS GPIO10, SCK GPIO6, MOSI GPIO7, MISO GPIO5, RESET GPIO3, DIO0 GPIO4, DIO1 GPIO0.
- `RegVersion(0x42) == 0x12` required at startup.
- SF7, BW125, CR4/5, 433 MHz, sync word `0x2D`, TX power 10 dBm at the programmed SX1278 PA.
- Continuous RX persists across short polling intervals. Never reset RX on each 24 ms poll or packets with longer airtime will be lost.
- `offgrid_sx1278_send` and `receive` must be called from **one owner task**. PHY CRC is an integrity check, not authentication; the parent LoRa task authenticates all packets using HMAC-SHA256.

See Semtech SX1276/77/78/79 Rev 7 datasheet for register descriptions: <https://cdn.sparkfun.com/assets/7/7/3/2/2/SX1276_Datasheet.pdf>.
