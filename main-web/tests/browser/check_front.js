async page => {
  await page.addInitScript(()=>localStorage.setItem('carerover.lang','zh'));
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const results={};
  for(const transport of ['mock','ws']) {
    await page.setViewportSize({width:1280,height:1000});
    await page.goto(`http://127.0.0.1:8087/?transport=${transport}&video=canvas`);
    await page.waitForFunction(()=>document.querySelector('#frontDistance').textContent.includes('cm'));
    if(!await page.locator('#demoBypass').isDisabled())throw Error('Demo available outside follow');
    await page.locator('[data-mode=PERSON_FOLLOW]').click();
    await page.locator('#demoBypass').click();
    const phases=new Set();
    for(let i=0;i<40;i++){phases.add(await page.locator('#frontPhase').textContent());await page.waitForTimeout(90);}
    for(const phase of ['停车确认','向右横移','车身让行余量','直行越过','重新确认人脸']) {
      if(![...phases].some(x=>x.includes(phase)))throw Error(`${transport} missing ${phase}`);
    }
    if(await page.locator('#demoBypass').isChecked())throw Error('Completed demo stayed enabled');
    if(await page.locator('#sMode').textContent()!=='跟随')throw Error('Follow did not resume');
    await page.locator('#demoBypass').click();
    await page.waitForFunction(()=>document.querySelector('#frontPhase').textContent.includes('向右横移'));
    await page.locator('#estopBtn').click();
    await page.waitForFunction(()=>document.querySelector('#sMode').textContent.includes('急停'));
    await page.waitForFunction(()=>!document.querySelector('#demoBypass').checked && document.querySelector('#frontPhase').textContent.includes('未执行'));
    results[transport]=[...phases];
  }
  await page.goto('http://127.0.0.1:8087/?transport=mock');
  await page.waitForFunction(()=>document.querySelector('#frontDistance').textContent.includes('cm'));
  for(const [name,width,height] of [['desktop',1280,1000],['mobile',390,844]]) {
    await page.setViewportSize({width,height});await page.locator('.front-panel').scrollIntoViewIfNeeded();
    if(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth))throw Error('Horizontal overflow '+name);
    await page.screenshot({path:`output/playwright/front-${name}.png`,fullPage:true});
  }
  await page.locator('#langToggle').click();
  if(await page.locator('#frontTitle').textContent()!=='Front obstacle distance')throw Error('English label missing');
  if(errors.length)throw Error(errors.join('\n'));
  return {...results,viewports:['1280x1000','390x844'],consoleErrors:errors};
}
