#!/usr/bin/env python3
"""Cross-platform build/verification tooling. No serial writes except explicit flash command."""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import locale
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
import urllib.request
import zipfile

# Keep a caller-provided short Windows path (for example a SUBST drive).  resolve()
# expands it back to the Unicode source path and breaks the ESP32 linker.
ROOT = Path(__file__).absolute().parents[1]
BUILD = ROOT / 'build'
FIRMWARE = ROOT / 'firmware/main_wireless'
LIBRARY = 'SparkFun MAX3010x Pulse and Proximity Sensor Library'
PARTITION_OFFSET = 0x610000
PARTITION_SIZE = 0x9E0000


def run(args, **kw):
    return subprocess.run([str(a) for a in args], check=True, cwd=ROOT, **kw)


def capture(args):
    return run(args, capture_output=True, text=True, encoding='utf-8').stdout


def source_info():
    """Archive builds retain export provenance without requiring a .git directory."""
    if (ROOT / '.git').exists():
        return {'source_commit': capture(['git', 'rev-parse', 'HEAD']).strip(),
                'source_dirty': bool(capture(['git', 'status', '--porcelain', '--untracked-files=normal']).strip())}
    metadata = ROOT / 'EXPORT_INFO.json'
    info = json.loads(metadata.read_text(encoding='utf-8')) if metadata.exists() else {}
    manifest = ROOT / 'SOURCE_MANIFEST.json'
    changed = True
    if manifest.exists():
        entries = json.loads(manifest.read_text(encoding='utf-8'))
        changed = any(not (ROOT / name).is_file() or sha(ROOT / name) != expected for name, expected in entries.items())
    return {'source_commit': info.get('source_commit', 'archive-unknown'),
            'source_dirty': bool(info.get('source_dirty', True) or changed),
            'archive_modified': changed}


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def read_log_text(path):
    """Decode tool output without hiding the original Windows compiler error."""
    data = Path(path).read_bytes()
    encodings = ('utf-8', locale.getpreferredencoding(False), 'gb18030')
    for encoding in dict.fromkeys(encodings):
        try:
            return data.decode(encoding)
        except (LookupError, UnicodeDecodeError):
            pass
    return data.decode('utf-8', errors='replace')


def console_safe(value, encoding=None):
    """Keep compiler diagnostics printable on a GBK-configured console."""
    selected = encoding or getattr(sys.stdout, 'encoding', None) or 'utf-8'
    return str(value).encode(selected, errors='replace').decode(selected, errors='replace')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def cli():
    candidates = [os.environ.get('ARDUINO_CLI'), shutil.which('arduino-cli'),
                  '/Applications/Arduino IDE.app/Contents/Resources/app/lib/backend/resources/arduino-cli']
    if os.name == 'nt':
        for parent in [Path(os.environ.get('LOCALAPPDATA', '.')) / 'Programs', Path(os.environ.get('ProgramFiles', '.'))]:
            candidates.append(str(parent / 'Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'))
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return candidate
    raise ValueError('Set ARDUINO_CLI to the IDE bundled arduino-cli executable, or install Arduino CLI.')


def runtime_files():
    files = [ROOT / 'index.html', ROOT / 'docs/protocol.md']
    for name in ['js', 'css', 'assets']:
        files += sorted(p for p in (ROOT / name).rglob('*') if p.is_file() and p.suffix in {'.js', '.css', '.png', '.svg', '.ico'})
    return files


def windows_main_build_directory(output, platform_name=None, temp_root=None):
    """Stage Arduino build outputs off Unicode Windows paths."""
    output = Path(output)
    if (os.name if platform_name is None else platform_name) == 'nt':
        root = Path(tempfile.gettempdir() if temp_root is None else temp_root) / 'CareRover-main-build'
        staged = root / output.name
        if any(ord(char) >= 128 for char in str(staged)):
            raise ValueError(f'Windows main staging path must be ASCII: {staged}')
        return staged
    return output


