#!/usr/bin/env python3
"""Send bounded input only to the owned emulator's explicit local QMP socket."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import socket
import tempfile
import time

from lab import DEFAULT_LAB, running_pid


def screen_position(x, y, size=None):
    """QMP coordinates use the screen origin: x rightward, y downward."""
    width, height = size if size is not None else (32768, 32768)
    if not 2 <= width <= 65536 or not 2 <= height <= 65536:
        raise ValueError("screen width and height must be in 2..65536")
    if not 0 <= x < width or not 0 <= y < height:
        raise ValueError(f"coordinates must be inside {width}x{height}")
    return round(x * 32767 / (width - 1)), round(y * 32767 / (height - 1))


def gesture_timing(hold, duration=1):
    if not 0 <= hold <= 5 or not 0 < duration <= 5:
        raise ValueError("hold must be in 0..5000 ms and duration in 1..5000 ms")


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
        screen_position(x, y)
        events = [{"type": "abs", "data": {"axis": "x", "value": x}},
                  {"type": "abs", "data": {"axis": "y", "value": y}}]
        if down is not None:
            events.append({"type": "btn", "data": {"button": "left", "down": down}})
        self.call("input-send-event", {"events": events})

    def tap(self, x, y, hold=.2):
        screen_position(x, y)
        gesture_timing(hold)
        try:
            self.pointer(x, y, True)
            time.sleep(hold)
        finally:
            self.pointer(x, y, False)

    def swipe(self, x0, y0, x1, y1, hold=.6, duration=1):
        screen_position(x0, y0)
        screen_position(x1, y1)
        gesture_timing(hold, duration)
        x, y = x0, y0
        try:
            self.pointer(x0, y0, True)
            time.sleep(hold)
            for step in range(1, 21):
                next_x = round(x0 + (x1 - x0) * step / 20)
                next_y = round(y0 + (y1 - y0) * step / 20)
                self.pointer(next_x, next_y)
                x, y = next_x, next_y
                time.sleep(duration / 20)
        finally:
            self.pointer(x, y, False)

    def unlock(self, wake=False, hold=.6, duration=1):
        """Send the native upward gesture; this does not bypass a passcode."""
        gesture_timing(hold, duration)
        if wake:
            self.call("send-key", {"keys": [{"type": "qcode", "data": "f5"}],
                                  "hold-time": 100})
            # The cold software-rendered lock screen needs time to resume.
            time.sleep(3)
            self.poll_display()
            time.sleep(1)
        self.swipe(16384, 32000, 16384, 8000, hold, duration)

    def poll_display(self):
        """Refresh headless scanout before touching a resumed lock screen."""
        with tempfile.NamedTemporaryFile(dir=self.lab, prefix=".input-wake-", suffix=".ppm", delete=False) as file:
            path = Path(file.name)
        try:
            self.screenshot(path)
        finally:
            path.unlink(missing_ok=True)

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
    unlock = commands.add_parser("unlock", help="send an upward gesture on the iOS lock screen")
    unlock.add_argument("--wake", action="store_true",
                        help="press f5 first; use only when the display is off")
    for command in (tap, swipe):
        command.add_argument("--screen-size", nargs=2, type=int, metavar=("WIDTH", "HEIGHT"),
                             help="interpret coordinates as screenshot pixels; origin is top-left")
    for command in (tap, swipe, unlock):
        command.add_argument("--hold-ms", type=int, default=200 if command is tap else 600,
                             help="initial contact dwell (0..5000 ms)")
    for command in (swipe, unlock):
        command.add_argument("--duration-ms", type=int, default=1000,
                             help="movement duration (1..5000 ms)")
    key = commands.add_parser("key")
    key.add_argument("code", choices=("f3", "f4", "f5", "f6"))
    args = parser.parse_args()
    try:
        if args.action == "tap":
            x, y = screen_position(args.x, args.y, args.screen_size)
            gesture_timing(args.hold_ms / 1000)
        elif args.action == "swipe":
            x0, y0 = screen_position(args.x0, args.y0, args.screen_size)
            x1, y1 = screen_position(args.x1, args.y1, args.screen_size)
            gesture_timing(args.hold_ms / 1000, args.duration_ms / 1000)
        elif args.action == "unlock":
            gesture_timing(args.hold_ms / 1000, args.duration_ms / 1000)
    except ValueError as exc:
        parser.error(str(exc))
    control = InputControl()
    try:
        if args.action == "screenshot":
            control.screenshot(args.path)
        elif args.action == "tap":
            control.tap(x, y, args.hold_ms / 1000)
        elif args.action == "swipe":
            control.swipe(x0, y0, x1, y1, args.hold_ms / 1000, args.duration_ms / 1000)
        elif args.action == "unlock":
            control.unlock(args.wake, args.hold_ms / 1000, args.duration_ms / 1000)
        else:
            control.call("send-key", {"keys": [{"type": "qcode", "data": args.code}], "hold-time": 100})
    finally:
        control.close()


if __name__ == "__main__":
    main()
