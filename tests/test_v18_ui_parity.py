#!/usr/bin/env python3
"""Actual CSS/DOM parity smoke against ESP-NOW's original media composer.

Runs Chromium in memory because the sandbox blocks localhost navigation.
This tests presence, visible styling, picker/microphone wiring and received
photo/voice UI, NOT physical LoRa transport or microphone hardware.
"""
import re
from pathlib import Path
from playwright.sync_api import sync_playwright

root = Path(__file__).resolve().parents[1] / 'web'
index = (root / 'index.html').read_text()
styles = (root / 'styles.css').read_text()
assert 'id="attachments"' in index and 'id="photoPicker"' in index and 'id="voiceButton"' in index
assert '#attachments, #voiceButton { display: none !important; }' not in styles
assert '.voice-clip' in styles and '.chat-photo' in styles and '.photo-viewer' in styles
html = re.sub(r'<script[^>]+src="[^"]+"[^>]*></script>', '', index)
with sync_playwright() as playwright:
    browser = playwright.chromium.launch(headless=True, executable_path='/usr/bin/chromium', args=['--no-sandbox'])
    try:
        for width in (1440, 820, 390):
            page = browser.new_page(viewport={'width': width, 'height': 860})
            js_errors = []
            page.on('pageerror', lambda error: js_errors.append(str(error)))
            page.set_content(html)
            page.add_style_tag(content=styles)
            for name in ('protocol.js', 'adaptive.js', 'lora_delivery.js', 'lora_scheduler.js',
                         'lora_bulk.js', 'lora_benchmark.js', 'lora_media.js',
                         'lora_receipts.js', 'app.js'):
                page.add_script_tag(content=(root / name).read_text())
            page.evaluate("verified=true; localMac='AA:BB:CC:DD:EE:FF'; secretKey=new Uint8Array(32).fill(7); refreshStatus()")
            if width <= 720:
                page.evaluate("document.body.classList.add('mobile-chat')")
            assert page.locator('#attachments').is_enabled() and page.locator('#attachments').is_visible(), width
            assert page.locator('#voiceButton').is_enabled() and page.locator('#voiceButton').is_visible(), width
            assert page.locator('#voiceButton').get_attribute('aria-label') == 'Record voice message'
            page.wait_for_timeout(350)  # CSS enter/leave animation settles on mobile
            layout = page.evaluate('''()=>{
              const a=document.getElementById('attachments'),v=document.getElementById('voiceButton');
              const c=document.getElementById('composer');
              return {attachment:getComputedStyle(a).display,voice:getComputedStyle(v).display,
                 voiceOpacity:getComputedStyle(v).opacity,composer:c.className,
                 attachWidth:a.getBoundingClientRect().width,voiceWidth:v.getBoundingClientRect().width};}''')
            assert layout['attachment']!='none' and layout['voice']!='none' and layout['voiceOpacity']=='1',layout
            assert layout['attachWidth']>20 and layout['voiceWidth']>20,layout
            # Clicking original '+' triggers the real native file input. Intercept
            # sendPhoto so this test never needs an attached ESP32 or DB.
            page.evaluate('()=>{sendPhoto=async(file)=>{window.__pickedFile=file.name;};}')
            with page.expect_file_chooser() as pick:
                page.locator('#attachments').click()
            picker = pick.value
            assert not picker.is_multiple()
            picker.set_files({'name':'test-photo.png','mimeType':'image/png','buffer':bytes.fromhex('89504e470d0a1a0a')})
            page.wait_for_function("window.__pickedFile==='test-photo.png'")
            # Original microphone button calls the recording action (controlled
            # click only; no fake microphone permissions are requested).
            page.evaluate('()=>{toggleVoiceRecording=async()=>{window.__micClicks=(window.__micClicks||0)+1;};}')
            page.locator('#voiceButton').click()
            assert page.evaluate('window.__micClicks')==1
            # Image and audio both use identical ESP-NOW chat classes and
            # control widgets. Only content bytes vary by radio transport.
            result = page.evaluate('''()=>{
              const img={id:'a1b2c3d4e5f60708',kind:'image',dir:'in',ts:Date.now(),
                mime:'image/png',bytes:new Uint8Array([137,80,78,71,13,10,26,10]),width:640,height:480,status:'received'};
              const voice={id:'a1b2c3d4e5f60709',kind:'voice',dir:'in',ts:Date.now(),
                mime:'audio/webm',bytes:new Uint8Array([26,69,223,163]),durationMs:4200,status:'received'};
              recordMap.set(img.id,img);recordMap.set(voice.id,voice);
              records.push(img,voice);makeBubble(img,false);makeBubble(voice,false);
              const photo=document.querySelector('.chat-photo'),audio=document.querySelector('.voice-clip audio');
              return {photo:!!photo,photoUsable:!!photo?.onclick,voice:!!audio,
                 audioControls:audio?.controls,voiceDuration:document.querySelector('.voice-clip-info')?.textContent};}''')
            assert result=={'photo':True,'photoUsable':True,'voice':True,'audioControls':True,'voiceDuration':'4s'},result
            # Received photo opens the same in-app viewer as original ESP-NOW.
            page.locator('.chat-photo').click()
            assert page.locator('#photoViewer').is_visible()
            page.locator('#viewerClose').click()
            assert not page.locator('#photoViewer').is_visible()
            # Typing swaps the mic for the send icon, exactly like ESP-NOW.
            page.locator('#messageInput').fill('hi')
            page.wait_for_function("getComputedStyle(document.querySelector('#voiceButton')).opacity < 0.1 && getComputedStyle(document.querySelector('#sendButton')).opacity > 0.9", timeout=4000)
            styles_with_text=page.evaluate('''()=>({
                mic:getComputedStyle(document.querySelector('#voiceButton')).opacity,
                send:getComputedStyle(document.querySelector('#sendButton')).opacity,
                voiceClass:document.querySelector('#composer').className
            })''')
            assert styles_with_text['voiceClass'].endswith('show-send'),styles_with_text
            assert float(styles_with_text['mic']) < 0.1 and float(styles_with_text['send']) > 0.9,styles_with_text
            assert not js_errors,js_errors
            print(f'PASS: {width}px ESP-NOW-style attach/mic composer, photo viewer/audio rendering, functional picker')
            page.close()
    finally:
        browser.close()
