#!/usr/bin/env python3
"""v1.10 preservation checks against the exact rc.6 Git tag."""
from pathlib import Path
import hashlib,subprocess
p=Path(__file__).resolve().parents[1]
tracked=subprocess.check_output(['git','ls-tree','-r','--name-only','v1.8.0-rc.6'],cwd=p,text=True).splitlines()
protected=[n for n in tracked if n.startswith(('firmware/main/','firmware/components/','android/')) or n in (
 'web/protocol.js','web/lora_bulk.js','web/lora_media.js','web/lora_receipts.js','web/lora_scheduler.js',
 'web/adaptive.js','web/lora_delivery.js','web/styles.css')]
for name in protected:
 old=subprocess.check_output(['git','show','v1.8.0-rc.6:'+name],cwd=p)
 new=(p/name).read_bytes()
 assert new==old,f'Protected baseline modified: {name} ({hashlib.sha256(old).hexdigest()})'
app=(p/'web/app.js').read_text()
assert 'OffGridLoRaRxGuard.AuthenticatedRxGuard' in app
assert 'rxGuard.accept(ticket)' in app and 'rxGuard.reject(ticket)' in app
assert 'for(const id of ticket.packetIds)' in app
assert 'lora_rx_guard.js' in (p/'web/index.html').read_text()
assert '1.10.0-rc1-lora' in app
print(f'PASS: {len(protected)} firmware/ARQ/Android/crypto/media/receipt source files byte-identical to v1.8.0-rc.6')
