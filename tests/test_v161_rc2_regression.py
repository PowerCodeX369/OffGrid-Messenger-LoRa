#!/usr/bin/env python3
"""Prove the working v1.6.1 data transport has not been rewritten."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
def old(path):
    return subprocess.check_output(['git','show','v1.6.1-rc.1:'+path],cwd=root)
def new(path):return (root/path).read_bytes()
for name in [
    'firmware/components/offgrid_arq/og_arq_bridge.c',
    'firmware/components/offgrid_arq/og_sr.c',
    'firmware/components/offgrid_arq/og_sr_usb.c',
    'firmware/components/offgrid_arq/include/og_arq_bridge.h',
    'firmware/components/offgrid_arq/include/og_sr.h',
    'firmware/components/offgrid_arq/include/og_sr_usb.h',
    'firmware/components/offgrid_sx1278/offgrid_sx1278.c',
    'firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c',
    'firmware/main/ble_bridge.c',
    'web/lora_bulk.js',
    'web/lora_receipts.js',
    'web/lora_scheduler.js',
    'web/lora_benchmark.js',
    'web/lora_delivery.js'
]:
    assert new(name)==old(name),'v1.6.1 known-working data path changed: '+name
# The new media envelope type and stricter voice budget must be explicit;
# no existing OGM4 encrypt/decrypt, text, or receipt behavior is rewritten.
protocol=new('web/protocol.js').decode()
old_protocol=old('web/protocol.js').decode()
assert protocol.replace('receiptBatch:11,mediaPart:12','receiptBatch:11').replace(
    'const VOICE_MAX=20*1024,VOICE_MAX_MS=120000;',
    'const VOICE_MAX=384*1024,VOICE_MAX_MS=120000;')==old_protocol
src=new('firmware/main/main.c').decode()
baseline=old('firmware/main/main.c').decode()
section=lambda x,start,end:x[x.index(start):x.index(end,x.index(start))]
for a,b in [('static void queue_data(', 'static void process_command('),
            ('static bool radio_hmac(', 'static void derive_radio_key(')]:
    assert section(src,a,b)==section(baseline,a,b),f'firmware {a} path modified'
web=new('web/app.js').decode()
assert 'PROFILE=AUTO3_ONE_CLICK' in src
assert 'PROFILES=3|SYNC=ONE_CLICK' in src
assert 'OG_MODE_EXPERIMENTAL' not in new('firmware/main/lora_modes.h').decode()
assert 'EXPERIMENTAL' not in web
assert "writeLine('POWER18 VERIFIED')" in web and 'writeLine(`PROFILE ${mode}`)' in web
assert 'PROFILE|PEER_REQUEST' in src and 'PROFILE|PEER_BUSY' in src
assert 'OG_TYPE_PROFILE_REJECT' in src and 'OG_TYPE_AUTO_CTRL' in src and 'auto_service();' in src
assert 'radio_hmac(bytes,plain,digest)' in src
print('PASS: v1.6.1 ARQ/radio driver/USB stages/crypto/receipts byte-identical; auto scheduler isolated')
