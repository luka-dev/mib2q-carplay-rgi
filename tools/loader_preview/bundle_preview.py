#!/usr/bin/env python3
"""Package the self-contained PRISM executable (close the running app first)."""
from pathlib import Path
import plistlib
import shutil

HERE = Path(__file__).resolve().parent
OUTPUT = HERE.parents[1] / 'output/loader-preview'
APP = OUTPUT / 'PRISM.app'
# No legacy assets remain in the app. Build a fresh bundle before replacing it.
STAGING = OUTPUT / 'PRISM.staging.app'
if STAGING.exists():
    shutil.rmtree(STAGING)
MACOS = STAGING / 'Contents/MacOS'
MACOS.mkdir(parents=True)
shutil.copy2(HERE / 'prism_preview', MACOS / 'prism_preview')
(STAGING / 'Contents/Info.plist').write_bytes(plistlib.dumps({
    'CFBundleIdentifier': 'dev.luka.altscreen.prism',
    'CFBundleName': 'PRISM',
    'CFBundleDisplayName': 'PRISM',
    'CFBundleExecutable': 'prism_preview',
    'CFBundlePackageType': 'APPL',
    'CFBundleVersion': '11',
    'CFBundleShortVersionString': '1.0',
    'NSHighResolutionCapable': True,
}))
if APP.exists():
    shutil.rmtree(APP)
STAGING.rename(APP)
shutil.copy2(HERE / 'prism_preview', OUTPUT / 'prism_preview')
print(APP)
