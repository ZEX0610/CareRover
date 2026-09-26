import asyncio
import json
import sys
import time
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'mock'))
from server import create_app, RobotSim
from aiohttp import ClientSession, WSServerHandshakeError
from aiohttp.test_utils import TestServer

def cmd(kind, **fields):
    return {'type':kind,'ts':int(time.time()*1000),**fields}

class SimulationTest(unittest.TestCase):
    def test_watchdog_and_estop(self):
        sim=RobotSim();sim.command(cmd('set_mode',mode='MANUAL'),0)
        sim.command(cmd('cmd_vel',vx=1,vy=0,wz=0),0);sim.step(.02,.02)
        self.assertGreater(sim.velocity[0],0)
        sim.step(.24,.26);self.assertEqual(sim.velocity,[0,0,0])
        sim.command(cmd('estop'),.3)
        self.assertEqual(sim.command(cmd('cmd_vel',vx=1,vy=0,wz=0),.3)[0]['code'],'ESTOP_ACTIVE')
        sim.command(cmd('clear_estop'),.4);self.assertEqual(sim.mode,'IDLE')
    def test_malformed_velocity_is_rejected(self):
        sim=RobotSim()
        for v in (True,'1',float('inf'),None,2):
            self.assertEqual(sim.command(cmd('cmd_vel',vx=v,vy=0,wz=0))[0]['code'],'INVALID_COMMAND')

class NetworkTest(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.app=create_app();self.server=TestServer(self.app);await self.server.start_server()
        self.session=ClientSession()
    async def asyncTearDown(self):
        await self.session.close();await self.server.close()
    async def receive(self, ws, kind, predicate=lambda x:True):
        async def matching():
            while True:
                msg=await ws.receive_json()
                if msg['type']==kind and predicate(msg): return msg
        return await asyncio.wait_for(matching(), 3)
    async def test_end_to_end_commands_reconnect_and_mjpeg(self):
        async with self.session.get(self.server.make_url('/')) as r:
            self.assertEqual(r.status,200);self.assertIn('js/app.js',await r.text())
        async with self.session.get(self.server.make_url('/stream')) as r:
            self.assertEqual(r.status,200);self.assertIn('multipart',r.headers['Content-Type'])
            chunk=await r.content.read(10000);self.assertIn(b'\xff\xd8',chunk)
        ws=await self.session.ws_connect(self.server.make_url('/ws'))
        await self.receive(ws,'telemetry')
        await ws.send_str('{');self.assertEqual((await self.receive(ws,'error'))['code'],'INVALID_JSON')
        await ws.send_json(cmd('set_mode',mode='MANUAL',request_id=7))
        ack=await self.receive(ws,'ack');self.assertEqual(ack['request_id'],7)
        await self.receive(ws,'telemetry',lambda m:m['robot']['mode']=='MANUAL')
        await ws.send_json(cmd('cmd_vel',vx=.6,vy=.4,wz=0))
        moving=await self.receive(ws,'telemetry',lambda m:m['robot']['vx']>0)
        self.assertGreater(moving['robot']['vy'],0)
        stopped=await self.receive(ws,'telemetry',lambda m:m['robot']['vx']==0)
        self.assertEqual(stopped['robot']['vy'],0)
        await ws.send_json(cmd('estop'));await self.receive(ws,'ack',lambda m:m['request_type']=='estop')
        await ws.send_json(cmd('cmd_vel',vx=1,vy=0,wz=0));self.assertEqual((await self.receive(ws,'error'))['code'],'ESTOP_ACTIVE')
        await ws.close()
        ws=await self.session.ws_connect(self.server.make_url('/ws'))
        self.assertTrue((await self.receive(ws,'telemetry'))['robot']['estop'])
        await ws.send_json(cmd('clear_estop',request_id=8));await self.receive(ws,'ack')
        self.assertEqual((await self.receive(ws,'telemetry'))['robot']['mode'],'IDLE')
        ppg=await self.receive(ws,'ppg_batch');self.assertEqual(len(ppg['samples']),5)
        await ws.close()
    async def test_multiple_clients_cannot_compete_for_motion(self):
        a=await self.session.ws_connect(self.server.make_url('/ws'));b=await self.session.ws_connect(self.server.make_url('/ws'))
        await a.send_json(cmd('set_mode',mode='MANUAL'));await self.receive(a,'ack')
        await b.send_json(cmd('set_mode',mode='PERSON_FOLLOW'))
        self.assertEqual((await self.receive(b,'error'))['code'],'CONTROL_BUSY')
        await b.send_json(cmd('estop'));await self.receive(b,'ack')
        self.assertTrue((await self.receive(a,'telemetry',lambda m:m['robot']['estop']))['robot']['estop'])
        await a.close();await b.close()
    async def test_bypass_command_ownership_and_observer_ping_cannot_renew_follow(self):
        a=await self.session.ws_connect(self.server.make_url('/ws'))
        b=await self.session.ws_connect(self.server.make_url('/ws'))
        await a.send_json(cmd('set_mode',mode='PERSON_FOLLOW'))
        await self.receive(a,'ack')
        owner=await self.receive(a,'telemetry',lambda m:m['robot']['mode']=='PERSON_FOLLOW')
        self.assertTrue(owner['robot']['control_allowed'])
        await a.send_json(cmd('set_demo_bypass',enabled=True,request_id=12))
        self.assertEqual((await self.receive(a,'ack',lambda m:m['request_type']=='set_demo_bypass'))['request_id'],12)
        await b.send_json(cmd('set_demo_bypass',enabled=False))
        self.assertEqual((await self.receive(b,'error'))['code'],'CONTROL_BUSY')
        for i in range(5):
            await b.send_json(cmd('ping',id=i))
            await asyncio.sleep(.07)
        stopped=await self.receive(a,'telemetry',lambda m:m['robot']['mode']=='IDLE')
        self.assertFalse(stopped['front']['demo_enabled'])
        self.assertEqual(stopped['robot']['vx'],0)
        await a.close();await b.close()

    async def test_private_paths_and_cross_origin_control_are_blocked(self):
        for path in ('/.git/config','/.venv/pyvenv.cfg','/mock/server.py','/.env'):
            async with self.session.get(self.server.make_url(path)) as r:self.assertEqual(r.status,404)
        with self.assertRaises(WSServerHandshakeError):
            await self.session.ws_connect(self.server.make_url('/ws'),origin='https://unrelated.example')

if __name__=='__main__':unittest.main()
