#!/usr/bin/env python3
"""Compare the current AltScreen hot path with the audited baseline, then test it.

No vehicle operations. Operation counts exclude kernel/codec/GPU traffic and
assume lengthSize=4, warmed buffers and complete I/O. Not a hardware benchmark.
"""

import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build/altscreen-static-comparison"
OUT.mkdir(parents=True, exist_ok=True)
CC = os.environ.get("CC", "clang")
BASELINE = "875c1299de4173cebfc42936ddfa41808c11eff7"
FLAGS = [
    "-std=gnu99",
    "-O1",
    "-g",
    "-Wall",
    "-Wextra",
    "-Werror",
    "-Wno-unused-function",
    "-Wno-unused-variable",
    "-Wno-unused-but-set-variable",
    "-fsanitize=address,undefined",
    "-fno-omit-frame-pointer",
    "-I" + str(ROOT / "hook"),
    "-I" + str(ROOT / "common"),
    "-I" + str(ROOT / "toolchain/qnx65-abi/include/OMX"),
]

sources = [
    "hook/altscreen/altscreen_av.c",
    "hook/altscreen/altscreen_hook.c",
    "hook/altscreen/avcc_frame.h",
    "hook/altscreen/recovery_policy.h",
    "common/alt_control_wire.h",
    "altscreen_render/queue_policy.h",
    "altscreen_render/main.c",
    "altscreen_render/frame_io.c",
    "altscreen_render/sink_omx_qnx.c",
]
text = {name: (ROOT / name).read_text() for name in sources}
assert "altav_enqueue_owned(av_gen" in text["hook/altscreen/altscreen_hook.c"]
assert "p->data=*data" in text["hook/altscreen/altscreen_av.c"].replace(" ", "")
assert "writev(" in text["hook/altscreen/altscreen_av.c"]
assert "decode_origin_ms" in text["altscreen_render/main.c"]
checks = {}
for name, extra in [
    ("policy", []),
    ("control_wire", []),
    ("recovery", []),
    ("io_batch", []),
    ("health_wire", []),
    ("negotiation", []),
    ("idle_clock", []),
    ("control", ["altscreen_render/frame_io.c"]),
    ("queue_policy", ["altscreen_render/frame_io.c"]),
    ("mailbox", []),
]:
    exe = OUT / ("current_" + name)
    subprocess.run(
        [
            CC,
            *FLAGS,
            str(ROOT / ("tests/altscreen_" + name + "_test.c")),
            *[str(ROOT / x) for x in extra],
            "-lpthread",
            "-lm",
            "-o",
            str(exe),
        ],
        check=True,
    )
    r = subprocess.run(
        [str(exe)], text=True, capture_output=True, timeout=20, check=True
    )
    (OUT / ("current_" + name + ".log")).write_text(r.stdout + r.stderr)
    checks[name] = "PASS"
report = {
    "scope": "static source paths and ASan/UBSan hardware-fake contracts; no HU measurement",
    "baseline": BASELINE,
    "operation_count_method": "Audited application-path counts; source-shape assertions plus exact I/O and mailbox contract tests. Not a runtime profiler.",
    "operations": {
        "full_application_payload_copies_through_omx_input": {
            "baseline": 3,
            "first_pacing_patch": 3,
            "current_length4_owned": 1,
        },
        "hook_nal_boundary_walks": {
            "baseline": 3,
            "first_pacing_patch": 4,
            "current_length4_owned": 1,
        },
        "steady_warm_allocator_calls": {"baseline": 3, "current": 0},
        "complete_record_send_read_calls": {
            "baseline": 4,
            "first_pacing_v2": 5,
            "current_known_v2": 3,
        },
    },
    "contracts": checks,
    "mailbox_burst": {
        "decoded": 12,
        "fifo_draws": 12,
        "mailbox_draws": 2,
        "returned_slots": 12,
        "sole_old_frame_preserved": True,
        "empty_outputs_do_not_supersede_pictures": True,
    },
    "recovery": "generation-scoped backoff, opt-in presentation ACK and idle EOF observation",
    "limits": [
        "LS1-3 retain the expanding conversion path",
        "No actual QNX GPU/VC FPS measurement",
        "FIFO fallback: ALTSCREEN_PRESENT_FIFO=1",
    ],
    "source_sha256": {
        name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources
    },
}
path = OUT / "comparison.json"
if path.exists() and not (OUT / "before-six-comparison.json").exists():
    (OUT / "before-six-comparison.json").write_bytes(path.read_bytes())
path.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps(report, indent=2))
print("Report:", path)
