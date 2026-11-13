#!/usr/bin/env python3
"""Browser orchestration diagnostic test. Fake RF timing only; NOT a speed result."""
import re
from pathlib import Path
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]/'web'
html=re.sub(r'<script[^>]+src="[^"]+"[^>]*></script>','',(root/'index.html').read_text())
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
    page=browser.new_page()
    problems=[]
    page.on('pageerror',lambda e:problems.append(str(e)))
    page.set_content(html)
    for name in ['protocol.js','adaptive.js','lora_delivery.js','lora_scheduler.js','lora_bulk.js',
                 'lora_benchmark.js','lora_receipts.js','lora_media.js','lora_rx_guard.js','app.js']:
        page.add_script_tag(content=(root/name).read_text())
    result=page.evaluate('''async()=>{
      writer={};verified=true;secretKey={};
      Object.defineProperty(crypto,'subtle',{configurable:true,
        value:{digest:async()=>new Uint8Array(32).buffer}});
      beginMediaFastSession=async()=>true;
      endMediaFastSession=async()=>{};
      let n=0;
      P.seal=async (_k,plain,_id)=>new Uint8Array(plain.length+28);
      transferFragments=async ({ciphertext},sub)=>{
        if(ciphertext.length>7168)throw Error('Ciphertext overflow');
        n++;
        sub.bulkMetrics={usbStageMs:50,radioWaitMs:100,RF_MS:80,FRAMES:32,RETX:0,TIMEOUT:0};
      };
      const r={id:'0123456789ABCDEF',kind:'image',mime:'image/webp',
        bytes:new Uint8Array(48*1024),ts:Date.now(),width:1280,height:720};
      const job={record:r};
      await transferMedia(job);
      const last=usbLog.filter(line=>line.includes('MEDIAPERF|')).at(-1)||'';
      const parts=usbLog.filter(line=>line.includes('MEDIASEG|'));
      return {segments:n,stats:job.mediaMetrics,finalLog:last,partLogs:parts.length};
    }''')
    assert result['segments']==7,result
    assert result['partLogs']==7,result
    assert result['stats']['segments']==7,result
    assert result['stats']['usbMs']==350,result
    assert result['stats']['radioWaitMs']==700,result
    assert result['stats']['deviceRfMs']==560,result
    assert 'SEG=7/7' in result['finalLog'] and 'RF_MS=560' in result['finalLog'],result
    assert not problems,problems
    print('PASS: 48 KiB browser media emits seven sequential segments and aggregate USB/RF/other timing without changing firmware')
    browser.close()
