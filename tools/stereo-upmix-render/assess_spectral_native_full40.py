#!/usr/bin/env python3
"""CPU actual-PCM float/int8 chain qualification, original full40 and events.

Full evaluation requires an acoustically promoted fixed terminal. TRAIN smoke
reads only the original four fitting crops and one detected entry per song.
Scene remains dormant. Native audio and rounding residual are retained; this
does not establish loaded device deadlines, untouched20 or owner preference.
"""
import os
os.environ['CUDA_VISIBLE_DEVICES'] = '-1'
import argparse
import csv
import gc
import hashlib
import json
from pathlib import Path
import time

import numpy as np
import torch
import bsrnn_reference_eval as E
import separator_causal_reference_probe as P
import spectral_chain_mask_replay as R

MODES = ('float', 'int8')
CHAIN_CLI_SHA = '651f8dd4b1af60281a977d39a0532973c692663e43751904395025f26ff429b4'


def array_sha(value):
    if isinstance(value, torch.Tensor):
        value = value.detach().cpu().numpy()
    value = np.ascontiguousarray(value)
    h = hashlib.sha256(json.dumps([str(value.dtype), list(value.shape)]).encode())
    h.update(value.tobytes())
    return h.hexdigest()


def identities(a):
    modules = (E, E.ref, E.DT, E.MN, E.SN, E.H, E.T, E.gate, P, R, R.S)
    return dict(script_sha256=R.sha(Path(__file__)),
                helpers={Path(m.__file__).name: R.sha(m.__file__) for m in modules},
                executable_sha256=R.sha(a.executable), checkpoint_sha256=R.sha(a.checkpoint),
                build_manifest_sha256=R.sha(a.build_manifest),
                assets_manifest_sha256=R.sha(a.assets/'manifest.json'),
                blobs={mode: R.sha(a.assets/('unified_'+mode+'.bin')) for mode in MODES},
                split_sha256=R.sha(a.cache/'split.json'))


def finite_mean(values):
    values = np.asarray(values)
    return float(values[np.isfinite(values)].mean()) if np.isfinite(values).any() else None


@torch.inference_mode()
def source_row(result, roles):
    estimate = result['estimate']
    row = dict(**E.ref.acoustic_sanity(estimate, roles[0]),
               transmission={name: E.fraction(result['components'][i], roles[i])
                             for i, name in enumerate(E.ROLES)}, bands={}, parity=result['parity'])
    reference = E.SN.stft(torch, roles[0][None])
    predicted = E.SN.stft(torch, estimate[None])
    attributed = E.SN.stft(torch, result['components'][0][None])
    hz = torch.arange(reference.shape[2])*48000/2048
    for name, lo, hi in [('8-14k', 8000, 14000), ('14-24k', 14000, 24001)]:
        selected = (hz >= lo) & (hz < hi)
        target = reference[:, :, selected]
        row['bands'][name] = dict(error_nse=E.fraction(predicted[:, :, selected]-target, target),
                                  eligible_tx=E.fraction(attributed[:, :, selected], target))
    return row


@torch.inference_mode()
def event_row(song, kind, onset, result, roles):
    # Same original reanalysis, 10s warm-up, 24 half-second bins and -55dB floor.
    cut = E.SN.stft(torch, roles[0][None]).shape[1] - E.SN.stft(torch, roles[0, :, E.DT.WARM:][None]).shape[1]
    re = lambda wave: E.SN.stft(torch, wave[None])[:, cut:]
    axis = np.arange(24)*.5-4
    ret = E.DT.per_frame(torch, re(result['components'][0]), re(roles[0]), 24, E.DT.FLOOR_DB)
    leak = E.DT.per_frame(torch, re(result['components'][1]), re(roles[1]), 24, E.DT.FLOOR_DB)
    peak = leak[(axis >= 0) & (axis < 2)]
    row = dict(song=song, kind=kind, t=onset/48000, ret_pre=finite_mean(ret[axis < 0]),
               ret_post=finite_mean(ret[axis >= 0]), leak_pre=finite_mean(leak[axis < 0]),
               leak_0_2=finite_mean(peak), peak_0_2=float(np.nanmax(peak)) if np.isfinite(peak).any() else None,
               leak_steady=finite_mean(leak[axis >= 4]), parity=result['parity'],
               retention_half_second=E.clean(ret.tolist()), leak_half_second=E.clean(leak.tolist()))
    row['duck'] = row['ret_post']/row['ret_pre'] if row['ret_pre'] is not None and row['ret_pre'] > 0 and row['ret_post'] is not None else None
    if kind == 'entry':
        row['settle'] = E.clean(float(E.DT.settle(leak, axis, row['leak_steady']))) if row['leak_steady'] is not None else None
    return row


