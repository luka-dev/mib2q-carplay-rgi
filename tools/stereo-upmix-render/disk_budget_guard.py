"""Hysteresis for non-destructive workflow pausing. Values are GiB, not GB."""
import math


class DiskBudgetGuard:
    def __init__(self, pause_gib=22, resume_gib=27):
        if not (math.isfinite(pause_gib) and math.isfinite(resume_gib)
                and 0 < pause_gib < resume_gib):
            raise ValueError('require 0 < pause threshold < resume threshold')
        self.pause_gib = pause_gib
        self.resume_gib = resume_gib
        self.paused = False

    def observe(self, free_gib):
        if not math.isfinite(free_gib) or free_gib < 0:
            raise ValueError('invalid free space reading')
        previous = self.paused
        if self.paused:
            if free_gib >= self.resume_gib:
                self.paused = False
        elif free_gib <= self.pause_gib:
            self.paused = True
        return 'pause' if self.paused and not previous else 'resume' if previous and not self.paused else None
