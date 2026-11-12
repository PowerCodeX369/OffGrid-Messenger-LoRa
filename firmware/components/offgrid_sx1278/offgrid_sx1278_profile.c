#include "offgrid_sx1278_profile.h"

#include <stddef.h>

bool offgrid_sx1278_profile_valid(const offgrid_sx1278_profile_t *p)
{
    return p != NULL &&
           p->frequency_hz >= 410000000U && p->frequency_hz <= 525000000U &&
           (p->bandwidth == OFFGRID_SX1278_BW_125_KHZ ||
            p->bandwidth == OFFGRID_SX1278_BW_250_KHZ ||
            p->bandwidth == OFFGRID_SX1278_BW_500_KHZ) &&
           p->spreading_factor >= 7U && p->spreading_factor <= 12U &&
           p->coding_denominator >= 5U && p->coding_denominator <= 8U &&
           p->tx_power_dbm >= 2 && (
               (!p->high_pa_dac && p->tx_power_dbm<=17) ||
               (p->high_pa_dac && p->tx_power_dbm==18)) &&
           p->preamble_symbols >= 6U;
}

bool offgrid_sx1278_make_registers(const offgrid_sx1278_profile_t *p,
                                   offgrid_sx1278_register_profile_t *r)
{
    if (!offgrid_sx1278_profile_valid(p) || r == NULL) {
        return false;
    }

    /* SX1278 Fstep = 32 MHz / 2^19; use 64-bit arithmetic to avoid overflow. */
    r->frf = (uint32_t)((((uint64_t)p->frequency_hz << 19U) + 16000000ULL) /
                         32000000ULL);

    /* RegModemConfig1: bandwidth [7:4], coding rate [3:1], explicit header. */
    const uint8_t bw_code = (p->bandwidth == OFFGRID_SX1278_BW_500_KHZ) ? 0x90U :
                            (p->bandwidth == OFFGRID_SX1278_BW_250_KHZ) ? 0x80U : 0x70U;
    r->modem_config1 = (uint8_t)(bw_code | ((p->coding_denominator - 4U) << 1U));
    /* RegModemConfig2: spreading factor [7:4], payload CRC enabled [2]. */
    r->modem_config2 = (uint8_t)((p->spreading_factor << 4U) | 0x04U);
    /* AGC on; low data rate optimization when symbol duration exceeds 16 ms. */
    const uint32_t symbol_us =
        ((1UL << p->spreading_factor) * 1000000UL) / (uint32_t)p->bandwidth;
    r->modem_config3 = (uint8_t)(0x04U | (symbol_us > 16000UL ? 0x08U : 0U));
    /* PA_BOOST at ordinary levels: OutputPower = Pout - 2 dBm. */
    if (p->high_pa_dac) {
        /* Special SX1278 PA_DAC Pout=5+OutputPower. +18 -> field 13.
         * Only module-rated nominal 18 dBm; NEVER +22 dBm. */
        r->pa_config=0x8DU;
        r->pa_dac=0x87U;
        r->ocp=0x32U;
    } else {
        r->pa_config=(uint8_t)(0x80U|(uint8_t)(p->tx_power_dbm-2));
        r->pa_dac=0x84U;
        r->ocp=0x2BU;
    }
    return true;
}
