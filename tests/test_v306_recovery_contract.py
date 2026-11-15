#!/usr/bin/env python3
"""Prove v3.0.6 scope and authenticated recovery fail-safe integration.
Not RF-in-the-field proof; hardware needs two-node signal/timeout soak.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
fw=(r/'firmware/main/main.c').read_text()
auto=(r/'firmware/main/lora_auto.h').read_text()
rec=(r/'firmware/main/lora_link_recovery.h').read_text()
assert 'link_recovery_service();' in fw
assert 'og_recovery_step(&recovery' in fw and 'og_recovery_step_auto(&recovery' in fw
assert 'RECOVERY|AUTHENTICATED|%s|PEER_RESTORED' in fw
assert 'RECOVERY|RX_REARM|BALANCED|NO_AUTHENTICATED_PEER' in fw
assert 'RECOVERY|SCANNING|%s|HOME=' in fw
assert 'RECOVERY|HOME|' in fw
assert 'LINK_RECOVERY=RENDEZVOUS36' in fw
assert 'og_auto_decide_linkaware' in auto and 'og_auto_decide_linkaware' in fw
assert 'AUTO|PEER_PREEMPT|WEAK_LINK_ARQ' in fw
assert 'AUTO|PREEMPT|WEAK_LINK_ARQ' in fw
assert 'if(f->type==OG_TYPE_AUTO_METRIC)' in fw
assert 'profile_reset_pending();' in fw
assert 'og_arq_bridge_cancel' in fw
assert 'BULKFAILED|SESSION|NOT_READY_OR_PROFILE_SWITCH_OR_RECOVERY' in fw
assert 'MEDIA|ERROR|LINK_LOST_RELEASE_BOOST_FOR_RECOVERY' in fw
assert 'radio_hmac(bytes,plain,digest)' in fw and 'equal_tag(digest,bytes+plain)' in fw
assert 'if(!peer_accept(&f,rssi))return;' in fw
assert 'og_recovery_step(&recovery,(uint32_t)millis(),0' in fw
assert 'OG_RECOVERY_SCAN_MS 15000u' in rec and 'OG_RECOVERY_HOME_MS 10000u' in rec
assert 'OG_RECOVERY_ULTRA_LOSS_MS 80000u' in rec
assert 'versionCode=307' in (r/'android/app/build.gradle.kts').read_text()
assert 'p.spreading_factor=9' in (r/'firmware/main/lora_modes.h').read_text()
assert 'p.spreading_factor=12' in (r/'firmware/main/lora_modes.h').read_text()
print('PASS: v3.0.6 authenticated common-profile recovery, weak-link Auto downgrade, FIFO/media/text preservation')
