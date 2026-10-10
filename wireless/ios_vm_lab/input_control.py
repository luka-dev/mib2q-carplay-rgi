#!/usr/bin/env python3
"""Send bounded input only to the owned emulator's explicit local QMP socket."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import socket
import time

from lab import DEFAULT_LAB, running_pid


class InputControl:
    def __init__(self, lab: Path = DEFAULT_LAB):
        self.lab = lab.resolve()
        if not running_pid(self.lab):
            raise RuntimeError("the owned iOS VM must be running")
        self.connection = socket.socket(socket.AF_UNIX)
        self.connection.settimeout(5)
        self.connection.connect(str(self.lab / "ios-input.sock"))
        self.stream = self.connection.makefile("rb")
        if "QMP" not in self.read():
            self.close()
            raise RuntimeError("expected QMP greeting")
        self.sequence = 0
        self.call("qmp_capabilities")

    def read(self):
        line = self.stream.readline(65537)
        if not line or len(line) > 65536:
            raise ValueError("invalid QMP response length")
        return json.loads(line)

    def call(self, command, arguments=None):
        self.sequence += 1
        request = {"execute": command, "id": self.sequence}
        if arguments is not None:
            request["arguments"] = arguments
        self.connection.sendall(json.dumps(request).encode() + b"\n")
        for _ in range(128):
            response = self.read()
            if response.get("id") == self.sequence:
                if "error" in response:
                    raise RuntimeError(response["error"])
                return response.get("return")
        raise RuntimeError("too many QMP events")

    def pointer(self, x, y, down=None):
        if not 0 <= x <= 32767 or not 0 <= y <= 32767:
            raise ValueError("coordinates must be in 0..32767")
        events = [{"type": "abs", "data": {"axis": "x", "value": x}},
                  {"type": "abs", "data": {"axis": "y", "value": y}}]
        if down is not None:
            events.append({"type": "btn", "data": {"button": "left", "down": down}})
        self.call("input-send-event", {"events": events})

    def screenshot(self, path):
        path = Path(path).resolve()
        if not path.is_relative_to(self.lab) or path.suffix != ".ppm":
            raise ValueError("screenshot must be a .ppm file inside the lab")
        self.call("screendump", {"filename": str(path)})
        # A headless frontend otherwise leaves scanout at its previous poll.
        time.sleep(.15)
        self.call("screendump", {"filename": str(path)})

    def close(self):
        self.stream.close()
        self.connection.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    shot = commands.add_parser("screenshot")
    shot.add_argument("path", type=Path)
    tap = commands.add_parser("tap")
    tap.add_argument("x", type=int)
    tap.add_argument("y", type=int)
    swipe = commands.add_parser("swipe")
    for axis in ("x0", "y0", "x1", "y1"):
        swipe.add_argument(axis, type=int)
    key = commands.add_parser("key")
    key.add_argument("code", choices=("f3", "f4", "f5", "f6"))
    args = parser.parse_args()
    coordinates = (args.x, args.y) if args.action == "tap" else (
        args.x0, args.y0, args.x1, args.y1) if args.action == "swipe" else ()
    if any(not 0 <= value <= 32767 for value in coordinates):
        parser.error("coordinates must be in 0..32767")
    control = InputControl()
    try:
        if args.action == "screenshot":
            control.screenshot(args.path)
        elif args.action == "tap":
            control.pointer(args.x, args.y, True)
            time.sleep(.7)
            control.pointer(args.x, args.y, False)
        elif args.action == "swipe":
            control.pointer(args.x0, args.y0, True)
            time.sleep(.15)
            for step in range(1, 21):
                control.pointer(round(args.x0 + (args.x1 - args.x0) * step / 20),
                                round(args.y0 + (args.y1 - args.y0) * step / 20))
                time.sleep(.03)
            control.pointer(args.x1, args.y1, False)
        else:
            control.call("send-key", {"keys": [{"type": "qcode", "data": args.code}], "hold-time": 100})
    finally:
        control.close()


if __name__ == "__main__":
    main()
