"""Q4 protection mask: the one learned part of the second scene.

Per (t,f,channel) a causal mask m_prot in [0,1] = share of the bin that must stay in
front (lead vocal + bass + drum kit). Everything else is the rear candidate; width /
texture only shape its level (DSP, see q4_second_scene.py). Two-way softmax over
{protect, rest} on the Q3MaskNet trunk; targets from true stems (q3 cache: lead_vocal
+ bass + drums + high_percussion, only songs whose protect heads are clean) and from
teacher labels (q4 teacher cache: lead_vocal + bass + drums on mastered music).

Loss (directly the product metrics, both normalised, mixture-consistent):
  leak    = sum |m_rest * S_prot|^2 / sum |S_prot|^2     (protected material reaching the rear)
  support = sum |m_prot * S_rest|^2 / sum |S_rest|^2     (support wrongly kept in front)
  loss    = w_leak * leak + support,   w_leak = 3 (asymmetric: leakage is the product failure)

  python q4_protect.py train --caches R/q3-cache-a R/q4-teacher-cache-a --out R/q4-protect-a --epochs 8
  python q4_protect.py score --caches R/q3-cache-a --out R/q4-protect-a
"""
import argparse, hashlib, json, math, queue, random, threading, time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from q2_source_data import CLASSES
from q3_cache import load_song
from q3_masknet import Q3MaskNet, band_edges, spectra_batch, HOP, BINS, macs_per_frame
from r1_refiner_audio import analyze, synthesize

PROTECT_TRUE = ('lead_vocal', 'bass', 'drums', 'high_percussion')     # MoisesDB stems
PROTECT_TEACHER = ('lead_vocal', 'bass', 'drums')                     # teacher kit includes hats
W_LEAK = 3.0


class ProtectNet(Q3MaskNet):
    """Q3 trunk with a 2-way head; forward returns m_prot [B,T,2,513]."""
    def __init__(self, bands=32, width=64, layers=2):
        nn.Module.__init__(self)
        self.edges = band_edges(bands)
        self.widths = [b - a for a, b in zip(self.edges[:-1], self.edges[1:])]
        self.width, self.layers = width, layers
        self.enc = nn.ModuleList([nn.Linear(w * 4, width) for w in self.widths])
        self.pos = nn.Parameter(torch.zeros(bands, width))
        self.grus = nn.ModuleList([nn.GRU(width, width, batch_first=True) for _ in range(layers)])
        self.convs = nn.ModuleList([nn.Conv1d(width, width, 5, padding=2) for _ in range(layers)])
        self.norms = nn.ModuleList([nn.LayerNorm(width) for _ in range(2 * layers)])
        self.dec = nn.ModuleList([nn.Linear(width, w * 2) for w in self.widths])
        self.config = dict(bands=bands, width=width, layers=layers, sources=2, bins=BINS)

    def trunk(self, feats, hidden=None):
        B, T = feats.shape[:2]
        h = torch.stack([self.enc[i](feats[:, :, a:b].reshape(B, T, -1)) for i, (a, b) in
                         enumerate(zip(self.edges[:-1], self.edges[1:]))], 2) + self.pos
        nb = h.shape[2]
        new_hidden = []
        for l in range(self.layers):
            g = h.permute(0, 2, 1, 3).reshape(B * nb, T, self.width)
            g, hl = self.grus[l](g, None if hidden is None else hidden[l])
            new_hidden.append(hl)
            h = h + self.norms[2 * l](g.reshape(B, nb, T, self.width).permute(0, 2, 1, 3))
            c = self.convs[l](h.reshape(B * T, nb, self.width).transpose(1, 2)).transpose(1, 2)
            h = h + self.norms[2 * l + 1](F.leaky_relu(c, 0.1).reshape(B, T, nb, self.width))
        logits = torch.cat([self.dec[i](h[:, :, i]).reshape(B, T, 2, w) for i, w in enumerate(self.widths)], 3)
        return torch.sigmoid(logits), new_hidden

    def forward(self, feats):
        return self.trunk(feats)[0]


def protect_heads(meta):
    return PROTECT_TEACHER if meta.get('partition') == 'TEACHER' else PROTECT_TRUE


def usable(meta):
    """A song trains the protect mask only if every protect head is clean or absent-known."""
    heads = protect_heads(meta)
    for c in heads:
        k = CLASSES.index(c)
        if meta['heads_present'][k] and not meta['heads_clean'][k]:
            return False
    return True


PARTS = ('vocal', 'bass', 'drums')            # per-source leak terms; vocal weighted highest
PART_HEADS = dict(vocal=('lead_vocal',), bass=('bass',), drums=('drums', 'high_percussion'))
W_PART = dict(vocal=4.0, bass=1.0, drums=1.0)
LOSS_VERSION = 'v2-per-source-leak+solo-crops'


