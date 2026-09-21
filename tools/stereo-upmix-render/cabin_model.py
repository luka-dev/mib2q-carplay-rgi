"""OBSOLETE: historical transposed six-bus model, NOT FOR TUNING.

Use tools/soundamp-virtual-cabin/amp_music_report.py and amp_dsp_chain.py.
This code is retained only to reproduce withdrawn historical reports. Both
CLI and analyze() require an explicit obsolete-model opt-in. It is NOT the
whole amplifier, current deployment state, or cabin acoustics.

Historical description (withdrawn): stereo master -> our C renderer (exact
deployed settings) -> SoundAmp bank-1 input matrix (pre output-EQ buses).
Reports overloads, level/spectral shifts per bus vs the dry stereo, front-bus
comb ripple from the amp's 0.708*Ls return, rear activity and time anomalies.

  python cabin_model.py --renderer BIN --model build/hssr_model.bin --guard build/hssr_guard.bin \
      --out R/cabin-model-a --variant fb0:0.40:0 --variant fb3:0.40:-3 IN/*.wav
"""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np, soundfile as sf
from scipy.signal import stft

SR = 48000
# SHARC bank 1 (sink 0x26), dB -> linear; rows = buses L R C LFE Ls Rs, cols = L R C LFE Ls Rs
DB = [[-2, -100, -8, -8, -3, -100], [-100, -2, -8, -8, -100, -3], [-6, -6, -3, -10, -100, -100],
      [-100, -100, -100, 3, -100, -100], [-100, -100, -100, -14, 6, -100], [-100, -100, -100, -14, -100, 6]]
M = 10 ** (np.array(DB, 'f8') / 20)
BUS = ['L', 'R', 'C', 'LFE', 'Ls', 'Rs']
BANDS = [(20, 60), (60, 120), (120, 250), (250, 500), (500, 1000), (1000, 2000), (2000, 4000), (4000, 8000), (8000, 16000)]


def db(x):
    return float(10 * np.log10(max(x, 1e-30)))


def band_db(x, lo, hi):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2
    f = np.fft.rfftfreq(len(x), 1 / SR)
    return db(X[(f >= lo) & (f < hi)].sum())


