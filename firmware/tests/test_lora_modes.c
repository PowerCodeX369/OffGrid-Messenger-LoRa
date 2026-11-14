#include "lora_modes.h"
#include "lora_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(!og_mode_parse("AUTO"));
    assert(!offgrid_sx1278_profile_valid(&(offgrid_sx1278_profile_t){
        .frequency_hz=433000000,.bandwidth=OFFGRID_SX1278_BW_500_KHZ,
        .spreading_factor=7,.coding_denominator=5,.tx_power_dbm=22,
        .sync_word=0x2d,.preamble_symbols=8,.high_pa_dac=true}));
    assert(og_mode_parse("EXPERIMENTAL")==0);
    assert(og_mode_parse("22")==0);
    assert(og_mode_parse("LOW")==OG_MODE_LOW);
    assert(og_mode_parse("BALANCED")==OG_MODE_BALANCED);
    assert(og_mode_parse("MAX")==OG_MODE_MAX);
    assert(og_mode_parse("ULTRA")==OG_MODE_ULTRA);
    for(unsigned i=OG_MODE_LOW;i<=OG_MODE_ULTRA;i++){
        const offgrid_sx1278_profile_t p=og_mode_profile((uint8_t)i);
        offgrid_sx1278_register_profile_t r={0};
        assert(offgrid_sx1278_make_registers(&p,&r));
        assert(p.frequency_hz==433000000u && p.sync_word==0x2d);
        assert(p.coding_denominator==5 && p.preamble_symbols==8);
        assert(og_mode_ack_wait_ms((uint8_t)i,2)>og_mode_ack_wait_ms((uint8_t)i,0));
    }
    offgrid_sx1278_profile_t low=og_mode_profile(OG_MODE_LOW);
    offgrid_sx1278_profile_t balanced=og_mode_profile(OG_MODE_BALANCED);
    offgrid_sx1278_profile_t max=og_mode_profile(OG_MODE_MAX);
    offgrid_sx1278_register_profile_t reg={0};
    assert(low.bandwidth==OFFGRID_SX1278_BW_500_KHZ && low.tx_power_dbm==18 && low.spreading_factor==7);
    assert(offgrid_sx1278_make_registers(&low,&reg));
    assert(reg.modem_config1==0x92 && reg.modem_config2==0x74 && reg.pa_config==0x8d && reg.pa_dac==0x87 && reg.ocp==0x32);
    assert(balanced.bandwidth==OFFGRID_SX1278_BW_250_KHZ && balanced.tx_power_dbm==10);
    assert(offgrid_sx1278_make_registers(&balanced,&reg));
    assert(reg.modem_config1==0x82 && reg.pa_config==0x88);
    assert(max.bandwidth==OFFGRID_SX1278_BW_125_KHZ && max.spreading_factor==9 && max.tx_power_dbm==18);
    assert(offgrid_sx1278_make_registers(&max,&reg));
    assert(reg.modem_config1==0x72 && reg.modem_config2==0x94 && reg.pa_config==0x8d && reg.pa_dac==0x87);
    assert(og_mode_ack_wait_ms(OG_MODE_LOW,1)<og_mode_ack_wait_ms(OG_MODE_MAX,1));
    offgrid_sx1278_profile_t ultra=og_mode_profile(OG_MODE_ULTRA);
    assert(ultra.spreading_factor==12 && ultra.bandwidth==OFFGRID_SX1278_BW_125_KHZ && ultra.high_pa_dac);
    assert(offgrid_sx1278_make_registers(&ultra,&reg));
    assert(reg.modem_config1==0x72 && reg.modem_config2==0xC4 && reg.modem_config3==0x0C);
    assert(max.spreading_factor==9 && max.bandwidth==OFFGRID_SX1278_BW_125_KHZ);
    assert(max.tx_power_dbm<=18);
    assert(!offgrid_sx1278_profile_valid(&(offgrid_sx1278_profile_t){
        .frequency_hz=433000000, .bandwidth=OFFGRID_SX1278_BW_500_KHZ,
        .spreading_factor=7,.coding_denominator=5,.tx_power_dbm=22,
        .sync_word=0x2d,.preamble_symbols=8,.high_pa_dac=true})); /* No experimental +20/+22 dBm path */
    max.tx_power_dbm=20; assert(!offgrid_sx1278_profile_valid(&max));
    og_frame_t offer={.magic=OG_MAGIC,.version=OG_VERSION,.type=OG_TYPE_PROFILE_OFFER,.length=6};
    offer.data[0]=OG_MODE_MAX;
    assert(og_wire_valid(&offer,og_wire_length(&offer)));
    offer.type=OG_TYPE_PROFILE_AGREE;
    assert(og_wire_valid(&offer,og_wire_length(&offer)));
    offer.data[0]=0;assert(!og_wire_valid(&offer,og_wire_length(&offer)));
    offer.data[0]=OG_MODE_MAX;offer.length=0;
    assert(!og_wire_valid(&offer,og_wire_length(&offer)));
    puts("PASS: manual profiles, nominal 18 dBm PA/DAC, rejected 22 dBm");
    return 0;
}
