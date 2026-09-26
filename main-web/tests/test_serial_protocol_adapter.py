import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "hardware"))
from serial_protocol_adapter import SerialProtocolAdapter


class SerialProtocolAdapterTests(unittest.TestCase):
    def setUp(self):
        self.adapter = SerialProtocolAdapter()
        self.adapter.set_serial_open(True)

    def test_stable_gesture_is_converted(self):
        output = self.adapter.ingest_line(json.dumps({
            "type": "gesture", "label": "five", "score": 0.91,
            "accepted": True, "infer_ms": 250,
        }), now_monotonic=10.0)
        gesture = output[0]["vision"]["gesture"]
        self.assertEqual("FIVE", gesture["label"])
        self.assertTrue(gesture["stable"])
        self.assertEqual(4.0, output[0]["vision"]["ai_fps"])

    def test_unaccepted_gesture_becomes_none(self):
        output = self.adapter.ingest_line(json.dumps({
            "type": "gesture", "label": "no_gesture", "score": 0,
            "accepted": False,
        }), now_monotonic=10.0)
        self.assertEqual("NONE", output[0]["vision"]["gesture"]["label"])

    def test_independent_health_values_are_preserved(self):
        output = self.adapter.ingest_line(json.dumps({
            "type": "health", "state": "hr_stable", "finger_present": True,
            "hr_valid": True, "spo2_valid": False, "hr_bpm": 73,
            "spo2_pct": None, "sqi": 80,
        }), now_monotonic=10.0)
        health = output[0]["health"]
        self.assertEqual(73, health["hr_bpm"])
        self.assertIsNone(health["spo2_pct"])
        self.assertEqual("VALID", health["state"])
        self.assertEqual(0.8, health["sqi"])

    def test_ppg_uses_ir_channel(self):
        output = self.adapter.ingest_line(
            '{"type":"ppg_raw","ms":123,"ir":91234,"red":88765}',
            now_monotonic=10.0,
        )
        self.assertEqual("ppg_batch", output[0]["type"])
        self.assertEqual(25, output[0]["sample_rate_hz"])
        self.assertEqual([91234], output[0]["samples"])

    def test_heartbeat_expires_each_link_independently(self):
        self.adapter.ingest_line('{"type":"gesture","accepted":false}', now_monotonic=10.0)
        live = self.adapter.heartbeat(now_monotonic=10.5)
        self.assertTrue(live["connection"]["camera"])
        self.assertTrue(live["connection"]["main_mcu"])
        stale = self.adapter.heartbeat(now_monotonic=13.0)
        self.assertFalse(stale["connection"]["camera"])
        self.assertFalse(stale["connection"]["main_mcu"])

    def test_motion_is_explicitly_disabled_for_current_hardware(self):
        reply = self.adapter.reply_to_browser({"type": "cmd_vel", "ts": 1, "vx": 1, "vy": 0, "wz": 0})
        self.assertEqual("MOTION_NOT_INSTALLED", reply[0]["code"])

    def test_zero_velocity_is_safely_accepted_without_noise(self):
        reply = self.adapter.reply_to_browser({"type": "cmd_vel", "ts": 1, "vx": 0, "vy": 0, "wz": 0})
        self.assertEqual([], reply)


if __name__ == "__main__":
    unittest.main()
