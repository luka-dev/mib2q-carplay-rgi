# CarPlay emulator laboratory

_Native iOS sender and stock210 QNX receiver integration tests._

[Laboratory documentation](../../docs/carplay-lab/README.md) · [vphone runbook](../../docs/carplay-lab/VPHONE_LAB.md) · [Timing evidence](../../docs/carplay-lab/VPHONE_TIMING.md)

The [native wireless/QNX runbook](../../docs/carplay-lab/NATIVE_WIRELESS_QNX.md)
uses the real Rust daemon through a byte-only API adapter. With the opt-in
VM association backend, native wireless pairing/media now yield independently
decoded 800×480 Maps. `wireless-native build --paired-altscreen` adds
independently verified 1440×542 stream 111; both streams must decode to pass.
The owned reconnect test reuses the receiver identity
and independently decodes fresh video after native disconnect. Physical
Wi-Fi/RFCOMM remain hardware tests.

## 🗂️ Ownership

| Directory | Purpose |
|---|---|
| `vphone/` | Active vphone guest, Direct NCM bridge, input and sender observations |
| `common/` | QNX controller, MFi signer, capture extraction and pacing reports |
| `qnx/` | QEMU capture harness, synthetic phones, virtual HID/RAM capture and native tests |
| `fixtures/diplay_mfi/` | Pinned public certificate/key fixture and provenance |
| `legacy_ios/` | Retained Inferno/iOS 14 helpers, patches and ticket fixtures |

Production wireless code remains in `wireless/`, `hook/wireless/` and the normal
deployment tree. The laboratory builds and tests those sources. Receiver
includes of virtual HID and fake-media fixtures are guarded by QEMU-only
compile flags; ordinary receiver builds do not load them.

## 🛠️ Commands and artifacts

Run from the repository root. The entry point remains `scripts/carplay_lab.sh`:

```bash
scripts/carplay_lab.sh plan
scripts/carplay_lab.sh status
scripts/carplay_lab.sh qnx-status
```

The [runbook](../../docs/carplay-lab/VPHONE_LAB.md) describes setup and live
authentication. `vphone` is the active backend; `legacy_ios` is explicitly
selected for historical reproduction. Direct Python imports now use
`tests.carplay_lab`; there are no fallback modules in `wireless/`.

VM images, captures, venvs and PID records retain their existing locations under
`build/vphone-lab/` and `build/ios-vm-lab/`. The generic older receiver fixtures
retain `wireless/receiver_bridge/out/` for their established log/archive paths.
This source move does not relocate guest machines or historical artifacts.

## 🧪 Offline validation

```bash
python3 -m unittest discover -s tests/carplay_lab/vphone -p 'test_*.py'
PYTHONPATH=tests/carplay_lab/legacy_ios python3 -m unittest discover \
  -s tests/carplay_lab/legacy_ios -p 'test_*.py'
clang -Wall -Wextra -Werror -Wframe-larger-than=4096 \
  -fsanitize=address,undefined -pthread \
  tests/carplay_lab/qnx/test_qnx_input_memory.c \
  -o build/vphone-lab/test-qnx-input-memory
build/vphone-lab/test-qnx-input-memory
```

These checks do not boot either guest. QNX image and diagnostic QEMU builds are
explicit `qnx-build` / `qemu-trace-build` actions. The QEMU builder requires an
already configured local QEMU build and refuses an active owned receiver.
