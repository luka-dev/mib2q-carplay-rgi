"""Q3 mask-net: causal per-bin 13-way softmax mask (12 identity heads + residual).

Research trainer for the Mac (MPS) over the NAS q3 cache. Not a sealed owner,
not a native export. Design (see docs-obsidian/Sources/UPMIX_INDEPENDENT_AUDIT_2026-09-17.md §7):
  frontend  : frozen r1_refiner_audio analysis, FFT1024/hop256 periodic Hann (CPU)
  features  : per bin log|X_L|^2, log|X_R|^2, cos/sin IPD  (phase-invariant)
  net       : band-split (quadratic bands) -> per-band Linear -> [GRU over time (shared,
              causal) + Conv1d over bands] x layers -> per-band Linear -> per-bin,
              per-channel logits over 13 sources -> softmax  (sum-to-mixture exact)
  output    : Y_k = m_k * X  (complex, per channel)
  loss      : complex MSE + 0.1 magnitude L1, per crop per head normalised by
              max(P_k, 0.01 P_mix); unknown (bleed) heads excluded; residual slot
              target = X - sum(targets)
  data      : random 6 s crops, per-head gain +-6 dB, L/R swap, polarity, head
              dropout, global gain. No compressor (would break sum-to-mix).

  python q3_masknet.py train --cache R/q3-cache-a --out R/q3-masknet-a --epochs 10
  python q3_masknet.py score --cache R/q3-cache-a --out R/q3-masknet-a --dev-scores R/q2-full-source-dev-predicted-a/attempts
"""
import argparse, hashlib, json, math, random, threading, queue, time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F

from q2_source_data import CLASSES
from r1_refiner_audio import analyze, synthesize

K = len(CLASSES)          # 12 identity heads
S = K + 1                 # + residual slot
BINS, HOP = 513, 256
EPS = 1e-9


def band_edges(bands):
    """Quadratic band edges over 513 bins, first bands 1-2 bins wide, all non-empty."""
    edges = [0]
    for i in range(1, bands + 1):
        e = int(round(BINS * (i / bands) ** 2))
        edges.append(max(e, edges[-1] + 1))
    edges[-1] = BINS
    return edges


class Q3MaskNet(nn.Module):
    def __init__(self, bands=32, width=64, layers=2):
        super().__init__()
        self.edges = band_edges(bands)
        self.widths = [b - a for a, b in zip(self.edges[:-1], self.edges[1:])]
        self.width, self.layers = width, layers
        self.enc = nn.ModuleList([nn.Linear(w * 4, width) for w in self.widths])
        self.pos = nn.Parameter(torch.zeros(bands, width))
        self.grus = nn.ModuleList([nn.GRU(width, width, batch_first=True) for _ in range(layers)])
        self.convs = nn.ModuleList([nn.Conv1d(width, width, 5, padding=2) for _ in range(layers)])
        self.norms = nn.ModuleList([nn.LayerNorm(width) for _ in range(2 * layers)])
        self.dec = nn.ModuleList([nn.Linear(width, w * 2 * S) for w in self.widths])
        self.config = dict(bands=bands, width=width, layers=layers, sources=S, bins=BINS)

    def features(self, X):
        """X complex64 [B,T,2,513] -> [B,T,513,4] float32."""
        p = X.real ** 2 + X.imag ** 2
        lp = torch.log(p + 1e-7)
        phase = X[..., 0, :] * X[..., 1, :].conj()
        mag = torch.sqrt(p[..., 0, :] * p[..., 1, :] + 1e-14)
        return torch.stack((lp[..., 0, :], lp[..., 1, :], phase.real / mag, phase.imag / mag), -1)

    def forward(self, feats):
        """feats [B,T,513,4] -> mask [B,T,2,513,S] (softmax over S)."""
        B, T = feats.shape[:2]
        h = torch.stack([self.enc[i](feats[:, :, a:b].reshape(B, T, -1)) for i, (a, b) in
                         enumerate(zip(self.edges[:-1], self.edges[1:]))], 2) + self.pos       # [B,T,bands,D]
        nb = h.shape[2]
        for l in range(self.layers):
            g = h.permute(0, 2, 1, 3).reshape(B * nb, T, self.width)
            g, _ = self.grus[l](g)
            h = h + self.norms[2 * l](g.reshape(B, nb, T, self.width).permute(0, 2, 1, 3))
            c = self.convs[l](h.reshape(B * T, nb, self.width).transpose(1, 2)).transpose(1, 2)
            h = h + self.norms[2 * l + 1](F.leaky_relu(c, 0.1).reshape(B, T, nb, self.width))
        logits = torch.cat([self.dec[i](h[:, :, i]).reshape(B, T, 2, w, S) for i, w in enumerate(self.widths)], 3)
        return torch.softmax(logits, -1)


