/*
 * Manually initiated LoRa power/range profiles. No automatic PHY switching.
 * The normal PA_BOOST path is limited to +17 dBm; never enable PA_DAC +20
 * dBm without a verified RF board and supply. Power does not equal bitrate.
 */
#ifndef OFFGRID_LORA_MODES_H
#define OFFGRID_LORA_MODES_H
#include <stdint.h>
#include <string.h>
#include "offgrid_sx1278_profile.h"
#define OG_MODE_LOW 1u
#define OG_MODE_BALANCED 2u
#define OG_MODE_MAX 3u
static inline const char *og_mode_name(uint8_t mode) {
    switch(mode) {case OG_MODE_LOW:return "LOW";
                  case OG_MODE_BALANCED:return "BALANCED";
                  case OG_MODE_MAX:return "MAX";
                  default:return "INVALID";}
}
static inline uint8_t og_mode_parse(const char *name) {
    if(!name)return 0;
    if(!strcmp(name,"LOW"))return OG_MODE_LOW;
    if(!strcmp(name,"BALANCED"))return OG_MODE_BALANCED;
    if(!strcmp(name,"MAX"))return OG_MODE_MAX;
    return 0;
}
/* Time starts after TxDone. ACK air time increases with SF and decreases
 * with bandwidth; allow turnaround and cooperative task scheduling margin. */
static inline uint32_t og_mode_ack_wait_ms(uint8_t mode, unsigned attempt) {
    if(attempt>4u)attempt=4u;
    const uint32_t base=mode==OG_MODE_LOW?230u:
                        mode==OG_MODE_BALANCED?360u:640u;
    return base+(uint32_t)attempt*115u;
}
static inline offgrid_sx1278_profile_t og_mode_profile(uint8_t mode) {
    offgrid_sx1278_profile_t p={
        .frequency_hz=433000000u,.bandwidth=OFFGRID_SX1278_BW_250_KHZ,
        .spreading_factor=7,.coding_denominator=5,.tx_power_dbm=10,
        .sync_word=0x2d,.preamble_symbols=8
    };
    if(mode==OG_MODE_LOW){
        p.bandwidth=OFFGRID_SX1278_BW_500_KHZ;
        p.tx_power_dbm=18;
        p.high_pa_dac=true;
    }else if(mode==OG_MODE_MAX){
        p.bandwidth=OFFGRID_SX1278_BW_125_KHZ;
        p.spreading_factor=9;
        p.tx_power_dbm=18; /* Ra-02 module-rated nominal; opt-in PA_DAC. */
        p.high_pa_dac=true;
    }
    return p;
}
#endif
