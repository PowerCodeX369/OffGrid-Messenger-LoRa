#!/usr/bin/env python3
"""Assert rc.4 changes only synchronized media mode control and diagnostics.
Checks actual Git objects, not copies produced from an earlier release.
"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
for file in ['firmware/components/offgrid_arq/og_sr.c','firmware/components/offgrid_arq/og_arq_bridge.c',
             'firmware/components/offgrid_sx1278/offgrid_sx1278.c',
             'web/protocol.js','web/lora_receipts.js',
             'web/lora_scheduler.js','web/styles.css','web/lora_delivery.js']:
    before=subprocess.check_output(['git','show','v1.8.0-rc.3:'+file],cwd=root)
    after=(root/file).read_bytes()
    assert after==before, f'working rc.3 component changed: {file}'
# The only intended change to the media wire codec is the chunk capacity.
bulk_before=subprocess.check_output(['git','show','v1.8.0-rc.4:web/lora_bulk.js'],cwd=root).decode()
bulk_after=(root/'web/lora_bulk.js').read_text().replace('PIPELINE=8','PIPELINE=4')
import re
strip_batch_comment=lambda x:re.sub(r'/\* Bounded .*?\*/','',x,flags=re.DOTALL)
assert strip_batch_comment(bulk_after)==strip_batch_comment(bulk_before),'USB codec changed beyond batch count'
media_before=subprocess.check_output(['git','show','v1.8.0-rc.4:web/lora_media.js'],cwd=root)
media_after=(root/'web/lora_media.js').read_bytes()
assert media_after.replace(b'SEGMENT=7040',b'SEGMENT=5800')==media_before, 'media codec changed beyond segment size'
fw=(root/'firmware/main/main.c').read_text()
assert 'media_restore_requested' in fw and 'MEDIA|RESTORED|' in fw
assert 'profile_start_offer(OG_MODE_LOW,false,false,true)' in fw
assert 'OG_PROFILE_MEDIA_FLAG' in (root/'firmware/main/lora_profile_control.h').read_text()
assert 'media_service();' in fw and 'media_boost_active' in fw
assert 'og_arq_bridge_cancel(&arq,&arq_io,"PROFILE_CHANGED_UNCONFIRMED")' in fw
assert 'FW=1.8.0-rc.4' in fw and 'MEDIAFAST=1' in fw
web=(root/'web/app.js').read_text()
assert "mediaModeCommand('MEDIA ON'" in web and "mediaModeCommand('MEDIA OFF'" in web
assert "FW=1.8.0-rc.4" in web and 'MEDIAFAST=1' in web
for line in (root/'docs/ANDROID_FROZEN_SHA256.txt').read_text().splitlines():
    if not line or line.startswith('#'):continue
    import hashlib
    sha,rel=line.split('  ',1)
    assert hashlib.sha256((root/'android'/rel).read_bytes()).hexdigest()==sha
print('PASS: radio, ARQ, crypto, UI, receipts and Android untouched; media changed only chunk size')
