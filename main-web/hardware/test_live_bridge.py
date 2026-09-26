"""End-to-end smoke test for a running serial_bridge.py instance."""

from __future__ import annotations

import argparse
import asyncio
import json

import aiohttp


async def run(url: str, duration: float) -> None:
    kinds: dict[str, int] = {}
    saw_main = False
    saw_camera = False
    saw_health = False
    saw_gesture = False
    saw_ppg = False
    async with aiohttp.ClientSession() as session:
        async with session.ws_connect(url, max_msg_size=65536) as ws:
            loop = asyncio.get_running_loop()
            deadline = loop.time() + duration
            await ws.send_json({"type": "ping", "ts": 1, "id": 12345})
            while loop.time() < deadline:
                try:
                    frame = await asyncio.wait_for(ws.receive(), min(1.0, deadline - loop.time()))
                except asyncio.TimeoutError:
                    continue
                if frame.type != aiohttp.WSMsgType.TEXT:
                    continue
                message = json.loads(frame.data)
                kind = message.get("type", "unknown")
                kinds[kind] = kinds.get(kind, 0) + 1
                connection = message.get("connection", {})
                saw_main |= connection.get("main_mcu") is True
                saw_camera |= connection.get("camera") is True
                saw_health |= isinstance(message.get("health"), dict)
                saw_gesture |= isinstance(message.get("vision", {}).get("gesture"), dict)
                saw_ppg |= kind in {"ppg", "ppg_batch"}

    print("COUNTS=" + json.dumps(kinds, sort_keys=True))
    print(
        f"main={saw_main} camera={saw_camera} health={saw_health} "
        f"gesture={saw_gesture} ppg={saw_ppg}"
    )
    missing = [
        name for name, present in (
            ("main", saw_main), ("camera", saw_camera), ("health", saw_health),
            ("gesture", saw_gesture), ("ppg", saw_ppg),
        ) if not present
    ]
    if missing:
        raise SystemExit("FAIL missing: " + ", ".join(missing))
    print("PASS: real serial data reached the website WebSocket protocol")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://127.0.0.1:8765/ws")
    parser.add_argument("--duration", type=float, default=8.0)
    args = parser.parse_args()
    asyncio.run(run(args.url, args.duration))
