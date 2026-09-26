"""Synthetic front obstacle fixture, never physical calibration. Matches browser demo."""
import math

class FrontSim:
    def __init__(self):
        self.distance=120
        self.phase='NONE'
        self.enabled=self.held=self.valid=self.blocked=False
        self.reason='boot'
        self.at=self.right_at=0
        self.sample_at=-1
        self.near=self.clear=0
        self.samples=[]
        self.raw=self.filtered=120

    def cancel(self, reason):
        self.phase='NONE'
        self.enabled=False
        self.reason=reason

    def set_demo(self, enabled, t):
        self.cancel('demo_changed')
        self.enabled=enabled
        self.at=t

    def enter(self, phase, t):
        self.phase=phase
        self.at=t
        if phase=='RIGHT': self.right_at=t

    def step(self, t, target, following):
        zero=[0.,0.,0.]
        distance=self.distance
        if self.enabled and distance==120:
            distance=18 if self.phase in ('NONE','HALT') or self.phase=='RIGHT' and t-self.right_at<.6 else 120
        if t-self.sample_at>=.07:
            self.sample_at=t
            self.valid=type(distance) in (int,float) and math.isfinite(distance) and 2<=distance<=400
            self.raw=distance
            if self.valid:
                self.samples=(self.samples+[distance])[-3:]
                self.filtered=sorted(self.samples)[len(self.samples)//2]
                self.near=self.near+1 if distance<=20 else 0
                self.clear=self.clear+1 if distance>=70 else 0
                if distance<=20: self.blocked=True
                elif self.clear>=3: self.blocked=False
            else:
                self.samples=[]
                self.near=self.clear=0
        if not self.valid:
            self.cancel('front_unknown')
            return zero,True
        if following and self.phase=='NONE' and (target[0]>0 or self.enabled) and self.blocked:
            if not self.enabled or self.raw>20:
                self.cancel('front_obstacle')
                return zero,True
            self.enter('HALT',t)
        if self.phase=='HALT' and self.near>=3 and t-self.at>=.21: self.enter('RIGHT',t)
        elif self.phase=='RIGHT' and self.clear>=3: self.enter('MARGIN',t)
        elif self.phase=='MARGIN':
            if self.raw<70:
                self.phase='RIGHT'
                self.clear=0
            elif t-self.at>=.4: self.enter('PASS',t)
        elif self.phase=='PASS':
            if self.raw<50:
                self.cancel('bypass_blocked')
                return zero,True
            if t-self.at>=.8: self.enter('REACQUIRE',t)
        elif self.phase=='REACQUIRE' and t-self.at>=.3: self.cancel('bypass_complete')
        if self.phase in ('RIGHT','MARGIN') and t-self.right_at>=4:
            self.cancel('bypass_timeout')
            return zero,True
        if self.phase in ('HALT','REACQUIRE') and t-self.at>=3:
            self.cancel('front_unstable')
            return zero,True
        if self.phase in ('HALT','REACQUIRE'): return zero,False
        if self.phase in ('RIGHT','MARGIN'): return [0.,.15,0.],False
        if self.phase=='PASS': return [.15,0.,0.],False
        if self.held: return zero,False
        if target[0]>0 and self.blocked:
            self.held=True
            self.reason='front_obstacle'
            return zero,False
        target=target[:]
        if target[0]>0: target[0]*=max(0,min(1,(min(self.raw,self.filtered)-20)/30))
        return target,False

    def telemetry(self,t):
        status='UNKNOWN' if not self.valid else 'STOPPED' if self.held else 'BYPASS' if self.phase!='NONE' else 'BLOCKED' if self.blocked else 'SLOW' if self.filtered<50 else 'WARN' if self.filtered<60 else 'CLEAR'
        return dict(enabled=True,ready=True,valid=self.valid,distance_cm=self.filtered if self.valid else None,
            age_ms=max(0,(t-self.sample_at)*1000),demo_ready=True,demo_enabled=self.enabled,
            release_required=self.held,phase=self.phase,stop_reason=self.reason,status=status)
