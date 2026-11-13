#!/usr/bin/env python3
"""Exercise actual browser-side media preflight/restore promise sequencing.
Not a physical radio test. No firmware or radio timings are simulated as claims.
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
      let commands=[];
      window.confirm=()=>true;
      currentProfile='BALANCED';highPaVerified=false;autoMode=false;writer={};
      writeLine=async line=>{
        commands.push(line);
        if(line==='POWER18 VERIFIED')highPaVerified=true;
        if(line==='MEDIA ON'){
           setTimeout(()=>onMediaLine('MEDIA|BOOST_REQUEST|LOW|RETURN=BALANCED'),2);
           setTimeout(()=>{onProfileLine('PROFILE|ACTIVE|LOW|SF=7|BW=500000|TX_DBM=18|AUTO=0');onMediaLine('MEDIA|READY|MODE=LOW')},30);
        }
        if(line==='MEDIA OFF'){
           setTimeout(()=>onMediaLine('MEDIA|RESTORE_PENDING|BALANCED'),2);
           setTimeout(()=>{onProfileLine('PROFILE|ACTIVE|BALANCED|SF=7|BW=250000|TX_DBM=10|AUTO=0');onMediaLine('MEDIA|RESTORED|MODE=BALANCED')},30);
        }
      };
      const start=performance.now();const ok=await beginMediaFastSession();
      if(!ok || performance.now()-start<25 || currentProfile!=='LOW')throw Error('Did not await Fast handover');
      await endMediaFastSession();
      if(currentProfile!=='BALANCED'||mediaResponseWaiter!==null)throw Error('Restore not confirmed');
      if(commands.join(',')!=='POWER18 VERIFIED,MEDIA ON,MEDIA OFF')throw Error(commands.join(','));
      highPaVerified=true;
      writeLine=async line=>{commands.push(line);setTimeout(()=>onMediaLine('MEDIA|BYPASS|PEER_BUSY'),5);};
      const bypass=await beginMediaFastSession();
      if(!bypass || currentProfile!=='BALANCED')throw Error('Busy peer should preserve current profile');
      let timedOut=false;
      writeLine=async _line=>{};
      try{await mediaModeCommand('MEDIA ON',['READY'],30);}catch(e){timedOut=/Timed out/.test(e.message);}
      if(!timedOut || mediaResponseWaiter!==null)throw Error('Waiter timeout not bounded');
      return {fastConfirmed:true,restored:true,commands,peerBusyFallback:true,timeoutBounded:true};
    }''')
    assert result['fastConfirmed'] and result['restored'] and result['peerBusyFallback'] and result['timeoutBounded']
    assert not errors, errors
    print('PASS: Chromium media Fast waits for authenticated-ready status, restores, handles busy peer and timeout')
    browser.close()
