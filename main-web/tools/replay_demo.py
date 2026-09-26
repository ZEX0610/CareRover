#!/usr/bin/env python3
"""Replay packaged evidence through the actual C++ signal processors; never drives hardware."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]

def replay(path,exe):
    commands=[];skipped=0;persons=[];raw_start=None
    for line in path.read_text().splitlines():
        record=json.loads(line)
        try: data=json.loads(record['text'])
        except (ValueError,KeyError): skipped+=1;continue
        ms=record['elapsed_ms']+1
        kind=data.get('type')
        if kind=='gesture':
            box=data.get('box',[0]*4);label=data.get('source_label','no_hand')
            commands.append(' '.join(map(str,['G',ms,int(data.get('hand',False)),label,round(data.get('source_score',0)*1000),*box])))
        elif kind=='ppg_raw':
            # Retain actual device sample timing, including burst reads.
            if raw_start is None:raw_start=data['ms']
            commands.append(f"R {data['ms']-raw_start+1} {data['red']} {data['ir']}")
        elif kind=='health':
            commands.append(' '.join(map(str,['H',ms,int(data.get('finger_present',False)),int(data.get('algorithm_hr_valid',False)),data.get('candidate_hr') or 0,int(data.get('algorithm_spo2_valid',False)),data.get('candidate_spo2') or 0])))
        elif kind=='person':persons.append(data)
    result=subprocess.run([str(exe)],input='\n'.join(commands)+'\n',text=True,capture_output=True,check=True)
    rows=[line.split() for line in result.stdout.splitlines()]
    gestures=[r for r in rows if r[0]=='G'];raw=[r for r in rows if r[0]=='R'];health=[r for r in rows if r[0]=='H']
    summary={'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'unparseable_text_records':skipped,
             'gesture_frames':len(gestures),'display_accepted':sum(r[3]=='1' for r in gestures),'actions':{str(i):sum(r[2]==str(i) for r in gestures) for i in range(1,5)},
             'raw_windows':len(raw),'raw_hr_values':[int(r[3]) for r in raw if r[2]=='1'],
             'raw_first_hr_ms':next((int(r[1]) for r in raw if r[2]=='1'),None),
             'health_candidate_replay_hr_valid':sum(r[2]=='1' for r in health),
             'person_frames':len(persons),'person_found':sum(p.get('found',False) for p in persons),
             'person_limit':'Packaged person summaries omit bbox; timing/count evidence only, not bbox or physical motion validation.'}
    return summary

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('evidence',type=Path);parser.add_argument('--output',type=Path);args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='carerover-replay-') as d:
        exe=Path(d)/'replay'
        subprocess.run(['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-DCAREROVER_TUNING_PROFILE=1','-Ifirmware/main_wireless','tests/firmware/demo_replay.cpp','-o',str(exe)],cwd=ROOT,check=True)
        reports=[replay(p,exe) for p in sorted(args.evidence.glob('*.jsonl'))]
    output=json.dumps({'profile':'DEMO_BALANCED','scope':'Offline C++ replay; no device access','reports':reports},ensure_ascii=False,indent=2)
    if args.output:args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(output+'\n')
    print(output)
    static=next((r for r in reports if r['file']=='final-motion-action-guard-static-60s.jsonl'),None)
    if static and any(static['actions'].values()):raise SystemExit('FAIL: static background generated an action')
    raw=next((r for r in reports if r['file']=='final-health-raw-finger-30s.jsonl'),None)
    if raw and any(v<45 or v>150 for v in raw['raw_hr_values']):raise SystemExit('FAIL: HR outside physiological candidate bounds')
if __name__=='__main__':main()
