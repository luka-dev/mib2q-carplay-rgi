"""MoisesDB q3 cache -> HSSR training cache (train_hssr.py --extra-cache-root) with REAL classes.

The shipped HSSR model learned six classes from MUSDB's four stems, with 'other' split
by a coherence/flux heuristic; keys and backing vocals therefore had no identity of their
own (keys read as 'vocal', backing = lead).  Same 6 output slots, same C ABI, new meaning:
  0 lead_vocal | 1 bass | 2 drums + percussion + high_percussion | 3 guitar + other_plucked
  4 backing_vocal + other_vocal + strings_winds + other_unknown (background) | 5 keys
Rearability/lateral targets come from the rearable heads (everything but lead/bass/drums).
Features are train_hssr.make_features on the mastered mixture, unchanged.

  python q6_hssr_moises_cache.py --cache R/q3-cache-a --out R/q6-hssr-moises-cache-a [--workers 3] [--ids a b]
Output: <out>/{train,validation}/<song>.npz  (features float32 [T,368], targets [T,264], source_sha256)
"""
import argparse, json, math, os, sys, time
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
import train_hssr as H
from q2_source_data import CLASSES

# v2 (q6b): ALL vocals in class 0 again — a lead-only class let wide/doubled leads through as
# 'backing' (Joy lead leak −27 → −7 dB).  Keys keep their own class 5; guitar 3; strings/unknown 4.
CLASS_OF = {0: 0, 1: 0, 2: 0, 3: 1, 4: 2, 6: 2, 5: 2, 7: 3, 10: 3, 9: 4, 11: 4, 8: 5}
REARABLE = (5, 6, 7, 8, 9, 10, 11)          # no vocals of any kind
BACKGROUND_LIKE = (8, 9, 11)                # the 'background/fx' share of the rearable material
VOCAL_HEADS = (0, 1, 2)
eps = 1.0e-8


def band_aggregates(stereo):
    spec = H.stft(np.ascontiguousarray(stereo, dtype=np.float32))
    agg = H.aggregate_bands(spec)
    return spec, agg


def make_targets(get_stem, present):
    """get_stem(k) -> float32 [N, 2] for head k (one head in memory at a time; the NAS has 7 GB)."""
    frames = None
    energy = {}; pl = {}; pr = {}
    rear_spec_sum = None
    for k in present:
        spec, agg = band_aggregates(get_stem(k))
        if frames is None: frames = spec.shape[0]
        energy[k] = (agg['pl'] + agg['pr'])[:frames]; pl[k] = agg['pl'][:frames]; pr[k] = agg['pr'][:frames]
        if k in REARABLE:
            rear_spec_sum = spec[:frames] if rear_spec_sum is None else rear_spec_sum + spec[:frames]
        del spec
    total = sum(energy.values()) + eps
    classes = np.zeros((frames, H.BANDS, H.CLASSES), np.float32)
    for k, e in energy.items():
        classes[:, :, CLASS_OF[k]] += e / total
    classes /= np.sum(classes, axis=2, keepdims=True) + eps
    lead = classes[:, :, 0]; bass = classes[:, :, 1]
    drums_head = energy.get(4, np.zeros_like(total)) / total
    rearable = sum(energy[k] for k in energy if k in REARABLE) / total if any(k in REARABLE for k in energy) else np.zeros_like(total)
    background = sum(energy[k] for k in energy if k in BACKGROUND_LIKE) / total if any(k in BACKGROUND_LIKE for k in energy) else np.zeros_like(total)
    background_share = background / (rearable + eps)
    rearability = None   # computed after the rearable-sum spatial analysis below
    if rear_spec_sum is not None:
        ragg = H.aggregate_bands(rear_spec_sum)
        rpl, rpr = ragg['pl'], ragg['pr']
        rcross = np.sqrt(ragg['cross_re'] ** 2 + ragg['cross_im'] ** 2)
        rear_coherence = np.clip(rcross / np.sqrt(rpl * rpr + eps), 0.0, 1.0)
        lateral = np.clip((np.sqrt(rpl + eps) - np.sqrt(rpr + eps)) / (np.sqrt(rpl + eps) + np.sqrt(rpr + eps)), -1.0, 1.0)
        # v3 (q6d): V11's spatial background/fx split applied to the REARABLE material, so a wide or
        # diffuse guitar/keys part gets full rearability (v2 gave guitar a flat 0.2 -> in-mix rear collapse).
        rear_edge = np.abs(rpl - rpr) / (rpl + rpr + eps)
        rear_diffuse = 1.0 - rear_coherence
        rear_flux = np.clip(ragg['flux'], 0.0, 1.0)
        fx_share = np.clip(rear_diffuse * (1.0 - rear_flux), 0.0, 0.75)
        bg_share = np.clip(0.25 + 0.45 * rear_diffuse + 0.30 * rear_edge, 0.0, 1.0) * (1.0 - fx_share)
        spatial_share = np.clip(bg_share + fx_share, 0.0, 1.0)
    else:
        rear_coherence = np.ones((frames, H.BANDS), np.float32); lateral = np.zeros((frames, H.BANDS), np.float32)
        spatial_share = np.zeros((frames, H.BANDS), np.float32)
    rearability = np.clip(np.power(rearable, 0.70) * np.power(1.0 - lead, 2.0) * np.power(1.0 - bass, 1.5)
                          * (1.0 - 0.65 * drums_head) * (0.20 + 0.80 * np.maximum(spatial_share, background_share)), 0.0, 1.0).astype(np.float32)
    per_band = np.concatenate([classes, rearability[:, :, None], lateral[:, :, None]], axis=2).reshape(frames, H.BANDS * 8)
    tot_frames = np.sum(total, axis=1)
    def gshare(k): return (np.sum(energy[k], axis=1) / tot_frames) if k in energy else np.zeros(frames, np.float32)
    lead_g, bass_g = gshare(0), gshare(3)
    keys_g = gshare(8)
    class_entropy = -np.sum(classes * np.log(classes + eps), axis=2) / math.log(H.CLASSES)
    global_rear = np.sum(rearability * total, axis=1) / np.sum(total, axis=1)
    scene_stability = np.ones(frames, np.float32)
    if frames > 1:
        scene_stability[1:] = np.clip(1.0 - np.mean(np.abs(np.diff(rearability, axis=0)), axis=1), 0.0, 1.0)
    global_targets = np.stack([lead_g, bass_g, np.mean(rear_coherence, axis=1), 1.0 - np.mean(class_entropy, axis=1),
                               global_rear, scene_stability, keys_g, np.clip(1.0 - lead_g - 0.5 * bass_g, 0.0, 1.0)], axis=1)
    result = np.concatenate([per_band, global_targets], axis=1).astype(np.float32)
    assert result.shape[1] == H.OUTPUTS, result.shape
    return np.nan_to_num(result, nan=0.0, posinf=1.0, neginf=0.0)


