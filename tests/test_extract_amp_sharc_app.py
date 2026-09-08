#!/usr/bin/env python3
"""Integrity tests for the checked-in J525 SHARC loader extraction."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "extract_amp_sharc_app.py"
SPEC = importlib.util.spec_from_file_location("extract_amp_sharc_app", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class SharcApplicationExtractionTests(unittest.TestCase):
    def setUp(self) -> None:
        output = ROOT / "output" / "r2" / "soundamp"
        self.raw = (output / MODULE.RAW_NAME).read_bytes()
        self.decoded = (output / MODULE.BITREV_NAME).read_bytes()

    def test_artifact_hashes_and_transform(self) -> None:
        self.assertEqual(len(self.raw), MODULE.SHARC_LENGTH)
        self.assertEqual(MODULE.sha256(self.raw), MODULE.RAW_SHA256)
        self.assertEqual(MODULE.sha256(self.decoded), MODULE.BITREV_SHA256)
        self.assertEqual(MODULE.bit_reverse_bytes(self.raw), self.decoded)
        self.assertEqual(MODULE.bit_reverse_bytes(self.decoded), self.raw)

    def test_complete_loader_parse(self) -> None:
        blocks = MODULE.validate(self.raw, self.decoded)
        self.assertEqual(len(blocks), 560)
        self.assertEqual(
            Counter(block.tag for block in blocks),
            Counter({0: 1, 1: 279, 2: 1, 3: 37, 4: 240, 5: 2}),
        )
        self.assertEqual(blocks[0].offset, MODULE.BOOT_KERNEL_SIZE)
        self.assertEqual(blocks[-1].offset, 0x27158)


if __name__ == "__main__":
    unittest.main()
