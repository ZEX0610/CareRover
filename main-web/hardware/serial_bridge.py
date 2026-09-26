"""Serve the CareRover website and bridge main-board USB serial to /ws."""

from __future__ import annotations

import argparse
import asyncio
import json
import time
from pathlib import Path

import serial
from serial.tools import list_ports
from aiohttp import WSMsgType, web

from serial_protocol_adapter import SerialProtocolAdapter


DEFAULT_SITE_ROOT = Path(__file__).resolve().parent.parent


class HardwareServer:
    def __init__(self, site_root: Path, port_name: str, baud: int) -> None:
        self.site_root = site_root.resolve()
        self.port_name = port_name
        self.baud = baud
        self.adapter = SerialProtocolAdapter()
        self.clients: set[web.WebSocketResponse] = set()
        self.serial_port: serial.Serial | None = None
        self.running = True

    async def serial_reader(self) -> None:
        while self.running:
            try:
                port = serial.Serial(self.port_name, self.baud, timeout=0.25, write_timeout=0.5)
                self.serial_port = port
                self.adapter.set_serial_open(True)
                port.reset_input_buffer()
                port.write(b"RAW_ON\nSTATUS\n")
                print(f"[serial] connected {self.port_name} @ {self.baud}")
                last_status = time.monotonic()
                while self.running and port.is_open:
                    raw = await asyncio.to_thread(port.readline)
                    if raw:
                        line = raw.decode("utf-8", errors="replace").strip()
                        for message in self.adapter.ingest_line(line):
                            await self.broadcast(message)
                    if time.monotonic() - last_status >= 2.0:
                        port.write(b"STATUS\n")
                        last_status = time.monotonic()
            except (serial.SerialException, OSError) as exc:
                print(f"[serial] unavailable: {exc}; retrying in 2 s")
            finally:
                self.adapter.set_serial_open(False)
                if self.serial_port is not None:
                    try:
                        self.serial_port.close()
                    except serial.SerialException:
                        pass
                self.serial_port = None
            await asyncio.sleep(2)

    async def heartbeat(self) -> None:
        while self.running:
            await self.broadcast(self.adapter.heartbeat())
            await asyncio.sleep(0.1)

    async def websocket(self, request: web.Request) -> web.WebSocketResponse:
        origin = request.headers.get("Origin")
        if origin and origin != f"{request.scheme}://{request.host}":
            raise web.HTTPForbidden(text="Use the CareRover page served by this bridge")
        ws = web.WebSocketResponse(max_msg_size=65536, heartbeat=10, timeout=1)
        await ws.prepare(request)
        self.clients.add(ws)
        await ws.send_json(self.adapter.heartbeat())
        try:
            async for frame in ws:
                if frame.type != WSMsgType.TEXT:
                    continue
                try:
                    command = json.loads(frame.data)
                except (TypeError, ValueError):
                    command = None
                for reply in self.adapter.reply_to_browser(command):
                    await ws.send_json(reply)
        finally:
            self.clients.discard(ws)
        return ws

    async def broadcast(self, message: dict) -> None:
        async def deliver(ws: web.WebSocketResponse) -> None:
            try:
                await asyncio.wait_for(ws.send_json(message), 0.15)
            except (ConnectionError, asyncio.TimeoutError, RuntimeError):
                await ws.close()

        if self.clients:
            await asyncio.gather(*(deliver(ws) for ws in tuple(self.clients)))

    async def static(self, request: web.Request) -> web.StreamResponse:
        name = request.match_info.get("name", "") or "index.html"
        path = (self.site_root / name).resolve()
        allowed_roots = tuple((self.site_root / folder).resolve() for folder in ("css", "js", "assets", "docs"))
        allowed = path == self.site_root / "index.html" or any(path.is_relative_to(root) for root in allowed_roots)
        if not allowed or not path.is_file() or any(part.startswith(".") for part in Path(name).parts):
            raise web.HTTPNotFound()
        return web.FileResponse(path, headers={"Cache-Control": "no-cache", "X-Content-Type-Options": "nosniff"})

    async def lifecycle(self, _app: web.Application):
        tasks = [asyncio.create_task(self.serial_reader()), asyncio.create_task(self.heartbeat())]
        yield
        self.running = False
        for task in tasks:
            task.cancel()
        await asyncio.gather(*tasks, return_exceptions=True)
        await asyncio.gather(*(ws.close() for ws in tuple(self.clients)), return_exceptions=True)


def create_app(site_root: Path, port_name: str, baud: int) -> web.Application:
    server = HardwareServer(site_root, port_name, baud)
    app = web.Application()
    app.router.add_get("/ws", server.websocket)
    app.router.add_get("/{name:.*}", server.static)
    app.cleanup_ctx.append(server.lifecycle)
    return app


def print_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found")
        return
    for item in ports:
        print(f"{item.device}\t{item.description}\t{item.hwid}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port", help="Main-board port, for example COM6")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--http-port", type=int, default=8080)
    parser.add_argument("--site-root", type=Path, default=DEFAULT_SITE_ROOT)
    parser.add_argument("--list-ports", action="store_true")
    args = parser.parse_args()
    if args.list_ports:
        print_ports()
        raise SystemExit(0)
    if not args.serial_port:
        parser.error("--serial-port is required; use --list-ports first")
    print(f"Open http://{args.host}:{args.http_port}/?transport=ws&video=canvas&source=usb")
    web.run_app(
        create_app(args.site_root, args.serial_port, args.baud),
        host=args.host,
        port=args.http_port,
        shutdown_timeout=2,
    )
