#!/usr/bin/env python3
"""Rebuild and verify the firmware images installed on the 2026-09-21 boards.

This command never opens a serial port.  It preserves the verified main-app
metadata image, rebuilds the functional main payload, and performs the two
timestamped CAM passes needed to reproduce the installed app and bootloader.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).absolute().parents[1]
MAIN_PACKAGE = ROOT / 'build/stage5-follow-0569bb8f3f3c66d2-device'
MAIN_ARCHIVE = ROOT / 'build/stage5-follow-0569bb8f3f3c66d2-device-board-archive-20260921'
CAM_PACKAGE = ROOT / 'build/cam-stream-demo_balanced-windows_baseline_confirmed'

SOURCE_HASHES = {
    'firmware/main_wireless/front_guard.h': '742ac674aacfeaf122135f77ff53d3df333ce02548e7d869cb23d64ad5eaf32e',
    'firmware/main_wireless/build_version.h': '93178d352ea307f98b3847f46f3d4c1558156be2bbb2dabdcf65fc3d4bfbe95d',
    'firmware/cam_tracking/main/CMakeLists.txt': 'f4acba5f5b96fc64aa91b76fff9310a1eaa7a310c0ef501fd39b05795794fc9d',
    'firmware/cam_tracking/main/idf_component.yml': '7b9bf33522224ef6570e2bbb771819306c92e1f545ac3b3a247fbbcfb5d4e6b8',
    'firmware/cam_tracking/main/Kconfig.projbuild': '83ed28857b55630e3cf1dbe45bb3600d985670e9b7353bd386f521d8b80f3fee',
    'firmware/cam_tracking/main/latest_frame.h': 'e3335f8da6ca27eaf907bcb4808f3c261d0e63f40243852e7b6f885cc7fdf7d5',
    'firmware/cam_tracking/main/main.cpp': '902d369482b354ad914c3edd9405a4b0aaabba86f77cfe614e7317f31637f34c',
    'firmware/cam_tracking/main/video_server.cpp': '999eb994d285a4517e3b0e54761dc93e41053d3339d7964c1a05d7529a50a614',
    'firmware/cam_tracking/main/video_server.h': '91ca021ea2da1e72b82c54d1e0f5e4ba6b17118059c12267cc1dff5bc41e435e',
    'firmware/cam_tracking/board_exact_shared/vision_protocol.h': 'fc815f36cce0ef984abdb4809c25488e09a24c04ac305b4e74cff9775276fbb9',
    'firmware/cam_tracking/board_exact_shared/box_track.h': 'fd465bffb1aa5f1e2f594462032fe2df983ce3bbcf31bfb5730bb5bba9c9385e',
    'firmware/cam_tracking/board_exact_shared/demo_tuning.h': 'bb87f76c5eb8acd96ba06b0200afe7e161daf0fa5dfd24f648408f9d9d19fa9f',
    'firmware/cam_tracking/CMakeLists.txt': '2b6ff70dc44fdc04d3286e50d3091dfa1994a4a6a1794c5397dc3007cdce805d',
    'firmware/cam_tracking/partitions.csv': '28ef39c6bb9ed8a1d0d58ab1ddbe88ae176f1699b8a55c3bda4005e3fffbe5f3',
    'firmware/cam_tracking/sdkconfig.defaults': '94a255ed6a31ddc5fb64d3baf2005138cc63d9de0a08e3e911e9922f8ff245d9',
}
EXPECTED_MAIN_SOURCE_VERSION = '0569bb8f3f3c66d2'
MAIN_HASHES = {
    'main_wireless.ino.bootloader.bin': 'b41be55ae9a52aeeb21645c51b86c14027f84c2c91bf67bee6aa0e1b15d18e8b',
    'main_wireless.ino.partitions.bin': 'ace02503447d0f470692e65fa76002f2d77a92dc81cd3813d8aa66718d716da9',
    'main_wireless.ino.bin': '1f8d8d09b17313f448af68368db931f5e793a87f9a4639ccafbbd31255e88aed',
    'ffat.bin': '19d5d98517ac7ab7de4c7380a75567a44fa644b00bb71f6bf2d048babb535f54',
    'boot_app0.bin': 'f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0',
}
CAM_HASHES = {
    'bootloader/bootloader.bin': 'f51065c5e8a35e13890669a8f71e23b837ec4ef6cc6abf5144f3896c4fb89e87',
    'carerover_cam_tracking.bin': '1978648c69452163589f01907eb19ccf8dd4d7bdd06deb341f4b41255c33573f',
    'partition_table/partition-table.bin': '790756bb2d460ac6007db512e1e8811da2b872d1632d787ab90a6d6bd45c06b1',
}


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command: list[object], env: dict[str, str]) -> None:
    subprocess.run([str(value) for value in command], cwd=ROOT, env=env, check=True)


def verify_source() -> None:
    for relative, expected in SOURCE_HASHES.items():
        path = ROOT / relative
        if not path.is_file() or digest(path) != expected:
            raise ValueError(f'Board-exact source mismatch: {relative}')
    # carerover.py hashes every main-controller source plus the embedded web
    # runtime.  Checking its version ID covers the complete compiled input set,
    # while the hashes above retain file-level diagnostics for generated/main
    # exceptions and every CAM input.
    from carerover import FIRMWARE, content_id, runtime_files
    main_sources = [path for path in FIRMWARE.iterdir()
                    if path.is_file() and path.name not in {'wifi_secrets.h', 'build_version.h'}]
    base = content_id(main_sources + runtime_files())
    version = hashlib.sha256((base + 'DEMO_BALANCED').encode()).hexdigest()[:16]
    if version != EXPECTED_MAIN_SOURCE_VERSION:
        raise ValueError(f'Main source set mismatch: {version}')


def verify_files(root: Path, expected: dict[str, str]) -> None:
    for relative, expected_hash in expected.items():
        path = root / relative
        if not path.is_file() or digest(path) != expected_hash:
            raise ValueError(f'Board-exact package mismatch: {path}')


def verify_packages() -> None:
    verify_files(MAIN_PACKAGE / 'binaries', MAIN_HASHES)
    verify_files(CAM_PACKAGE, CAM_HASHES)


def update_main_manifest() -> None:
    path = MAIN_PACKAGE / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    manifest['files'] = {name: digest(MAIN_PACKAGE / 'binaries' / name) for name in MAIN_HASHES}
    manifest['board_exact_rebuild'] = {
        'reference': '2026-09-21 full-flash backup',
        'temp_root': r'C:\CareRoverTemp',
        'source_date_epoch': 1789768863,
        'functional_image_match': True,
        'binary_metadata_note': '65 bytes are app ELF SHA, checksum, and validation hash only',
        'packaged_app': 'board-exact verified image',
        'status': 'PACKAGE_SHA256_MATCHED',
    }
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def update_cam_manifest() -> None:
    path = CAM_PACKAGE / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    for relative in CAM_HASHES:
        image = CAM_PACKAGE / relative
        manifest['files'][relative]['sha256'] = digest(image)
        manifest['files'][relative]['bytes'] = image.stat().st_size
    manifest['board_exact_rebuild'] = {
        'reference': '2026-09-21 full-flash backup',
        'temp_root': r'C:\CareRoverTemp2',
        'app_source_date_epoch': 1789518943,
        'bootloader_source_date_epoch': 1789518976,
        'status': 'REBUILT_AND_SHA256_MATCHED',
    }
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def main_reference_app() -> Path:
    candidates = [MAIN_PACKAGE / 'binaries/main_wireless.ino.bin',
                  MAIN_ARCHIVE / 'binaries/main_wireless.ino.bin']
    expected = MAIN_HASHES['main_wireless.ino.bin']
    for path in candidates:
        if path.is_file() and digest(path) == expected:
            return path
    raise ValueError('The verified main application reference is missing')


def verify_main_functional_image(rebuilt: Path, reference: Path) -> None:
    actual = rebuilt.read_bytes()
    expected = reference.read_bytes()
    if len(actual) != len(expected):
        raise ValueError('Rebuilt main application size differs from the installed image')
    differences = {index for index, pair in enumerate(zip(actual, expected)) if pair[0] != pair[1]}
    allowed = set(range(176, 208)) | set(range(len(actual) - 33, len(actual)))
    if differences != allowed:
        outside = sorted(differences - allowed)[:12]
        raise ValueError(f'Rebuilt main functional bytes differ outside metadata: {outside}')


def build_main(reference_copy: Path) -> None:
    Path(r'C:\CareRoverTemp').mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, TEMP=r'C:\CareRoverTemp', TMP=r'C:\CareRoverTemp',
               PYTHONUTF8='1', SOURCE_DATE_EPOCH='1789768863')
    run([sys.executable, ROOT / 'tools/carerover.py', 'build', '--profile',
         ROOT / 'config/board.local.json', '--stage', '5', '--integration', 'follow',
         '--tuning-profile', 'DEMO_BALANCED'], env)
    rebuilt = MAIN_PACKAGE / 'binaries/main_wireless.ino.bin'
    verify_main_functional_image(rebuilt, reference_copy)
    shutil.copyfile(reference_copy, rebuilt)
    update_main_manifest()


def build_cam(saved_app: Path, saved_partition: Path) -> None:
    if not os.environ.get('IDF_PATH'):
        raise ValueError('Activate the locked ESP-IDF 5.3.4 environment before rebuilding CAM')
    Path(r'C:\CareRoverTemp2').mkdir(parents=True, exist_ok=True)
    command = [sys.executable, ROOT / 'tools/tracking.py', 'cam-build', '--variant', 'stream',
               '--tuning-profile', 'DEMO_BALANCED', '--profile', ROOT / 'config/cam-board.local.json']
    base = dict(os.environ, TEMP=r'C:\CareRoverTemp2', TMP=r'C:\CareRoverTemp2', PYTHONUTF8='1')
    run(command, dict(base, SOURCE_DATE_EPOCH='1789518943'))
    if digest(CAM_PACKAGE / 'carerover_cam_tracking.bin') != CAM_HASHES['carerover_cam_tracking.bin']:
        raise ValueError('CAM application did not reproduce the installed image')
    shutil.copyfile(CAM_PACKAGE / 'carerover_cam_tracking.bin', saved_app)
    shutil.copyfile(CAM_PACKAGE / 'partition_table/partition-table.bin', saved_partition)
    run(command, dict(base, SOURCE_DATE_EPOCH='1789518976'))
    if digest(CAM_PACKAGE / 'bootloader/bootloader.bin') != CAM_HASHES['bootloader/bootloader.bin']:
        raise ValueError('CAM bootloader did not reproduce the installed image')
    shutil.copyfile(saved_app, CAM_PACKAGE / 'carerover_cam_tracking.bin')
    shutil.copyfile(saved_partition, CAM_PACKAGE / 'partition_table/partition-table.bin')
    update_cam_manifest()


def rebuild() -> None:
    verify_source()
    with tempfile.TemporaryDirectory(prefix='carerover-board-exact-') as directory:
        temp = Path(directory)
        reference = temp / 'main-reference.bin'
        shutil.copyfile(main_reference_app(), reference)
        build_main(reference)
        build_cam(temp / 'cam-app.bin', temp / 'cam-partition.bin')
    verify_packages()
    print('PASS: source snapshots and all 8 programmed image segments match the installed boards.')
    print('NO DEVICE WAS FLASHED.')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['verify', 'build'])
    args = parser.parse_args()
    verify_source()
    if args.action == 'build':
        rebuild()
    else:
        verify_packages()
        print('PASS: source snapshots and all 8 programmed image segments match the installed boards.')
        print('NO DEVICE WAS FLASHED.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'ERROR: {error}', file=sys.stderr)
        raise SystemExit(1)
