#!/usr/bin/env python3
"""Always-run, zero-dependency guard against hiding ESP-NOW media controls."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
html=(root/'web/index.html').read_text()
css=(root/'web/styles.css').read_text()
js=(root/'web/app.js').read_text()
assert '#attachments, #voiceButton { display: none !important; }' not in css
assert 'lora180rc3' in html
for token in ('id="attachments"','id="photoPicker"','id="voiceButton"','id="photoViewer"','id="viewerImage"'):
    assert token in html,token
for token in ('composer-attach','voice-button','chat-photo','voice-clip','photo-viewer','voice-played-icon'):
    assert token in css or token in js,token
for token in ("$('attachments').onclick", "$('photoPicker').addEventListener('change'", "$('voiceButton').onclick",
              'async function sendPhoto(file)', 'async function toggleVoiceRecording()',
              "const container=el('div','voice-clip')", "img.className='chat-photo'", "sound.controls=true"):
    assert token in js,token
print('PASS: original ESP-NOW photo/voice UI remains available and is not CSS-hidden')
