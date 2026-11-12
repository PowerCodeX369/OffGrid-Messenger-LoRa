#ifndef OFFGRID_SX1278_PROFILE_H
#define OFFGRID_SX1278_PROFILE_H

#include <stdbool.h>
#include <stdint.h>

/*
 * This file contains the pure, hardware-independent SX1278 radio policy.
 * It can be compiled and unit-tested without ESP-IDF or connected hardware.
 * All frequencies are in Hz; power is dBm at the programmed transceiver PA.
 */

typedef enum {
    OFFGRID_SX1278_BW_125_KHZ = 125000,
    OFFGRID_SX1278_BW_250_KHZ = 250000,
    OFFGRID_SX1278_BW_500_KHZ = 500000,
} offgrid_sx1278_bandwidth_t;

typedef struct {
    uint32_t frequency_hz;
    offgrid_sx1278_bandwidth_t bandwidth;
    uint8_t spreading_factor;     /* SF7..SF12; SF6 needs a different header mode. */
    uint8_t coding_denominator;    /* 5..8 for rate 4/5..4/8. */
    int8_t tx_power_dbm;          /* 2..17 normal PA, or 18 with high_pa_dac enabled. */
    uint8_t sync_word;            /* Must match the peer. */
    uint16_t preamble_symbols;
    bool high_pa_dac;          /* Explicit module-rated 18 dBm mode; requires stable 3.3 V. */
} offgrid_sx1278_profile_t;

typedef struct {
    uint32_t frf;                 /* 24-bit SX1278 synthesizer register value. */
    uint8_t modem_config1;
    uint8_t modem_config2;
    uint8_t modem_config3;
    uint8_t pa_config;
    uint8_t pa_dac;
    uint8_t ocp;
} offgrid_sx1278_register_profile_t;

/* Reject unsupported/unsafe settings. Returns false without side effects. */
bool offgrid_sx1278_profile_valid(const offgrid_sx1278_profile_t *profile);

/* Converts a validated profile into SX1278 register values. */
bool offgrid_sx1278_make_registers(const offgrid_sx1278_profile_t *profile,
                                   offgrid_sx1278_register_profile_t *registers);

#endif
