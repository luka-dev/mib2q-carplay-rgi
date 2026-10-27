"""Input regressions use a recording QMP endpoint; no guest is started."""
import random
import sys
import unittest
from unittest.mock import patch

import input_control


class RecordingInput(input_control.InputControl):
    def __init__(self, fail_at=None):
        self.commands = []
        self.fail_at = fail_at
        self.closed = False

    def call(self, command, arguments=None):
        self.commands.append((command, arguments))
        if len(self.commands) == self.fail_at:
            raise RuntimeError("injected QMP error")

    def close(self):
        self.closed = True

    def poll_display(self):
        self.commands.append(("poll-display", None))


class InputTests(unittest.TestCase):
    def test_screen_corners_and_normalized_coordinates_keep_y_downward(self):
        for size in ((828, 1792), (2, 2), (65536, 65536)):
            width, height = size
            self.assertEqual(input_control.screen_position(0, 0, size), (0, 0))
            self.assertEqual(input_control.screen_position(width-1, height-1, size),
                             (32767, 32767))
        for position in ((0, 0), (16384, 32000), (32767, 32767)):
            self.assertEqual(input_control.screen_position(*position), position)

    def test_random_screen_positions_stay_inside_and_round_trip(self):
        rng = random.Random(8030)
        for _ in range(10000):
            width, height = rng.randint(2, 65536), rng.randint(2, 65536)
            x, y = rng.randrange(width), rng.randrange(height)
            nx, ny = input_control.screen_position(x, y, (width, height))
            self.assertTrue(0 <= nx <= 32767 and 0 <= ny <= 32767)
            self.assertLessEqual(abs(nx*(width-1)/32767-x), (width-1)/65534+1e-9)
            self.assertLessEqual(abs(ny*(height-1)/32767-y), (height-1)/65534+1e-9)

    def test_invalid_coordinates_and_dimensions_are_rejected(self):
        for x, y, size in ((-1, 0, None), (0, 32768, None), (828, 0, (828, 1792)),
                           (0, 1792, (828, 1792)), (0, 0, (1, 1792)),
                           (0, 0, (828, 65537))):
            with self.subTest(x=x, y=y, size=size), self.assertRaises(ValueError):
                input_control.screen_position(x, y, size)

    def test_unlock_swipes_up_without_toggling_the_power_key_by_default(self):
        control = RecordingInput()
        with patch.object(input_control.time, "sleep"):
            control.unlock()
        self.assertTrue(all(command == "input-send-event" for command, _ in control.commands))
        reports = [params["events"] for _, params in control.commands]
        self.assertTrue(reports[0][-1]["data"]["down"])
        self.assertFalse(reports[-1][-1]["data"]["down"])
        ys = [report[1]["data"]["value"] for report in reports]
        self.assertGreater(ys[0], ys[-1])
        self.assertEqual(ys, sorted(ys, reverse=True))
        self.assertEqual(len({report[0]["data"]["value"] for report in reports}), 1)

    def test_explicit_wake_sends_power_before_the_contact(self):
        control = RecordingInput()
        with patch.object(input_control.time, "sleep") as sleep:
            control.unlock(wake=True)
        self.assertEqual(control.commands[0], ("send-key", {
            "keys": [{"type": "qcode", "data": "f5"}], "hold-time": 100}))
        self.assertEqual(sleep.call_args_list[0].args, (3,))
        self.assertEqual(control.commands[1], ("poll-display", None))
        self.assertEqual(control.commands[2][0], "input-send-event")

    def test_motion_failure_still_releases_the_finger(self):
        control = RecordingInput(fail_at=3)
        with patch.object(input_control.time, "sleep"), self.assertRaisesRegex(RuntimeError, "QMP"):
            control.swipe(1000, 30000, 1000, 1000)
        release = control.commands[-1][1]["events"][-1]
        self.assertEqual(release, {"type": "btn", "data": {"button": "left", "down": False}})
        self.assertEqual(control.commands[-1][1]["events"][:2],
                         control.commands[1][1]["events"][:2])

    def test_tap_is_a_short_contact(self):
        control = RecordingInput()
        with patch.object(input_control.time, "sleep") as sleep:
            control.tap(12000, 20000)
        self.assertLess(sleep.call_args.args[0], .5)
        self.assertEqual([params["events"][-1]["data"]["down"]
                          for _, params in control.commands], [True, False])

    def test_pixel_cli_tap_targets_the_screen_position_and_closes(self):
        control = RecordingInput()
        argv = ["input_control.py", "tap", "827", "1791", "--screen-size", "828", "1792"]
        with patch.object(sys, "argv", argv), \
                patch.object(input_control, "InputControl", return_value=control), \
                patch.object(input_control.time, "sleep"):
            input_control.main()
        events = control.commands[0][1]["events"]
        self.assertEqual([event["data"]["value"] for event in events[:2]], [32767, 32767])
        self.assertTrue(control.closed)

    def test_invalid_cli_input_never_connects_to_the_guest(self):
        for args in (["tap", "828", "0", "--screen-size", "828", "1792"],
                     ["swipe", "0", "0", "1", "1", "--duration-ms", "0"],
                     ["unlock", "--hold-ms", "-1"], ["unlock", "--duration-ms", "5001"]):
            with self.subTest(args=args), patch.object(sys, "argv", ["input_control.py", *args]), \
                    patch.object(input_control, "InputControl") as connect, \
                    patch.object(sys, "stderr"), self.assertRaises(SystemExit):
                input_control.main()
            connect.assert_not_called()


if __name__ == "__main__":
    unittest.main()
