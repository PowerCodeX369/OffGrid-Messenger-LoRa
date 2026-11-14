from pathlib import Path
root=Path(__file__).resolve().parents[2]
src=(root/'android/app/src/main/java/com/offgrid/messenger/MainActivity.kt').read_text()
fw=(root/'firmware/main/main.c').read_text()
policy=(root/'firmware/main/ble_command_policy.h').read_text()
# BLE firmware already supports the authenticated POWER18 + MEDIA control flow.
assert all(x in policy for x in ['"POWER18 VERIFIED"','"MEDIA ON"','"MEDIA OFF"','"PROFILE "','"AUTO ON"'])
assert 'MEDIA|BYPASS|POWER18_NOT_VERIFIED' in fw
assert 'MEDIA|READY|MODE=LOW' in fw
assert 'MEDIA|RESTORED|MODE=' in fw
# Native Android must wait for firmware replies; the previous implementation
# had no POWER18 VERIFIED authorization and always fell back to Balanced.
assert 'link?.send("POWER18 VERIFIED")' in src
assert '"POWER18" -> if(event=="ENABLED")' in src
assert 'if(action=="MEDIA")beginMediaSession()' in src
assert 'link?.send("MEDIA ON")' in src
assert 'mediaMode=="entering" && event=="READY"' in src
assert 'mediaMode=="entering" && event=="BYPASS"' in src
assert 'Fast unavailable' in src
assert 'link?.send("MEDIA OFF")' in src
assert 'mediaMode=="restoring" && event in listOf("RESTORED","OFF","RESTORE_FAILED")' in src
assert 'power18Confirmed=false;powerApprovalRequest=null;waitingForPowerAck=null' in src
assert 'Confirm BOTH ESP32 radio boards' in src
assert 'setRadioMode=::chooseRadioMode' in src
assert '"SPEED","LR500"->"PROFILE LOW"' not in src
print('PASS: authenticated BLE high-PA confirmation, Fast handover, fallback visibility, restore, connection-scoped approval')
