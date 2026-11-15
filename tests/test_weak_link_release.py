"""v3.0.5 integration guard. Not equivalent to ESP-IDF / Android target build."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
f=lambda p:(r/p).read_text()
a=f('android/app/src/main/java/com/offgrid/messenger/MainActivity.kt')
w=f('web/app.js');fw=f('firmware/main/main.c')
assert 'versionCode=306' in f('android/app/build.gradle.kts')
assert 'RF_POLICY=ULTRA12_FEC8' in fw and 'RF_POLICY=ULTRA12_FEC8' in f('firmware/main/ble_bridge.c')
assert 'RF_POLICY=ULTRA12_FEC8' in a and 'RF_POLICY=ULTRA12_FEC8' in w
assert 'peer_ultra_capable=f.data[0]>=3' in fw and 'cap.data[0]=3' in fw
assert 'coding_denominator=8' in f('firmware/main/lora_modes.h')
assert 'og_sr_tx_largest_attempt' in fw
assert 'OG_SR_ULTRA_MAX_ATTEMPTS 12u' in f('firmware/components/offgrid_arq/include/og_sr.h')
assert 'weakRetry.holdForReceipt' in a and 'weakRetry.attempt' in a
assert 'historyRestored=true' in a and 'freshLoRaPeer()' in a
assert 'WeakTextRetry' in f('android/tests/WeakTextRetryTest.kt')
assert 'weakTextRetry.holdForReceipt' in w and 'pumpWeakText' in w
assert 'hydrateHistory' in w and "d.kind==='text'?'waiting':'failed'" in w
assert 'weak_text_queue.js' in f('web/index.html')
assert 'EVENT_QUEUE_FULL' in w and 'bounded retry' in w
# Recovery build isolates the v3.0.5 queue reshuffle by restoring
# the proven v3.0.4 FIFO job dispatch. Urgent receipts retain their own path.
assert "const job=transferQueue.shift();" in w
assert "const urgentIndex=transferQueue.findIndex" not in w
assert 'RFHEARD' in w and "rssi>=-160" in w
for p in ('firmware/components/offgrid_sx1278/offgrid_sx1278.c',
          'firmware/main/ble_auth_psa.h', 'web/protocol.js',
          'android/app/src/main/java/com/offgrid/messenger/OgmCodec.kt'):
 import subprocess
 assert (r/p).read_bytes()==subprocess.check_output(['git','show','v3.0.4-rc.1-ultra:'+p],cwd=r),p
print('PASS: v3.0.5 durable weak-link text outbox, FEC8 compatibility gate, authenticated receipts, original crypto and radio driver')
