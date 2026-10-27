#!/usr/bin/env python3
"""export_masknet_weights.py — the w__ keys of masknet_oracle.npz (scene_mask_fixtures.py: ConvNet.modules'
state_dict as float32) -> the SMSKNET1 weight blob of hook/audio/scene_mask.h, i.e. the memory image of
struct scene_mask_weights: 8-byte magic, u32 ch / hidden / n_out, then 8131 little-endian float32 in the
order of the struct's fields (c1 w+b, c2 w+b, c3 w+b, GRU w_ih, w_hh, b_ih, b_hh, head w+b, feat_mean,
feat_std, elig_w, has_calib, calib_a, calib_b). The hook reads the file straight into the struct.

  python3 export_masknet_weights.py masknet_oracle.npz masknet.bin
"""
import struct
import sys

import numpy as np

CH, HID, OUT, FEAT, K1, K2 = 24, 12, 4, 4, 8, 5
MAGIC = b"SMSKNET1"
NPARAM = 8131


def blob(z):
    """z: the opened npz (or a dict of its arrays) -> bytes."""
    arch = tuple(int(v) for v in z["arch"])
    if arch != (CH, HID, OUT):
        raise ValueError(f"arch {arch} != the header's (ch, hidden, n_out) = {(CH, HID, OUT)}")
    if int(z["softmax_out"]) != 0:
        raise ValueError("softmax_out nets are not supported: the runtime is the sigmoid path of out_act()")

    def w(name, shape):
        a = np.ascontiguousarray(z["w__" + name], dtype="<f4")
        if a.shape != shape:
            raise ValueError(f"{name}: shape {a.shape} != {shape}")
        return a

    mean, std = np.asarray(z["feat_mean"]), np.asarray(z["feat_std"])
    if mean.size == 0:                                    # standardize() is the identity when the checkpoint has no stats
        mean, std = np.zeros(FEAT), np.ones(FEAT)
    elig = np.zeros(OUT)
    elig[np.asarray(z["roles_eligible"], dtype=int)] = 1.0
    calib = np.asarray(z["calib"], dtype=np.float64)
    has_calib = 0.0 if np.isnan(calib).any() else 1.0
    parts = [
        w("0__weight", (CH, FEAT, K1)), w("0__bias", (CH,)),
        w("1__weight", (CH, CH, K2)), w("1__bias", (CH,)),
        w("2__weight", (CH, CH, K2)), w("2__bias", (CH,)),
        w("3__weight_ih_l0", (3 * HID, CH)), w("3__weight_hh_l0", (3 * HID, HID)),
        w("3__bias_ih_l0", (3 * HID,)), w("3__bias_hh_l0", (3 * HID,)),
        w("4__weight", (OUT, CH + HID)), w("4__bias", (OUT,)),
        mean, std, elig,
        np.array([has_calib, calib[0] if has_calib else 0.0, calib[1] if has_calib else 0.0]),
    ]
    body = b"".join(np.ascontiguousarray(p, dtype="<f4").tobytes() for p in parts)
    out = MAGIC + struct.pack("<III", CH, HID, OUT) + body
    assert len(out) == 20 + 4 * NPARAM, len(out)
    return out


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = blob(np.load(sys.argv[1]))
    with open(sys.argv[2], "wb") as fh:
        fh.write(data)
    print(sys.argv[2], len(data), "bytes")
