"""ARM/QNX RX/TX lifetime regressions with real Opus and private SDK AEAD."""
import json
from .wireless_screen_runtime import run

if __name__ == '__main__':
    print(json.dumps(run(audio=True), indent=2))
