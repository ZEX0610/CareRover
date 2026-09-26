#!/usr/bin/env python3
"""Read-only wireless checks by default; --exercise-targets tests the explicitly uninstalled motion backend."""
import argparse
import asyncio
import json
from pathlib import Path
import statistics
import time
import aiohttp


def now_ms(): return int(time.time()*1000)


class Device:
    def __init__(self, http, url): self.http=http; self.url=url; self.ws=None; self.sequence=0
    async def connect(self):
        self.ws=await self.http.ws_connect(self.url, max_msg_size=65536)
        stamp=now_ms(); await self.send('ping',id=stamp)
        await self.until(lambda m:m.get('type')=='pong' and m.get('id')==stamp)
        return self
    async def close(self):
        if self.ws and not self.ws.closed: await self.ws.close()
    async def send(self, kind, **fields):
        await self.ws.send_json({'type':kind,'ts':now_ms(),**fields})
    async def until(self, predicate, timeout=5):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            msg=await self.ws.receive(timeout=max(.01,deadline-time.monotonic()))
            if msg.type != aiohttp.WSMsgType.TEXT:
                raise RuntimeError(f'WebSocket ended or returned non-text: {msg.type}')
            data=json.loads(msg.data)
            if predicate(data): return data
        raise TimeoutError('Expected device response not received')
    async def request(self, kind, **fields):
        self.sequence+=1; n=self.sequence
        await self.send(kind, request_id=n, **fields)
        return await self.until(lambda m:m.get('type') in {'ack','error'} and m.get('request_id')==n)
    async def mode(self):
        result=await self.request('set_mode',mode='MANUAL')
        if result.get('type')!='ack' or result.get('ok') is not True: raise AssertionError(result)
        return await self.robot(lambda r:r.get('mode')=='MANUAL' and not r.get('estop'))
    async def robot(self,predicate,after=-1):
        return await self.until(lambda m:m.get('type')=='telemetry' and m.get('device',{}).get('uptime_ms',-1)>after and predicate(m.get('robot',{})))


async def read_only(http, base, seconds):
    async with http.get(base+'/',allow_redirects=True) as response:
        assert response.status==200, f'HTTP returned {response.status}'
        text=await response.text(); assert 'carerover-web-version' in text, 'Generated FFat payload/version marker missing'
    async with http.get(base+'/version.json') as response:
        assert response.status==200; version=await response.json()
    for path in ['/hardware/serial_bridge.py','/wifi_secrets.h','/js/%2e%2e/index.html']:
        async with http.get(base+path) as response: assert response.status==404, f'Private path accessible: {path}'
    c=await Device(http,base+'/ws').connect()
    count=0; sample_count=0; intervals=[]; last=None; sample_rates=set(); source_updates={'health':0,'vision':0}; devices=[]
    began=time.monotonic(); next_ping=began+2
    try:
        while time.monotonic()-began<seconds:
            if time.monotonic()>=next_ping:
                await c.send('ping',id=now_ms()); next_ping=time.monotonic()+2
            m=await c.until(lambda _:True,timeout=5)
            if m.get('type')=='telemetry':
                t=time.monotonic(); count+=1
                if last is not None: intervals.append(t-last)
                last=t
                for key in source_updates: source_updates[key]+=int(key in m)
                devices.append(m.get('device',{}))
                assert m['robot'].get('motion_output_installed') is False, 'Unexpected execution backend'
                assert not any(m['robot'].get(a,0) for a in ['vx','vy','wz']), 'Read-only soak expects zero targets'
            elif m.get('type')=='ppg_batch':
                assert m.get('sample_rate_hz')==25; assert len(m['samples'])==5
                sample_rates.add(m['sample_rate_hz']); sample_count+=len(m['samples'])
    finally: await c.close()
    assert count>=seconds*7, f'Telemetry too sparse: {count} frames / {seconds}s'
    assert source_updates['health']>0 and source_updates['vision']>0, 'Missing source blocks'
    assert sample_count>=seconds*15, 'PPG missing or too sparse; verify MAX30102 and device loop'
    assert devices and all(d.get('firmware')==devices[0].get('firmware') for d in devices)
    assert all(d.get('max_safety_gap_ms',999)<=10 for d in devices), 'Safety scheduling gap exceeded reserved budget'
    return {'web_version':version,'firmware':devices[0].get('firmware'),'seconds':seconds,'telemetry_frames':count,
            'median_telemetry_interval_ms':statistics.median(intervals)*1000 if intervals else None,
            'ppg_samples':sample_count,'sample_rates':sorted(sample_rates),'source_updates':source_updates,
            'max_safety_gap_ms':max(d.get('max_safety_gap_ms',0) for d in devices),
            'heap_first':devices[0].get('free_heap'),'heap_last':devices[-1].get('free_heap')}