def macs_per_frame(model):
    b, D, L = len(model.widths), model.width, model.layers
    enc = sum(w * 4 * D for w in model.widths)
    gru = L * b * 3 * D * (2 * D)
    conv = L * b * 5 * D * D
    dec = sum(D * w * 2 * S for w in model.widths)
    return dict(encoder=enc, gru=gru, conv=conv, decoder=dec, total=enc + gru + conv + dec,
                per_second=(enc + gru + conv + dec) * 48000 / HOP)


# ----------------------------------------------------------------------------- data

from q3_cache import load_song  # expands compact/legacy cache layouts to [N,13,2]


def make_crop(a, meta, frames, rng, augment=True):
    """Return mix [N,2], targets [N,12,2], residual [N,2], known [12] for one crop."""
    n = frames * HOP
    start = rng.integers(0, max(1, a.shape[0] - n))
    x = a[start:start + n]
    if x.shape[0] < n:
        x = np.concatenate((x, np.zeros((n - x.shape[0], 13, 2), 'f4')), 0)
    tg = x[:, 1:].copy()
    residual = x[:, 0] - tg.sum(1)
    known = np.array(meta['heads_clean'], bool) | ~np.array(meta['heads_present'], bool)   # absent clean heads are known zeros
    if augment:
        g = 10 ** (rng.uniform(-6, 6, K) / 20)
        drop = (rng.random(K) < 0.15) & known & np.array(meta['heads_present'], bool)
        g[drop] = 0.0
        tg *= g[None, :, None]
        if rng.random() < 0.5:
            tg, residual = tg[:, :, ::-1].copy(), residual[:, ::-1].copy()
        if rng.random() < 0.5:
            tg, residual = -tg, -residual
        gg = 10 ** (rng.uniform(-12, 0) / 20)
        tg *= gg
        residual = residual * gg
    mix = tg.sum(1) + residual
    return mix, tg, residual, known


def spectra_batch(waves):
    """list of [N,2] float32 -> complex64 [B,T,2,513] (CPU frozen analysis)."""
    return torch.stack([analyze(torch.from_numpy(np.ascontiguousarray(w))).frames[0] for w in waves], 0)


def loss_fn(mask, X, T, R, known):
    """mask [B,T,2,513,S]; X [B,T,2,513] complex; T [B,K,T,2,513] complex; R [B,T,2,513]; known [B,K] bool."""
    Xr = torch.view_as_real(X)                                   # [B,T,2,513,2]
    Y = mask.unsqueeze(-1) * Xr.unsqueeze(-2)                     # [B,T,2,513,S,2]
    tgt = torch.cat((torch.view_as_real(T).permute(0, 2, 3, 4, 1, 5), torch.view_as_real(R).unsqueeze(-2)), -2)
    pmix = (Xr ** 2).sum(-1).mean((1, 2, 3))                     # [B]
    ptgt = (tgt ** 2).sum(-1).mean((1, 2, 3))                    # [B,S]
    den = torch.maximum(ptgt, 0.01 * pmix[:, None]).clamp_min(EPS)
    err = ((Y - tgt) ** 2).sum(-1).mean((1, 2, 3))               # [B,S]
    magl1 = (torch.sqrt((Y ** 2).sum(-1) + 1e-12) - torch.sqrt((tgt ** 2).sum(-1) + 1e-12)).abs().mean((1, 2, 3))
    w = torch.cat((known, torch.ones_like(known[:, :1])), 1).float()
    per = (err / den + 0.1 * magl1 / torch.sqrt(den)) * w
    return per.sum() / w.sum().clamp_min(1), (err / den * w).sum() / w.sum().clamp_min(1)


