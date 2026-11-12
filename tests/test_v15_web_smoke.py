#!/usr/bin/env python3
"""Optional Chromium smoke test; needs `pip install playwright` and Chromium.

Uses in-memory HTML/JS rather than network requests. This verifies rendering
and client-side handler wiring, NOT Web Serial or real radio transmissions.
"""
from pathlib import Path
import re
from playwright.sync_api import sync_playwright
root = Path(__file__).resolve().parents[1] / "web"
html = re.sub(r'<script[^>]+src="[^"]+"[^>]*></script>', "", (root / 'index.html').read_text())
with sync_playwright() as playwright:
    browser = playwright.chromium.launch(headless=True, executable_path='/usr/bin/chromium', args=['--no-sandbox'])
    page = browser.new_page()
    errors = []
    page.on('pageerror', lambda error: errors.append(str(error)))
    page.set_content(html)
    for name in ['protocol.js','adaptive.js','lora_delivery.js','lora_scheduler.js',
                 'lora_benchmark.js','lora_receipts.js','lora_bulk.js','app.js']:
        page.add_script_tag(content=(root / name).read_text())
    page.locator('#settingsButton').click()
    page.get_by_role('button', name='Manual LoRa profiles').click()
    text = page.locator('#modalBody').inner_text()
    for phrase in ['Fast / Nominal','Balanced','Max Power','500 kHz','250 kHz','125 kHz','18 dBm']:
        assert phrase in text, (phrase, text)
    assert 'Experimental' not in text, text
    outcome = page.evaluate("""async () => {
      verified=true;
      writer={};
      secretKey=new Uint8Array(32);
      let confirms=0;
      window.confirm=()=>{confirms++;return true};
      const commands=[];
      writeLine=async line=>{commands.push(line)};
      await requestProfileChange('LOW');
      const queued=commands.slice();
      onProfileLine('PROFILE|NEGOTIATING|LOW|MANUAL=1');
      onProfileLine('PROFILE|AGREED|LOW');
      onProfileLine('PROFILE|VERIFYING|LOW');
      onProfileLine('PROFILE|ACTIVE|LOW|SF=7|BW=500000|TX_DBM=18|AUTO=0');
      const active=currentProfile;
      const pending=profileTransition;
      onProfileLine('PROFILE|PEER_REQUEST|BALANCED');
      onProfileLine('PROFILE|ACTIVE|BALANCED|SF=7|BW=250000|TX_DBM=10|AUTO=0');
      return {commands:queued,confirms,active,pending,finalMode:currentProfile,finalPending:profileTransition};
    }""")
    assert outcome['commands']==['POWER18 VERIFIED','PROFILE LOW'], outcome
    assert outcome['confirms']==1, outcome
    assert outcome['active']=='LOW' and not outcome['pending'], outcome
    assert outcome['finalMode']=='BALANCED' and not outcome['finalPending'], outcome
    assert not errors, errors
    print('PASS: in-memory Chromium profile modal (no USB/RF test)')
    browser.close()