def make_crop(a, meta, frames, rng, augment=True):
    """Returns mix [N,2], parts {name: [N,2]} (protected sources present in this song), rest [N,2]."""
    n = frames * HOP
    start = rng.integers(0, max(1, a.shape[0] - n))
    x = a[start:start + n]
    if x.shape[0] < n:
        x = np.concatenate((x, np.zeros((n - x.shape[0], 13, 2), 'f4')), 0)
    mix, tg = x[:, 0].copy(), x[:, 1:].copy()
    heads = set(protect_heads(meta))
    parts = {}
    for name, hs in PART_HEADS.items():
        hs = [h for h in hs if h in heads]
        if hs:
            parts[name] = sum(tg[:, CLASSES.index(h)] for h in hs)
    prot = sum(parts.values()) if parts else np.zeros_like(mix)
    rest = mix - prot
    if augment:
        for k in parts:
            parts[k] = parts[k] * 10 ** (rng.uniform(-6, 6) / 20)
        rest = rest * 10 ** (rng.uniform(-6, 6) / 20)
        r = rng.random()
        if r < 0.12:                      # solo protected material: everything must stay in front
            rest = np.zeros_like(rest)
        elif r < 0.24:                    # no protected material: everything may go rear
            parts = {}
        if rng.random() < 0.5:
            parts = {k: v[:, ::-1].copy() for k, v in parts.items()}; rest = rest[:, ::-1].copy()
        if rng.random() < 0.5:
            parts = {k: -v for k, v in parts.items()}; rest = -rest
        gg = 10 ** (rng.uniform(-12, 0) / 20)
        parts = {k: v * gg for k, v in parts.items()}; rest = rest * gg
    prot = sum(parts.values()) if parts else np.zeros_like(rest)
    return (prot + rest).astype('f4'), {k: v.astype('f4') for k, v in parts.items()}, rest.astype('f4')


def loss_fn(m_prot, Sparts, Sr):
    """m_prot [B,T,2,513]; Sparts {name: complex [B,T,2,513]} (zeros where absent); Sr complex.
    leak_k normalised per source and per crop; absent sources (tiny power) are excluded."""
    Pr = Sr.real ** 2 + Sr.imag ** 2
    support = ((m_prot ** 2) * Pr).sum((1, 2, 3)) / Pr.sum((1, 2, 3)).clamp_min(1e-9)
    loss = support.clone(); leak_all = torch.zeros_like(support); nparts = torch.zeros_like(support)
    for name, Sp in Sparts.items():
        Pp = Sp.real ** 2 + Sp.imag ** 2
        tot = Pp.sum((1, 2, 3))
        present = (tot > 1e-6 * Pr.sum((1, 2, 3)).clamp_min(1e-9) + 1e-9).float()
        leak = (((1 - m_prot) ** 2) * Pp).sum((1, 2, 3)) / tot.clamp_min(1e-9) * present
        loss = loss + W_PART[name] * leak; leak_all = leak_all + leak; nparts = nparts + present
    return loss.mean(), (leak_all / nparts.clamp_min(1)).mean(), support.mean()


def prefetch(caches, songs, q):
    for cache, s, meta in songs:
        q.put((s, load_song(cache, s), meta))
    q.put(None)


@torch.no_grad()
def render_mask(model, mix, device, chunk_frames=4096):
    X = analyze(torch.from_numpy(np.ascontiguousarray(mix, dtype='f4'))).frames
    feats = model.features(X)
    out = torch.zeros(X.shape[1], 2, BINS)
    hidden = None
    for t0 in range(0, X.shape[1], chunk_frames):
        m, hidden = model.trunk(feats[:, t0:t0 + chunk_frames].to(device), hidden)
        out[t0:t0 + m.shape[1]] = m[0].cpu()
    return X[0], out                                    # [T,2,513] complex, [T,2,513] mask


