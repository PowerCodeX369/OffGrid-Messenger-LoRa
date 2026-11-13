"""Source-level architecture guards (not a substitute for two-board RF tests)."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
fw=(root/'firmware/main/main.c').read_text()
web=(root/'web/app.js').read_text()
wire=(root/'firmware/main/lora_wire.h').read_text()
policy=(root/'firmware/main/lora_auto.h').read_text()
ctrl=(root/'firmware/main/lora_profile_control.h').read_text()
for kind in ('OG_TYPE_AUTO_CTRL','OG_TYPE_AUTO_ACK','OG_TYPE_AUTO_METRIC'):
 assert kind in wire and kind in fw
assert 'radio_hmac(bytes,plain,digest)' in fw
assert 'auto_seen_control' in fw and 'auto_last_accepted_id' in fw
assert 'auto_ctrl_nonce' in fw and 'auto_ctrl_retries>=6' in fw
assert 'og_profile_is_auto(&f)' in fw and 'OG_PROFILE_AUTO_FLAG' in ctrl
assert 'OG_PROFILE_VERIFY_MS' in fw and 'profile_report("ROLLED_BACK")' in fw
assert 'auto_peer_enabled' in fw and 'memcmp(own_mac,peer_mac,6)>=0' in fw
assert 'og_auto_decide' in fw and 'og_auto_quality_note' in fw
assert 'profile_has_pending()' in fw and 'OG_AUTO_COOLDOWN_MS' in policy
assert 'OG_AUTO_FAST_DOWN_RSSI' in policy and 'OG_AUTO_FAST_UP_RSSI' in policy
assert 'OG_AUTO_BAL_DOWN_SNR_Q4' in policy
assert 'p.high_pa_dac=true' in fw and 'og_mode_profile' in fw
assert 'AUTO ON' in fw and 'AUTO OFF' in fw
assert "requestAutoMode(enable)" in web and 'Automatic + manual LoRa modes' in web
assert 'autoMode' in web and "s.startsWith('AUTO|')" in web
assert '22 dBm' in web and 'OG_MODE_EXPERIMENTAL' not in fw
assert 'PROFILE,POWER18 VERIFIED' not in fw
print('PASS: authenticated opt-in ADR, leader selection, control retries, two-way metrics, manual override')