def content_id(files):
    h = hashlib.sha256()
    for p in sorted(files):
        h.update(p.relative_to(ROOT).as_posix().encode() + b'\0')
        h.update(p.read_bytes())
    return h.hexdigest()[:16]


def payload(destination):
    files = runtime_files()
    version = content_id(files)
    destination.mkdir(parents=True, exist_ok=True)
    # The destination is always a dedicated generated tree under build/.
    for old in destination.iterdir():
        if old.is_dir(): shutil.rmtree(old)
        else: old.unlink()
    for p in files:
        target = destination / p.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(p.read_bytes())
    index = destination / 'index.html'
    index.write_text(index.read_text(encoding='utf-8').replace('<head>', f'<head>\n<meta name="carerover-web-version" content="{version}">', 1), encoding='utf-8')
    write_json(destination / 'version.json', {'web': version, 'source_commit': source_info()['source_commit']})
    return version


def fetch_tools():
    entry = json.loads((ROOT / 'config/tools.lock.json').read_text(encoding='utf-8'))['fatfs_generator']
    target = BUILD / 'tools/esp-idf-fatfs'
    if (target / '.archive-sha256').exists() and (target / '.archive-sha256').read_text(encoding='utf-8').strip() == entry['sha256']:
        return target
    data = urllib.request.urlopen(entry['url'], timeout=120).read()
    if hashlib.sha256(data).hexdigest() != entry['sha256']:
        raise ValueError('FAT generator archive checksum mismatch')
    target.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        for member in archive.getmembers():
            marker = '/components/fatfs/'
            if not member.isfile() or marker not in member.name: continue
            name = member.name.split(marker, 1)[1]
            if '..' in Path(name).parts: raise ValueError('Unsafe archive path')
            if name.endswith('.py') and (name.startswith('fatfs_utils/') or '/' not in name):
                p = target / name
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_bytes(archive.extractfile(member).read())
    (target / '.archive-sha256').write_text(entry['sha256'])
    return target


def sensor_library(version):
    # Arduino's immutable versioned library archive, checked against a checked-in hash.
    lock = json.loads((ROOT / 'config/sensor-libraries.lock.json').read_text(encoding='utf-8'))
    if version not in lock:
        raise ValueError(f'Sensor library {version} needs a reviewed URL/checksum entry in config/sensor-libraries.lock.json')
    item = lock[version]
    destination = BUILD / 'deps' / f'sparkfun-{version}'
    if (destination / '.archive-sha256').exists() and (destination / '.archive-sha256').read_text(encoding='utf-8') == item['sha256']:
        return destination
    data = urllib.request.urlopen(item['url'], timeout=60).read()
    if hashlib.sha256(data).hexdigest() != item['sha256']: raise ValueError('Sensor library checksum mismatch')
    destination.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        for item_path in archive.infolist():
            if item_path.is_dir(): continue
            parts = Path(item_path.filename).parts[1:]
            if not parts or '..' in parts: continue
            p = destination.joinpath(*parts); p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(archive.read(item_path))
    (destination / '.archive-sha256').write_text(item['sha256'])
    return destination


def ffat_image(source, output, version):
    generator = fetch_tools()
    sys.path.insert(0, str(generator))
    from fatfs_utils import boot_sector
    from wl_fatfsgen import WLFATFS
    # Stable volume/device IDs make the generated filesystem reproducible.
    volume_id = int(version[:8], 16) or 1
    boot_sector.generate_4bytes_random = lambda: volume_id
    fs = WLFATFS(size=PARTITION_SIZE, sector_size=4096, long_names_enabled=True,
                 use_default_datetime=True, device_id=volume_id)
    fs.plain_fatfs.generate(str(source))
    fs.init_wl()
    fs.wl_write_filesystem(str(output))
    if output.stat().st_size != PARTITION_SIZE: raise ValueError('Unexpected FFat image size')


