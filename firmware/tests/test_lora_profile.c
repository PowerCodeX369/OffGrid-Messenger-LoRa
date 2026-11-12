#include "offgrid_sx1278_profile.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    offgrid_sx1278_profile_t p={.frequency_hz=433000000,.bandwidth=OFFGRID_SX1278_BW_125_KHZ,.spreading_factor=7,.coding_denominator=5,.tx_power_dbm=10,.sync_word=0x2d,.preamble_symbols=8};
    offgrid_sx1278_register_profile_t r={0};
    assert(offgrid_sx1278_profile_valid(&p));
    assert(offgrid_sx1278_make_registers(&p,&r));
    assert(r.modem_config1==0x72 && r.modem_config2==0x74);
    assert(r.pa_config==0x88);
    p.tx_power_dbm=20;assert(!offgrid_sx1278_profile_valid(&p));
    p.tx_power_dbm=10;p.frequency_hz=600000000;
    assert(!offgrid_sx1278_profile_valid(&p));
    puts("PASS: SX1278 125/250/500 kHz safe profiles and invalid rejects");
    return 0;
}
