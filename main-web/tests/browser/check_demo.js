async page => {
  await page.addInitScript({path:'tests/browser/tracking_ws_fixture.js'});
  await page.reload();
  await page.waitForFunction(()=>window.trackingFixture);
  await page.evaluate(()=>{
    Object.defineProperty(window.trackingFixture,'signalPayload',{configurable:true,get(){return {
      health:{hr_bpm:72,hr_valid:true,hr_held:true,hr_age_ms:8200,spo2_pct:null,spo2_valid:false,spo2_held:false,finger_detected:true,state:'VALID',sqi:.8},
      vision:{image_width:320,image_height:240,person:{seq:this.seq,age_ms:550,found:true,predicted:true,x:124,y:32,w:76,h:176,confidence:.54},gesture:{label:'LIKE',confidence:.18,stable:true,held:true,age_ms:420}}
    }}});
  });
  await page.waitForFunction(()=>document.getElementById('mHr').textContent==='72 ~');
  const text=await page.locator('#gestureDetail').textContent();if(!text.includes('保持'))throw Error('Gesture hold absent');
  if(await page.locator('#mSpo2').textContent()!=='—')throw Error('Missing oxygen was invented');
  await page.setViewportSize({width:1280,height:960});
  await page.waitForTimeout(200);
  await page.screenshot({path:'output/playwright/0915-held-desktop.png',fullPage:true});
  await page.setViewportSize({width:390,height:844});
  await page.waitForTimeout(200);
  await page.screenshot({path:'output/playwright/0915-held-mobile.png',fullPage:true});
  const overflow=await page.evaluate(()=>document.documentElement.scrollWidth>window.innerWidth);if(overflow)throw Error('Mobile horizontal overflow');
  await page.evaluate(()=>{window.trackingFixture.silent=true;});
  await page.waitForFunction(()=>document.getElementById('mHr').textContent==='—',{timeout:4000});
  console.log('PASS: independent held metrics, held gesture, responsive layout and telemetry-loss clearing');
}
