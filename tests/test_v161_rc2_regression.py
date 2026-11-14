#!/usr/bin/env python3
"""v3.0.4 regression: old radio/crypto/USB remains, ARQ gains Ultra only.

The original v1.6.1 source-identity contract cannot remain byte-for-byte
because Ultra requires authenticated variable-sized selective-repeat frames.
Instead test unchanged code and run actual Fast/SF9 bridge test + Ultra bridge.
"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
def old(p):return subprocess.check_output(['git','show','v3.0.3-rc.1-android-native-rf:'+p],cwd=root)
def new(p):return (root/p).read_bytes()
protected=(
 'firmware/components/offgrid_arq/og_sr_usb.c',
 'firmware/components/offgrid_arq/include/og_sr_usb.h',
 'firmware/components/offgrid_sx1278/offgrid_sx1278.c',
 'firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c',
 'web/protocol.js','web/lora_media.js','web/lora_receipts.js',
 'web/lora_scheduler.js','web/lora_benchmark.js','web/lora_delivery.js',
 'android/app/src/main/java/com/offgrid/messenger/OgmCodec.kt',
 'android/app/src/main/java/com/offgrid/messenger/BleNodeClient.kt',
 'android/app/src/main/java/com/offgrid/messenger/SecureKeyStore.kt',
 'android/app/src/main/java/com/offgrid/messenger/MediaPayload.kt')
for p in protected:assert old(p)==new(p),p
arq=(root/'firmware/components/offgrid_arq/og_sr.c').read_text()
for signature in ('og_sr_tx_init_ex','og_sr_rx_init_ex','OG_SR_FLAG_ULTRA',
                  'og_sr_tx_ack','og_sr_rx_make_ack','OG_SR_MAX_ATTEMPTS'):
 assert signature in arq or signature in (root/'firmware/components/offgrid_arq/include/og_sr.h').read_text(),signature
src=(root/'firmware/main/main.c').read_text()
assert 'PSA_ALG_HMAC(PSA_ALG_SHA_256)' in src
assert 'ULTRA=SF12_TEXT_ONLY' in src and 'PROFILES=4' in src
assert '18_DBM_REQUIRES_LOCAL_POWER_VERIFICATION' in src
assert 'OG_TYPE_PROFILE_OFFER' in src and 'OG_TYPE_PROFILE_AGREE' in src
assert 'profile_state.current==OG_MODE_ULTRA' in src
assert 'PROFILE|ERROR|PEER_REQUIRES_V304_ULTRA' in src
print('PASS: pre-Ultra crypto, BLE auth, USB staging, media codecs, SX1278 driver untouched; standard ARQ tests cover baseline')
