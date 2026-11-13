#!/usr/bin/env python3
"""RC6: text queue can interleave only after a finished media segment.
Fake bulk/crypto results are for ordering validation, NOT RF-speed claims.
"""
import re
from pathlib import Path
from playwright.sync_api import sync_playwright
root=Path(__file__).resolve().parents[1]/'web'
html=re.sub(r'<script[^>]+src="[^"]+"[^>]*></script>','',(root/'index.html').read_text())
with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,executable_path='/usr/bin/chromium',args=['--no-sandbox'])
    page=browser.new_page()
    errors=[]
    page.on('pageerror',lambda e:errors.append(str(e)))
    page.set_content(html)
    for name in ['protocol.js','adaptive.js','lora_delivery.js','lora_scheduler.js','lora_bulk.js',
                 'lora_benchmark.js','lora_receipts.js','lora_media.js','app.js']:
        page.add_script_tag(content=(root/name).read_text())
    result=page.evaluate('''async()=>{
      verified=true;secretKey={};writer={};bulkLink={};
      const ids=['AABBCCDD11223344','AABBCCDD11223355'];
      const texts=ids.map(id=>({id,kind:'text',dir:'out',text:'hi',status:'sending'}));
      for(const record of texts)transferQueue.push({kind:'text',record,control:false,createdMono:performance.now()});
      const media={id:'12345678AABBCCDD',kind:'image',dir:'out',status:'sending'};
      activeTransfer={kind:'image',record:media};
      let sent=[],sealed=0;
      sealJob=async job=>{sealed++;return {transferId:'123456789ABCDEF0',ciphertext:new Uint8Array(72)};};
      transferFragments=async (packet,job)=>{
        sent.push(job.record.id);
        job.bulkMetrics={usbStageMs:5,radioWaitMs:120,RF_MS:90,FRAMES:1,RETX:0,TIMEOUT:0};
      };
      setState=async(r,status)=>{r.status=status;};
      watchDeliveryReceipt=()=>{};
      await sendOneQueuedTextBetweenMediaSegments();
      if(sent.length!==1||sent[0]!==ids[0]||activeTransfer.record!==media||transferQueue.length!==1)
        throw Error('Boundary priority or media ownership incorrect');
      profileTransition=true;
      await sendOneQueuedTextBetweenMediaSegments();
      if(sent.length!==1||transferQueue.length!==1)throw Error('Interleaved during radio profile change');
      profileTransition=false;
      await sendOneQueuedTextBetweenMediaSegments();
      if(sent.length!==2||sent[1]!==ids[1]||transferQueue.length!==0)throw Error('Second text not sent');
      const diagnostic=usbLog.filter(x=>x.includes('TEXTPERF|'));
      if(diagnostic.length!==2||!diagnostic.every(x=>x.includes('QUEUE_MS=')&&x.includes('RFWAIT_MS=')))
         throw Error('Missing content-free text diagnostic');
      return {sent,mediaActive:activeTransfer.record.id,diagnostics:diagnostic.length,encryptedJobs:sealed};
    }''')
    assert result['sent']==['AABBCCDD11223344','AABBCCDD11223355'],result
    assert result['diagnostics']==2,result
    media_result=page.evaluate('''async()=>{
      verified=true;secretKey={};writer={};bulkLink={};profileTransition=false;
      transferQueue.length=0;
      let outgoing=[];
      const media={id:'12345678AABBCCDD',kind:'image',dir:'out',status:'sending',
        bytes:new Uint8Array(10000),mime:'image/webp',width:640,height:480,progress:0};
      activeTransfer={kind:'image',record:media};
      beginMediaFastSession=async()=>true;
      endMediaFastSession=async()=>{};
      Object.defineProperty(crypto,'subtle',{configurable:true,value:{digest:async()=>new Uint8Array(32).buffer}});
      P.seal=async()=>new Uint8Array(90);
      let mediaCount=0;
      transferFragments=async(_packet,job)=>{
        if(job.mediaSegment){
          mediaCount++;outgoing.push('M'+mediaCount);
          if(mediaCount===1)transferQueue.push({kind:'text',control:false,
            record:{id:'1111222233334444',kind:'text',dir:'out',text:'urgent',status:'sending'},
            createdMono:performance.now()});
        }else outgoing.push('TEXT');
        job.bulkMetrics={usbStageMs:2,radioWaitMs:3,RF_MS:2,FRAMES:1,RETX:0,TIMEOUT:0};
      };
      await transferMedia({kind:'image',record:media});
      if(outgoing.join(',')!=='M1,TEXT,M2')throw Error('Text was not scheduled between authenticated media sessions: '+outgoing.join(','));
      if(activeTransfer.record!==media)throw Error('Media cancellation ownership lost');
      return outgoing;
    }''')
    assert media_result==['M1','TEXT','M2'],media_result
    assert not errors,errors
    print('PASS: Chromium short texts run at verified media-segment boundaries (M1, TEXT, M2), not mid-radio-packet')
    browser.close()