def convert(cache, sid, out_dir, split, reuse=None, routing='universal-v5'):
    meta = json.load(open(cache / f'{sid}.json'))
    a = np.load(cache / f'{sid}.npy', mmap_mode='r')          # float16 [N, 1+P, 2], read lazily
    stored = meta.get('stored_heads') or list(range(len(CLASSES)))
    present = [k for k in stored if meta['heads_present'][k]]
    prev = None
    if reuse is not None:
        for sub in ('train', 'validation'):
            q = reuse / sub / f'{sid}.npz'
            if q.exists(): prev = q; break
    if prev is not None:
        with np.load(prev) as c: features_sel = c['features'].astype(np.float32)   # already decimated
    else:
        mix = np.asarray(a[:, 0], dtype=np.float32)
        spec_mix = H.stft(np.ascontiguousarray(mix))
        features = H.make_features(mix.mean(axis=1).astype(np.float32), spec_mix)
        del spec_mix, mix
        features_sel = None
    get_stem = lambda k: np.asarray(a[:, 1 + stored.index(k)], dtype=np.float32)
    targets = make_targets(get_stem, present)
    if features_sel is None:
        frames = min(features.shape[0], targets.shape[0])
        select = np.arange(H.CONTROL_HOPS - 1, frames, H.CONTROL_HOPS)
        features_sel = features[select].astype(np.float32); targets_sel = targets[select].astype(np.float32)
    else:
        select = np.arange(H.CONTROL_HOPS - 1, targets.shape[0], H.CONTROL_HOPS)
        n = min(len(select), features_sel.shape[0])
        features_sel = features_sel[:n]; targets_sel = targets[select[:n]].astype(np.float32)
    if routing == 'universal-v5':      # V11-style coverage target, computed offline and locked
        targets_sel = H.apply_universal_v5_routing_targets(features_sel, targets_sel).astype(np.float32)
    elif routing == 'edm-v2':
        targets_sel = H.apply_edm_v2_routing_targets(features_sel, targets_sel).astype(np.float32)
    out = out_dir / split / f'{sid}.npz'
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix('.tmp.npz')
    np.savez_compressed(tmp, features=features_sel, targets=targets_sel, routing_locked=np.array(routing != 'classic'),
                        source_sha256=np.array(meta['sha256']), song_id=np.array(sid), partition=np.array(split),
                        heads_present=np.array(meta['heads_present']), classes=np.array('vocals|bass|drums+perc|guitar+plucked|strings+unknown|keys'), routing=np.array(routing))
    os.replace(tmp, out)
    return out, targets_sel.shape[0], present


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cache', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--ids', nargs='*', default=None, help='song id prefixes (default: all)')
    ap.add_argument('--override', nargs='*', default=[], help='id=train|validation (smoke tests)')
    ap.add_argument('--workers', type=int, default=1)
    ap.add_argument('--index', type=int, nargs='*', default=None, help='worker shard: take songs where i %% workers in these')
    ap.add_argument('--reuse-features', type=Path, default=None, help='previous q6 cache dir: reuse its features, recompute targets only')
    ap.add_argument('--routing', choices=['classic', 'edm-v2', 'universal-v5'], default='universal-v5')
    args = ap.parse_args()
    metas = {p.stem: json.load(open(p)) for p in sorted(args.cache.glob('*.json')) if p.name != 'manifest.json'}
    ids = sorted(metas)
    if args.ids: ids = [s for s in ids if any(s.startswith(p) for p in args.ids)]
    if args.index is not None: ids = [s for i, s in enumerate(ids) if i % args.workers in args.index]
    over = dict(o.split('=') for o in args.override)
    split_of = {'TRAIN': 'train', 'DEV': 'validation', 'TEST': None}
    t0 = time.time(); done = 0
    for i, sid in enumerate(ids):
        split = over.get(sid[:8], over.get(sid, split_of.get(metas[sid].get('partition', 'TRAIN'))))
        if split is None: print(f'[{i+1}/{len(ids)}] {sid[:8]} TEST skipped'); continue
        if (args.out / split / f'{sid}.npz').exists(): print(f'[{i+1}/{len(ids)}] {sid[:8]} exists'); continue
        t1 = time.time()
        out, n, present = convert(args.cache, sid, args.out, split, reuse=args.reuse_features, routing=args.routing)
        done += 1
        print(f'[{i+1}/{len(ids)}] {sid[:8]} {split:10s} {n} frames heads={len(present)} {time.time()-t1:.0f}s (total {time.time()-t0:.0f}s)', flush=True)
    print('done', done)


if __name__ == '__main__':
    main()
