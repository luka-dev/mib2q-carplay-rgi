"""Package the built host viewer for normal macOS open/activation."""
from pathlib import Path
import plistlib
import shutil

root = Path(__file__).resolve().parents[2]
app = root / "build/HU Cursor Preview.app"
contents = app / "Contents"
(contents / "MacOS").mkdir(parents=True, exist_ok=True)
# Replace the executable inode atomically. Rewriting a launched Mach-O in place
# can leave macOS's code-signature cache rejecting later launches (errno 162).
staged = contents / "MacOS/cursor_preview.new"
shutil.copy2(root / "build/cursor_preview", staged)
staged.replace(contents / "MacOS/cursor_preview")
with (contents / "Info.plist").open("wb") as f:
    plistlib.dump({
        "CFBundleIdentifier": "dev.luka.hucursorpreview",
        "CFBundleName": "HU Cursor Preview",
        "CFBundleExecutable": "cursor_preview",
        "CFBundlePackageType": "APPL",
        "CFBundleVersion": "1",
        "NSHighResolutionCapable": True,
    }, f)
print(app)
