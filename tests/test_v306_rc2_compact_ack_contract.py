#!/usr/bin/env python3
"""v3.0.6 rc.2 wiring guards; host C tests provide behavioral coverage.
These checks are NOT a replacement for an ESP-IDF target or physical RF test.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
f=lambda path:(r/path).read_text()
main=f('firmware/main/main.c');bridge=f('firmware/components/offgrid_arq/og_arq_bridge.c')
head=f('firmware/components/offgrid_arq/include/og_sr.h')
sr=f('firmware/components/offgrid_arq/og_sr.c')
timing=f('firmware/main/lora_arq_timing.h')
assert 'ACK_COMPACT=16|ARQ_CAP=4|MAX_TEXT_FRAG=128' in main
assert 'cap.data[0]=4' in main and 'peer_compact_ack_capable' in main
assert 'PEER_REQUIRES_V3062_COMPACT_ACK' in main
assert 'og_sr_compact_ack_encode' in bridge and 'og_sr_compact_ack_decode' in bridge
assert 'n=OG_SR_TAG' in bridge and 'n==OG_SR_TAG && arq.compact_ack_active' in main
assert 'OG_SR_TAG 16u' in head and 'OG_SR_HEADER 14u' in head
assert 'og_sr_compact_ack_decode' in sr and 'verify(crypto,prefix,sizeof(prefix),tag16)' in sr
assert 'count>3' in sr and 'const uint32_t bitmap=t->acknowledged|subset' in sr
assert 'og_lora_arq_silence_ms(' in main and 'og_lora_arq_ack_rto_ms(' in main
assert 'airtime+180u' in timing and 'og_ultra_ack_wait_ms(OG_SR_TAG' in timing
assert 'og_arq_bridge_set_range_text_payload(&arq,c->bulk_media?OG_SR_PAYLOAD:128u)' in main
assert 'if(arq.tx_used && (profile_state.current==OG_MODE_MAX ||' in main
assert 'OG_MODE_MAX?3u:6u' in main
assert 'OG_MODE_ULTRA' in f('firmware/main/lora_modes.h')
# No changes to the native Android/web UI, radio power, encryption or driver.
import subprocess
protected=['android','web','firmware/components/offgrid_sx1278',
           'firmware/main/ble_bridge.c','firmware/main/ble_auth_psa.h',
           'firmware/main/lora_modes.h']
changed=subprocess.check_output(['git','diff','--name-only','v3.0.6-rc.1-link-recovery',
                                 '--',*protected],cwd=r).decode().strip()
assert not changed, changed
print('PASS: v3.0.6-rc.2 compact authenticated ACK, per-profile size/timing, safe cap-4 peer gate, frozen Android/web/SX1278')
