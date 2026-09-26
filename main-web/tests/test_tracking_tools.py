import json
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch
from tools.tracking import crc8, analyze, cam_profile, cam_flash
from tools import carerover
import hashlib
import zipfile
from types import SimpleNamespace
from tools import tracking
class TrackingToolTests(unittest.TestCase):
    def test_board_exact_source_snapshots(self):
        expected={
            'firmware/main_wireless/front_guard.h':'742ac674aacfeaf122135f77ff53d3df333ce02548e7d869cb23d64ad5eaf32e',
            'firmware/main_wireless/build_version.h':'93178d352ea307f98b3847f46f3d4c1558156be2bbb2dabdcf65fc3d4bfbe95d',
            'firmware/cam_tracking/board_exact_shared/vision_protocol.h':'fc815f36cce0ef984abdb4809c25488e09a24c04ac305b4e74cff9775276fbb9',
            'firmware/cam_tracking/board_exact_shared/box_track.h':'fd465bffb1aa5f1e2f594462032fe2df983ce3bbcf31bfb5730bb5bba9c9385e',
            'firmware/cam_tracking/board_exact_shared/demo_tuning.h':'bb87f76c5eb8acd96ba06b0200afe7e161daf0fa5dfd24f648408f9d9d19fa9f',
        }
        for relative,digest in expected.items():
            source=tracking.ROOT/relative
            self.assertTrue(source.is_file(),relative)
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),digest,relative)

    def test_front_diagnostics_and_private_installation_remain_protected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);firmware=root/'firmware/main_wireless';firmware.mkdir(parents=True)
            for name in ['README.md','index.html','package.json']:(root/name).write_text('test fixture')
            (firmware/'front_config.local.h').write_text('private installation')
            (firmware/'front_guard.h').write_text('public source')
            log=root/'front.jsonl';log.write_text(json.dumps({'elapsed_ms':1,'text':json.dumps({'type':'front_status','distance_cm':42})}))
            self.assertEqual(analyze(log)['metrics'][0]['distance_cm'],42)
            out=root/'release.zip'
            with patch.object(tracking,'ROOT',root),patch.object(tracking,'source_info',return_value={}):
                tracking.release(SimpleNamespace(output=str(out),include_reference=False))
            with zipfile.ZipFile(out) as archive:
                self.assertIn('firmware/main_wireless/front_guard.h',archive.namelist())
                self.assertNotIn('firmware/main_wireless/front_config.local.h',archive.namelist())

    def test_archive_provenance_without_git_and_after_edit(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);source=root/'example.cpp';source.write_text('original')
            (root/'EXPORT_INFO.json').write_text(json.dumps({'source_commit':'exported-commit','source_dirty':False}))
            (root/'SOURCE_MANIFEST.json').write_text(json.dumps({'example.cpp':hashlib.sha256(source.read_bytes()).hexdigest()}))
            with patch.object(carerover,'ROOT',root),patch.object(carerover,'capture') as git:
                info=carerover.source_info()
                self.assertEqual(info['source_commit'],'exported-commit')
                self.assertFalse(info['source_dirty']);self.assertFalse(info['archive_modified'])
                source.write_text('edited')
                self.assertTrue(carerover.source_info()['archive_modified'])
                self.assertTrue(carerover.source_info()['source_dirty'])
                git.assert_not_called()

    def test_crc_and_capture_summary(self):
        self.assertEqual(crc8('123456789'),0xf4)
        body='P,1,100,1,900,100,60,180,140,40'
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'serial.jsonl'
            p.write_text('\n'.join(json.dumps({'elapsed_ms':t,'text':s}) for t,s in [(10,f'@{body}*{crc8(body):02X}'),(210,'@P,2,200,0,0,0,0,0,0,10*00')]))
            r=analyze(p);self.assertEqual(r['uart_valid'],1);self.assertEqual(r['uart_bad_crc'],1);self.assertEqual(r['person_found'],1)
    def test_main_profile_cannot_be_used_as_cam(self):
        with self.assertRaises(ValueError):cam_profile('config/tracking-development.json')
        p=cam_profile('config/cam-board.example.json');self.assertEqual(p['verification'],'compile_only')
    def test_compile_cam_package_rejected_before_any_serial_call(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'manifest.json').write_text(json.dumps({'target':'cam','verification':'compile_only','flash_size':'8MB'}))
            with patch('tools.tracking.run') as execute:
                with self.assertRaises(ValueError):cam_flash(type('Args',(),{'package':d,'port':'COM6'})())
                execute.assert_not_called()
if __name__=='__main__':unittest.main()