def transition_summary(rows):
    result = {}
    for kind in ('entry', 'exit', 'sustained'):
        selected = [r for r in rows if r['kind'] == kind]
        item = dict(count=len(selected), song_count=len({r['song'] for r in selected}))
        for key in ('ret_pre', 'ret_post', 'duck', 'leak_pre', 'leak_0_2', 'peak_0_2', 'leak_steady'):
            values = [r[key] for r in selected if r[key] is not None and np.isfinite(r[key])]
            item[key] = dict(valid=len(values), median=E.med(values),
                             p10=float(np.percentile(values, 10)) if values else None,
                             p90=float(np.percentile(values, 90)) if values else None)
        result[kind] = item
    return result


def detect_events(cache, songs, smoke):
    result = []
    for song in songs:
        path = cache/(song+'.npy')
        shape = E.MN.npy_header(str(path))[0]
        if shape[:2] != (7, 2):
            raise ValueError('Original seven-slot source cache required')
        vocal = E.MN.read_window(str(path), 0, shape[2], slots=[0, 1]).astype(np.float32).sum(0)/32768
        detected = [(kind, onset) for kind, onset in E.DT.events(vocal)
                    if onset-E.DT.PRE-E.DT.WARM >= 0 and onset+E.DT.POST <= shape[2]]
        if smoke:
            detected = [event for event in detected if event[0] == 'entry'][:1]
        result += [dict(song=song, kind=kind, onset=int(onset)) for kind, onset in detected]
    keys = {(r['song'], r['kind'], r['onset']) for r in result}
    if len(keys) != len(result):
        raise ValueError('Duplicate transition')
    if not result:
        raise ValueError('No transition coverage; smoke must exercise actual entries')
    return result


def verify_marker(marker, binding):
    saved = json.loads(marker.read_bytes())
    if saved['binding'] != binding:
        raise ValueError('Saved item has different true sources/protocol')
    for mode in MODES:
        for name, digest in saved['arms'][mode]['native_hashes'].items():
            if R.sha(marker.parent/mode/name) != digest:
                raise ValueError('Saved native waveform/mask changed')
    return saved


def assess_item(a, folder, mixture, roles, binding, meta=None, stems=None, event=None):
    marker = folder/'complete.json'
    if a.resume and marker.exists():
        return verify_marker(marker, binding)
    if folder.exists():
        raise FileExistsError('Incomplete item retained; use a fresh run rather than overwrite it')
    folder.mkdir(parents=True)
    arms = {}
    for mode in MODES:
        replay = R.execute(a.executable, a.assets/('unified_'+mode+'.bin'), mixture.numpy(), folder/mode)
        applied = R.attribute(replay, roles)
        item = dict(native_hashes=replay['hashes'], performance=replay['performance'],
                    source=source_row(applied, roles))
        if stems is not None:
            item['rows'] = E.rows_from_output(meta, stems, applied['output6'])
        if event is not None:
            item['event'] = event_row(event['song'], event['kind'], event['onset'], applied, roles)
        arms[mode] = item
        del replay, applied
        gc.collect()
    saved = dict(binding=binding, arms=arms)
    E.write(marker, saved)
    return saved


