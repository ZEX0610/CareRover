async page => {
  await page.addInitScript({path:'tests/browser/tracking_ws_fixture.js'});
  await page.goto('http://127.0.0.1:8086/?transport=ws');
  await page.waitForFunction(()=>document.querySelector('[data-mode=PERSON_FOLLOW]')?.disabled===false);
  await page.getByRole('button',{name:'跟随',exact:true}).click();
  await page.waitForTimeout(650);
  const follow=await page.evaluate(()=>({mode:trackingFixture.mode,pings:trackingFixture.messages.filter(m=>m.type==='ping').length,imu:document.querySelector('#sImuStatus').textContent,source:document.querySelector('#videoStream').getAttribute('src')}));
  if(follow.mode!=='PERSON_FOLLOW'||follow.pings<5||!follow.source.endsWith('/stream'))throw Error(JSON.stringify(follow));
  await page.evaluate(()=>{trackingFixture.freeze=true;trackingFixture.imu=false;});
  await page.waitForTimeout(650);
  const stale=await page.evaluate(()=>({hidden:document.querySelector('#visionStale').hidden,imu:document.querySelector('#sImuStatus').textContent}));
  if(stale.hidden||!stale.imu.includes('故障'))throw Error(JSON.stringify(stale));
  await page.evaluate(()=>{trackingFixture.freeze=false;trackingFixture.imu=true;trackingFixture.stream='/stream?changed=1';trackingFixture.owner=false;});
  await page.waitForTimeout(250);
  const owner=await page.evaluate(()=>({disabled:document.querySelector('[data-mode=PERSON_FOLLOW]').disabled,src:document.querySelector('#videoStream').getAttribute('src')}));
  if(!owner.disabled||!owner.src.endsWith('changed=1'))throw Error(JSON.stringify(owner));
  await page.getByRole('button',{name:'紧急停止',exact:true}).click();
  await page.waitForTimeout(150);
  const stopped=await page.evaluate(()=>trackingFixture.estop);
  if(!stopped)throw Error('Observer emergency stop failed');
  return {follow,stale,owner,observerEstop:stopped};
}
