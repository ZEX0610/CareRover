import copy
import json
from pathlib import Path
import struct
import tempfile
import unittest
from tools.carerover import check_partition_binary, validate_profile, read_log_text, runtime_files, windows_main_build_directory
from tools.tracking import console_safe, windows_cam_build_directory, windows_idf_build_settings

class BuildGuardTests(unittest.TestCase):
    def test_windows_cam_build_uses_utf8_and_disables_ccache(self):
        env, definitions = windows_idf_build_settings({}, platform_name='nt')
        self.assertEqual('1', env['PYTHONUTF8'])
        self.assertEqual('0', env['IDF_CCACHE_ENABLE'])
        self.assertIn('-DCCACHE_ENABLE=0', definitions)

    def test_windows_cam_build_stages_unicode_output_in_ascii_temp(self):
        package = Path('C:/workspace/硬件/CareRover/build/cam-gesture-compile_only')
        staged = windows_cam_build_directory(package, platform_name='nt', temp_root=Path('C:/Temp'))
        self.assertEqual(Path('C:/Temp/CareRover-cam-build/cam-gesture-compile_only'), staged)
        self.assertTrue(all(ord(char) < 128 for char in str(staged)))

    def test_windows_cam_build_stages_subst_drive_too(self):
        package = Path('R:/build/cam-gesture-compile_only')
        staged = windows_cam_build_directory(package, platform_name='nt', temp_root=Path('C:/Temp'))
        self.assertEqual(Path('C:/Temp/CareRover-cam-build/cam-gesture-compile_only'), staged)

    def test_windows_main_build_stages_unicode_output_in_ascii_temp(self):
        output = Path('C:/workspace/硬件/CareRover/build/stage5-observe-device')
        staged = windows_main_build_directory(output, platform_name='nt', temp_root=Path('C:/Temp'))
        self.assertEqual(Path('C:/Temp/CareRover-main-build/stage5-observe-device'), staged)
        self.assertTrue(all(ord(char) < 128 for char in str(staged)))

    def test_windows_main_build_stages_subst_drive_too(self):
        output = Path('R:/build/stage5-observe-device')
        staged = windows_main_build_directory(output, platform_name='nt', temp_root=Path('C:/Temp'))
        self.assertEqual(Path('C:/Temp/CareRover-main-build/stage5-observe-device'), staged)

    def test_console_output_replaces_characters_missing_from_gbk(self):
        rendered = console_safe('path contains \u04f2', encoding='gbk')
        self.assertEqual('path contains ?', rendered)

    def test_main_console_output_replaces_characters_missing_from_gbk(self):
        from tools.carerover import console_safe as main_console_safe
        self.assertEqual('path contains ?', main_console_safe('path contains \u04f2', encoding='gbk'))

    def test_compile_log_uses_windows_fallback_encoding(self):
        with tempfile.TemporaryDirectory() as d:
            log = Path(d) / 'compile.log'
            log.write_bytes('链接错误：找不到输出路径'.encode('gb18030'))
            self.assertIn('链接错误', read_log_text(log))

    def test_compile_profile_cannot_be_flashed(self):
        p=json.loads(Path('config/development.json').read_text())
        validate_profile(p)
        with self.assertRaisesRegex(ValueError,'forbidden'): validate_profile(p,flashing=True)
        p['verification']='windows_baseline_confirmed'; validate_profile(p,flashing=True)
        p['flash_size']='8MB'
        with self.assertRaises(ValueError): validate_profile(p,flashing=True)

    def test_pending_baseline_never_silently_uses_development_defaults(self):
        p=json.loads(Path('config/board.example.json').read_text())
        with self.assertRaisesRegex(ValueError,'pending'): validate_profile(p)

    def test_real_binary_partition_offsets_and_sizes_are_checked(self):
        entries=[('nvs',1,2,0x9000,0x5000),('otadata',1,0,0xe000,0x2000),
                 ('app0',0,16,0x10000,0x300000),('app1',0,17,0x310000,0x300000),
                 ('ffat',1,129,0x610000,0x9e0000),('coredump',1,3,0xff0000,0x10000)]
        def binary(es): return b''.join(struct.pack('<HBBII16sI',0x50aa,t,s,o,n,name.encode(),0) for name,t,s,o,n in es)
        with tempfile.TemporaryDirectory() as d:
            f=Path(d)/'partitions.bin'; f.write_bytes(binary(entries)); check_partition_binary(f)
            bad=copy.copy(entries); bad[4]=('ffat',1,129,0x620000,0x9d0000); f.write_bytes(binary(bad))
            with self.assertRaisesRegex(ValueError,'differs'): check_partition_binary(f)

    def test_payload_excludes_dev_bridge_tests_secrets_and_recordings(self):
        files=runtime_files()
        self.assertTrue(any(p.name=='index.html' for p in files))
        self.assertFalse(any(p.suffix in {'.py','.log','.h','.txt'} for p in files))
        self.assertFalse(any('DELIVERY' in p.parts or 'hardware' in p.parts for p in files))

if __name__=='__main__': unittest.main()
