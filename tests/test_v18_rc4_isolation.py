#!/usr/bin/env python3
"""v3.0.4 verifies rc.4 media and privacy features survive Ultra additions."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
for path in ('web/protocol.js','web/lora_media.js','web/lora_receipts.js',
             'web/styles.css','firmware/components/offgrid_sx1278/offgrid_sx1278.c',
             'firmware/components/offgrid_sx1278/offgrid_sx1278_profile.c',
             'firmware/components/offgrid_arq/og_sr_usb.c'):
 assert (r/path).read_bytes()==subprocess.check_output(['git','show','v3.0.3-rc.1-android-native-rf:'+path],cwd=r),path
fw=(r/'firmware/main/main.c').read_text();web=(r/'web/app.js').read_text()
assert 'media_restore_requested' in fw and 'MEDIA|RESTORED|' in fw
assert 'profile_start_offer(OG_MODE_LOW,false,false,true)' in fw
assert 'MEDIA|ERROR|ULTRA_TEXT_ONLY' in fw
assert 'og_arq_bridge_cancel(&arq,&arq_io,"PROFILE_CHANGED_UNCONFIRMED")' in fw
assert 'FW=1.8.0-rc.4' in fw and 'FW=1.8.0-rc.4' in web
assert "mediaModeCommand('MEDIA ON'" in web and "mediaModeCommand('MEDIA OFF'" in web
assert 'Ultra SF12 is text only' in web
print('PASS: original encrypted media codecs, receipts, radio SPI driver, USB staging and app preserved; Ultra text-only enforced')
