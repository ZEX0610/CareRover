"""Convert CareRover main-board serial JSON into the website WebSocket protocol."""

from __future__ import annotations

import json
import math
import time
from typing import Any


GESTURE_LABELS = {
    "no_hand": "NONE",
    "no_gesture": "NONE",
    "one": "ONE",
    "two": "TWO",
    "three": "THREE",
    "four": "FOUR",
    "five": "FIVE",
    "ok": "OK",
    "call": "CALL",
    "like": "LIKE",
    "dislike": "DISLIKE",
}


def timestamp_ms() -> int:
    return int(time.time() * 1000)


def finite_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def website_health_state(source: str, has_value: bool) -> str:
    if source == "no_finger":
        return "NO_FINGER"
    if source in {"sensor_missing", "sensor_error", "wrong_part"}:
        return "ERROR"
    if source == "poor_signal":
        return "LOW_QUALITY"
    if source == "acquiring":
        return "ACQUIRING"
    if has_value or source in {
        "stable", "holding", "hr_stable", "hr_holding", "spo2_stable", "spo2_holding"
    }:
        return "VALID"
    return "MEASURING"


class SerialProtocolAdapter:
    """Stateful adapter. Its methods do not access the serial port or network."""

    def __init__(self) -> None:
        self.serial_open = False
        self.last_main_at = 0.0
        self.last_cam_at = 0.0
        self.last_status: dict[str, Any] = {}
        self.robot_mode = "IDLE"
        self.robot_state = "IDLE"
        self.estop = False

    def set_serial_open(self, opened: bool) -> None:
        self.serial_open = opened
        if not opened:
            self.last_main_at = 0.0

    def ingest_line(self, line: str, now_monotonic: float | None = None) -> list[dict[str, Any]]:
        try:
            message = json.loads(line)
        except (TypeError, ValueError):
            return []
        if not isinstance(message, dict) or not isinstance(message.get("type"), str):
            return []

        now = time.monotonic() if now_monotonic is None else now_monotonic
        self.last_main_at = now
        kind = message["type"]
        if kind == "gesture":
            self.last_cam_at = now
            accepted = bool(message.get("accepted"))
            raw_label = str(message.get("label", "no_gesture")).lower()
            label = GESTURE_LABELS.get(raw_label, "UNKNOWN") if accepted else "NONE"
            score = message.get("score", 0)
            confidence = max(0.0, min(1.0, float(score))) if finite_number(score) else 0.0
            return [{
                "type": "telemetry",
                "ts": timestamp_ms(),
                "connection": {"camera": True, "main_mcu": True},
                "vision": {
                    "image_width": 320,
                    "image_height": 240,
                    "ai_fps": self._ai_fps(message),
                    "gesture": {"label": label, "confidence": confidence, "stable": accepted},
                },
            }]

        if kind == "health":
            hr_valid = bool(message.get("hr_valid", message.get("valid", False)))
            spo2_valid = bool(message.get("spo2_valid", message.get("valid", False)))
            hr = message.get("hr_bpm") if hr_valid and finite_number(message.get("hr_bpm")) else None
            spo2 = message.get("spo2_pct") if spo2_valid and finite_number(message.get("spo2_pct")) else None
            sqi = message.get("sqi", 0)
            sqi_normalized = max(0.0, min(1.0, float(sqi) / 100.0)) if finite_number(sqi) else 0.0
            state = website_health_state(str(message.get("state", "")), hr is not None or spo2 is not None)
            return [{
                "type": "telemetry",
                "ts": timestamp_ms(),
                "connection": {"main_mcu": True},
                "health": {
                    "hr_bpm": hr,
                    "spo2_pct": spo2,
                    "sqi": sqi_normalized,
                    "finger_detected": bool(message.get("finger_present")),
                    "state": state,
                },
            }]

        if kind == "ppg_raw":
            value = message.get("ir")
            if finite_number(value):
                return [{
                    "type": "ppg_batch",
                    "ts": timestamp_ms(),
                    "sample_rate_hz": 25,
                    "samples": [value],
                }]
            return []

        if kind == "system_status":
            self.last_status = message.copy()
            if message.get("cam_connected") is True:
                self.last_cam_at = now
        return []

    def heartbeat(self, now_monotonic: float | None = None) -> dict[str, Any]:
        now = time.monotonic() if now_monotonic is None else now_monotonic
        main_connected = self.serial_open and self.last_main_at > 0 and now - self.last_main_at <= 2.5
        camera_connected = main_connected and self.last_cam_at > 0 and now - self.last_cam_at <= 1.5
        return {
            "type": "telemetry",
            "ts": timestamp_ms(),
            "connection": {"camera": camera_connected, "main_mcu": main_connected},
            "robot": {
                "mode": "ESTOP" if self.estop else self.robot_mode,
                "state": "ESTOP" if self.estop else self.robot_state,
                "estop": self.estop,
                "vx": 0,
                "vy": 0,
                "wz": 0,
            },
        }

    def reply_to_browser(self, message: Any) -> list[dict[str, Any]]:
        if not isinstance(message, dict):
            return [self._error("INVALID_COMMAND", "Command must be a JSON object")]
        kind = message.get("type")
        if kind == "ping":
            return [{"type": "pong", "ts": timestamp_ms(), "id": message.get("id")}]
        if kind == "estop":
            self.estop = True
            return [self._ack(message)]
        if kind == "cmd_vel":
            velocities = [message.get(axis) for axis in ("vx", "vy", "wz")]
            if all(finite_number(value) and float(value) == 0.0 for value in velocities):
                return []
            return [self._error(
                "MOTION_NOT_INSTALLED",
                "Display-only bridge: non-zero motion is disabled until motor firmware is integrated",
            )]
        if kind in {"set_mode", "clear_estop"}:
            return [self._error(
                "MOTION_NOT_INSTALLED",
                "Display-only bridge: motion forwarding is disabled until motor firmware is integrated",
            )]
        return [self._error("UNKNOWN_TYPE", "Unsupported command type")]

    @staticmethod
    def _ai_fps(message: dict[str, Any]) -> float | None:
        infer_ms = message.get("infer_ms")
        if finite_number(infer_ms) and infer_ms > 0:
            return round(1000.0 / float(infer_ms), 1)
        return None

    @staticmethod
    def _ack(message: dict[str, Any]) -> dict[str, Any]:
        reply = {
            "type": "ack",
            "ts": timestamp_ms(),
            "request_type": message.get("type"),
            "ok": True,
        }
        if finite_number(message.get("request_id")):
            reply["request_id"] = message["request_id"]
        return reply

    @staticmethod
    def _error(code: str, message: str) -> dict[str, Any]:
        return {"type": "error", "ts": timestamp_ms(), "code": code, "message": message}
