"""Local CareRover robot simulator: static files + /ws + /stream.
All motion is simulated. No hardware drivers or outside network requests.
"""
import argparse
import asyncio
import io
import json
import math
import random
import time
from pathlib import Path
from aiohttp import web, WSMsgType
from PIL import Image, ImageDraw
try:
    from .front_sim import FrontSim
except ImportError:
    from front_sim import FrontSim

ROOT = Path(__file__).resolve().parent.parent
MODES = ('IDLE', 'MANUAL', 'PERSON_FOLLOW', 'GESTURE_CONTROL', 'HEALTH_CHECK')
GESTURES = ('NONE', 'PALM', 'FIST', 'THUMB_UP', 'VICTORY', 'POINT_LEFT', 'POINT_RIGHT')

def stamp():
    return int(time.time() * 1000)

def finite(v):
    return type(v) in (int, float) and math.isfinite(v)

class RobotSim:
    def __init__(self):
        self.front=FrontSim()
        self.last_ping=-math.inf
        self.mode = 'IDLE'
        self.estop = False
        self.velocity = [0., 0., 0.]
        self.target = [0., 0., 0.]
        self.last_command = -math.inf
        self.t = 0.
        self.yaw = 0.
        self.phase = 0.
        self.hr, self.spo2, self.sqi = 74, 98, .94
        self.person = {'found': True, 'x': 124, 'y': 32, 'w': 76, 'h': 176, 'confidence': .94}
        self.gesture = 'NONE'
        self.finger = True
        self.health_state = 'VALID'

    def stop(self, idle=False, reason="mode_exit"):
        self.velocity = [0., 0., 0.]
        self.target = [0., 0., 0.]
        if idle:
            self.front.cancel(reason)
            self.mode = 'IDLE'

    def command(self, msg, now=None):
        now = time.monotonic() if now is None else now
        def error(code, message):
            return [{'type': 'error', 'ts': stamp(), 'code': code, 'message': message}]
        if not isinstance(msg, dict) or not finite(msg.get('ts')):
            return error('INVALID_COMMAND', 'An object with a finite ts is required')
        kind = msg.get('type')
        ack = {'type': 'ack', 'ts': stamp(), 'request_type': kind, 'ok': True}
        if finite(msg.get('request_id')):
            ack['request_id'] = msg['request_id']
        if kind == 'set_demo_bypass':
            if type(msg.get('enabled')) is not bool: return error('INVALID_COMMAND','enabled must be boolean')
            if self.estop or self.mode!='PERSON_FOLLOW': return error('NOT_IN_FOLLOW','Follow mode required')
            if not msg['enabled'] and self.front.phase!='NONE': self.stop(idle=True)
            self.front.set_demo(msg['enabled'],self.t)
            return [ack]
        if kind == 'cmd_vel':
            v = [msg.get(k) for k in ('vx', 'vy', 'wz')]
            if any(not finite(x) or abs(x) > 1 for x in v):
                return error('INVALID_COMMAND', 'Velocity components must be finite and in [-1,1]')
            if any(v) and self.estop:
                return error('ESTOP_ACTIVE', 'Motion rejected while emergency stop is active')
            if any(v) and self.mode != 'MANUAL':
                return error('NOT_IN_MANUAL', 'Choose manual mode before moving')
            if not any(v): self.front.held=False
            elif self.front.held: return error('FRONT_RELEASE_REQUIRED','Release joystick first')
            self.target = v
            self.last_command = now
            if not any(v):
                self.stop(idle=self.mode=='PERSON_FOLLOW')
            return []
        if kind == 'set_mode':
            if msg.get('mode') not in MODES:
                return error('INVALID_MODE', 'Unknown or reserved mode')
            if self.estop:
                return error('ESTOP_ACTIVE', 'Clear emergency stop before changing mode')
            if msg['mode'] in ('MANUAL','PERSON_FOLLOW') and self.front.held: return error('FRONT_RELEASE_REQUIRED','Release joystick first')
            self.stop()
            self.front.cancel('mode_changed')
            self.mode = msg['mode']
            if self.mode=='PERSON_FOLLOW': self.last_ping=now
            return [ack]
        if kind == 'estop':
            self.front.cancel('estop')
            self.estop = True
            self.stop()
            return [ack]
        if kind == 'clear_estop':
            self.estop = False
            self.stop(idle=True)
            return [ack]
        if kind == 'ping':
            self.last_ping=now
            return [{'type': 'pong', 'ts': stamp(), 'id': msg.get('id')}]
        return error('UNKNOWN_TYPE', 'Unsupported command type')

    def step(self, dt, now=None):
        now = time.monotonic() if now is None else now
        self.t += dt
        if self.estop or (self.mode == 'MANUAL' and now - self.last_command > .250):
            self.stop()
        if self.mode=='PERSON_FOLLOW' and (not self.person['found'] or now-self.last_ping>=.240): self.stop(idle=True,reason='target_lost' if not self.person['found'] else 'owner_watchdog')
        target = self.target[:] if self.mode == 'MANUAL' else [0., 0., 0.]
        if not self.estop and self.mode == 'PERSON_FOLLOW' and self.person['found']:
            target = [.15, 0., math.sin(self.t * .42) * .3]
        elif not self.estop and self.mode == 'GESTURE_CONTROL':
            target = {'THUMB_UP': [.4, 0., 0.], 'FIST': [-.3, 0., 0.],
                      'POINT_LEFT': [0., -.4, 0.], 'POINT_RIGHT': [0., .4, 0.]}.get(self.gesture, [0., 0., 0.])
        if self.mode!='PERSON_FOLLOW' and self.front.enabled: self.front.cancel('mode_exit')
        protected,abort=self.front.step(self.t,target,self.mode=='PERSON_FOLLOW')
        if self.mode in ('MANUAL','PERSON_FOLLOW','GESTURE_CONTROL'):
            target=protected
            if abort: self.stop(idle=True,reason=self.front.reason)
            if not any(target): self.velocity=[0.,0.,0.]
        k = min(1., dt * 6)
        self.velocity = [v + (a - v) * k for v, a in zip(self.velocity, target)]
        self.yaw = (self.yaw + self.velocity[2] * 90 * dt + 180) % 360 - 180
        w = int(76 + math.sin(self.t * .3) * 10)
        h = int(w * 2.2)
        self.person = {'found': int(self.t) % 33 < 30,
                       'x': int(max(0, min(320-w, 160 + math.sin(self.t * .42) * 85 - w/2))),
                       'y': int(max(0, min(240-h, 129 + math.sin(self.t * .7) * 10 - h/2))),
                       'w': w, 'h': h, 'confidence': round(.91 + math.sin(self.t) * .05, 2)}
        self.gesture = GESTURES[int(self.t / 3.4) % len(GESTURES)]
        self.hr = round(74 + math.sin(self.t * .13) * 5)
        self.spo2 = round(98 + math.sin(self.t * .07))
        self.sqi = .88 + math.sin(self.t * .1) * .08
        cycle = self.t % 28
        self.finger = cycle < 24
        self.health_state = 'NO_FINGER' if not self.finger else 'ACQUIRING' if cycle < 1.5 else 'MEASURING' if self.mode == 'HEALTH_CHECK' else 'VALID'

    def telemetry(self):
        moving = any(abs(v) > .02 for v in self.velocity)
        status = 'ESTOP' if self.estop else 'DRIVING' if moving else 'READY' if self.mode == 'MANUAL' else 'IDLE'
        if not self.estop and self.mode == 'PERSON_FOLLOW':
            status = 'TRACKING' if self.person['found'] else 'SEARCHING'
        if not self.estop and self.mode == 'HEALTH_CHECK':
            status = 'MEASURING'
        return {'type': 'telemetry', 'ts': stamp(), 'front':self.front.telemetry(self.t), 'connection': {'camera': True, 'main_mcu': True, 'simulated': True},
                'robot': {'mode': 'ESTOP' if self.estop else self.mode, 'state': status, 'estop': self.estop,
                          'battery_pct': max(5, round(87 - self.t * .004)), **dict(zip(('vx', 'vy', 'wz'), (round(v, 3) for v in self.velocity)))},
                'imu': {'yaw_deg': round(self.yaw, 1), 'pitch_deg': round(math.sin(self.t) * .4, 1), 'roll_deg': round(math.cos(self.t) * .3, 1)},
                'vision': {'image_width': 320, 'image_height': 240, 'ai_fps': round(5.8 + math.sin(self.t) * .5, 1),
                           'person': self.person, 'gesture': {'label': self.gesture, 'confidence': .91, 'stable': True}},
                'health': {'hr_bpm': self.hr, 'spo2_pct': self.spo2, 'sqi': round(self.sqi, 2), 'finger_detected': self.finger, 'state': self.health_state}}

    def ppg(self):
        samples = []
        for _ in range(5):
            self.phase = (self.phase + self.hr / 60 / 50) % 1
            def g(c, w):
                return math.exp(-(self.phase-c)**2 / (2*w*w))
            shaped = g(.17,.055) - .14*g(.33,.028) + .34*g(.44,.075) if self.finger else 0
            samples.append(round(18400 + 2600 * (shaped + .09*math.sin(self.t*1.5) + random.uniform(-.01,.01))))
        return {'type': 'ppg_batch', 'ts': stamp(), 'sample_rate_hz': 50, 'samples': samples}

    def jpeg(self):
        image = Image.new('RGB', (640,480), '#565f60')
        d = ImageDraw.Draw(image)
        d.rectangle((65,45,175,280), fill='#a7b7ab')
        d.line((120,45,120,280), fill='#6b8074', width=3)
        d.polygon([(0,345),(640,330),(640,480),(0,480)], fill='#39484a')
        for x in range(-300,1100,180):
            d.line((350,330,x,480), fill='#4a595b', width=1)
        d.rounded_rectangle((480,260,590,326), radius=4, fill='#303f43')
        p = self.person
        if p['found']:
            x,y,w,h = [p[k]*2 for k in ('x','y','w','h')]
            d.ellipse((x,y+h-5,x+w,y+h+6), fill='#293638')
            d.ellipse((x+w*.33,y,x+w*.67,y+h*.20), fill='#b3b2a5')
            d.rounded_rectangle((x+w*.27,y+h*.21,x+w*.73,y+h*.66), radius=18, fill='#324551')
            for a,b in ((.1,.24),(.76,.9)):
                d.rounded_rectangle((x+w*a,y+h*.24,x+w*b,y+h*.61), radius=10, fill='#344957')
            for a,b in ((.29,.48),(.53,.72)):
                d.rounded_rectangle((x+w*a,y+h*.63,x+w*b,y+h*.98), radius=8, fill='#283e4a')
        data = io.BytesIO(); image.save(data, format='JPEG', quality=80); return data.getvalue()

