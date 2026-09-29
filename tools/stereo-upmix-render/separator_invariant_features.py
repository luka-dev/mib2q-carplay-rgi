"""Track B experiment: replace six absolute-phase input features, no trunk change.

The baseline encoder pre-hook receives normalized Re L/R, Im L/R, log L/R.
The candidate gets log1p normalized magnitudes L/R, the same standardized log
magnitudes L/R, and cos/sin interchannel phase. Global polarity cancels exactly.
Checkpoints MUST use this loader, not sep_net.load, for invariant6 features.
"""
import sep_net as SN


def attach(net, mode):
    if mode == "absolute6":
        return net
    if mode != "invariant6":
        raise ValueError("unknown feature mode")
    torch = net.torch

    def replace(module, inputs):
        original = inputs[0]
        f = original.reshape(*original.shape[:-1], -1, 6)
        re, im, log = f[..., :2], f[..., 2:4], f[..., 4:6]
        mag = torch.sqrt(re.square() + im.square())
        den = (mag[..., 0] * mag[..., 1]).clamp_min(1e-8)
        cos = (re[..., 0] * re[..., 1] + im[..., 0] * im[..., 1]) / den
        sin = (im[..., 0] * re[..., 1] - re[..., 0] * im[..., 1]) / den
        new = torch.cat((torch.log1p(mag), log, cos[..., None], sin[..., None]), -1)
        return (new.reshape_as(original),)

    net.feature_handles = [enc.register_forward_pre_hook(replace) for enc in net.enc]
    return net


def make(torch, mode):
    return attach(SN.BandSepNet(torch, norm="stream", mask="complex2"), mode)


def load(path, device=None):
    torch = SN._torch()
    device = device or ("cuda" if torch.cuda.is_available() else "cpu")
    ck = torch.load(path, map_location=device)
    if ck.get("arch") != "band" or ck.get("norm") != "stream" or ck.get("mask") != "complex2":
        raise ValueError("causal complex baseline checkpoint required")
    if any(ck.get(k, 0) for k in ("brnn", "lookahead", "bidir", "dec_skip")):
        raise ValueError("baseline topology required")
    net = make(torch, ck.get("feature_mode", "absolute6")).to(device)
    net.modules.load_state_dict(ck["state"])
    net.modules.eval()
    SN.STATS.update(ck["stats"])
    return torch, net, device
