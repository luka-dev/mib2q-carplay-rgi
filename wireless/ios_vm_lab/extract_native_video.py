#!/usr/bin/env python3
"""Compatibility entry point for the retained Inferno laboratory."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
import wireless.lab_common.extract_native_video as implementation
if __name__ == "__main__":
    implementation.main()
else:
    sys.modules[__name__] = implementation