def score_song(model, a, meta, device):
    mix = a[:, 0]
    prot = sum(a[:, 1 + CLASSES.index(c)] for c in protect_heads(meta))
    rest = mix - prot
    X, m = render_mask(model, mix, device)
    Sp, Sr = analyze(torch.from_numpy(np.ascontiguousarray(prot, dtype='f4'))).frames[0], analyze(torch.from_numpy(np.ascontiguousarray(rest, dtype='f4'))).frames[0]
    Pp, Pr = (Sp.real ** 2 + Sp.imag ** 2).double(), (Sr.real ** 2 + Sr.imag ** 2).double()
    md = m.double()
    leak = float((((1 - md) ** 2) * Pp).sum() / Pp.sum().clamp_min(1e-9))
    support = float(((md ** 2) * Pr).sum() / Pr.sum().clamp_min(1e-9))
    rear_kept = float((((1 - md) ** 2) * Pr).sum() / Pr.sum().clamp_min(1e-9))
    out = dict(leak_db=10 * math.log10(max(leak, 1e-12)), support_lost_db=10 * math.log10(max(support, 1e-12)),
               rear_kept_db=10 * math.log10(max(rear_kept, 1e-12)), mask_mean=float(m.mean()))
    heads = set(protect_heads(meta))
    for name, hs in PART_HEADS.items():
        hs = [h for h in hs if h in heads and meta['heads_present'][CLASSES.index(h)]]
        if not hs:
            continue
        part = sum(a[:, 1 + CLASSES.index(h)] for h in hs)
        Sk = analyze(torch.from_numpy(np.ascontiguousarray(part, dtype='f4'))).frames[0]
        Pk = (Sk.real ** 2 + Sk.imag ** 2).double()
        if float(Pk.sum()) <= 0:
            continue
        out[f'leak_{name}_db'] = 10 * math.log10(max(float((((1 - md) ** 2) * Pk).sum() / Pk.sum()), 1e-12))
        if name == 'vocal':                       # acceptance-style: the vocal alone through the mask
            _, ms = render_mask(model, part, device)
            out['solo_vocal_leak_db'] = 10 * math.log10(max(float((((1 - ms.double()) ** 2) * Pk).sum() / Pk.sum()), 1e-12))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('mode', choices=('train', 'score', 'smoke'))
    ap.add_argument('--caches', type=Path, nargs='+', required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--bands', type=int, default=32)
    ap.add_argument('--width', type=int, default=64)
    ap.add_argument('--layers', type=int, default=2)
    ap.add_argument('--epochs', type=int, default=8)
    ap.add_argument('--crops', type=int, default=32)
    ap.add_argument('--batch', type=int, default=8)
    ap.add_argument('--frames', type=int, default=1125)
    ap.add_argument('--lr', type=float, default=1e-3)
    ap.add_argument('--seed', type=int, default=20260917)
    ap.add_argument('--device', default='mps' if torch.backends.mps.is_available() else 'cpu')
    ap.add_argument('--limit-songs', type=int, default=None)
    ap.add_argument('--curve-songs', type=int, default=3)
    ap.add_argument('--resume', action='store_true')
    args = ap.parse_args()
    torch.manual_seed(args.seed); rng = np.random.default_rng(args.seed)
    songs = []
    for cache in args.caches:
        for p in sorted(cache.glob('*.json')):
            if p.name == 'manifest.json':
                continue
            m = json.load(open(p))
            songs.append((cache, p.stem, m))
    train = [s for s in songs if s[2].get('partition') in ('TRAIN', 'TEACHER') and usable(s[2])]
    dev = [s for s in songs if s[2].get('partition') == 'DEV' and s[2].get('strict_admitted') and usable(s[2])]
    if args.limit_songs:
        train, dev = train[:args.limit_songs], dev[:max(1, args.limit_songs // 4)]
    device = torch.device(args.device)
    model = ProtectNet(args.bands, args.width, args.layers).to(device)
    nparams = sum(p.numel() for p in model.parameters())
    print(f'ProtectNet params={nparams:,} MACs/frame~{macs_per_frame(model)["total"]:,} device={device} '
          f'train={len(train)} (teacher {sum(1 for s in train if s[2]["partition"]=="TEACHER")}) dev={len(dev)}', flush=True)
    args.out.mkdir(parents=True, exist_ok=True)

    if args.mode == 'smoke':
        cache, sid, meta = train[0]
        a = load_song(cache, sid)
        mix, parts, rest = make_crop(a, meta, args.frames, rng)
        X = spectra_batch([mix]); Sr = spectra_batch([rest])
        Sparts = {k: spectra_batch([parts.get(k, np.zeros_like(rest))]).to(device) for k in PARTS}
        m = model(model.features(X).to(device))
        loss, leak, sup = loss_fn(m, Sparts, Sr.to(device))
        print('smoke loss', float(loss), 'leak', float(leak), 'support', float(sup))
        print('score', score_song(model.eval(), a[:48000 * 20], meta, device))
        return

    if args.mode == 'score':
        cp = torch.load(args.out / 'latest.pt', map_location='cpu', weights_only=False); model.load_state_dict(cp['model']); model.eval()
        report = dict(schema='q4-protect-score-v1', checkpoint_sha256=cp.get('sha256'), epoch=cp.get('epoch'), songs={})
        for cache, sid, meta in dev:
            r = score_song(model, load_song(cache, sid), meta, device); report['songs'][sid] = dict(r, cache=str(cache))
            print(f"{sid[:8]} leak {r['leak_db']:+.1f} dB (vocal {r.get('leak_vocal_db', float('nan')):+.1f}, solo vocal {r.get('solo_vocal_leak_db', float('nan')):+.1f})  support lost {r['support_lost_db']:+.1f} dB  rear kept {r['rear_kept_db']:+.1f} dB", flush=True)
            (args.out / 'score.json').write_text(json.dumps(report, indent=1))
        ls = [r['leak_db'] for r in report['songs'].values()]; ss = [r['support_lost_db'] for r in report['songs'].values()]
        print(f'DEV median leak {np.median(ls):+.1f} dB (worst {max(ls):+.1f}), median support lost {np.median(ss):+.1f} dB')
        return

    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    start_epoch = 0
    if args.resume and (args.out / 'latest.pt').exists():
        cp = torch.load(args.out / 'latest.pt', map_location='cpu', weights_only=False); model.load_state_dict(cp['model']); opt.load_state_dict(cp['optimizer']); start_epoch = cp['epoch'] + 1
    (args.out / 'config.json').write_text(json.dumps(dict(vars(args), model=model.config, params=nparams, w_leak=W_LEAK, loss_version=LOSS_VERSION, w_part=W_PART,
                                                          train=[s[1] for s in train], dev=[s[1] for s in dev]), indent=1, default=str))
    log = open(args.out / 'train.log', 'a')
    steps_per_epoch = len(train) * math.ceil(args.crops / args.batch)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, args.epochs * steps_per_epoch, eta_min=args.lr / 10)
    for _ in range(start_epoch * steps_per_epoch):
        sched.step()
    for epoch in range(start_epoch, args.epochs):
        model.train(); order = list(train); random.Random(args.seed + epoch).shuffle(order)
        q = queue.Queue(maxsize=2); threading.Thread(target=prefetch, args=(None, order, q), daemon=True).start()
        t0 = time.time(); tot = tl = ts = 0.0; nsteps = 0; io_wait = 0.0
        while True:
            tw = time.time(); item = q.get(); io_wait += time.time() - tw
            if item is None:
                break
            sid, a, meta = item
            crops = [make_crop(a, meta, args.frames, rng) for _ in range(args.crops)]
            for b0 in range(0, args.crops, args.batch):
                batch = crops[b0:b0 + args.batch]
                X = spectra_batch([c[0] for c in batch]); Sr = spectra_batch([c[2] for c in batch])
                zero = np.zeros_like(batch[0][2])
                Sparts = {k: spectra_batch([c[1].get(k, zero) for c in batch]).to(device) for k in PARTS}
                m = model(model.features(X).to(device))
                loss, leak, sup = loss_fn(m, Sparts, Sr.to(device))
                opt.zero_grad(set_to_none=True); loss.backward(); nn.utils.clip_grad_norm_(model.parameters(), 5.0); opt.step(); sched.step()
                tot += float(loss); tl += float(leak); ts += float(sup); nsteps += 1
            if nsteps % 40 < math.ceil(args.crops / args.batch):
                print(f'epoch {epoch} step {nsteps}/{steps_per_epoch} loss {tot/nsteps:.4f} leak {10*math.log10(max(tl/nsteps,1e-9)):+.1f}dB support {10*math.log10(max(ts/nsteps,1e-9)):+.1f}dB {time.time()-t0:.0f}s io {io_wait:.0f}s', flush=True)
        model.eval(); curve = {}
        for cache, sid, meta in dev[:args.curve_songs]:
            curve[sid[:8]] = score_song(model, load_song(cache, sid), meta, device)
        row = dict(epoch=epoch, steps=nsteps, loss=tot / max(nsteps, 1), leak_db=10 * math.log10(max(tl / max(nsteps, 1), 1e-9)),
                   support_db=10 * math.log10(max(ts / max(nsteps, 1), 1e-9)), seconds=time.time() - t0, io_wait=io_wait, curve=curve)
        log.write(json.dumps(row) + '\n'); log.flush()
        print('EPOCH', json.dumps({k: (round(v, 3) if isinstance(v, float) else v) for k, v in row.items() if k != 'curve'}),
              {k: {kk: round(vv, 1) for kk, vv in v.items()} for k, v in curve.items()}, flush=True)
        state = dict(model=model.state_dict(), optimizer=opt.state_dict(), epoch=epoch, config=model.config, args=vars(args))
        torch.save(state, args.out / f'checkpoint-epoch{epoch:02d}.pt')
        state['sha256'] = hashlib.sha256(open(args.out / f'checkpoint-epoch{epoch:02d}.pt', 'rb').read()).hexdigest()
        torch.save(state, args.out / 'latest.pt')


if __name__ == '__main__':
    main()