def render(renderer, model, guard, wav_in, wav_out, surround, front_bass):
    cmd = [renderer, '--mode', 'hssr-universal', '--model', model, '--guard-model', guard, '--lfe', 'assist',
           '--surround', str(surround), '--front-bass', str(front_bass), str(wav_in), str(wav_out)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(r.stderr[-300:])
    y, sr = sf.read(wav_out, dtype='int16', always_2d=True)
    assert sr == SR and y.shape[1] == 6
    return y.astype('f8')


def analyze(x_in, y6, surround, *, allow_obsolete_transposed_model=False):
    if allow_obsolete_transposed_model is not True:
        raise ValueError('obsolete transposed six-bus model: use amp_music_report.py / AmpChain v7+; historical reproduction requires explicit opt-in')
    # align: the C renderer output lags the input (512 samples measured); estimate it
    seg = min(len(x_in), len(y6), SR * 20)
    cc = np.fft.irfft(np.fft.rfft(y6[:seg, 0]) * np.conj(np.fft.rfft(x_in[:seg, 0])))
    lag = int(np.argmax(cc[:SR // 2]))
    n = min(len(x_in) - lag, len(y6) - lag)
    x_in = x_in[:n]; y6 = y6[lag:lag + n]
    n0, n1 = SR * 5, len(y6) - SR * 5           # skip edges (filter settle)
    x = x_in[n0:n1]; y = y6[n0:n1]
    bus = y @ M.T
    rep = {'lag_samples': lag}
    rep['front_pcm_peak_re_in_db'] = db((np.abs(y[:, 0]).max() / (np.abs(x[:, 0]).max() + 1e-9)) ** 2)
    # 1. overload / peaks per bus (bus is what the amp DSP sees; 32768 = 0 dBFS)
    for i, name in enumerate(BUS):
        b = bus[:, i]
        rep[f'peak_{name}_dbfs'] = db((np.abs(b).max() / 32768) ** 2)
        rep[f'over_{name}'] = int((np.abs(b) > 32767).sum())
        rep[f'rms_{name}_dbfs'] = db(np.mean(b ** 2) / 32768 ** 2)
    rep['rms_in_dbfs'] = db(np.mean(x[:, 0] ** 2) / 32768 ** 2)
    # 2. band deltas vs dry input L (front bus and centre) and rear/LFE absolute vs input
    for lo, hi in BANDS:
        ref = band_db(x[:, 0], lo, hi)
        rep[f'front_delta_{lo}'] = band_db(bus[:, 0], lo, hi) - ref
        rep[f'centre_re_in_{lo}'] = band_db(bus[:, 2], lo, hi) - ref
        rep[f'rear_re_in_{lo}'] = band_db(bus[:, 4], lo, hi) - ref
        rep[f'lfe_re_in_{lo}'] = band_db(bus[:, 3], lo, hi) - ref
    # 3. comb ripple on the front bus: smoothed transfer |FrontBus / InL| in 300..3000 Hz
    f, t, X = stft(x[:, 0], SR, nperseg=8192); _, _, Y = stft(bus[:, 0], SR, nperseg=8192)
    h = (np.abs(Y) ** 2).mean(1) / ((np.abs(X) ** 2).mean(1) + 1e-9)
    sel = (f > 300) & (f < 3000)
    hd = 10 * np.log10(h[sel] + 1e-12)
    rep['front_ripple_db_pp'] = float(hd.max() - hd.min())
    rep['front_ripple_db_std'] = float(hd.std())
    # 4. rear activity: fraction of time rear bus is within 12 dB of front bus (short-time)
    w = SR // 20
    fr = np.array([np.mean(bus[i:i + w, 0] ** 2) for i in range(0, len(bus) - w, w)])
    rr = np.array([np.mean(bus[i:i + w, 4] ** 2) for i in range(0, len(bus) - w, w)])
    rep['rear_active_frac'] = float(np.mean(10 * np.log10(rr + 1) > 10 * np.log10(fr + 1) - 12))
    rep['rear_re_front_db_median'] = float(np.median(10 * np.log10((rr + 1) / (fr + 1))))
    # 5. rear PCM at the cap = limiter/clamp activity
    cap = int(round(surround * 32768))
    rep['rear_pcm_at_cap_frac'] = float(np.mean(np.abs(y[:, 4]) >= cap - 1))
    # 6. time anomalies: 50 ms RMS jumps > 8 dB on the front bus not present in the input
    xin = np.array([np.mean(x[i:i + w, 0] ** 2) for i in range(0, len(x) - w, w)])
    jf = np.abs(np.diff(10 * np.log10(fr + 1))); ji = np.abs(np.diff(10 * np.log10(xin + 1)))
    rep['front_jumps_gt8db_not_in_input'] = int(np.sum((jf > 8) & (ji < 4)))
    # 7. front-vs-rear correlation at the amp's 12 ms return (echo perception risk)
    d = int(0.012 * SR)
    a = bus[d:, 0] - bus[d:, 0].mean(); b = bus[:-d, 4] - bus[:-d, 4].mean()
    rep['front_rear_corr_12ms'] = float((a * b).mean() / (a.std() * b.std() + 1e-9))
    return rep


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--allow-obsolete-transposed-model', action='store_true',
                    help='historical reproduction only; results must not be used for current tuning')
    ap.add_argument('--renderer', required=True); ap.add_argument('--model', required=True); ap.add_argument('--guard', required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--variant', action='append', default=[], help='name:surround:front_bass_db')
    ap.add_argument('wavs', nargs='+', type=Path)
    a = ap.parse_args()
    if not a.allow_obsolete_transposed_model:
        ap.error('obsolete transposed six-bus model; use tools/soundamp-virtual-cabin/amp_music_report.py instead')
    print('WARNING: reproducing WITHDRAWN transposed six-bus results, NOT current model/tuning.', flush=True)
    a.out.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='cabin-'))
    variants = [v.split(':') for v in (a.variant or ['fb0:0.40:0'])]
    report = {'schema': 'cabin-model-v1', 'status': 'obsolete_transposed_six_bus_model_not_for_tuning',
              'current_replacement': 'tools/soundamp-virtual-cabin/amp_music_report.py',
              'matrix_db': DB, 'variants': variants, 'songs': {}}
    for wav in a.wavs:
        x, sr = sf.read(wav, dtype='int16', always_2d=True); x = x.astype('f8'); assert sr == SR
        report['songs'][wav.stem] = {}
        for name, sur, fb in variants:
            y = render(a.renderer, a.model, a.guard, wav, work / f'{wav.stem}-{name}.wav', float(sur), float(fb))
            r = analyze(x, y, float(sur), allow_obsolete_transposed_model=True); report['songs'][wav.stem][name] = r
            print(f"{wav.stem[:24]:24s} {name:4s} peakL {r['peak_L_dbfs']:+5.1f} peakC {r['peak_C_dbfs']:+5.1f} peakLs {r['peak_Ls_dbfs']:+5.1f} "
                  f"over {r['over_L']+r['over_R']+r['over_C']+r['over_Ls']+r['over_Rs']+r['over_LFE']:5d} | front d60-120 {r['front_delta_60']:+4.1f} d120-250 {r['front_delta_120']:+4.1f} "
                  f"| centre re in 60-120 {r['centre_re_in_60']:+4.1f} | rear/front med {r['rear_re_front_db_median']:+5.1f} act {r['rear_active_frac']:.2f} cap {r['rear_pcm_at_cap_frac']:.4f} "
                  f"| ripple {r['front_ripple_db_pp']:4.1f} jumps {r['front_jumps_gt8db_not_in_input']} lag {r['lag_samples']} frontPCMpeak re in {r['front_pcm_peak_re_in_db']:+4.1f}", flush=True)
        (a.out / 'report.json').write_text(json.dumps(report, indent=1))
    print('done', a.out)


if __name__ == '__main__':
    main()