def validate_profile(profile, flashing=False):
    if profile.get('verification') not in {'compile_only', 'windows_baseline_confirmed'}:
        raise ValueError('Windows baseline configuration is still pending; use development.json only for compile checks.')
    if flashing and profile.get('verification') != 'windows_baseline_confirmed':
        raise ValueError('COMPILE-ONLY PROFILE: flashing is forbidden')
    for key in ['fqbn', 'core_version', 'sensor_library_version']:
        if not isinstance(profile.get(key), str) or not profile[key]: raise ValueError(f'Missing {key}')
    if profile.get('flash_size') != '16MB' or profile.get('wl_sector_size') != 4096:
        raise ValueError('This baseline requires 16MB Flash and 4096-byte wear-levelling sectors')
    if profile.get('flash_mode') != 'dio' or profile.get('flash_freq') != '80m':
        raise ValueError('Flash settings differ from the preserved baseline; review before changing them')


def build(args):
    profile = json.loads(Path(args.profile).read_text(encoding='utf-8'))
    validate_profile(profile)
    arduino = cli()
    cores = json.loads(capture([arduino, 'core', 'list', '--json']))
    platform = next((p for p in cores.get('platforms', []) if p.get('id') == 'esp32:esp32'), None)
    if not platform or platform.get('installed_version') != profile['core_version']:
        raise ValueError(f"Install the exact core first: arduino-cli core install esp32:esp32@{profile['core_version']}")
    lib = sensor_library(profile['sensor_library_version'])
    source_files = [p for p in FIRMWARE.iterdir() if p.is_file() and p.name not in {'wifi_secrets.h', 'build_version.h'}]
    integration = getattr(args, 'integration', 'legacy')
    integration_id = {'legacy':0,'observe':1,'manual':2,'follow':3}[integration]
    if integration_id and 'PSRAM=opi' not in profile['fqbn']:
        raise ValueError('Tracking integration requires explicit PSRAM=opi board profile')
    tuning_profile = getattr(args, "tuning_profile", "SAFE_BASELINE")
    tuning_id = {"SAFE_BASELINE": 0, "DEMO_BALANCED": 1, "DIAGNOSTIC_RAW": 2}[tuning_profile]
    source_version = content_id(source_files + runtime_files())
    source_version = hashlib.sha256((source_version + tuning_profile).encode()).hexdigest()[:16]
    name = f"stage{args.stage}-{integration}-{source_version}-{'check' if profile['verification']=='compile_only' else 'device'}"
    final_output = BUILD / name
    output = windows_main_build_directory(final_output)
    if output != final_output and output.exists():
        shutil.rmtree(output)
    sketch = output / 'sketch/main_wireless'
    sketch.mkdir(parents=True, exist_ok=True)
    for p in source_files: shutil.copyfile(p, sketch / p.name)
    secret = FIRMWARE / 'wifi_secrets.h'
    if profile['verification'] == 'compile_only':
        (sketch / 'wifi_secrets.h').write_text(
            '#define CAREROVER_AP_PASSWORD "COMPILE_CHECK_ONLY"\n'
            '#define CAREROVER_WECHAT_SENDKEY "REPLACE_WITH_SENDKEY"\n'
            '#define CAREROVER_STA_SSID "REPLACE_WITH_HOME_WIFI_SSID"\n'
            '#define CAREROVER_STA_PASSWORD "REPLACE_WITH_HOME_WIFI_PASSWORD"\n')
    elif not secret.is_file():
        raise ValueError('Create firmware/main_wireless/wifi_secrets.h locally before a device build')
    else: shutil.copyfile(secret, sketch / secret.name)
    build_version = f'{source_version}-s{args.stage}-{integration}'
    (sketch / 'build_version.h').write_text(f'#define CAREROVER_BUILD_VERSION "{build_version}"\n#define CAREROVER_STAGE {args.stage}\n#define CAREROVER_INTEGRATION {integration_id}\n#define CAREROVER_TUNING_PROFILE {tuning_id}\n')
    binaries = output / 'binaries'; binaries.mkdir(exist_ok=True)
    with (output / 'compile.log').open('w', encoding='utf-8') as log:
        try:
            run([arduino, 'compile', '--fqbn', profile['fqbn'], '--library', lib,
                 '--build-path', output / 'objects', '--output-dir', binaries, sketch], stdout=log, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError:
            if output != final_output and (output / 'compile.log').is_file():
                final_output.mkdir(parents=True, exist_ok=True)
                shutil.copy2(output / 'compile.log', final_output / 'compile.log')
            print(console_safe(read_log_text(output / 'compile.log')[-8000:])); raise
    # Always validate the actual binary partition table, not only the source CSV.
    check_partition_binary(binaries / 'main_wireless.ino.partitions.bin')
    web_version = payload(output / 'webroot')
    ffat_image(output / 'webroot', binaries / 'ffat.bin', web_version)
    expected = ['main_wireless.ino.bootloader.bin', 'main_wireless.ino.partitions.bin', 'main_wireless.ino.bin', 'ffat.bin']
    # Core 3.x emits boot_app0.bin into the build directory; retain exact compiled output.
    boot_app = next(iter((output / 'objects').rglob('boot_app0.bin')), None)
    if boot_app is None:
        data_root = json.loads(capture([arduino, 'config', 'dump', '--format', 'json']))['directories']['data']
        boot_app = Path(data_root) / f"packages/esp32/hardware/esp32/{profile['core_version']}/tools/partitions/boot_app0.bin"
    shutil.copyfile(boot_app, binaries / 'boot_app0.bin'); expected.append('boot_app0.bin')
    manifest = {'profile': profile, 'stage': args.stage, 'firmware_version': build_version,
                'web_version': web_version, **source_info(),
                'arduino_cli': capture([arduino,'version']).strip(), 'test_backend_only': integration_id < 2, 'integration': integration,
                'files': {n: sha(binaries / n) for n in expected},
                'tuning_profile': tuning_profile,
                'addresses': {'main_wireless.ino.bootloader.bin': 0, 'main_wireless.ino.partitions.bin': 0x8000,
                              'boot_app0.bin': 0xe000, 'main_wireless.ino.bin': 0x10000, 'ffat.bin': PARTITION_OFFSET}}
    write_json(output / 'manifest.json', manifest)
    if output != final_output:
        if final_output.exists():
            shutil.rmtree(final_output)
        final_output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(output, final_output)
        output = final_output
    print(console_safe(read_log_text(output / 'compile.log').split('Used library')[0][-1600:]))
    print(console_safe(f'Build package: {output}\nProfile: {profile["verification"]}\nNO DEVICE WAS FLASHED.'))


def check_partition_binary(path):
    import struct
    entries = {}
    for offset in range(0, len(path.read_bytes()), 32):
        chunk = path.read_bytes()[offset:offset+32]
        if len(chunk) != 32: break
        magic, typ, subtype, start, size, label, flags = struct.unpack('<HBBII16sI', chunk)
        if magic != 0x50aa: break
        entries[label.rstrip(b'\0').decode()] = (typ, subtype, start, size)
    expected = {'nvs': (1,2,0x9000,0x5000), 'otadata':(1,0,0xe000,0x2000),
                'app0':(0,16,0x10000,0x300000), 'app1':(0,17,0x310000,0x300000),
                'ffat':(1,129,PARTITION_OFFSET,PARTITION_SIZE), 'coredump':(1,3,0xff0000,0x10000)}
    if entries != expected: raise ValueError(f'Compiled partition table differs from baseline: {entries}')


def verify_package(package, flashing=False):
    manifest = json.loads((package / 'manifest.json').read_text(encoding='utf-8'))
    validate_profile(manifest['profile'], flashing=flashing)
    expected_addresses = {'main_wireless.ino.bootloader.bin':0, 'main_wireless.ino.partitions.bin':0x8000,
                          'boot_app0.bin':0xe000, 'main_wireless.ino.bin':0x10000, 'ffat.bin':PARTITION_OFFSET}
    if manifest.get('addresses') != expected_addresses or set(manifest['files']) != set(expected_addresses):
        raise ValueError('Unexpected flash file list/addresses')
    integration = manifest.get('integration', 'legacy')
    if integration not in {'legacy','observe','manual','follow'}: raise ValueError('Unknown integration backend')
    if manifest.get('test_backend_only') != (integration in {'legacy','observe'}): raise ValueError('Inconsistent motion backend')
    for name, digest in manifest['files'].items():
        if sha(package / 'binaries' / name) != digest: raise ValueError(f'Checksum mismatch: {name}')
    check_partition_binary(package / 'binaries/main_wireless.ino.partitions.bin')
    if (package / 'binaries/ffat.bin').stat().st_size != PARTITION_SIZE: raise ValueError('Incorrect FFat size')
    return manifest


def flash(args):
    package = Path(args.package).resolve()
    manifest = verify_package(package, flashing=True) # Must run before ANY serial operation.
    command = [sys.executable, '-m', 'esptool', '--chip', 'esp32s3', '--port', args.port, '--baud', str(args.baud)]
    identification = capture(command + ['flash-id'])
    print(identification)
    if '16MB' not in identification: raise ValueError('Connected device did not report 16MB Flash')
    backups = BUILD / 'backups'; backups.mkdir(parents=True, exist_ok=True)
    backup = backups / f'main-before-{time.strftime("%Y%m%d-%H%M%S")}.bin'
    run(command + ['read-flash', '0', '0x1000000', backup])
    if backup.stat().st_size != 0x1000000: raise ValueError('Incomplete main-controller backup')
    write_json(backup.with_suffix('.json'), {'sha256':sha(backup), 'bytes':backup.stat().st_size})
    if args.only == 'ffat': names = ['ffat.bin']
    elif args.only == 'firmware': names = [n for n in manifest['addresses'] if n != 'ffat.bin']
    else: names = list(manifest['addresses'])
    pairs = [value for n in names for value in [hex(manifest['addresses'][n]), package/'binaries'/n]]
    run(command + ['write-flash','--flash-mode','dio','--flash-freq','80m','--flash-size','16MB'] + pairs)
    print('Flashed explicit partitions only. Full backup:', backup)


def environment_report():
    arduino = cli()
    cores = json.loads(capture([arduino,'core','list','--json']))
    report = {'arduino_cli':capture([arduino,'version']).strip(),
              'cores':[{k:p.get(k) for k in ['id','installed_version']} for p in cores.get('platforms',[])],
              'libraries':json.loads(capture([arduino,'lib','list','--json'])),
              'next':'Copy actual Windows FQBN including all menu settings into config/board.local.json; this report cannot infer board PSRAM.'}
    write_json(BUILD/'environment-report.json', report)
    print(BUILD/'environment-report.json')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest='action', required=True)
    sub.add_parser('environment')
    sub.add_parser('fetch-tools')
    sub.add_parser('payload')
    b = sub.add_parser('build'); b.add_argument('--profile', required=True); b.add_argument('--stage', type=int, choices=range(1,6), default=5)
    b.add_argument('--tuning-profile', choices=['SAFE_BASELINE','DEMO_BALANCED','DIAGNOSTIC_RAW'], default='SAFE_BASELINE')
    b.add_argument('--integration', choices=['legacy','observe','manual','follow'], default='legacy')
    v = sub.add_parser('verify'); v.add_argument('package')
    f = sub.add_parser('flash'); f.add_argument('package'); f.add_argument('--port',required=True); f.add_argument('--baud',type=int,default=460800); f.add_argument('--only',choices=['all','firmware','ffat'],default='all')
    args = p.parse_args()
    if args.action=='environment': environment_report()
    elif args.action=='fetch-tools': print(fetch_tools())
    elif args.action=='payload': print('Web version:',payload(BUILD/'webroot'))
    elif args.action=='build': build(args)
    elif args.action=='verify': verify_package(Path(args.package)); print('Checksums and partition table verified')
    elif args.action=='flash': flash(args)

if __name__ == '__main__':
    try: main()
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        print(console_safe(f'ERROR: {exc}'),file=sys.stderr); sys.exit(1)