async def targets(http,base):
    owner=await Device(http,base+'/ws').connect()
    observer=await Device(http,base+'/ws').connect()
    results=[]
    try:
        initial=await owner.robot(lambda _:True)
        assert initial['robot'].get('motion_output_installed') is False
        assert initial['device'].get('backend')=='test_targets' and initial['device'].get('stage',0)>=4
        assert initial['connection'].get('camera') is True, 'CAM must be online for MANUAL tests'
        assert not initial['robot']['estop'], 'Clear preexisting ESTOP intentionally before running tests'
        rejected=await owner.request('set_mode',mode='PERSON_FOLLOW')
        assert rejected.get('code')=='UNSUPPORTED_MODE', rejected
        for i in range(20):
            state=await owner.mode(); marker=state['device']['uptime_ms']
            busy=await observer.request('set_mode',mode='MANUAL')
            assert busy.get('code')=='CONTROL_BUSY',busy
            # Renew mode after waiting for the observer response, then issue one target.
            await owner.send('cmd_vel',vx=.2,vy=0,wz=0)
            moving=await owner.robot(lambda r:r.get('vx')==.2,after=marker)
            before=moving['device']; marker=before['uptime_ms']; scenario=i%4
            if scenario==0:
                stopped=await owner.robot(lambda r:r.get('mode')=='IDLE' and r.get('vx')==0,after=marker)
                d=stopped['device']; assert d['stop_reason']=='watchdog'
                assert 0<=d['stopped_at_ms']-d['last_cmd_ms']<=250,d
            elif scenario==1:
                await owner.close()
                stopped=await observer.robot(lambda r:r.get('mode')=='IDLE' and r.get('vx')==0,after=marker)
                assert stopped['device']['stop_reason'] in {'owner_disconnected','watchdog'}
                owner=await Device(http,base+'/ws').connect()
                restored=await owner.robot(lambda _:True)
                assert restored['robot']['mode']=='IDLE' and restored['robot']['vx']==0
            elif scenario==2:
                ack=await observer.request('estop'); assert ack.get('ok') is True
                stopped=await owner.robot(lambda r:r.get('estop') is True and r.get('vx')==0,after=marker)
                await owner.close(); owner=await Device(http,base+'/ws').connect()
                latched=await owner.robot(lambda _:True); assert latched['robot']['estop'] is True
                cleared=await owner.request('clear_estop'); assert cleared.get('ok') is True,cleared
                await owner.robot(lambda r:r.get('mode')=='IDLE' and r.get('estop') is False)
            else:
                await owner.send('cmd_vel',vx='bad',vy=0,wz=0)
                await observer.send('cmd_vel',vx=.9,vy=0,wz=0)
                await owner.ws.send_json({'type':'cmd_vel','ts':now_ms()-1000,'vx':.9,'vy':0,'wz':0})
                stopped=await owner.robot(lambda r:r.get('mode')=='IDLE' and r.get('vx')==0,after=marker)
                d=stopped['device']; assert d['stop_reason']=='watchdog'
                assert d['last_cmd_ms']==before['last_cmd_ms'], 'Rejected commands refreshed lease'
                assert d['stopped_at_ms']-d['last_cmd_ms']<=250,d
            results.append({'case':i+1,'scenario':['watchdog','disconnect','estop_reconnect','invalid_commands'][scenario],
                            'stop_reason':stopped['device']['stop_reason'],
                            'stopped_at_ms':stopped['device']['stopped_at_ms'], 'last_cmd_ms':stopped['device']['last_cmd_ms']})
        # Parsing guards: malformed/nested JSON never change mode. Oversize frame closes its client.
        for raw in ['{','{"type":"ping","type":"estop"}', '['*9+']'*9]:
            await observer.ws.send_str(raw)
            await observer.until(lambda m:m.get('type')=='error' and m.get('code') in {'INVALID_JSON','INVALID_COMMAND'})
        await observer.ws.send_str(' '*65537)
        for _ in range(100):
            message=await observer.ws.receive(timeout=5)
            if message.type in {aiohttp.WSMsgType.CLOSE,aiohttp.WSMsgType.CLOSED,aiohttp.WSMsgType.ERROR}: break
        else: raise AssertionError('Oversized client was not closed')
        return results
    finally:
        if owner.ws and not owner.ws.closed:
            try: await owner.send('cmd_vel',vx=0,vy=0,wz=0)
            finally: await owner.close()
        await observer.close()


async def main(args):
    base=args.url.rstrip('/')
    report={'hardware_test':True,'completed':False,'read_only':None,'target_cases':None}
    try:
        async with aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=None,sock_connect=5,sock_read=10)) as http:
            report['read_only']=await read_only(http,base,args.seconds)
            if args.exercise_targets: report['target_cases']=await targets(http,base)
        report['completed']=True
    finally:
        out=Path(args.report); out.parent.mkdir(parents=True,exist_ok=True)
        out.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        print(out)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--url',default='http://192.168.4.1')
    p.add_argument('--seconds',type=int,default=30)
    p.add_argument('--exercise-targets',action='store_true')
    p.add_argument('--report',default='build/wireless-probe.json')
    args=p.parse_args()
    if args.seconds<2: p.error('--seconds must be >=2')
    asyncio.run(main(args))
