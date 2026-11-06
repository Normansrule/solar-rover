"""Serial link to the ESP32 (text protocol, docs/protocol.md)."""
from __future__ import annotations

import json
import threading
import time
from dataclasses import dataclass, field


@dataclass
class Telemetry:
    t: int = 0
    mode: str = "auto"
    src: str = "none"
    ovr: int = 0
    bat_v: float = 0.0
    bat_pct: float = 0.0
    chg_ma: float = 0.0
    sol_v: float = 0.0
    sol_mw: float = 0.0
    temp_c: float = 25.0
    chg: str = "idle"
    low: int = 0
    crit: int = 0
    enc: list = field(default_factory=lambda: [0, 0, 0, 0])
    spd: list = field(default_factory=lambda: [0.0, 0.0, 0.0, 0.0])
    imu: list = field(default_factory=lambda: [0.0, 0.0, 1.0, 0.0, 0.0, 0.0])
    tof: int = -1
    arm: list = field(default_factory=lambda: [0.0, 110.0, 120.0, -70.0, 20.0])
    lr: list = field(default_factory=lambda: [0, 0, 999999])   # LoRa link: module ok, RSSI dBm, ms since last frame
    received_at: float = 0.0

    @classmethod
    def parse(cls, line: str) -> "Telemetry | None":
        try:
            d = json.loads(line)
        except (ValueError, TypeError):
            return None
        if not isinstance(d, dict):
            return None
        known = {k: v for k, v in d.items() if k in cls.__dataclass_fields__}
        tel = cls(**known)
        tel.received_at = time.monotonic()
        return tel


class _SerialTransport:
    def __init__(self, port, baud):
        import serial  # pyserial
        self.ser = serial.Serial(port, baud, timeout=0.1)

    def read(self) -> bytes:
        return self.ser.read(256)

    def write(self, data: bytes):
        self.ser.write(data)

    def close(self):
        self.ser.close()


class _WsTransport:
    """Talk to the car over WiFi (the same WebSocket the phone app uses), so the
    agent can run on a PC instead of the Pi."""

    def __init__(self, url):
        import websocket  # websocket-client
        self.ws = websocket.create_connection(url, timeout=0.2)

    def read(self) -> bytes:
        import websocket
        try:
            msg = self.ws.recv()
        except websocket.WebSocketTimeoutException:
            return b""
        return (msg if isinstance(msg, bytes) else msg.encode()) + b"\n"

    def write(self, data: bytes):
        for line in data.decode().splitlines():
            self.ws.send(line)

    def close(self):
        self.ws.close()


class CarLink:
    """Reads telemetry and replies in a background thread; send() is thread-safe.
    Use port="/dev/serial0" on the Pi, or url="ws://solar-rover.local/ws" from a PC."""

    def __init__(self, port: str = "/dev/serial0", baud: int = 115200, url: str | None = None, on_line=None):
        self.ser = _WsTransport(url) if url else _SerialTransport(port, baud)
        self.lock = threading.Lock()
        self.latest: Telemetry | None = None
        self.shutdown_requested = False
        self.on_line = on_line
        self._stop = False
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        buf = b""
        while not self._stop:
            chunk = self.ser.read(256)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode(errors="replace").strip()
                if not line:
                    continue
                if line.startswith("{"):
                    tel = Telemetry.parse(line)
                    if tel:
                        self.latest = tel
                elif line == "SHUTDOWN":
                    self.shutdown_requested = True
                if self.on_line:
                    self.on_line(line)

    def send(self, line: str):
        with self.lock:
            self.ser.write((line.strip() + "\n").encode())

    # convenience wrappers
    def drive(self, left: float, right: float):
        self.send(f"D {int(round(left * 1000))} {int(round(right * 1000))}")

    def arm(self, yaw=None, shoulder=None, elbow=None, wrist=None, grip=None):
        vals = ["*" if v is None else f"{v:.1f}" for v in (yaw, shoulder, elbow, wrist, grip)]
        self.send("A " + " ".join(vals))

    def stop(self):
        self.send("S")

    def close(self):
        self._stop = True
        try:
            self.stop()
        finally:
            self.ser.close()
