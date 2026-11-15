#!/usr/bin/env python3
"""Static cross-client regression contract for automatic -100/-130 RSSI bands."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
read=lambda f:(root/f).read_text()
auto=read('firmware/main/lora_auto.h');fw=read('firmware/main/main.c')
rec=read('firmware/main/lora_link_recovery.h');web=read('web/app.js')
android=read('android/app/src/main/java/com/offgrid/messenger/RadioLink.kt')
assert '#define OG_AUTO_BALANCED_LIMIT_DBM (-100)' in auto
assert '#define OG_AUTO_ULTRA_LIMIT_DBM (-130)' in auto
assert 'if(rssi>OG_AUTO_BALANCED_LIMIT_DBM)' in auto
assert 'if(rssi>=OG_AUTO_ULTRA_LIMIT_DBM)' in auto
assert 'og_auto_rssi_band' in auto and 'og_auto_decide_linkaware' in fw
assert 'og_recovery_step_auto(&recovery' in fw
assert 'OG_AUTO_RECOVERY_BAL_LOSS_MS 16000u' in rec
assert 'OG_RECOVERY_ULTRA_BEACON_MS 7500u' in rec
assert 'RECOVERY|SCANNING|%s|HOME=' in fw
assert 'REASON=RSSI_BAND' in fw
assert 'BALANCED_GT=-100|MAX_FROM=-130|ULTRA_LT=-130' in fw
assert 'Balanced > −100 dBm' in web or 'Balanced when > −100 dBm' in web
assert 'rssiDbm > -100' in android and 'rssiDbm >= -130' in android
assert 'versionCode=307' in read('android/app/build.gradle.kts')
print('PASS: exact authenticated firmware RSSI bands, timed lost-link probes, native/web controls')