class Server:
    def __init__(self):
        self.sim = RobotSim()
        self.clients = set()
        self.owner = None
        self.running = True

    async def ws(self, request):
        # A web page on another origin cannot claim the local robot's controls.
        origin = request.headers.get('Origin')
        if origin and origin != f'{request.scheme}://{request.host}':
            raise web.HTTPForbidden(text='Use the console served by this server')
        ws = web.WebSocketResponse(max_msg_size=65536, heartbeat=10, timeout=1)
        await ws.prepare(request); self.clients.add(ws)
        await ws.send_json(self.sim.telemetry())
        try:
            async for frame in ws:
                if frame.type != WSMsgType.TEXT:
                    continue
                try:
                    msg = json.loads(frame.data)
                except (ValueError, TypeError):
                    await ws.send_json({'type':'error','ts':stamp(),'code':'INVALID_JSON','message':'Malformed JSON'}); continue
                kind = msg.get('type') if isinstance(msg, dict) else None
                claims = kind in ('set_mode','clear_estop','set_demo_bypass') or (kind == 'cmd_vel' and any(msg.get(k) for k in ('vx','vy','wz')))
                if claims and self.owner is not None and self.owner is not ws:
                    await ws.send_json({'type':'error','ts':stamp(),'code':'CONTROL_BUSY','message':'Another browser owns motion control'}); continue
                # Non-owner zero commands must not interfere with the owner's motion.
                if kind == 'cmd_vel' and self.owner is not None and self.owner is not ws:
                    continue
                if kind=='ping' and self.owner is not None and self.owner is not ws:
                    await ws.send_json({'type':'pong','ts':stamp(),'id':msg.get('id')})
                    continue
                replies = self.sim.command(msg)
                if claims and not any(r['type'] == 'error' for r in replies):
                    self.owner = ws
                for reply in replies:
                    await ws.send_json(reply)
                if kind in ('set_mode','estop','clear_estop','set_demo_bypass'):
                    await self.broadcast(self.sim.telemetry())
        finally:
            self.clients.discard(ws)
            if self.owner is ws:
                self.owner = None; self.sim.stop(idle=True,reason='network_down')
        return ws

    async def broadcast(self, msg):
        async def deliver(ws):
            try:
                payload=msg
                if msg.get('type')=='telemetry':
                    payload={**msg,'robot':{**msg['robot'],'control_allowed':self.owner is None or self.owner is ws}}
                await asyncio.wait_for(ws.send_json(payload), .15)
            except (ConnectionError, asyncio.TimeoutError, RuntimeError):
                await ws.close()
        await asyncio.gather(*(deliver(ws) for ws in tuple(self.clients)))

    async def stream(self, request):
        response = web.StreamResponse(headers={'Content-Type':'multipart/x-mixed-replace; boundary=frame','Cache-Control':'no-store'})
        await response.prepare(request)
        try:
            while self.running:
                data = self.sim.jpeg()
                await response.write(b'--frame\r\nContent-Type: image/jpeg\r\nContent-Length: '+str(len(data)).encode()+b'\r\n\r\n'+data+b'\r\n')
                await asyncio.sleep(.1)
        except (ConnectionError, asyncio.CancelledError):
            pass
        return response

    async def physics(self):
        previous = time.monotonic()
        while True:
            now = time.monotonic(); self.sim.step(now-previous, now); previous = now
            await asyncio.sleep(.02)

    async def publisher(self):
        while True:
            await self.broadcast(self.sim.telemetry()); await self.broadcast(self.sim.ppg())
            await asyncio.sleep(.1)

    async def lifecycle(self, app):
        tasks = [asyncio.create_task(self.physics()), asyncio.create_task(self.publisher())]
        yield
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        await asyncio.gather(*(ws.close() for ws in tuple(self.clients)))

    async def shutdown(self, app):
        self.running = False
        self.sim.stop(idle=True)
        await asyncio.gather(*(ws.close() for ws in tuple(self.clients)))

async def static(request):
    name = request.match_info.get('name', '') or 'index.html'
    path = (ROOT / name).resolve()
    allowed = path == ROOT / 'index.html' or any(path.is_relative_to(ROOT / folder) for folder in ('css','js','assets','docs')) or path == ROOT / 'README.md'
    if not allowed or not path.is_file() or any(part.startswith('.') for part in Path(name).parts):
        raise web.HTTPNotFound()
    return web.FileResponse(path, headers={'Cache-Control':'no-cache','X-Content-Type-Options':'nosniff'})

def create_app():
    server = Server(); app = web.Application()
    app.router.add_get('/ws', server.ws)
    app.router.add_get('/stream', server.stream)
    app.router.add_get('/{name:.*}', static)
    app.cleanup_ctx.append(server.lifecycle)
    app.on_shutdown.append(server.shutdown)
    return app

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8080)
    args = parser.parse_args()
    web.run_app(create_app(), host=args.host, port=args.port, shutdown_timeout=2)
