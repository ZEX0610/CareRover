import unittest
from mock.server import RobotSim
from mock.front_sim import FrontSim
class FrontTests(unittest.TestCase):
    def test_manual_latch(self):
        s=RobotSim()
        s.command(dict(type='set_mode',mode='MANUAL',ts=1),0)
        s.front.distance=15
        s.command(dict(type='cmd_vel',vx=1,vy=0,wz=0,ts=1),0)
        s.step(.08,.08)
        self.assertEqual(s.velocity,[0,0,0])
        s.front.distance=120
        s.step(.08,.16)
        self.assertEqual(s.command(dict(type='cmd_vel',vx=1,vy=0,wz=0,ts=1),.32)[0]['code'],'FRONT_RELEASE_REQUIRED')
        s.step(.08,.24);s.step(.08,.32)
        s.command(dict(type='cmd_vel',vx=0,vy=0,wz=0,ts=1),.32)
        s.command(dict(type='cmd_vel',vx=1,vy=0,wz=0,ts=1),.32)
        s.step(.08,.40)
        self.assertGreater(s.velocity[0],0)
        s.front.distance=None
        s.step(.08,.48)
        self.assertEqual(s.mode,'IDLE')
        self.assertIsNone(s.telemetry()['front']['distance_cm'])
    def test_demo_and_heartbeat(self):
        s=RobotSim()
        s.command(dict(type='set_mode',mode='PERSON_FOLLOW',ts=1),0)
        s.command(dict(type='set_demo_bypass',enabled=True,ts=1),0)
        phases=set()
        for i in range(160):
            s.command(dict(type='ping',id=i,ts=1),i*.02)
            s.step(.02,i*.02)
            phases.add(s.front.phase)
        self.assertTrue({'HALT','RIGHT','MARGIN','PASS','REACQUIRE','NONE'}<=phases)
        s.step(.3,4)
        self.assertEqual(s.mode,'IDLE')
    def test_blocked_timeout(self):
        f=FrontSim();f.distance=18;f.set_demo(True,0)
        for i in range(250):
            _,abort=f.step(i*.02,[.1,0,0],True)
            if abort: break
        self.assertTrue(abort)
        self.assertEqual(f.reason,'bypass_timeout')
