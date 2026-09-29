"""Isolated Track B 48-band allocation experiment; checkpoints need this loader.

Only construction uses a temporary band_edges override, restored even on failure.
The ordinary shared separator remains unchanged. This factory is single-threaded.
"""
import sep_net as SN

WIDTHS = {
    "original": [4]*11 + [8]*11 + [16]*13 + [32]*8 + [85]*4 + [89],
    "redistributed": [8]*11 + [16]*11 + [24]*13 + [32]*8 + [38]*4 + [41],
}


def edges(widths):
    if len(widths) != 48 or any(type(w) is not int or w <= 0 for w in widths) or sum(widths) != SN.N_BINS:
        raise ValueError("48 positive integer bands must cover 1025 bins")
    ends = [0]
    for w in widths:
        ends.append(ends[-1]+w)
    return list(zip(ends[:-1], ends[1:]))


def make(torch, mode):
    widths = WIDTHS[mode]
    bands = edges(widths)
    original = SN.band_edges
    if edges(WIDTHS["original"]) != original():
        raise ValueError("original separator geometry changed")
    try:
        SN.band_edges = lambda: bands.copy()
        net = SN.BandSepNet(torch, norm="stream", mask="complex2")
    finally:
        SN.band_edges = original
    if net.bands != bands:
        raise ValueError("geometry construction failed")
    return net


def load(path, device="cpu"):
    torch = SN._torch()
    ck = torch.load(path, map_location=device)
    mode = ck.get("band_geometry")
    if mode not in WIDTHS or ck.get("band_widths") != WIDTHS[mode]:
        raise ValueError("explicit, supported band geometry required")
    if (ck.get("arch"), ck.get("norm"), ck.get("mask")) != ("band", "stream", "complex2"):
        raise ValueError("original causal complex topology required")
    if any(ck.get(k, 0) for k in ("brnn", "lookahead", "bidir", "dec_skip")):
        raise ValueError("baseline topology required")
    net = make(torch, mode).to(device)
    net.modules.load_state_dict(ck["state"])
    net.modules.eval()
    SN.STATS.update(ck["stats"])
    return torch, net, device
