"""Bridges the pinned driver ARM code to the pinned io-pkt ARM callout code.

The driver's moal_start_timer/moal_stop_timer/free_bsd boundaries forward into
real callout_msec/callout_stop on the io-pkt Unicorn; softclock callbacks fire
wlan_flush_data back on the driver Unicorn. Suspending softclock between the
mutex release and the indirect call constructs an interleaving; it does not
prove the HU scheduler permits it (see the external-serialization note in
docs/stock-patches/devnp-mrvl_wlan-sdiorm.so.md).
"""
from __future__ import annotations

from unicorn import UC_PROT_NONE, UC_HOOK_MEM_INVALID

from .callout_machine import CalloutMachine
from .reorder_machine import ReorderMachine


class LinkedMachine:
    """One driver world + one io-pkt world with a shadow callout struct.

    The io-pkt queue must not dereference driver addresses, so the driver's
    &timer->callout maps to one shadow struct inside io-pkt memory. Freeing the
    44-byte driver timer block poisons that shadow, reproducing the allocator
    fact that the queued callout lives inside the freed moal_timer allocation.
    """
    shadow = 0x700000
    # io-pkt callout struct fields written by callout_reset/softclock (evidence 2026-10-18).
    C_FUNC, C_ARG, C_TIME, C_FLAG = 8, 12, 16, 20

    def __init__(self, image, io_pkt, *, start, window=16, candidate=None,
                 window_candidate=None, harden_candidate=None, hz=119):
        self.io = CalloutMachine(io_pkt, hz=hz)
        self.io.cpu.mem_map(self.shadow, 0x1000)
        # softclock fetches the driver's callback address inside the io-pkt world;
        # a landing page plus a code-hook interception keep that fetch defined.
        self.entry_page = image.symbols["wlan_flush_data"][0] // 0x1000 * 0x1000
        self.io.cpu.mem_map(self.entry_page, 0x1000)
        self.io.callback_entry = image.symbols["wlan_flush_data"][0]
        self.io.callback = self._fire
        self.driver = ReorderMachine(image, start=start, window=window, candidate=candidate,
                                     window_candidate=window_candidate,
                                     harden_candidate=harden_candidate, callout_bridge=self)
        self.ticks = 0
        self.fired = []
        self.faults = []
        self.io.cpu.hook_add(UC_HOOK_MEM_INVALID, self._fault)
        self.driver.cpu.hook_add(UC_HOOK_MEM_INVALID, self._fault)

    def _fault(self, uc, _access, address, size, _value, _user):
        self.faults.append({"address": address, "size": size})
        return False  # let Unicorn raise the native read/write fault

    # --- QNX callout boundary, called from the driver machine's hooks ----------
    def arm(self, callout, ms, func, arg):
        if callout != self.driver.timer + 20 or func != self.driver.address("wlan_flush_data") \
                or arg != self.driver.table + 32:
            raise AssertionError("unexpected native scheduling contract")
        self.io.call("callout_msec", self.shadow, ms, func, arg)

    def stop(self, callout):
        self.io.call("callout_stop", self.shadow)

    def timer_freed(self):
        # moal_timer is a single 44-byte malloc containing the callout at +20:
        # freeing the block frees the queued callout struct with it.
        self.io.cpu.mem_protect(self.shadow, 0x1000, UC_PROT_NONE)

    def _fire(self, argument):
        if argument != self.driver.table + 32:
            raise AssertionError("softclock invoked a foreign callback argument")
        if self.io.get(self.shadow + self.C_FUNC) != self.driver.address("wlan_flush_data"):
            raise AssertionError("softclock invoked a foreign callback function")
        self.fired.append(self.ticks)
        self.driver.call("wlan_flush_data", argument)

    # --- harness API -----------------------------------------------------------
    def callout_state(self):
        return {"queued": bool(self.io.get(self.shadow + self.C_FLAG) & 2),
                "running": bool(self.io.get(self.shadow + self.C_FLAG) & 8),
                "time": self.io.get(self.shadow + self.C_TIME),
                "ticks": self.io.get(self.io.symbols["hardclock_ticks"][0]),
                "freed": self.driver.timer_alive is False}

    def advance_ticks(self, count, *, pause=False):
        """Runs native callout_hardclock+softclock per tick; returns "captured"
        when softclock is suspended before its indirect callback."""
        if not 1 <= count <= 4096:
            raise ValueError("tick advance is bounded")
        for _ in range(count):
            self.ticks += 1
            self.driver.now_ms += 1
            if self.io.advance(1, pause=pause) == "captured":
                return "captured"
        return None

    def resume(self):
        self.io.resume()
