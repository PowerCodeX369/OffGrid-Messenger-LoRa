#!/usr/bin/env python3
"""Browser-level async duplicate/replay regression. Fake decryption/handlers,
not RF or on-device cryptographic proof; the actual WebCrypto integrity tests
run in firmware/tests/test-lora-security-fuzz.cjs.
"""
import re
from pathlib import Path
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]/'web'
html=re.sub(r'<script[^>]+src="[^"]+"[^>]*></script>','',(root/'index.html').read_text())
with sync_playwright() as pw:
 browser=pw.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
 try:
  page=browser.new_page();errors=[]
  page.on('pageerror',lambda e:errors.append(str(e)))
  page.set_content(html)
  for name in ('protocol.js','adaptive.js','lora_delivery.js','lora_scheduler.js','lora_bulk.js',
               'lora_benchmark.js','lora_receipts.js','lora_media.js','lora_rx_guard.js','app.js'):
   page.add_script_tag(content=(root/name).read_text())
  result=page.evaluate('''async()=>{
    verified=true;secretKey={};
    let handlers=0,decrypts=0,acks=[];
    writeLine=async line=>acks.push(line);
    P.open=async()=>{decrypts++;await new Promise(r=>setTimeout(r,25));return new Uint8Array(24);};
    P.parseEnvelope=()=>({kind:P.types.text,id:'0123456789ABCDEF',body:new Uint8Array(1),ts:Date.now()});
    handleEnvelope=async()=>{handlers++;await new Promise(r=>setTimeout(r,30));};
    const bulk='0123456789ABCDEF',mac='AA:BB:CC:DD:EE:FF',cipher=new Uint8Array(120);
    await Promise.all([processRxBulk(mac,bulk,cipher),processRxBulk(mac,bulk,cipher)]);
    await processRxBulk(mac,bulk,cipher);
    if(handlers!==1||decrypts!==1)throw Error('Duplicate bulk work '+handlers+'/'+decrypts);
    let failed=true;
    P.open=async()=>{decrypts++;if(failed){failed=false;throw Error('bad gcm');}return new Uint8Array(24);};
    await processRxBulk(mac,'1122334455667788',cipher);
    await processRxBulk(mac,'1122334455667788',cipher);
    if(handlers!==2||decrypts!==3)throw Error('Failed authentication wrongly cached');
    rxGuard.clear();
    P.open=async()=>{decrypts++;await new Promise(r=>setTimeout(r,25));return new Uint8Array(24);};
    const frame=P.frames(bulk,new Uint8Array(64))[0],hex=P.hex(frame);
    await Promise.all([processRx(mac,'91','100',hex),processRx(mac,'91','101',hex)]);
    await processRx(mac,'91','102',hex);
    if(handlers!==3||decrypts!==4)throw Error('Legacy duplicate work '+handlers+'/'+decrypts);
    const legacyAcks=acks.filter(a=>a.startsWith('APPACK '));
    if(legacyAcks.length!==3)throw Error('Expected 3 authenticated ACKs, got '+legacyAcks.length);
    return {handlers,decrypts,legacyAcks:legacyAcks.length};
  }''')
  assert result=={'handlers':3,'decrypts':4,'legacyAcks':3},result
  assert not errors,errors
  print('PASS: Chromium 1x authenticated handler under concurrent duplicate bulk and legacy RX; invalid GCM remains retryable')
 finally:browser.close()
