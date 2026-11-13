#!/usr/bin/env python3
"""In-memory Chromium media UI, WebP/JPEG and synthetic Opus capture (not RF)."""
import re
import hashlib
from pathlib import Path
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]/'web'
html=re.sub(r'<script[^>]+src="[^"]+"[^>]*></script>','',(root/'index.html').read_text())
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox','--autoplay-policy=no-user-gesture-required'])
    page=browser.new_page()
    errors=[]
    page.on('pageerror',lambda error:errors.append(str(error)))
    page.set_content(html)
    for name in ['protocol.js','adaptive.js','lora_delivery.js','lora_scheduler.js','lora_bulk.js',
                 'lora_benchmark.js','lora_receipts.js','lora_media.js','app.js']:
        page.add_script_tag(content=(root/name).read_text())
    # about:blank is not a secure context. Expose Python's SHA-256 ONLY for
    # this compression/DOM smoke; real AES-GCM/SHA-256 is tested separately
    # under Node's standard WebCrypto implementation.
    page.expose_function('__testSha',lambda xs:list(hashlib.sha256(bytes(xs)).digest()))
    page.evaluate("Object.defineProperty(crypto,'subtle',{configurable:true,value:{digest:async(_a,b)=>new Uint8Array(await __testSha(Array.from(new Uint8Array(b)))).buffer}})")
    result=page.evaluate('''async()=>{
      if(!window.OffGridLoRaMedia||!window.OGP)throw Error('Media and AES codec missing');
      const canvas=document.createElement('canvas');canvas.width=1280;canvas.height=768;
      const ctx=canvas.getContext('2d');const g=ctx.createLinearGradient(0,0,1280,768);
      g.addColorStop(0,'#2370bf');g.addColorStop(.5,'#fff9b8');g.addColorStop(1,'#2e9563');
      ctx.fillStyle=g;ctx.fillRect(0,0,1280,768);
      ctx.fillStyle='#111';ctx.font='42px sans-serif';ctx.fillText('OffGrid 1.8 High Quality Encrypted Image Test',50,240);
      const input=await new Promise(resolve=>canvas.toBlob(resolve,'image/png'));
      const file=new File([input],'photo.png',{type:'image/png'});
      const photo=await preparePhoto(file);
      if(!photo.bytes.length||photo.bytes.length>48*1024)throw Error('Photo exceeds budget');
      if(photo.width<640||photo.height<380)throw Error('Photo dimensions unexpectedly small');
      const parts=await OffGridLoRaMedia.split('image',photo.mime,photo.bytes);
      return {size:photo.bytes.length,width:photo.width,height:photo.height,mime:photo.mime,parts:parts.length,
        voiceSupported:MediaRecorder.isTypeSupported('audio/webm;codecs=opus'),
        attachmentButtonExists:!!document.getElementById('attachments'),voiceButtonExists:!!document.getElementById('voiceButton')};
    }''')
    assert result['size']<=48*1024 and result['parts']>=1
    assert result['attachmentButtonExists'] and result['voiceButtonExists']
    assert not errors,errors
    print('PASS: Chromium compressed image and media UI',result)
    if result['voiceSupported']:
        audio=page.evaluate('''async()=>{
          const audio=new AudioContext(),source=audio.createOscillator(),dest=audio.createMediaStreamDestination();
          source.frequency.value=430;source.connect(dest);source.start();
          const rec=new MediaRecorder(dest.stream,{mimeType:'audio/webm;codecs=opus',audioBitsPerSecond:8000});
          let chunks=[];rec.ondataavailable=e=>{if(e.data.size)chunks.push(e.data)};
          const done=new Promise(resolve=>rec.onstop=resolve);
          rec.start(250);await new Promise(resolve=>setTimeout(resolve,1600));rec.stop();await done;
          source.stop();await audio.close();const blob=new Blob(chunks,{type:rec.mimeType});
          return {size:blob.size,type:blob.type};
        }''')
        assert 256<audio['size']<=40*1024,audio
        print('PASS: Chromium 1.6-second synthetic Opus sample',audio)
    else: print('SKIP: Chromium Opus MediaRecorder unsupported')
    browser.close()