def main(a):
    torch.set_num_threads(2)
    assets = json.loads((a.assets/'manifest.json').read_bytes())
    identity = identities(a)
    build = json.loads(a.build_manifest.read_bytes())
    compiled = {Path(path).name: digest for path, digest in build['sources'].items()}
    if build['executable_sha256'] != identity['executable_sha256'] or compiled.get('spectral_chain_cli.c') != CHAIN_CLI_SHA:
        raise ValueError('Reviewed fixed.40 single-network chain build required')
    if assets['checkpoint_sha256'] != identity['checkpoint_sha256'] or assets['checkpoint_mode'] != 'spectral':
        raise ValueError('Checkpoint and exported assets differ')
    for mode in MODES:
        if identity['blobs'][mode] != assets['files']['unified_'+mode+'.bin']['sha256']:
            raise ValueError('Float/int8 blob changed')
    if a.train_smoke:
        if a.initial is None:
            raise ValueError('TRAIN smoke needs the original fixture directory')
        raw, fixture = P.data(a.cache, a.initial)
        songs = [r['song'] for r in fixture['windows']]
        scope = 'original four TRAIN8s plus first eligible detected entry per TRAIN song; no DEV or product promotion'
        provenance = dict(fixture_manifest_sha256=R.sha(a.initial/'manifest.json'))
        expected = None
    else:
        if a.broad is None or a.runs is None or a.moisesdb is None:
            raise ValueError('Full40 needs completed broad pair, original runs and raw MoisesDB')
        full = json.loads((a.broad/'full40/complete.json').read_bytes())
        audit = json.loads((a.broad/'audit.json').read_bytes())
        manifest = json.loads((a.broad/'manifest.json').read_bytes())
        if not full['complete'] or not full['acoustic_promotion_pass'] or not audit['verified'] or audit['smoke']:
            raise ValueError('Acoustically promoted, numerically verified terminal required')
        if full['protocol_sha256'] != R.sha(a.broad/'full40/protocol.json') or audit['protocol_sha256'] != R.sha(a.broad/'manifest.json'):
            raise ValueError('Broad protocol identity differs')
        if assets['checkpoint_step'] != 40000 or any(record['checkpoint_sha256'] != identity['checkpoint_sha256'] for record in (audit['arms']['spectral'], full['arms']['spectral'])):
            raise ValueError('Only the fixed audited spectral40k checkpoint is eligible')
        declared = json.loads((a.runs/'direct_object_full40/manifest.json').read_bytes())
        songs = list(declared['song_track_ids'].values())
        expected = {(r['song'], r['stem']) for r in E.gate.read(a.runs/'direct_object_full40/baseline/stems.csv')}
        if len(songs) != 40 or len(set(songs)) != 40 or len(expected) != 231 or set(songs) != {r['song'] for r in manifest['evaluation']['dev']}:
            raise ValueError('Original full40/231 coverage differs')
        original_events = json.loads((a.broad/'full40/spectral/transitions.json').read_bytes())['rows']
        expected_events = {(r['song'], r['kind'], int(round(r['t']*48000))) for r in original_events}
        if len(original_events) != 265 or len(expected_events) != 265:
            raise ValueError('Original265 transition coverage required')
        provenance = {str(path.relative_to(a.broad)): R.sha(path) for path in
                      (a.broad/'manifest.json', a.broad/'audit.json', a.broad/'full40/complete.json',
                       a.broad/'full40/protocol.json', a.broad/'full40/spectral/transitions.json')}
        provenance['declared_full40_sha256'] = R.sha(a.runs/'direct_object_full40/manifest.json')
        provenance['expected_stems_sha256'] = R.sha(a.runs/'direct_object_full40/baseline/stems.csv')
        scope = 'original inspected40DEVx60s/231stem rows and same265 events, actual native float/int8 full chain'
    events = detect_events(a.cache, songs, a.train_smoke)
    if not a.train_smoke and {(r['song'], r['kind'], r['onset']) for r in events} != expected_events:
        raise ValueError('Detected event inventory changed')
    cache_identity = {song: R.sha(a.cache/(song+'.npy')) for song in songs}
    protocol = dict(identity=identity, compiled_sources=compiled, provenance=provenance, cache_identity=cache_identity, scope=scope,
                    train_smoke=a.train_smoke, songs=songs, events=events, operating_point=.40,
                    gain='full40 0.7/dry mixture peak, unity source; original TRAIN/event cache gain; no rescue',
                    input='shared nearest-even PCM16x32768; older Python WAV uses truncationx32767',
                    source_labels='true four roles unchanged; rounding/sum residual separate from original four-source closure',
                    scoring_alignment_samples=1536, scene='computed but dormant, center zero',
                    gates='original13 absolute observed gates in both float/int8; quantized eligible SDR median loss<=.3dB/each<=1dB; same fixed-level and relative C/entry gates',
                    limits='source components are conditional operator diagnostics, not literal audible voice; preserved native6 output; fixed delay compensation is offline scoring only; no independent20/device/ears proof')
    if a.resume:
        if json.loads((a.out/'protocol.json').read_bytes()) != protocol:
            raise ValueError('Explicit resume protocol differs')
    else:
        if a.out.exists():
            raise FileExistsError('Preserve prior run')
        a.out.mkdir(parents=True)
        E.write(a.out/'protocol.json', protocol)
    protocol_sha = R.sha(a.out/'protocol.json')
    rows = {mode: [] for mode in MODES}
    extractions = {mode: [] for mode in MODES}
    event_rows = {mode: [] for mode in MODES}
    for index, song in enumerate(songs):
        began = time.monotonic()
        meta = stems = None
        if a.train_smoke:
            wave = torch.from_numpy(raw[index])
            roles = torch.stack([wave[6], wave[0]+wave[1], wave[2:5].sum(0), wave[5]])
            mixture = roles.sum(0)
        else:
            track = songs[index]
            meta, stems = E.H.load_song(str(a.moisesdb/track), 60)
            n = min(w.shape[-1] for w in stems.values())
            stems = {key: value[:, :n] for key, value in stems.items()}
            mixture = torch.from_numpy(sum(stems.values()))
            gain = .7/(float(mixture.abs().max())+1e-9)
            roles = E.roles_from_stems(stems)*gain
            mixture = mixture*gain
        binding = dict(protocol_sha256=protocol_sha, song=song, mixture_sha256=array_sha(mixture),
                       true_roles_sha256=array_sha(roles), samples=mixture.shape[-1],
                       stem_sha256={key: array_sha(value) for key, value in stems.items()} if stems is not None else None)
        saved = assess_item(a, a.out/'songs'/song, mixture, roles, binding, meta, stems)
        for mode in MODES:
            item = saved['arms'][mode]
            extractions[mode].append(dict(song=song, **item['source']))
            if stems is not None:
                rows[mode] += item['rows']
        print('NATIVE_SPECTRAL_SONG', index+1, len(songs), 'seconds', time.monotonic()-began, flush=True)
        del mixture, roles, stems
        gc.collect()
    for index, event in enumerate(events):
        song, onset = event['song'], event['onset']
        start = onset-E.DT.PRE-E.DT.WARM
        raw_event = E.ref.read_crop(a.cache/(song+'.npy'), start, E.DT.WARM+E.DT.PRE+E.DT.POST)
        wave = torch.from_numpy(raw_event.astype(np.float32)/32768)
        roles = torch.stack([wave[6], wave[0]+wave[1], wave[2:5].sum(0), wave[5]])
        mixture = roles.sum(0)
        binding = dict(protocol_sha256=protocol_sha, event=event, mixture_sha256=array_sha(mixture), true_roles_sha256=array_sha(roles))
        saved = assess_item(a, a.out/'events'/(song+'_'+event['kind']+'_'+str(onset)), mixture, roles, binding, event=event)
        for mode in MODES:
            event_rows[mode].append(saved['arms'][mode]['event'])
        print('NATIVE_SPECTRAL_EVENT', index+1, len(events), flush=True)
        del mixture, roles, wave, raw_event
        gc.collect()
    arms = {}
    for mode in MODES:
        trans = transition_summary(event_rows[mode])
        gates = {}
        if not a.train_smoke:
            if len(rows[mode]) != 231 or {(r['song'], r['stem']) for r in rows[mode]} != expected:
                raise ValueError('Native original231 stem coverage differs')
            with (a.out/(mode+'_stems.csv')).open('w', newline='', encoding='utf-8') as handle:
                writer = csv.DictWriter(handle, fieldnames=list(rows[mode][0]))
                writer.writeheader(); writer.writerows(rows[mode])
            numeric = [{key: float('nan') if value is None else value for key, value in row.items()} for row in rows[mode]]
            summary = E.gate.summary(numeric)
            gates = E.gate.passes(summary, summary, 'full40')
            gates.pop('rear_rms_within_half_db')
            for statistic, threshold in [('median', .85), ('p10', .60)]:
                value = trans['entry']['duck'][statistic]
                gates['entry_'+statistic] = value is not None and value >= threshold
        else:
            summary = None
        arms[mode] = dict(extractions=extractions[mode], summary=summary, transitions=trans,
                          events=event_rows[mode], observed_gates=gates,
                          observed_gates_pass=not a.train_smoke and len(gates) == 13 and all(gates.values()))
    delta = [new['other_si_sdr_channel_mean_db']-old['other_si_sdr_channel_mean_db']
             for old, new in zip(extractions['float'], extractions['int8'])]
    quantization = dict(eligible_median_loss_at_most_03db=float(np.median(delta)) >= -.3,
                        no_eligible_crop_over_1db_loss=min(delta) >= -1)
    if not a.train_smoke:
        old, new = (arms[mode]['summary'] for mode in MODES)
        quantization['rear_level_within_half_db'] = abs(new['rear_rms_db']-old['rear_rms_db']) <= .5
        quantization['any_stem_swings_nonworse'] = len(new['swings']) <= len(old['swings'])
        for stem in ('guitar', 'other_keys', 'piano', 'bowed_strings'):
            x, y = new['shares'][stem], old['shares'][stem]
            quantization[stem+'_nonworse_001'] = x is not None and y is not None and x >= y-.01
        for stem in ('vocals', 'drums', 'bass'):
            x, y = new['shares'][stem], old['shares'][stem]
            quantization[stem+'_nonworse_0005'] = x is not None and y is not None and x <= y+.005
        for statistic in ('median', 'p10'):
            x, y = (arms[mode]['transitions']['entry']['duck'][statistic] for mode in ('int8', 'float'))
            quantization['entry_'+statistic+'_nonworse'] = x is not None and y is not None and x >= y
    if identities(a) != identity or any(R.sha(a.cache/(song+'.npy')) != digest for song, digest in cache_identity.items()):
        raise ValueError('Inputs/runtime/helpers changed during qualification')
    result = dict(complete=True, train_smoke=a.train_smoke, protocol_sha256=protocol_sha, arms=arms,
                  quantization_gates=quantization, eligible_paired_sdr_delta_db=delta,
                  native_acoustic_pass=not a.train_smoke and all(v['observed_gates_pass'] for v in arms.values()) and all(quantization.values()),
                  device_or_product_pass=False, limits=protocol['limits'])
    E.write(a.out/'complete.json', result)
    print('NATIVE_SPECTRAL_COMPLETE', json.dumps(dict(train_smoke=a.train_smoke, quantization=quantization,
                                                   native_acoustic_pass=result['native_acoustic_pass'])), flush=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('cache', 'checkpoint', 'assets', 'executable', 'build-manifest', 'out'):
        p.add_argument('--'+name, type=Path, required=True)
    for name in ('initial', 'broad', 'runs', 'moisesdb'):
        p.add_argument('--'+name, type=Path)
    p.add_argument('--train-smoke', action='store_true')
    p.add_argument('--resume', action='store_true')
    main(p.parse_args())
