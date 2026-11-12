#ifndef OFFGRID_SX1278_H
#define OFFGRID_SX1278_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "offgrid_sx1278_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Narrow hardware driver interface: no chat protocol, no keys, no RF handover.
 * One task owns the radio. Callers must not access a handle concurrently.
 * The driver initializes and owns ESP32-C3 SPI2; do not initialize SPI2 twice.
 */
#define OFFGRID_SX1278_MAX_PAYLOAD 220U /* 202-byte OGM2 frame plus 16-byte HMAC */

typedef struct offgrid_sx1278 offgrid_sx1278_t;

typedef struct {
    gpio_num_t nss;
    gpio_num_t sck;
    gpio_num_t mosi;
    gpio_num_t miso;
    gpio_num_t reset;
    gpio_num_t dio0;
    gpio_num_t dio1;
    uint32_t spi_clock_hz;  /* 4 MHz default, up to 10 MHz chip limit. */
} offgrid_sx1278_pins_t;

typedef struct {
    int16_t rssi_dbm;
    float snr_db;
    bool dio0_asserted;   /* DIO0 observed on radio event, not a liveness proof. */
} offgrid_sx1278_packet_info_t;

typedef struct {
    uint32_t tx_ok;
    uint32_t tx_timeouts;
    uint32_t rx_ok;
    uint32_t rx_crc_errors;
    uint32_t rx_timeouts;
    uint32_t rx_oversize;
} offgrid_sx1278_counters_t;

offgrid_sx1278_pins_t offgrid_sx1278_default_pins(void);
esp_err_t offgrid_sx1278_open(const offgrid_sx1278_pins_t *pins,
                              const offgrid_sx1278_profile_t *profile,
                              offgrid_sx1278_t **out);
esp_err_t offgrid_sx1278_set_profile(offgrid_sx1278_t *radio,
                                     const offgrid_sx1278_profile_t *profile);
/* Receiver-only channel activity detection (not a packet send). */
esp_err_t offgrid_sx1278_cad(offgrid_sx1278_t *radio,
                              bool *activity_detected, uint32_t timeout_ms);
esp_err_t offgrid_sx1278_send(offgrid_sx1278_t *radio, const uint8_t *data,
                               size_t size, uint32_t timeout_ms);
esp_err_t offgrid_sx1278_receive(offgrid_sx1278_t *radio, uint8_t *buffer,
                                  size_t capacity, size_t *out_len,
                                  offgrid_sx1278_packet_info_t *info,
                                  uint32_t timeout_ms);
/* Physically disable the SX1278 RF modem while the radio is inactive.
 * It remains powered at 3.3 V but no LoRa RX/TX runs. */
esp_err_t offgrid_sx1278_sleep(offgrid_sx1278_t *radio);
esp_err_t offgrid_sx1278_standby(offgrid_sx1278_t *radio);
void offgrid_sx1278_counters(const offgrid_sx1278_t *radio,
                             offgrid_sx1278_counters_t *out);
void offgrid_sx1278_close(offgrid_sx1278_t *radio);

#ifdef __cplusplus
}
#endif
#endif
