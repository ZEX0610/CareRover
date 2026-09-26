#!/usr/bin/env python3
"""CAM dependencies/build/flash, read-only serial evidence, and source handoff."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile

try:
    from .carerover import read_log_text, source_info
except ImportError:
    from carerover import read_log_text, source_info

# Preserve an English SUBST path on Windows; resolve() expands it back to the
# Unicode source path and can make the ESP-IDF linker fail.
ROOT=Path(__file__).absolute().parents[1]
BUILD=ROOT/'build'
LOCK=ROOT/'config/cam-toolchain.lock.json'
CAM_BOARD_SHARED=ROOT/'firmware/cam_tracking/board_exact_shared'
CAM_BOARD_SHARED_NAMES=('vision_protocol.h','box_track.h','demo_tuning.h')
def run(args,**kw):
    return subprocess.run([str(a) for a in args],cwd=ROOT,check=True,**kw)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write_json(p,value):
    p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def windows_idf_build_settings(base_env=None, platform_name=None):
    """Return process-local settings needed by ESP-IDF in a Unicode Windows path."""
    env=dict(os.environ if base_env is None else base_env)
    definitions=[]
    if (os.name if platform_name is None else platform_name)=='nt':
        # Kconfig helper files are UTF-8, while the Windows locale may default
        # to GBK. ccache 4.11 also fails to parse Unicode prefix-map paths.
        env['PYTHONUTF8']='1'
        env['IDF_CCACHE_ENABLE']='0'
        definitions.append('-DCCACHE_ENABLE=0')
    return env,definitions

def windows_cam_build_directory(package_out, platform_name=None, temp_root=None):
    """Keep generated IDF objects off Windows paths that may resolve to Unicode."""
    package_out=Path(package_out)
    if (os.name if platform_name is None else platform_name)=='nt':
        root=Path(tempfile.gettempdir() if temp_root is None else temp_root)/'CareRover-cam-build'
        staged=root/package_out.name
        if any(ord(c)>=128 for c in str(staged)):
            raise ValueError(f'Windows CAM staging path must be ASCII: {staged}')
        return staged
    return package_out

def console_safe(value, encoding=None):
    """Make diagnostic text printable even when the Windows console uses GBK."""
    selected=encoding or getattr(sys.stdout,'encoding',None) or 'utf-8'
    return str(value).encode(selected,errors='replace').decode(selected,errors='replace')

def checkout(key,destination,recursive=False):
    spec=json.loads(LOCK.read_text())[key]
    if not destination.exists():
        args=['git','clone','--depth','1','--branch',spec['tag']]
        if recursive:args+=['--recursive','--shallow-submodules']
        run(args+[spec['url'],destination])
    actual=run(['git','-C',destination,'rev-parse','HEAD'],capture_output=True,text=True).stdout.strip()
    if actual!=spec['commit']:raise ValueError(f'{destination}: expected {spec["commit"]}, found {actual}; existing checkout left intact')
    if run(['git','-C',destination,'status','--porcelain'],capture_output=True,text=True).stdout.strip():raise ValueError(f'Dependency has local edits: {destination}')
def prepare(args):
    checkout('esp_dl',BUILD/'dependencies/esp-dl')
    if args.idf:checkout('esp_idf',BUILD/'toolchains/esp-idf-5.3.4',True)
    print('Pinned source dependencies ready. Use ESP-IDF 5.3.4 install/export scripts for your OS.')
def cam_profile(path):
    p=json.loads(Path(path).read_text(encoding='utf-8'))
    if p.get('verification') not in {'compile_only','windows_baseline_confirmed'}:raise ValueError('Unverified CAM profile')
    if (p.get('chip'),p.get('flash_size'),p.get('psram'))!=('esp32s3','8MB','opi'):raise ValueError('CAM requires esp32s3 / 8MB flash / OPI PSRAM; do not use main-controller profile')
    if not isinstance(p.get('ssid'),str) or not 1<=len(p['ssid'].encode())<=32:raise ValueError('Invalid SSID')
    if not isinstance(p.get('password'),str) or not 8<=len(p['password'])<=63:raise ValueError('Invalid WPA2 password')
    if p['verification']!='compile_only' and (p['ssid']=='CareRover-XXXX' or p['password']=='COMPILE_CHECK_ONLY'):raise ValueError('Device build needs actual private AP credentials')
    return p

def cam_board_shared_sources():
    sources=[CAM_BOARD_SHARED/name for name in CAM_BOARD_SHARED_NAMES]
    missing=[str(path) for path in sources if not path.is_file()]
    if missing:raise ValueError('Missing board-exact CAM shared source: '+', '.join(missing))
    return sources

def cam_build(args):
    profile=cam_profile(args.profile)
    board_shared=cam_board_shared_sources()
    checkout('esp_dl',BUILD/'dependencies/esp-dl')
    idf=Path(os.environ.get('IDF_PATH',''))/'tools/idf.py'
    if not idf.is_file():raise ValueError('Activate ESP-IDF 5.3.4 first (export.sh / export.bat).')
    version=run([sys.executable,idf,'--version'],capture_output=True,text=True).stdout
    if 'v5.3.4' not in version:raise ValueError('Expected ESP-IDF v5.3.4: '+version)
    tuning_profile=getattr(args,'tuning_profile','SAFE_BASELINE')
    tuning_id={'SAFE_BASELINE':0,'DEMO_BALANCED':1,'DIAGNOSTIC_RAW':2}[tuning_profile]
    out=BUILD/f'cam-{args.variant}-{tuning_profile.lower()}-{profile["verification"]}';out.mkdir(parents=True,exist_ok=True)
    work_out=windows_cam_build_directory(out)
    work_project=ROOT/'firmware/cam_tracking'
    work_defaults=None
    if work_out!=out:
        workspace=work_out.with_name(work_out.name+'-workspace')
        if work_out.exists():shutil.rmtree(work_out)
        if workspace.exists():shutil.rmtree(workspace)
        work_project=workspace/'firmware/cam_tracking'
        shutil.copytree(ROOT/'firmware/cam_tracking',work_project)
        shutil.copytree(ROOT/'firmware/main_wireless',workspace/'firmware/main_wireless')
        # Recreate the exact shared-header layout used for the installed CAM
        # image without replacing the newer main-controller copies.
        for source in board_shared:
            shutil.copyfile(source,workspace/'firmware/main_wireless'/source.name)
        staged_esp_dl=workspace/'build/dependencies/esp-dl'
        for relative in ['esp-dl','models/hand_detect','models/hand_gesture_recognition','models/human_face_detect']:
            shutil.copytree(BUILD/'dependencies/esp-dl'/relative,staged_esp_dl/relative)
    work_out.mkdir(parents=True,exist_ok=True)
    defaults=out/'variant.defaults'
    defaults_text='\n'.join([
        'CONFIG_CAREROVER_TUNING_PROFILE='+str(tuning_id),
        'CONFIG_CAREROVER_FACE='+('n' if args.variant=='gesture' else 'y'),
        'CONFIG_CAREROVER_VIDEO='+('y' if args.variant in {'stream','pico'} else 'n'),
        'CONFIG_CAREROVER_PICO_FACE='+('y' if args.variant=='pico' else 'n'),
        'CONFIG_FLASH_ESPDET_PICO_224_224_FACE='+('y' if args.variant=='pico' else 'n'),
        'CONFIG_FLASH_HUMAN_FACE_DETECT_MSRMNP_S8_V1='+('n' if args.variant=='pico' else 'y'),
        'CONFIG_CAREROVER_WIFI_SSID='+json.dumps(profile['ssid']),
        'CONFIG_CAREROVER_WIFI_PASSWORD='+json.dumps(profile['password'])])+'\n'
    defaults.write_text(defaults_text,encoding='utf-8')
    work_defaults=defaults
    if work_project!=ROOT/'firmware/cam_tracking':
        work_defaults=work_project/'variant.defaults';work_defaults.write_text(defaults_text,encoding='utf-8')
    # Generated SDK config is an output, never a user-maintained input.
    sdk=work_out/'sdkconfig'
    if sdk.exists():sdk.unlink()
    frozen=ROOT/'config/cam-dependencies.lock'
    if frozen.exists():
        lock_root=workspace if work_project!=ROOT/'firmware/cam_tracking' else ROOT
        (work_project/'dependencies.lock').write_text(frozen.read_text().replace('${PROJECT_ROOT}',lock_root.as_posix()))
    build_env,build_definitions=windows_idf_build_settings()
    command=[sys.executable,idf,'-C',work_project,'-B',work_out,
      '-DIDF_TARGET=esp32s3',f'-DSDKCONFIG={sdk}',f'-DSDKCONFIG_DEFAULTS={work_project/"sdkconfig.defaults"};{work_defaults}',
      *build_definitions,'build']
    with (out/'compile.log').open('w',encoding='utf-8') as log:
        try:run(command,stdout=log,stderr=subprocess.STDOUT,env=build_env)
        except subprocess.CalledProcessError:
            print(console_safe(read_log_text(out/'compile.log')[-9000:]));raise
    flash_args=work_out/'flasher_args.json'
    if work_out!=out:
        shutil.copy2(flash_args,out/'flasher_args.json')
        shutil.copy2(sdk,out/'sdkconfig')
    flash=json.loads(flash_args.read_text())['flash_files']
    expected={0x0:0x8000,0x8000:0x1000,0x10000:0x700000}
    if set(map(lambda x:int(x,0),flash))!=set(expected):raise ValueError('Unexpected CAM partition addresses')
    files={}
    for address,name in flash.items():
        source=work_out/name;p=out/name;p.parent.mkdir(parents=True,exist_ok=True)
        if source!=p:shutil.copy2(source,p)
        if p.stat().st_size>expected[int(address,0)]:raise ValueError('CAM binary exceeds partition')
        files[name]={'address':int(address,0),'sha256':digest(p),'bytes':p.stat().st_size}
    write_json(out/'manifest.json',{'target':'cam','variant':args.variant,'tuning_profile':tuning_profile,'verification':profile['verification'],
      'flash_size':'8MB','idf':'5.3.4','dependencies':json.loads(LOCK.read_text()),'files':files,
      'hardware_status':'NOT RUN',
      **source_info(),
      # Track the board-exact CAM copies rather than the independently evolving
      # main-controller headers that they replace in the staged workspace.
      'source_files':{str(p.relative_to(ROOT)):digest(p) for p in [*sorted((ROOT/'firmware/cam_tracking/main').glob('*')),*board_shared,ROOT/'firmware/cam_tracking/CMakeLists.txt',ROOT/'firmware/cam_tracking/partitions.csv',ROOT/'firmware/cam_tracking/sdkconfig.defaults'] if p.is_file()},
      'sdkconfig_sha256':digest(out/'sdkconfig')})
    print(console_safe(read_log_text(out/'compile.log')[-1700:]));print('CAM package:',out,'\nNO DEVICE WAS FLASHED.')

def calibration_build(args):
    # Reuse the single maintained PWM/kinematics sources in a staged Arduino sketch.
    from carerover import cli, validate_profile
    profile=json.loads(Path(args.profile).read_text());validate_profile(profile)
    cores=json.loads(run([cli(),'core','list','--json'],capture_output=True,text=True).stdout)
    installed=next((c.get('installed_version') for c in cores.get('platforms',[]) if c.get('id')=='esp32:esp32'),None)
    if installed!=profile['core_version']:raise ValueError('Installed Arduino core differs from calibration profile')
    out=BUILD/'calibration';sketch=out/'motion_calibration';sketch.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'firmware/motion_calibration/motion_calibration.ino',sketch/'motion_calibration.ino')
    for name in ['continuous_servo_drive.h','continuous_servo_drive.cpp','omni_kinematics.h','motion_layout.h','partitions.csv']:
        shutil.copyfile(ROOT/'firmware/main_wireless'/name,sketch/name)
    with (out/'compile.log').open('w',encoding='utf-8') as log:
        try:run([cli(),'compile','--fqbn',profile['fqbn'],'--output-dir',out/'binaries',sketch],stdout=log,stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError:print(read_log_text(out/'compile.log')[-6000:]);raise
    print('Calibration compiled:',out,'; upload only after board verification and backup.')

def esptool_operation(version_output, operation):
    """Map esptool 5's hyphenated command names to IDF 5.3's esptool 4."""
    match=re.search(r'\bv(\d+)\.',version_output)
    if not match:raise ValueError('Unrecognized esptool version: '+version_output.strip())
    return operation.replace('-','_') if int(match.group(1))<5 else operation

def esptool_write_flash_flags(version_output):
    legacy=esptool_operation(version_output,'write-flash')=='write_flash'
    suffix='_' if legacy else '-'
    return [f'--flash{suffix}mode','dio',f'--flash{suffix}freq','80m',f'--flash{suffix}size','8MB']

def validated_existing_cam_backup(path, expected_sha256, backup_root=None):
    """Reuse a complete, hash-verified read of this board without repeating it."""
    root=(BUILD/'backups' if backup_root is None else Path(backup_root)).resolve()
    backup=Path(path).resolve()
    if not backup.is_relative_to(root) or not backup.is_file():
        raise ValueError('Existing CAM backup must be a file inside build/backups')
    if backup.stat().st_size!=0x800000 or not expected_sha256 or digest(backup).lower()!=expected_sha256.lower():
        raise ValueError('Existing CAM backup size/SHA-256 mismatch')
    return backup

def cam_flash(args):
    out=Path(args.package).resolve();m=json.loads((out/'manifest.json').read_text())
    if m.get('target')!='cam' or m.get('verification')!='windows_baseline_confirmed' or m.get('flash_size')!='8MB':raise ValueError('Only a verified 8MB CAM device package can be flashed')
    expected={0:0x8000,0x8000:0x1000,0x10000:0x700000}
    if len(m['files'])!=3 or {v['address'] for v in m['files'].values()}!=set(expected):raise ValueError('Unexpected CAM addresses')
    pairs=[]
    for name,value in sorted(m['files'].items(),key=lambda x:x[1]['address']):
        p=(out/name).resolve()
        if not p.is_relative_to(out) or digest(p)!=value['sha256'] or p.stat().st_size>expected[value['address']]:raise ValueError('CAM file/size/checksum mismatch')
        pairs += [hex(value['address']),p]
    command=[sys.executable,'-m','esptool','--chip','esp32s3','--port',args.port,'--baud',str(getattr(args,'baud',115200))]
    version=run([sys.executable,'-m','esptool','version'],capture_output=True,text=True).stdout
    identity=run(command+[esptool_operation(version,'flash-id')],capture_output=True,text=True).stdout;print(identity)
    if '8MB' not in identity:raise ValueError('Connected board is not the configured 8MB CAM')
    if getattr(args,'existing_backup',None):
        backup=validated_existing_cam_backup(args.existing_backup,getattr(args,'backup_sha256',None))
        print('Reusing verified full CAM backup:',backup)
    else:
        backup=BUILD/'backups'/f'cam-{time.strftime("%Y%m%d-%H%M%S")}.bin';backup.parent.mkdir(parents=True,exist_ok=True)
        run(command+[esptool_operation(version,'read-flash'),'0','0x800000',backup]);write_json(backup.with_suffix('.json'),{'sha256':digest(backup),'bytes':backup.stat().st_size})
    if backup.stat().st_size!=0x800000:raise ValueError('Incomplete backup')
    run(command+[esptool_operation(version,'write-flash')]+esptool_write_flash_flags(version)+pairs)

def crc8(body):
    crc=0
    for b in body.encode('ascii'):
        crc^=b
        for _ in range(8):crc=((crc<<1)^7)&255 if crc&128 else (crc<<1)&255
    return crc

def analyze(path):
    result={'status':'CAPTURED','lines':0,'uart_valid':0,'uart_bad_crc':0,'person_found':0,'person_missing':0,'metrics':[],'max_person_gap_ms':0}
    previous=None
    for line in Path(path).read_text(encoding='utf-8',errors='replace').splitlines():
        result['lines']+=1
        try:record=json.loads(line);text=record.get('text',line);at=record.get('elapsed_ms')
        except (ValueError,AttributeError):text=line;at=None
        if text.startswith(('@P,','@G,')):
            try:
                body,checksum=text[1:].rsplit('*',1)
                if len(checksum)!=2 or crc8(body)!=int(checksum,16):raise ValueError('crc')
                result['uart_valid']+=1
                if text.startswith('@P,'):
                    fields=body.split(',');result['person_found' if fields[3]=='1' else 'person_missing']+=1
                    if at is not None and previous is not None:result['max_person_gap_ms']=max(result['max_person_gap_ms'],at-previous)
                    previous=at
            except (ValueError,IndexError,UnicodeError):result['uart_bad_crc']+=1
        try:
            obj=json.loads(text)
            if obj.get('type') in {'cam_metrics','wireless_status','imu_status','motion_status','vision_link','front_status'}:result['metrics'].append(obj)
        except (ValueError,AttributeError):pass
    result['uart_validation']='CRC only; field/range/sequence acceptance is verified by firmware tests'
    result['hardware_acceptance']='NOT EVALUATED: capture alone does not prove physical motion or latency'
    return result

def capture(args):
    import serial
    out=Path(args.output);out.parent.mkdir(parents=True,exist_ok=True)
    # Configure before open to avoid an implicit reset pulse.  An explicit reset only
    # toggles EN through RTS; DTR remains inactive so GPIO0 stays out of download mode.
    port=serial.Serial(port=None,baudrate=115200,timeout=.1);port.dtr=False;port.rts=False;port.port=args.port
    with port,out.open('w',encoding='utf-8') as log:
        if getattr(args,'reset',False):
            port.dtr=False;port.rts=True;time.sleep(.12);port.rts=False
        began=time.monotonic()
        while time.monotonic()-began<args.seconds:
            raw=port.readline(4096)
            if raw:log.write(json.dumps({'elapsed_ms':round((time.monotonic()-began)*1000),'text':raw.decode('utf-8',errors='replace').strip()},ensure_ascii=False)+'\n')
    write_json(out.with_suffix('.summary.json'),analyze(out))

def release(args):
    out=Path(args.output).resolve();out.parent.mkdir(parents=True,exist_ok=True)
    allowed=['firmware','js','css','assets','config','docs','tests','tools','mock','hardware','.github']
    files=[ROOT/'index.html',ROOT/'README.md',ROOT/'package.json']
    for name in ['WINDOWS_START_HERE.md','AGENTS.md']:
        if (ROOT/name).exists():files.append(ROOT/name)
    if getattr(args,'include_reference',False):
        files += [p for p in (ROOT/'CareRover_Tracking_Motion_Handoff_2026-09-11').rglob('*') if p.is_file()]
    for directory in allowed:
        for p in (ROOT/directory).rglob('*'):
            if not p.is_file():continue
            if any(x in p.parts for x in ['managed_components','__pycache__','build','.venv']):continue
            if p.name in {'wifi_secrets.h','front_config.local.h','sdkconfig','sdkconfig.old','build_version.h'} or p.name.endswith('.local.json') or p.suffix in {'.log','.pyc'}:continue
            files.append(p)
    files=sorted(set(files))
    evidence=sorted(p for p in (ROOT/'output/evidence').rglob('*') if p.is_file())
    manifest={str(p.relative_to(ROOT)):digest(p) for p in files}
    evidence_manifest={str(Path('evidence')/p.relative_to(ROOT/'output/evidence')):digest(p) for p in evidence}
    with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
        for p in files:z.write(p,str(p.relative_to(ROOT)))
        for p in evidence:z.write(p,str(Path('evidence')/p.relative_to(ROOT/'output/evidence')))
        z.writestr('EXPORT_INFO.json',json.dumps(source_info(),indent=2)+'\n')
        z.writestr('SOURCE_MANIFEST.json',json.dumps(manifest,indent=2)+'\n')
        z.writestr('EVIDENCE_MANIFEST.json',json.dumps(evidence_manifest,indent=2)+'\n')
    out.with_suffix(out.suffix+'.sha256').write_text(digest(out)+'  '+out.name+'\n');print(out)

def main():
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    s=sub.add_parser('prepare');s.add_argument('--idf',action='store_true')
    s=sub.add_parser('cam-build');s.add_argument('--tuning-profile',choices=['SAFE_BASELINE','DEMO_BALANCED','DIAGNOSTIC_RAW'],default='SAFE_BASELINE');s.add_argument('--variant',choices=['gesture','vision','stream','pico'],default='stream');s.add_argument('--profile',default='config/cam-board.example.json')
    s=sub.add_parser('cam-flash');s.add_argument('package');s.add_argument('--port',required=True);s.add_argument('--baud',type=int,default=115200);s.add_argument('--existing-backup');s.add_argument('--backup-sha256')
    s=sub.add_parser('calibration-build');s.add_argument('--profile',default='config/tracking-development.json')
    s=sub.add_parser('capture');s.add_argument('--port',required=True);s.add_argument('--seconds',type=float,default=120);s.add_argument('--output',required=True);s.add_argument('--reset',action='store_true',help='Reset the board through RTS after opening the capture port')
    s=sub.add_parser('analyze');s.add_argument('log')
    s=sub.add_parser('release');s.add_argument('--output',default='output/CareRover_Tracking_Software.zip');s.add_argument('--include-reference',action='store_true')
    args=p.parse_args()
    {'prepare':prepare,'cam-build':cam_build,'cam-flash':cam_flash,'calibration-build':calibration_build,'capture':capture,'analyze':lambda a:print(json.dumps(analyze(a.log),ensure_ascii=False,indent=2)),'release':release}[args.action](args)
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError) as e:print('ERROR:',e,file=sys.stderr);sys.exit(1)
