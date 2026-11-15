"""Contract guards on REAL firmware/browser/native Android integration.
Source-level assertions complement C ARQ/auto tests and WebCrypto/browser tests;
not a hardware/Android-SDK build.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
read=lambda n:(p/n).read_text()
fw=read('firmware/main/main.c');wire=read('firmware/main/lora_wire.h')
bridge=read('firmware/components/offgrid_arq/og_arq_bridge.c')
sr=read('firmware/components/offgrid_arq/og_sr.c');auto=read('firmware/main/lora_auto.h')
mode=read('firmware/main/lora_modes.h');profile=read('firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c')
app=read('android/app/src/main/java/com/offgrid/messenger/MainActivity.kt')
radio=read('android/app/src/main/java/com/offgrid/messenger/RadioLink.kt')
web=read('web/app.js');bulk=read('web/lora_bulk.js')
assert 'OG_MODE_ULTRA 4u' in mode and 'p.spreading_factor=12' in mode and 'p.high_pa_dac=true' in mode
assert 'p.spreading_factor=9' in mode and 'p.bandwidth=OFFGRID_SX1278_BW_125_KHZ' in mode
assert 'modem_config3' in profile and '0x08' in profile  # LDRO present in driver
assert 'OG_SR_FLAG_ULTRA' in read('firmware/components/offgrid_arq/include/og_sr.h')
assert 'og_sr_payload_from_flags' in sr and 'og_sr_tx_init_ex' in sr and 'og_arq_bridge_set_ultra' in bridge
assert 'OG_AUTO_METRIC_FRESH_MS 90000u' in auto and 'current==OG_MODE_ULTRA' in auto
assert 'peer_ultra_capable' in fw and 'ARQ_CAPS' in fw and 'ULTRA_REQUIRES_BOTH' in fw
assert 'PROFILE LOW/BALANCED/MAX/ULTRA' in fw
assert 'MEDIA|ERROR|ULTRA_TEXT_ONLY' in fw and 'c->bulk_media' in fw
assert 'ULTRA_TEXT_TOO_LONG' in fw and 'og_lora_arq_ack_rto_ms' in fw and 'og_ultra_airtime_ms' in fw
assert 'og_ultra_ack_wait_ms' in read('firmware/main/lora_arq_timing.h')
assert 'og_arq_bridge_set_ultra(&arq,false' in fw
assert 'SF12' in radio and 'PROFILE ULTRA' in radio
assert 'Ultra SF12 is text only. Photos are disabled' in app
assert 'Ultra SF12 is text only. Voice notes are disabled' in app
assert 'job.media && radioLink?.activeProfile=="ULTRA"' in app
assert 'Ultra Long Range' in read('android/app/src/main/java/com/offgrid/messenger/PremiumUi.kt')
assert 'Ultra Long Range' in web and 'currentProfile===\'ULTRA\'' in web
assert 'a.media?\'MEDIA\'' in bulk and 'Math.min(1_800_000,180_000+Math.ceil(ciphertext.length/48)*40_000)' in bulk
assert "currentProfile==='ULTRA'" in web and 'Ultra SF12 is text only. Voice notes are disabled' in web
assert 'ULTRA=SF12_TEXT_ONLY' in fw and 'PROFILES=4' in fw
assert 'versionCode=306' in read('android/app/build.gradle.kts')
assert 'ULTRA=SF12_TEXT_ONLY' in read('firmware/main/ble_bridge.c')
print('PASS: native/web/firmware SF12 mode + authenticated capability gates + text-only/media rejection + timing contracts')
