import unittest
import hashlib
import tempfile
from pathlib import Path

from tools.tracking import esptool_operation, esptool_write_flash_flags, validated_existing_cam_backup


class EsptoolCompatibilityTests(unittest.TestCase):
    def test_idf_53_esptool_4_uses_underscores(self):
        version = "esptool.py v4.12.0\n4.12.0"
        self.assertEqual(esptool_operation(version, "flash-id"), "flash_id")
        self.assertEqual(esptool_operation(version, "read-flash"), "read_flash")
        self.assertEqual(esptool_operation(version, "write-flash"), "write_flash")
        self.assertEqual(esptool_write_flash_flags(version),
                         ["--flash_mode", "dio", "--flash_freq", "80m", "--flash_size", "8MB"])

    def test_esptool_5_uses_hyphens(self):
        self.assertEqual(esptool_operation("esptool v5.1.0", "flash-id"), "flash-id")
        self.assertEqual(esptool_write_flash_flags("esptool v5.1.0"),
                         ["--flash-mode", "dio", "--flash-freq", "80m", "--flash-size", "8MB"])

    def test_existing_backup_requires_full_size_and_exact_hash(self):
        with tempfile.TemporaryDirectory() as folder:
            backup = Path(folder) / "cam.bin"
            backup.write_bytes(b"x")
            with self.assertRaises(ValueError):
                validated_existing_cam_backup(backup, hashlib.sha256(b"x").hexdigest(), folder)
            data = b"x" * 0x800000
            backup.write_bytes(data)
            sha = hashlib.sha256(data).hexdigest()
            self.assertEqual(validated_existing_cam_backup(backup, sha, folder), backup)
            with self.assertRaises(ValueError):
                validated_existing_cam_backup(backup, "0" * 64, folder)


if __name__ == "__main__":
    unittest.main()
