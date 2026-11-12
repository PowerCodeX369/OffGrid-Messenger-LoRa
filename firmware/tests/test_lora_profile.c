#include "offgrid_sx1278_profile.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    offgrid_sx1278_profile_t p={433000000,OFFGRID_SX1278_BW_125_KHZ,7,5,10,0x2d,8};
    offgrid_sx1278_register_profile_t r={0};
    assert(offgrid_sx1278_profile_valid(&p));
    assert(offgrid_sx1278_make_registers(&p,&r));
    assert(r.modem_config1==0x72 && r.modem_config2==0x74);
    assert(r.pa_config==0x88);
    p.tx_power_dbm=20;assert(!offgrid_sx1278_profile_valid(&p));
    p.tx_power_dbm=10;p.frequency_hz=600000000;
    assert(!offgrid_sx1278_profile_valid(&p));
    puts("PASS: SX1278 safe fixed profile and invalid-parameter rejects");
    return 0;
}