def prefetch(cache, songs, manifest, q):
    for s in songs:
        q.put((s, load_song(cache, s), manifest[s]))
    q.put(None)


# ----------------------------------------------------------------------------- score

@torch.no_grad()
def render_song(model, a, device, chunk_frames=4096):
    """Whole-song inference with carried GRU state: returns sources [N,12,2] float32 (input-aligned)."""
    x = torch.from_numpy(np.ascontiguousarray(a[:, 0]))
    X = analyze(x).frames                                         # [1,T,2,513]
    Tn = X.shape[1]
    out = torch.zeros(Tn, 2, BINS, K, dtype=torch.complex64)
    feats = model.features(X)
    # stream in chunks with explicit state carry (GRU hidden), zero lookahead
    hidden = [None] * model.layers
    for t0 in range(0, Tn, chunk_frames):
        f = feats[:, t0:t0 + chunk_frames].to(device)
        B, T = f.shape[:2]
        h = torch.stack([model.enc[i](f[:, :, s:e].reshape(B, T, -1)) for i, (s, e) in
                         enumerate(zip(model.edges[:-1], model.edges[1:]))], 2) + model.pos
        nb = h.shape[2]
        for l in range(model.layers):
            g = h.permute(0, 2, 1, 3).reshape(B * nb, T, model.width)
            g, hidden[l] = model.grus[l](g, hidden[l])
            h = h + model.norms[2 * l](g.reshape(B, nb, T, model.width).permute(0, 2, 1, 3))
            c = model.convs[l](h.reshape(B * T, nb, model.width).transpose(1, 2)).transpose(1, 2)
            h = h + model.norms[2 * l + 1](F.leaky_relu(c, 0.1).reshape(B, T, nb, model.width))
        logits = torch.cat([model.dec[i](h[:, :, i]).reshape(B, T, 2, w, S) for i, w in enumerate(model.widths)], 3)
        m = torch.softmax(logits, -1)[0, ..., :K].cpu()           # [T,2,513,K]
        out[t0:t0 + T] = m * X[0, t0:t0 + T].unsqueeze(-1)
    n = a.shape[0]
    srcs = np.stack([synthesize(out[..., k].unsqueeze(0), n).numpy() for k in range(K)], 1)
    return srcs                                                   # [N,K,2]


def score_song(model, a, device):
    y = render_song(model, a, device).astype('f8')
    s = a[:, 1:].astype('f8')
    ss = (s * s).sum(0)
    nmse = ((y - s) ** 2).sum(0) / np.maximum(ss, 1e-30)
    pout = (y * y).sum(0)
    return dict(nmse=nmse.tolist(), out_minus_target_db=(10 * np.log10(np.maximum(pout, 1e-30) / np.maximum(ss, 1e-30))).tolist(),
                out_db_re_mix=(10 * np.log10(np.maximum(pout, 1e-30) / max(float((a[:, 0].astype('f8') ** 2).sum()), 1e-30))).tolist(),
                target_power=(ss / len(a)).tolist())


