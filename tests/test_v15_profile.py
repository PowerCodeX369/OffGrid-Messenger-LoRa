#!/usr/bin/env python3
"""Structural regression checks; not a two-radio timing integration test."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
main=(root/'firmware/main/main.c').read_text()
web=(root/'web/app.js').read_text()
wire=(root/'firmware/main/lora_wire.h').read_text()
profile=(root/'firmware/main/lora_modes.h').read_text()
assert 'OG_TYPE_PROFILE_OFFER' in wire and 'OG_TYPE_PROFILE_AGREE' in wire
assert 'profile_nonce_valid(&f)' in main and 'radio_hmac' in main
assert 'profile_state.nonce==f.id' in main
assert 'profile_state.phase=OG_PROFILE_VERIFYING' in main
assert 'profile_state.phase=OG_PROFILE_IDLE' in main
assert 'profile_report("ROLLED_BACK")' in main
assert 'profile_report("ACTIVE")' in main
assert 'profile_assist_until_ms' in main
assert 'CMD_PROFILE' in main and 'PROFILE_SWITCH_OR_ARQ_BUSY' in main
assert 'og_mode_ack_wait_ms(profile_state.current,p->tries)' in main
assert 'og_mode_parse(name)' in main and 'c.force_profile' in main
assert 'profileTransition' in web and "s.startsWith('PROFILE|')" in web
assert 'requestProfileChange' in web and 'showProfileDialog' in web
assert 'rfScheduler.submit(' in web
assert 'OFFGRID_SX1278_BW_500_KHZ' in profile
assert 'tx_power_dbm=18' in profile and 'high_pa_dac=true' in profile
for forbidden in ('esp_now_init(', 'esp_wifi_init(', 'esp_wifi_start('):
    assert forbidden not in main
print('PASS: v1.5 manual authenticated profile-change boundaries and RX/TX isolation')
