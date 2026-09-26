#!/usr/bin/env python3
"""Run host tests against the same C++ headers/drivers compiled into firmware."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='carerover-tests-') as d:
    for name in ['safety','tracking','drive','front','ultrasonic','tuning','demo','demo_balanced']:
        command=[os.environ.get('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-Ifirmware/main_wireless','-Ifirmware/cam_tracking/main']
        source_name='demo' if name=='demo_balanced' else name
        if name=='demo_balanced': command+=['-DCAREROVER_TUNING_PROFILE=1']
        if name=='ultrasonic':command+=['-Itests/fakes/ultrasonic']
        if name=='drive':command+=['-Itests/fakes','firmware/main_wireless/continuous_servo_drive.cpp']
        exe=Path(d)/(name+('.exe' if os.name=='nt' else ''))
        subprocess.run(command+[f'tests/firmware/{source_name}_test.cpp','-o',str(exe)],cwd=ROOT,check=True)
        subprocess.run([str(exe)],check=True)