# ----------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('mode', choices=('train', 'score', 'smoke'))
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--dev-scores', type=Path, default=None)
    ap.add_argument('--bands', type=int, default=32)
    ap.add_argument('--width', type=int, default=64)
    ap.add_argument('--layers', type=int, default=2)
    ap.add_argument('--epochs', type=int, default=10)
    ap.add_argument('--crops', type=int, default=16, help='crops per song per epoch')
    ap.add_argument('--batch', type=int, default=8)
    ap.add_argument('--frames', type=int, default=1125, help='crop length in hops (1125 = 6 s)')
    ap.add_argument('--lr', type=float, default=1e-3)
    ap.add_argument('--seed', type=int, default=20260917)
    ap.add_argument('--device', default='mps' if torch.backends.mps.is_available() else 'cpu')
    ap.add_argument('--train-set', default='all', choices=('all', 'strict'))
    ap.add_argument('--curve-songs', type=int, default=3)
    ap.add_argument('--limit-songs', type=int, default=None)
    ap.add_argument('--resume', action='store_true')
    args = ap.parse_args()
    torch.manual_seed(args.seed)
    rng = np.random.default_rng(args.seed)
    # sidecars are authoritative while the cache build is still running
    manifest = {p.stem: json.load(open(p)) for p in sorted(args.cache.glob('*.json')) if p.name != 'manifest.json'}
    train = sorted(s for s, m in manifest.items() if m['partition'] == 'TRAIN' and (args.train_set == 'all' or m['strict_admitted']))
    dev = sorted(s for s, m in manifest.items() if m['partition'] == 'DEV' and m['strict_admitted'])
    if args.limit_songs:
        train, dev = train[:args.limit_songs], dev[:max(1, args.limit_songs // 4)]
    device = torch.device(args.device)
    model = Q3MaskNet(args.bands, args.width, args.layers).to(device)
    nparams = sum(p.numel() for p in model.parameters())
    print(f'Q3MaskNet bands={args.bands} width={args.width} layers={args.layers} params={nparams:,} MACs/frame={macs_per_frame(model)["total"]:,} '
          f'({macs_per_frame(model)["per_second"]/1e6:.0f} M MAC/s) device={device} train={len(train)} dev={len(dev)}', flush=True)
    args.out.mkdir(parents=True, exist_ok=True)

    if args.mode == 'smoke':
        a = load_song(args.cache, train[0])
        mix, tg, res, known = make_crop(a, manifest[train[0]], args.frames, rng)
        X = spectra_batch([mix]); T = torch.stack([spectra_batch(list(tg.transpose(1, 0, 2)))]); R = spectra_batch([res])
        feats = model.features(X).to(device)
        mask = model(feats)
        loss, base = loss_fn(mask.cpu(), X, T, R, torch.from_numpy(known)[None])
        print('smoke loss', float(loss), float(base), 'mask sum-to-one err', float((mask.sum(-1) - 1).abs().max()))
        srcs = render_song(model, a[:48000 * 20], device)
        print('render ok', srcs.shape, 'closure err', float(np.abs(srcs.sum(1) - a[:48000 * 20, 0]).max()))
        return

    if args.mode == 'score':
        cp = torch.load(args.out / 'latest.pt', map_location='cpu', weights_only=False)
        model.load_state_dict(cp['model']); model.eval()
        dev_cells = {}
        if args.dev_scores:
            for p in sorted(args.dev_scores.glob('*/score.json')):
                s = json.load(open(p)); dev_cells[s['song_id']] = s['whole_song']['families']
        report = dict(schema='q3-masknet-score-v1', checkpoint_sha256=cp.get('sha256'), epoch=cp.get('epoch'), songs={})
        for i, song in enumerate(dev):
            r = score_song(model, load_song(args.cache, song), device)
            if song in dev_cells:
                r['sealed'] = {c['identity']: dict(active=c['active'], nmse_predicted=c['nmse'], nmse_raw=c['controls_nmse']['raw-pcm'],
                                                   nmse_source_relative=c['controls_nmse']['source-relative']) for c in dev_cells[song]}
            report['songs'][song] = r
            print(f'[{i+1}/{len(dev)}] {song[:8]} ' + ' '.join(f'{CLASSES[k][:6]}={r["nmse"][k][0]:.2f}' for k in range(K)), flush=True)
            (args.out / 'score.json').write_text(json.dumps(report, indent=1))
        return

    # ---- train
    opt = torch.optim.Adam(model.parameters(), lr=args.lr)
    start_epoch = 0
    if args.resume and (args.out / 'latest.pt').exists():
        cp = torch.load(args.out / 'latest.pt', map_location='cpu', weights_only=False)
        model.load_state_dict(cp['model']); opt.load_state_dict(cp['optimizer']); start_epoch = cp['epoch'] + 1
        rng = np.random.default_rng(args.seed + start_epoch)
        print('resumed from epoch', cp['epoch'], flush=True)
    (args.out / 'config.json').write_text(json.dumps(dict(vars(args), out=str(args.out), cache=str(args.cache), model=model.config,
                                                          params=nparams, macs=macs_per_frame(model), train=train, dev=dev), indent=1, default=str))
    log = open(args.out / 'train.log', 'a')
    steps_per_epoch = len(train) * math.ceil(args.crops / args.batch)
    sched = torch.optim.lr_scheduler.CosineAnnealingLR(opt, args.epochs * steps_per_epoch, eta_min=args.lr / 10)
    for _ in range(start_epoch * steps_per_epoch):
        sched.step()
    for epoch in range(start_epoch, args.epochs):
        model.train()
        order = list(train); random.Random(args.seed + epoch).shuffle(order)
        q = queue.Queue(maxsize=2)
        threading.Thread(target=prefetch, args=(args.cache, order, manifest, q), daemon=True).start()
        t0 = time.time(); tot = 0.0; totb = 0.0; nsteps = 0; io_wait = 0.0
        while True:
            tw = time.time(); item = q.get(); io_wait += time.time() - tw
            if item is None:
                break
            song, a, meta = item
            crops = [make_crop(a, meta, args.frames, rng) for _ in range(args.crops)]
            for b0 in range(0, args.crops, args.batch):
                batch = crops[b0:b0 + args.batch]
                X = spectra_batch([c[0] for c in batch])
                T = torch.stack([spectra_batch(list(c[1].transpose(1, 0, 2))) for c in batch])
                R = spectra_batch([c[2] for c in batch])
                known = torch.from_numpy(np.stack([c[3] for c in batch]))
                feats = model.features(X).to(device)
                mask = model(feats)
                loss, base = loss_fn(mask, X.to(device), T.to(device), R.to(device), known.to(device))
                opt.zero_grad(set_to_none=True)
                loss.backward()
                nn.utils.clip_grad_norm_(model.parameters(), 5.0)
                opt.step(); sched.step()
                tot += float(loss); totb += float(base); nsteps += 1
            if nsteps % 20 < math.ceil(args.crops / args.batch):
                print(f'epoch {epoch} step {nsteps}/{steps_per_epoch} loss {tot/nsteps:.4f} base {totb/nsteps:.4f} '
                      f'{time.time()-t0:.0f}s io_wait {io_wait:.0f}s', flush=True)
        model.eval()
        curve = {}
        for song in dev[:args.curve_songs]:
            r = score_song(model, load_song(args.cache, song), device)
            curve[song] = dict(nmse=[(v[0] if tp[0] > 0 else None) for v, tp in zip(r['nmse'], r['target_power'])])
        row = dict(epoch=epoch, steps=nsteps, loss=tot / max(nsteps, 1), base=totb / max(nsteps, 1), seconds=time.time() - t0,
                   io_wait=io_wait, lr=sched.get_last_lr()[0], curve=curve)
        log.write(json.dumps(row) + '\n'); log.flush()
        print('EPOCH', json.dumps({k: v for k, v in row.items() if k != 'curve'}),
              {s[:8]: {CLASSES[k][:6]: round(v, 2) for k, v in enumerate(c['nmse']) if v is not None} for s, c in curve.items()}, flush=True)
        state = dict(model=model.state_dict(), optimizer=opt.state_dict(), epoch=epoch, config=model.config, args=vars(args))
        torch.save(state, args.out / f'checkpoint-epoch{epoch:02d}.pt')
        state['sha256'] = hashlib.sha256(open(args.out / f'checkpoint-epoch{epoch:02d}.pt', 'rb').read()).hexdigest()
        torch.save(state, args.out / 'latest.pt')
        json.dump(dict(epoch=epoch, path=f'checkpoint-epoch{epoch:02d}.pt', sha256=state['sha256']), open(args.out / 'latest.json', 'w'))


if __name__ == '__main__':
    main()
