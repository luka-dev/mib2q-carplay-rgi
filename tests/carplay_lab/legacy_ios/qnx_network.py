#!/usr/bin/env python3
"""Compatibility entry point for the retained Inferno laboratory."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[3]))
import tests.carplay_lab.common.qnx_receiver as implementation
if __name__ == "__main__":
    implementation.OUT = Path(__file__).resolve().parents[3] / "build/ios-vm-lab/qnx-network"
    implementation.main()
else:
    sys.modules[__name__] = implementation
