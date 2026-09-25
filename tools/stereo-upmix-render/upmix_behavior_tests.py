#!/usr/bin/env python3
"""Synthetic behavioural scenes for the HSSR stereo->5.1 upmix.

Each scene is built from known stems, rendered through the host renderer
(bit-exact with the hook), and the "lead" stem is tracked over time: in the
time-frequency cells where the dry lead dominates the dry mix, how much of the
output energy lands in the rears (Ls+Rs) vs the front (L+R+C).  That share
over time is what a listener hears as "the sound goes back and front".

    python3 tools/stereo-upmix-render/upmix_behavior_tests.py --render BIN \
        --model models/hssr-q6d-moises-a/hssr_model.bin \
        --guard models/hssr-selection-v11/hssr_guard.bin --out DIR
"""
import argparse
import os
import subprocess
import sys
import warnings
import wave

import numpy as np

# Empty slices (a scene without an onset, a silent quarter) are expected: NaN is the answer.
warnings.filterwarnings("ignore", category=RuntimeWarning)

SR = 48000
N_FFT = 2048
HOP = 512

# Car knobs from deploy/smartphone_integrator/carplay_startup.sh.
# The car until 2026-09-25 (owner attack + HF taper, no vocal gate).
CAR_0922 = ["--mode", "hssr-universal", "--lfe", "assist", "--lfe-gain", "0",
            "--surround", "0.40", "--front-bass", "-3", "--front-retention", "0.5",
            "--bed-delays", "--bed-attack", "owner", "--attack-collapse", "0.75",
            "--vocal-gate", "0"]
# Candidate 2026-09-25: attack detector off (its per-hit collapse was the largest
# jump generator on MoisesDB), vocal gate on (renderer default), retention 0.5.
SHIPPED = ["--mode", "hssr-universal", "--lfe", "assist", "--lfe-gain", "0",
           "--surround", "0.40", "--front-bass", "-3", "--front-retention", "0.5",
           "--bed-delays", "--bed-attack", "off", "--rear-air", "-12"]
VARIANTS = {
    "shipped": SHIPPED,
    "attack_front": [a if a != "owner" else "front" for a in SHIPPED],
    "attack_off": [a for a in SHIPPED if a not in ("--bed-attack", "owner",
                                                    "--attack-collapse", "0.75")],
    "taper_flat": SHIPPED + ["--retention-hf", "0:0"],
    "flat_body0": SHIPPED + ["--retention-hf", "0:0", "--rear-body", "0"],
    "gate_off": SHIPPED + ["--vocal-gate", "0"],
    "car_0922": CAR_0922,
    # Knob options for the car A/B around the candidate (all env, no rebuild).
    "cand_sur05": [a if a != "0.40" else "0.5" for a in SHIPPED],       # AUDIO_STEREO_SURROUND=0.5
    "cand_gate07": SHIPPED + ["--vocal-gate", "0.7"],                    # AUDIO_HSSR_VOCAL_GATE=0.7
    "cand_ret035": [a if a != "0.5" else "0.35" for a in SHIPPED],      # AUDIO_STEREO_FRONT_RETENTION=0.35
    "cand_noair": SHIPPED[:-2],                                          # AUDIO_STEREO_REAR_AIR_DB unset
    "cand_treble3": SHIPPED + ["--front-treble", "-3"],                  # AUDIO_STEREO_FRONT_TREBLE_DB=-3
    "cand_air9": SHIPPED[:-2] + ["--rear-air", "-9"],
    "cand_air6": SHIPPED[:-2] + ["--rear-air", "-6"],
    "owner": [a if a != "off" else "owner" for a in SHIPPED] + ["--attack-collapse", "0.75"],
}


# ---------------------------------------------------------------- synthesis
rng = np.random.default_rng(1)


def t_axis(sec):
    return np.arange(int(sec * SR)) / SR


def pluck_line(sec, notes_hz, bpm=100, bright=6, decay=3.0):
    """Monophonic melodic line: saw-ish harmonics, pluck envelope, light vibrato."""
    out = np.zeros(int(sec * SR))
    step = int(SR * 60 / bpm / 2)
    for i, start in enumerate(range(0, len(out), step)):
        f = notes_hz[i % len(notes_hz)]
        n = min(step * 2, len(out) - start)
        t = np.arange(n) / SR
        ph = 2 * np.pi * f * t + 0.3 * np.sin(2 * np.pi * 5.5 * t)
        tone = sum(np.sin(k * ph) / k for k in range(1, bright + 1))
        out[start:start + n] += tone * np.exp(-decay * t) * np.minimum(1, t * 200)
    return out


def pad(sec, chord_hz=(130.8, 164.8, 196.0)):
    t = t_axis(sec)
    x = sum(np.sin(2 * np.pi * f * t) + 0.3 * np.sin(4 * np.pi * f * t) for f in chord_hz)
    return x * (0.6 + 0.4 * np.sin(2 * np.pi * 0.1 * t))


def room(sec, cutoff=1500):
    """Decorrelated diffuse noise, low-passed: the 'bed' a real mix has."""
    n = int(sec * SR)
    def lp(x):
        X = np.fft.rfft(x)
        X[int(cutoff * len(x) / SR):] = 0
        return np.fft.irfft(X, len(x))
    return lp(rng.standard_normal(n)), lp(rng.standard_normal(n))


def drums(sec, bpm=120):
    out = np.zeros(int(sec * SR))
    beat = int(SR * 60 / bpm)
    k = np.arange(int(0.25 * SR)) / SR
    kick = np.sin(2 * np.pi * (50 + 80 * np.exp(-k * 30)) * k) * np.exp(-k * 12)
    snare = rng.standard_normal(len(k)) * np.exp(-k * 25) * 0.6
    for i, s in enumerate(range(0, len(out) - len(k), beat)):
        out[s:s + len(k)] += kick if i % 2 == 0 else snare
    return out


def sustain_line(sec, notes_hz, bpm=60, bright=6, vib=0.5, glide=0.0):
    """Bowed/sung line: slow attack, held notes, vibrato (rad), optional glide
    (fraction of the note spent sliding from the previous pitch)."""
    out = np.zeros(int(sec * SR))
    step = int(SR * 60 / bpm)
    prev = notes_hz[-1]
    for i, start in enumerate(range(0, len(out), step)):
        f = notes_hz[i % len(notes_hz)]
        n = min(step, len(out) - start)
        t = np.arange(n) / SR
        if glide:
            fr = prev + (f - prev) * np.minimum(1, t / (glide * n / SR))
            ph = 2 * np.pi * np.cumsum(fr) / SR
        else:
            ph = 2 * np.pi * f * t
        ph = ph + vib * np.sin(2 * np.pi * 5 * t) * np.minimum(1, t)
        prev = f
        tone = sum(np.sin(k * ph) / k ** 1.3 for k in range(1, bright + 1))
        env = np.minimum(1, t / 0.08) * np.minimum(1, (n / SR - t) / 0.05)
        out[start:start + n] += tone * env
    return out


def formant_voice(sec, notes_hz, bpm=50, vowel=(800, 1200, 2800)):
    """Sung 'ah': glottal pulse train with vibrato and a little breath, through
    three formant resonances.  Phrases of 4 notes then a one-note rest."""
    out = np.zeros(int(sec * SR))
    step = int(SR * 60 / bpm)
    for i, start in enumerate(range(0, len(out), step)):
        if i % 5 == 4:
            continue
        f = notes_hz[i % len(notes_hz)]
        n = min(step, len(out) - start)
        t = np.arange(n) / SR
        f_t = f * (1 + 0.03 * np.sin(2 * np.pi * 5.5 * t) * np.minimum(1, t / 0.3) +
                   0.02 * np.exp(-t * 8))                      # vibrato + scoop
        ph = 2 * np.pi * np.cumsum(f_t) / SR
        pulses = sum(np.sin(k * ph) / k for k in range(1, 40)) # saw-ish glottal
        env = np.minimum(1, t / 0.05) * np.minimum(1, (n / SR - t) / 0.08)
        out[start:start + n] += (pulses + 0.05 * rng.standard_normal(n)) * env
    X = np.fft.rfft(out); fq = np.fft.rfftfreq(len(out), 1 / SR)
    res = sum(1 / (1 + ((fq - fc) / bw) ** 2) for fc, bw in zip(vowel, (80, 100, 150)))
    return np.fft.irfft(X * res, len(out)) / 3


def noise_hat(sec, bpm=120, per_beat=2):
    """Closed hi-hat: 40 ms band-passed noise bursts."""
    out = np.zeros(int(sec * SR))
    step = int(SR * 60 / bpm / per_beat)
    k = np.arange(int(0.04 * SR)) / SR
    for st in range(0, len(out) - len(k), step):
        out[st:st + len(k)] += rng.standard_normal(len(k)) * np.exp(-k * 80)
    X = np.fft.rfft(out); fq = np.fft.rfftfreq(len(out), 1 / SR)
    X[(fq < 5000) | (fq > 14000)] = 0
    return np.fft.irfft(X, len(out)) * 0.5


def strum(sec, chord_hz=(196, 247, 294, 392), bpm=80):
    """Four-note chord strummed (12 ms between strings)."""
    out = np.zeros(int(sec * SR))
    for j, f in enumerate(chord_hz):
        one = pluck_line(sec, [f], bpm=bpm, bright=5, decay=2.0)
        out[j * 576:] += one[:len(out) - j * 576]
    return out / len(chord_hz)


def stereo_reverb(x, rt60=1.8, wet=0.35):
    """Two decorrelated exponential noise tails: a diffuse stereo room."""
    n = int(rt60 * SR)
    env = np.exp(-6.9 * np.arange(n) / n)
    irs = [rng.standard_normal(n) * env for _ in range(2)]
    irs = [ir / np.sqrt((ir ** 2).sum()) for ir in irs]
    wetp = [np.fft.irfft(np.fft.rfft(x, len(x) + n) * np.fft.rfft(ir, len(x) + n))[:len(x)]
            for ir in irs]
    return np.stack([x * (1 - wet) + wet * wetp[0], x * (1 - wet) + wet * wetp[1]])


def ping_pong(x, delay_s=0.375, fb=0.5):
    d = int(delay_s * SR)
    l, r = x.copy(), np.zeros_like(x)
    echo, side = x * fb, 1
    for k in range(1, 6):
        sh = np.zeros_like(x); sh[k * d:] = echo[:len(x) - k * d]
        (r if side else l)[:] += sh
        echo, side = echo * fb, 1 - side
    return np.stack([l, r])


def gate(x, on_sec, off_sec, start_sec=0.0):
    t = np.arange(len(x)) / SR - start_sec
    period = on_sec + off_sec
    g = (t >= 0) & ((t % period) < on_sec) if off_sec else (t >= 0)
    # 10 ms ramps, a real instrument does not click in
    ramp = np.convolve(g.astype(float), np.ones(480) / 480, mode="same")
    return x * ramp, g


def pan(x, p):
    """Constant-power pan, p in -1 (L) .. +1 (R)."""
    a = (p + 1) * np.pi / 4
    return np.stack([x * np.cos(a), x * np.sin(a)])


LEAD_NOTES = [880, 987.8, 1174.7, 1318.5, 1174.7, 987.8]
OCTAVE_BPM = 50       # octave_jump: one note every 0.6 s, 440 then 1760 Hz


def scenes(sec=20.0):
    """name -> (lead stereo, rest stereo, lead-active mask per sample, description)."""
    rl, rr = room(sec)
    bed = pan(pad(sec), 0) * 0.25 + np.stack([rl, rr]) * 0.05
    lead_raw = pluck_line(sec, LEAD_NOTES) * 0.25
    out = {}

    lead, on = gate(lead_raw, sec, 0)
    out["steady"] = (pan(lead, -0.5), bed, on,
                     "lead panned half-left over a pad from t=0")

    lead, on = gate(lead_raw, sec, 0, start_sec=5)
    out["late_entry"] = (pan(lead, -0.5), bed, on,
                         "lead enters at 5 s: is capture a jump or a glide?")

    lead, on = gate(lead_raw, 2.0, 1.5, start_sec=2)
    out["gappy"] = (pan(lead, -0.5), bed, on,
                    "lead 2 s on / 1.5 s off: does every re-entry get recaptured?")

    lead, on = gate(lead_raw, sec, 0)
    hard = np.stack([drums(sec) * 0.35, np.zeros(int(sec * SR))])
    bass = pan(pluck_line(sec, [55, 73.4, 82.4, 65.4], bright=3, decay=1) * 0.3, 0)
    out["hard_pan_60s"] = (pan(lead, 1.0), hard + bass, on,
                           "1960s stereo: drums hard L, lead hard R, bass centre")

    lead, on = gate(lead_raw, sec, 0)
    m = pan(lead, 0)
    out["mono"] = (m, pan(pad(sec), 0) * 0.25 + pan(drums(sec) * 0.3, 0), on,
                   "everything dual-mono: nothing should move")

    lead, on = gate(lead_raw, sec, 0)
    out["drums_lead"] = (pan(lead, -0.5), bed + pan(drums(sec) * 0.35, 0), on,
                         "lead over a 120 bpm kick/snare: pumping on the beat?")

    lead, on = gate(pluck_line(sec, [440, 1760], bpm=OCTAVE_BPM) * 0.25, sec, 0)
    out["octave_jump"] = (pan(lead, -0.5), bed, on,
                          "one instrument alternating 440/1760 Hz: does depth follow pitch?")

    lead, on = gate(lead_raw, sec, 0)
    t = t_axis(sec)
    sweep = np.clip(np.sin(2 * np.pi * t / sec - np.pi / 2), -1, 1)  # L -> R -> L
    a = (sweep + 1) * np.pi / 4
    out["pan_sweep"] = (np.stack([lead * np.cos(a), lead * np.sin(a)]), bed, on,
                        "lead slowly panned L->R->L: smooth or stepping?")

    lead, on = gate(lead_raw * 10 ** (-24 * t / sec / 20), sec, 0)
    out["fade_out"] = (pan(lead, -0.5), bed, on,
                       "lead fades 0 -> -24 dB: does depth follow loudness?")

    lead, on = gate(lead_raw, sec, 0)
    rival, _ = gate(pluck_line(sec, [330, 392, 440, 494], bpm=140, bright=4) * 0.25,
                    sec, 0, start_sec=sec * 0.4)
    out["rival"] = (pan(lead, -0.5), bed + pan(rival, -0.65), on,
                    "second instrument joins the lead's side at 40 %: does it steal the lead?")

    lead, on = gate(lead_raw, sec, 0)
    a = (np.linspace(-0.8, 0.8, len(t)) + 1) * np.pi / 4
    out["cross_sides"] = (np.stack([lead * np.cos(a), lead * np.sin(a)]), bed, on,
                          "lead pans once L -> R: slot hand-off at the centre")

    voice = pluck_line(sec, [220, 247, 262, 294], bpm=40, bright=8, decay=0.5) * 0.3
    lead, on = gate(voice, sec, 0)
    guitar = pan(pluck_line(sec, [659, 784, 880], bpm=150) * 0.2, 0.7)
    out["vocal_centre"] = (pan(lead, 0), bed + guitar, on,
                           "centred sustained 'vocal' with a side guitar: the vocal stays front")

    lead, on = gate(lead_raw, sec, 0)
    band = (pan(pluck_line(sec, [523, 587, 659], bpm=90, bright=5) * 0.15, 0.6) +
            pan(pluck_line(sec, [196, 220, 247], bpm=70, bright=7) * 0.15, -0.8) +
            pan(pluck_line(sec, [1568, 1760], bpm=180, bright=3) * 0.08, 0.3) +
            pan(drums(sec) * 0.3, 0.1))
    out["dense"] = (pan(lead, -0.5), bed + band, on,
                    "lead inside a dense five-part mix: slot competition")

    lead, on = gate(lead_raw, sec, 0)
    out["phase_inverted"] = (np.stack([lead, -lead]) * 0.7, bed, on,
                             "lead in anti-phase (L = -R): an old-stereo / wiring artefact")

    lead, on = gate(lead_raw, sec, 0)
    out["reverb_lead"] = (stereo_reverb(lead) * [[1.2], [0.6]], bed, on,
                          "lead left with a wide stereo room: dry and tail must stay together")

    lead, on = gate(lead_raw, sec, 0)
    out["ping_pong"] = (ping_pong(lead) + pan(lead, 0) * 0.0, bed, on,
                        "lead with 375 ms ping-pong echoes: echoes alternate hard L/R")

    lead, on = gate(sustain_line(sec, [440, 494, 523]) * 0.2, sec, 0)
    det, _ = gate(sustain_line(sec, [441.5, 495.6, 524.8]) * 0.2, sec, 0)
    out["wide_chorus"] = (np.stack([lead, det]), bed, on,
                          "detuned wide synth (L/R 3 Hz apart): width without a pan position")

    half = int(sec * SR / 2)
    song_a = pan(pluck_line(sec, LEAD_NOTES) * 0.25, -0.6) + bed
    song_a[:, half:] = 0
    voice, _ = gate(sustain_line(sec, [220, 247, 262]) * 0.3, sec / 2, 0, start_sec=sec / 2)
    on = np.arange(int(sec * SR)) >= half
    song_b = pan(drums(sec) * 0.3, 0); song_b[:, :half] = 0
    out["song_change"] = (pan(voice, 0), song_a + song_b, on,
                          "hard cut to a mono song at 50 %: no stale slot drags it rear")

    chirp_f = 200 * (40 ** (t / sec))                       # 200 Hz -> 8 kHz, log
    chirp = np.sin(2 * np.pi * np.cumsum(chirp_f) / SR) * 0.2
    lead, on = gate(chirp, sec, 0)
    out["chirp"] = (pan(lead, -0.5), bed, on,
                    "sine sweep 200 Hz -> 8 kHz at one position: depth vs frequency map")

    # ---- second batch (2026-09-25 evening): arrangement, doubling, voice, timing
    lead, on = gate(lead_raw, sec, 0)
    dbl = pluck_line(sec, LEAD_NOTES, bpm=100.6) * 0.25          # second take, 0.6 % timing drift
    out["double_tracked"] = (np.stack([lead, dbl]), bed, on,
                             "same line hard L and hard R (double-tracked guitar)")

    lead, on = gate(lead_raw, sec, 0)
    other = sustain_line(sec, [262, 294, 330], bpm=45, bright=5) * 0.22
    out["two_objects_lr"] = (pan(lead, -0.6), bed + pan(other, 0.6), on,
                             "different instruments left and right: both held independently")

    arp = pluck_line(sec, [262, 330, 392, 523, 659, 784, 1047, 784, 659, 523, 392, 330], bpm=240)
    lead, on = gate(arp * 0.25, sec, 0)
    out["arpeggio"] = (pan(lead, -0.5), bed, on,
                       "two-octave arpeggio at 240 bpm: realistic melodic range")

    lead, on = gate(strum(sec) * 0.6, sec, 0)
    out["chord_strum"] = (pan(lead, -0.5), bed, on,
                          "strummed four-note chord: one object, four pitches")

    lead, on = gate(sustain_line(sec, [440, 660, 494, 587], bpm=40, glide=0.4) * 0.2, sec, 0)
    out["portamento"] = (pan(lead, -0.5), bed, on,
                         "held notes joined by 0.6 s pitch slides")

    lead, on = gate(lead_raw, sec, 0)
    pump = 1 - 0.8 * np.exp(-((t * 128 / 60) % 1) * 6)          # 128 bpm sidechain
    out["sidechain_pump"] = (pan(lead, -0.5), bed * pump + pan(drums(sec, 128) * 0.3, 0), on,
                             "EDM sidechain: the bed ducks on every kick")

    lead, on = gate(lead_raw, sec, 0)
    hole = (t < 8) | (t >= 14)
    out["bed_dropout"] = (pan(lead, -0.5), bed * hole, on,
                          "bed disappears 8-14 s while the lead plays on")

    lead, on = gate(lead_raw, sec, 0)
    entry = np.minimum(1, np.maximum(0, (t - 6) / 0.02))
    out["band_enters"] = (pan(lead, -0.5), (bed + pan(drums(sec) * 0.35, 0)) * entry, on,
                          "solo intro, the band joins at 6 s")

    lead, on = gate(lead_raw, sec, 0)
    p = np.where(t < sec / 2, -0.7, 0.7)
    a = (p + 1) * np.pi / 4
    out["pan_jump"] = (np.stack([lead * np.cos(a), lead * np.sin(a)]), bed, on,
                       "instant pan jump L -> R at 50 %: slot hand-off without a hole")

    lead, on = gate(lead_raw, sec, 0)
    silent = (t >= 8) & (t < 11)
    on = on & ~silent
    out["break_silence"] = (pan(lead, -0.5) * ~silent, bed * ~silent, on,
                            "the whole mix stops for 3 s, then returns")

    lead, on = gate(lead_raw, sec, 0)
    out["solo_no_bed"] = (pan(lead, -0.5), np.zeros((2, len(lead))), on,
                          "lead alone, nothing else (solo piano intro)")

    everything = pan(pad(sec), 0) * 0.25 + np.stack([rl, rr]) * 0.05 + pan(drums(sec) * 0.3, 0)
    out["bed_only"] = (everything, np.zeros_like(everything), np.ones(int(sec * SR), bool),
                       "no object at all: share of the whole mix that goes rear")

    voice = formant_voice(sec, [220, 247, 262, 294])
    lead, on = gate(voice * 0.3, sec, 0)
    side = pan(pluck_line(sec, [659, 784, 880], bpm=150) * 0.2, 0.7)
    out["vocal_formant"] = (pan(lead, 0), bed + side, on,
                            "formant 'ah' voice at centre with a side guitar")

    lead, on = gate(voice * 0.3, sec, 0)
    out["vocal_formant_side"] = (pan(lead, -0.4), bed + side, on,
                                 "the same voice panned -0.4 (a backing vocal)")

    lead, on = gate(noise_hat(sec) * 0.5, sec, 0)
    out["hihat_side"] = (pan(lead, -0.6), bed + pan(drums(sec) * 0.3, 0), on,
                         "closed hi-hat eighths panned left: percussion presence")

    lead, on = gate(lead_raw, sec, 0)
    out["drums_same_side"] = (pan(lead, -0.5), bed + pan(drums(sec) * 0.35, -0.5), on,
                              "kick/snare at the lead's own pan")

    lead, on = gate(lead_raw, sec, 0)
    mono_bed = pan(pad(sec), 0) * 0.25 + pan(rl, 0) * 0.05
    out["bed_mono"] = (pan(lead, -0.5), mono_bed, on,
                       "same lead over a mono bed (vs steady = wide bed)")

    def bandlimit(x, lo=300, hi=3000):
        X = np.fft.rfft(x, axis=-1); fq = np.fft.rfftfreq(x.shape[-1], 1 / SR)
        X[..., (fq < lo) | (fq > hi)] = 0
        return np.fft.irfft(X, x.shape[-1], axis=-1)
    lead, on = gate(lead_raw, sec, 0)
    out["lofi_band"] = (bandlimit(pan(lead, -0.5)), bandlimit(bed), on,
                        "whole mix 300-3000 Hz (phone / AM radio)")

    lead, on = gate(pluck_line(sec, [131, 165, 196, 262, 330, 392, 523, 659, 784, 1047, 784, 523,
                                     392, 262, 196, 131], bpm=120) * 0.25, sec, 0)
    out["melody_3oct"] = (pan(lead, -0.5), bed, on,
                          "melody climbing C3 -> C6 and back at 120 bpm")

    lead, on = gate(sustain_line(sec, [1760, 1976, 2349], bpm=50) * 0.2, sec, 0)
    swell = 10 ** ((-12 + 24 * t / sec) / 20)                 # -12 dB -> +12 dB re bed
    out["crescendo"] = (pan(lead * swell, -0.35), bed, on,
                        "high line at pan -0.35 swelling -12 -> +12 dB: does depth follow?")
    return out


def long_scene(sec=90.0):
    rl, rr = room(sec)
    bed = pan(pad(sec), 0) * 0.25 + np.stack([rl, rr]) * 0.05
    lead, on = gate(pluck_line(sec, LEAD_NOTES) * 0.25, sec, 0)
    return pan(lead, -0.5), bed, on


# ---------------------------------------------------------------- io
def write_wav(path, x):
    x = np.clip(x, -1, 1)
    pcm = (x.T * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[0]); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def read_wav6(path):
    """The renderer writes WAVE_FORMAT_EXTENSIBLE, which the wave module rejects."""
    b = open(path, "rb").read()
    i = b.index(b"data") + 8
    return np.frombuffer(b[i:], "<i2").reshape(-1, 6).T.astype(float) / 32768


# ---------------------------------------------------------------- analysis
def stft_pow(x):
    win = np.hanning(N_FFT)
    idx = np.arange(0, len(x) - N_FFT, HOP)
    return np.abs(np.fft.rfft(np.stack([x[i:i + N_FFT] * win for i in idx]), axis=1)) ** 2


def rear_share(lead, rest, out6):
    """Per-50ms rear share of the output in cells where the dry lead dominates."""
    # Per-channel power: an anti-phase lead (L = -R) has no L+R at all.
    dl = stft_pow(lead[0]) + stft_pow(lead[1]); dr = stft_pow(rest[0]) + stft_pow(rest[1])
    own = dl > 10 * (dr + 1e-12)            # lead owns the cell by >= 10 dB
    own[:, : int(300 * N_FFT / SR)] = False  # below the model's 350 Hz floor
    front = sum(stft_pow(out6[c]) for c in (0, 1, 2))
    rear = sum(stft_pow(out6[c]) for c in (4, 5))
    n = min(len(own), len(front))
    own, front, rear = own[:n], front[:n], rear[:n]
    per = 5                                  # 5 hops = 53 ms
    shares, weight = [], []
    for s in range(0, n - per, per):
        o = own[s:s + per]
        f = (front[s:s + per] * o).sum(); r = (rear[s:s + per] * o).sum()
        shares.append(r / (f + r) if f + r > 0 else np.nan)
        weight.append(o.sum())
    return np.array(shares), np.array(weight), per * HOP / SR


def metrics(share, weight, dt, on):
    act = np.array([on[int(i * dt * SR):int((i + 1) * dt * SR)].mean() > 0.9
                    for i in range(len(share))]) & (weight > 20) & ~np.isnan(share)
    s = np.where(act, share, np.nan)
    steps = np.abs(np.diff(s))
    steady = s[int(len(s) * 0.5):]
    # onset jump: the largest frame-to-frame change in the first 0.3 s of each
    # lead entry; a capture that glides keeps it small, a snap-back does not.
    target = np.nanmedian(s) if np.any(act) else np.nan
    onsets = np.flatnonzero(np.diff(act.astype(int)) == 1) + 1
    if act[0]:
        onsets = np.r_[0, onsets]
    # The entry frame sits in the fade-in ramp, outside `act`: judge any frame the
    # lead already owns cells in, starting three frames (the ramp plus STFT lag) before the onset.
    heard = np.where((weight > 20) & ~np.isnan(share), share, np.nan)
    jumps = [np.nanmax(np.abs(np.diff(heard[max(o - 3, 0):o + int(0.3 / dt) + 1])), initial=0)
             for o in onsets]
    # octave_jump only: rear share of the 1760 Hz notes minus the 440 Hz notes
    # (second half of the scene, so the tracker has settled).
    t = np.arange(len(share)) * dt
    note = (t // (60 / OCTAVE_BPM / 2)).astype(int) % 2
    late = t > t[-1] / 2
    pitch_gap = (np.nanmedian(np.where(late & (note == 1), share, np.nan)) -
                 np.nanmedian(np.where(late & (note == 0), share, np.nan)))
    third = int(len(s) / 3)
    freq_slope = np.nanmedian(s[-third:]) - np.nanmedian(s[int(2 / dt):third])
    quarter = int(len(s) / 4)
    drift = np.nanmedian(s[-quarter:]) - np.nanmedian(s[int(2 / dt):int(2 / dt) + quarter])
    first_second = np.nanmax(s[np.argmax(act):np.argmax(act) + int(1 / dt)]) if np.any(act) else np.nan
    w05, w1 = int(0.5 / dt), int(1 / dt)
    body = s[int(2 / dt):]
    win05 = [np.nanmedian(body[i:i + w05]) for i in range(0, len(body) - w05, w05)
             if np.sum(~np.isnan(body[i:i + w05])) > w05 / 2]
    win1 = [np.nanmedian(body[i:i + w1]) for i in range(0, len(body) - w1, w1)
            if np.sum(~np.isnan(body[i:i + w1])) > w1 / 2]
    return {
        "worst_half_s": (target - np.min(win05)) if win05 else np.nan,   # deepest 0.5 s dropout
        "range_1s": (np.max(win1) - np.min(win1)) if win1 else np.nan,   # slow slides

        "freq_slope": freq_slope,
        "first_second_max": first_second,
        "drift": drift,
        "pitch_gap": pitch_gap,
        "median_rear": target,
        "max_step": np.nanmax(steps) if np.any(~np.isnan(steps)) else np.nan,
        "p95_step": np.nanpercentile(steps, 95) if np.any(~np.isnan(steps)) else np.nan,
        "steady_std": np.nanstd(steady),
        "onset_jump": np.median(jumps) if jumps else np.nan,
    }


# (scene, metric, limit, xfail) evaluated on the shipped car knobs.  xfail marks a
# known defect the target describes; when it starts passing, --check fails with
# XPASS so the marker gets removed and the fix stays locked in.
# ponytail: limits set by eye from the 2026-09-25 baseline, tighten after listening.
CHECKS = [
    ("mono", "median_rear", 0.05, False),        # dual-mono has no object to move
    ("mono", "max_step", 0.05, False),
    ("steady", "steady_std", 0.08, False),       # a held line does not wander
    ("steady", "p95_step", 0.20, False),
    ("drums_lead", "steady_std", 0.08, False),   # the beat does not pump the lead
    # Depth follows pitch: +0.43 on 09-25 = retention HF taper (~0.22) + the
    # -4 dB/500 Hz rear low shelf (~0.17); with both flat it is +0.02.
    ("octave_jump", "abs_pitch_gap", 0.10, True),
    ("gappy", "onset_jump", 0.20, True),         # each entry snaps back (0.69 on 09-25)
    ("steady", "abs_drift", 0.05, False),        # no slow creep over the scene
    ("fade_out", "abs_drift", 0.10, False),      # depth does not follow loudness
    ("rival", "steady_std", 0.08, False),        # a newcomer does not unseat the lead
    ("cross_sides", "max_step", 0.30, False),    # no snap at the slot hand-off
    # A centred voice with side content is captured by a half-plane slot: its
    # 600-2500 Hz go 44-68 % rear (0.17 overall on 09-25).  A centre gate on the
    # slot score only reaches 0.10 and breaks pan_sweep/cross_sides hold.
    ("vocal_centre", "median_rear", 0.10, True),
    ("dense", "steady_std", 0.08, False),
]
# Set from the first run of these scenes; see the SCENE_NOTES printed by --check.
CHECKS += [
    ("phase_inverted", "steady_std", 0.10, False),
    ("reverb_lead", "steady_std", 0.10, False),
    ("ping_pong", "steady_std", 0.10, False),
    ("wide_chorus", "steady_std", 0.10, False),
    ("song_change", "first_second_max", 0.20, False),  # no stale slot after a cut
    # Same cause as octave_jump: taper + rear low shelf (0.59 on 09-25).
    ("chirp", "freq_slope", 0.15, True),
]
# Second batch, 2026-09-25 evening.  Passing ones lock in what holds today.
CHECKS += [
    ("two_objects_lr", "steady_std", 0.08, False),   # L and R objects held independently
    ("pan_jump", "worst_half_s", 0.15, False),       # slot hand-off leaves no hole
    ("break_silence", "steady_std", 0.10, False),    # a 3 s break does not reset the scene
    ("sidechain_pump", "steady_std", 0.08, False),   # a ducking bed does not pump the lead
    ("drums_same_side", "steady_std", 0.08, False),
    ("bed_mono", "steady_std", 0.08, False),
    ("lofi_band", "steady_std", 0.08, False),
    ("portamento", "steady_std", 0.08, False),
    ("long_run", "abs_drift", 0.05, False),          # 90 s: no slow creep
    # Known defects (values from the first run):
    ("crescendo", "range_1s", 0.20, True),           # high line moves FRONT as it gets louder (0.7+)
    # The synthetic pluck alone reads as "vocal" to the guard (17 % of frames on
    # real solo guitars too): the vocal gate holds it front, so a band entry or
    # a breakdown moves it 0.6.  Real fix = a better solo-instrument/vocal call.
    ("bed_dropout", "worst_half_s", 0.15, True),
    ("band_enters", "range_1s", 0.15, True),
    ("solo_no_bed", "worst_half_s", 0.15, False),    # periodic dropouts gone with the attack detector off
    ("bed_only", "range_1s", 0.15, True),            # the bed's rear share follows the pad swell (0.22)
    ("double_tracked", "steady_std", 0.08, True),    # hard L/R takes flicker (0.12)
    ("hihat_side", "steady_std", 0.15, True),        # each hat hit is front OR rear (0.33); sub-50 ms, listen
    ("arpeggio", "steady_std", 0.08, True),          # pitch dependence on real melodic material (0.11)
    ("melody_3oct", "steady_std", 0.08, True),       # (0.12)
    # Synthetic formant voice at centre goes 41 % rear.  The guard is a learned
    # model and this is not a real voice: confirm with MoisesDB vocal stems.
    ("vocal_formant", "median_rear", 0.15, True),
]


def robustness(render, args, out):
    """Renderer sanity on hostile input, shipped knobs.  Returns failure lines."""
    fails = []

    def run(name, x):
        src, dst = os.path.join(out, f"rb_{name}.wav"), os.path.join(out, f"rb_{name}.6ch.wav")
        write_wav(src, x)
        subprocess.run([render] + args + [src, dst], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return read_wav6(dst), open(dst, "rb").read()

    sec = 8.0
    lead, rest, on, _ = scenes(sec)["steady"]
    mix = lead + rest
    norm = 0.7 / np.abs(mix).max()

    y, _ = run("silence", np.zeros((2, int(sec * SR))))
    if np.abs(y).max() > 2 / 32768:
        fails.append(f"silence in -> {np.abs(y).max() * 32768:.0f} LSB out")

    # -40 dB: same scene, the lead must sit at the same depth (no level gate).
    y, _ = run("quiet", mix * norm * 0.01)
    loud, _ = run("loud", mix * norm)
    a = np.nanmedian(rear_share(lead * norm * 0.01, rest * norm * 0.01, y)[0][40:])
    b = np.nanmedian(rear_share(lead * norm, rest * norm, loud)[0][40:])
    if not abs(a - b) < 0.10:
        fails.append(f"-40 dB lead rear share {a:.2f} vs {b:.2f} at 0 dB")

    # Brick-walled input: the rear limiter must hold, no wrapped samples anywhere.
    y, _ = run("hot", np.clip(mix * norm * 4, -1, 1))
    rear_peak = np.abs(y[4:6]).max()
    if rear_peak > 32275 / 32768 + 1e-4:
        fails.append(f"hot input: rear peak {rear_peak:.4f} > limiter 0.985")
    jumps = np.abs(np.diff(y, axis=1)).max()
    if jumps > 1.5:
        fails.append(f"hot input: sample-to-sample jump {jumps:.2f} FS (wrap-around)")

    # Streaming starts at an arbitrary sample: the answer must not depend on
    # where the STFT hops fall.  Delay the same input by 0/32/64/96 samples.
    meds, stds = [], []
    for off in (0, 32, 64, 96):
        shifted = np.pad(mix * norm, ((0, 0), (off, 0)))[:, :mix.shape[1]]
        y, _ = run(f"hop{off}", shifted)
        sl = np.pad(lead * norm, ((0, 0), (off, 0)))[:, :mix.shape[1]]
        sr_ = np.pad(rest * norm, ((0, 0), (off, 0)))[:, :mix.shape[1]]
        sh = rear_share(sl, sr_, y)[0][40:]
        meds.append(np.nanmedian(sh)); stds.append(np.nanstd(sh))
    if max(meds) - min(meds) > 0.05 or max(stds) - min(stds) > 0.03:
        fails.append(f"hop phase: rear share {min(meds):.2f}..{max(meds):.2f}, "
                     f"std {min(stds):.2f}..{max(stds):.2f} across 0..96 sample offsets")

    _, first = run("det_a", mix * norm)
    _, second = run("det_b", mix * norm)
    if first != second:
        fails.append("same input rendered twice gives different bytes")
    return fails


def check(results):
    lines, bad, xfail_n = [], 0, 0
    for scene, key, limit, xfail in CHECKS:
        v = abs(results[scene][key[4:]]) if key.startswith("abs_") else results[scene][key]
        ok = v < limit
        tag = ("XPASS" if xfail else "ok") if ok else ("XFAIL" if xfail else "FAIL")
        bad += tag in ("FAIL", "XPASS")
        xfail_n += tag == "XFAIL"
        lines.append(f"  {tag:5s} {scene}.{key} = {v:.3f} (< {limit})")
    print(f"OK ({xfail_n} xfail)" if not bad else "FAILED\n" + "\n".join(lines))
    return bad


def render_one(render, args, out, name, lead, rest, on, variant_name):
    """Normalise, render, measure.  Safe to call from threads (own files)."""
    mix = lead + rest
    peak = np.abs(mix).max()
    lead, rest, mix = lead / peak * 0.7, rest / peak * 0.7, mix / peak * 0.7
    src = os.path.join(out, f"{name}.wav")
    if not os.path.exists(src):
        write_wav(src, mix)
    dst = os.path.join(out, f"{name}.{variant_name}.6ch.wav")
    subprocess.run([render] + args + [src, dst],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    share, w, dt = rear_share(lead, rest, read_wav6(dst))
    return metrics(share, w, dt, on), (share, dt)


def pmap(fn, items):
    from concurrent.futures import ThreadPoolExecutor
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        return list(ex.map(lambda it: fn(*it), items))


# ---------------------------------------------------------------- sweep
SWEEP_PANS = [-1, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75, 1]
SWEEP_PITCH = [110, 220, 440, 880, 1760, 3520]
SWEEP_TIMBRE = {"pluck": lambda sec, f: pluck_line(sec, [f, f * 1.12, f * 1.26]) * 0.25,
                "sustain": lambda sec, f: sustain_line(sec, [f, f * 1.12, f * 1.26]) * 0.2}


def sweep(render, args, out, sec=8.0):
    """Grid: pan x pitch x timbre x background, one instrument per clip.
    Invariants: depth independent of pitch; no cliff between neighbouring pans;
    centre stays front; each clip stable in time.  Writes sweep.csv + sweep.png."""
    rl, rr = room(sec)
    beds = {"pad": pan(pad(sec), 0) * 0.25 + np.stack([rl, rr]) * 0.05,
            "dense": pan(pad(sec), 0) * 0.2 + pan(drums(sec) * 0.3, 0.1) +
                     pan(pluck_line(sec, [523, 587, 659], bpm=90) * 0.12, 0.6)}
    items, keys = [], []
    for tn, tf in SWEEP_TIMBRE.items():
        for f in SWEEP_PITCH:
            dry = tf(sec, f)
            for bn, bed in beds.items():
                for p in SWEEP_PANS:
                    name = f"sw_{tn}_{f}_{bn}_{p:+.2f}"
                    items.append((render, args, out, name, pan(dry, p), bed,
                                  np.ones(int(sec * SR), bool), "shipped"))
                    keys.append((tn, f, bn, p))
    res = pmap(render_one, items)
    grid = {k: m for k, (m, _) in zip(keys, res)}
    with open(os.path.join(out, "sweep.csv"), "w") as fh:
        fh.write("timbre,pitch,bed,pan,median_rear,steady_std,onset_jump\n")
        for (tn, f, bn, p), m in grid.items():
            fh.write(f"{tn},{f},{bn},{p},{m['median_rear']:.3f},{m['steady_std']:.3f},{m['onset_jump']:.3f}\n")

    findings = []
    for tn in SWEEP_TIMBRE:
        for bn in beds:
            for p in SWEEP_PANS:
                v = [grid[(tn, f, bn, p)]["median_rear"] for f in SWEEP_PITCH]
                if np.nanmax(v) - np.nanmin(v) > 0.20:
                    findings.append(f"pitch-dependent depth  {tn:7s} {bn:5s} pan {p:+.2f}: "
                                    + " ".join(f"{f}:{x:.2f}" for f, x in zip(SWEEP_PITCH, v)))
            for f in SWEEP_PITCH:
                v = [grid[(tn, f, bn, p)]["median_rear"] for p in SWEEP_PANS]
                d = np.abs(np.diff(v))
                if np.nanmax(d) > 0.30:
                    i = int(np.nanargmax(d))
                    findings.append(f"pan cliff              {tn:7s} {bn:5s} {f:5d} Hz: "
                                    f"pan {SWEEP_PANS[i]:+.2f}->{SWEEP_PANS[i+1]:+.2f} "
                                    f"rear {v[i]:.2f}->{v[i+1]:.2f}")
                c = grid[(tn, f, bn, 0)]["median_rear"]
                if c > 0.15:
                    findings.append(f"centre goes rear       {tn:7s} {bn:5s} {f:5d} Hz: {c:.2f}")
                for p in SWEEP_PANS:
                    sd = grid[(tn, f, bn, p)]["steady_std"]
                    if sd > 0.10:
                        findings.append(f"unstable in time       {tn:7s} {bn:5s} {f:5d} Hz pan {p:+.2f}: std {sd:.2f}")

    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    panels = [(tn, bn, key) for key in ("median_rear", "steady_std")
              for tn in SWEEP_TIMBRE for bn in beds]
    fig, axs = plt.subplots(2, 4, figsize=(16, 7))
    for ax, (tn, bn, key) in zip(axs.flat, panels):
        img = np.array([[grid[(tn, f, bn, p)][key] for p in SWEEP_PANS] for f in SWEEP_PITCH])
        im = ax.imshow(img, origin="lower", aspect="auto", cmap="magma",
                       vmin=0, vmax=1 if key == "median_rear" else 0.2)
        ax.set_xticks(range(len(SWEEP_PANS)), [f"{p:+.2g}" for p in SWEEP_PANS], fontsize=7)
        ax.set_yticks(range(len(SWEEP_PITCH)), SWEEP_PITCH, fontsize=7)
        ax.set_title(f"{key} · {tn} · {bn}", fontsize=9)
        for (i, j), v in np.ndenumerate(img):
            ax.text(j, i, f"{v:.2f}", ha="center", va="center", fontsize=6,
                    color="w" if v < (0.5 if key == "median_rear" else 0.1) else "k")
    fig.supxlabel("pan (-1 = L)"); fig.supylabel("pitch Hz")
    fig.tight_layout(); fig.savefig(os.path.join(out, "sweep.png"), dpi=110)
    return findings


# ---------------------------------------------------------------- grids 2/3
def run_grid(render, args, out, prefix, specs, sec=8.0):
    """specs: [(key, lead2, rest2)] -> {key: (metrics, (share, dt))}."""
    on = np.ones(int(sec * SR), bool)
    items = [(render, args, out, f"{prefix}_" + "_".join(str(k) for k in key), lead, rest, on, "shipped")
             for key, lead, rest in specs]
    res = pmap(render_one, items)
    return {key: r for (key, _, _), r in zip(specs, res)}


def note_flip(share, dt, note_len, start=2.0):
    """Rear share late in each note minus early in the note (median over notes)."""
    t = np.arange(len(share)) * dt
    ph = (t % note_len) / note_len
    ok = t > start
    early = np.nanmedian(np.where(ok & (ph > 0.05) & (ph < 0.30), share, np.nan))
    late = np.nanmedian(np.where(ok & (ph > 0.65) & (ph < 0.95), share, np.nan))
    return late - early


NOTE_LENS = [0.15, 0.3, 0.6, 1.2, 2.5, 5.0]
# (pitch, pan) spots for the time grid: a well-behaved one and the two corners
# the pan x pitch sweep flagged (high notes near the centre / high notes).
TIME_SPOTS = [(880, -0.5), (1760, -0.25), (3520, -0.5)]
LEAD_DB = [-12, -6, 0, 6, 12]
HARMONICS = [1, 2, 4, 8, 16]
VIBRATO = [0.0, 0.2, 0.8]


def sweep_time(render, args, out, sec=10.0):
    """Grid 2: note length x lead/bed level, sustain 880 Hz at pan -0.5.
    Looks for: in-note flips (capture then release), depth that follows the
    lead's level, notes too short to be captured."""
    rl, rr = room(sec)
    bed = pan(pad(sec), 0) * 0.25 + np.stack([rl, rr]) * 0.05
    specs = []
    for f, p in TIME_SPOTS:
        for nl in NOTE_LENS:
            dry = sustain_line(sec, [f, f * 1.12, f * 1.335], bpm=60 / nl) * 0.2
            for db in LEAD_DB:
                specs.append(((f, p, nl, db), pan(dry * 10 ** (db / 20), p), bed))
    res = run_grid(render, args, out, "t", specs, sec)
    grid = {k: m for k, (m, _) in res.items()}
    flips = {k: note_flip(tr[0], tr[1], k[2]) for k, (_, tr) in res.items()}
    with open(os.path.join(out, "sweep_time.csv"), "w") as fh:
        fh.write("pitch,pan,note_len,lead_db,median_rear,steady_std,note_flip\n")
        for (f, p, nl, db), m in grid.items():
            fh.write(f"{f},{p},{nl},{db},{m['median_rear']:.3f},{m['steady_std']:.3f},{flips[(f, p, nl, db)]:.3f}\n")
    findings = []
    for f, p in TIME_SPOTS:
        spot = f"{f} Hz pan {p:+.2f}"
        for nl in NOTE_LENS:
            v = [grid[(f, p, nl, db)]["median_rear"] for db in LEAD_DB]
            if np.nanmax(v) - np.nanmin(v) > 0.25:
                findings.append(f"depth follows lead level  {spot} note {nl:4.2f} s: "
                                + " ".join(f"{db:+d}dB:{x:.2f}" for db, x in zip(LEAD_DB, v)))
        for db in LEAD_DB:
            v = [grid[(f, p, nl, db)]["median_rear"] for nl in NOTE_LENS]
            if np.nanmax(v) - np.nanmin(v) > 0.30:
                findings.append(f"depth follows note length {spot} lead {db:+d} dB: "
                                + " ".join(f"{nl}s:{x:.2f}" for nl, x in zip(NOTE_LENS, v)))
        for (ff, pp, nl, db), fl in flips.items():
            if (ff, pp) == (f, p) and abs(fl) > 0.20:
                findings.append(f"in-note flip              {spot} note {nl:4.2f} s lead {db:+d} dB: "
                                f"late-early {fl:+.2f} (rear {grid[(ff, pp, nl, db)]['median_rear']:.2f})")
            if (ff, pp) == (f, p) and grid[(ff, pp, nl, db)]["steady_std"] > 0.10:
                findings.append(f"unstable in time          {spot} note {nl:4.2f} s lead {db:+d} dB: "
                                f"std {grid[(ff, pp, nl, db)]['steady_std']:.2f}")
    for f, p in TIME_SPOTS:
        sub_grid = {(nl, db): m for (ff, pp, nl, db), m in grid.items() if (ff, pp) == (f, p)}
        sub_flips = {(nl, db): fl for (ff, pp, nl, db), fl in flips.items() if (ff, pp) == (f, p)}
        _heat(out, f"sweep_time_{f}_{p:+.2f}.png",
              [("median_rear", sub_grid, lambda m: m["median_rear"], 1),
               ("steady_std", sub_grid, lambda m: m["steady_std"], 0.3),
               ("note_flip", sub_grid, lambda m: None, 0.5)],
              NOTE_LENS, LEAD_DB, "lead level dB", "note length s", sub_flips)
    return findings


def sweep_timbre(render, args, out, sec=8.0):
    """Grid 3: harmonics x vibrato x pitch, sustained notes at pan -0.5."""
    rl, rr = room(sec)
    bed = pan(pad(sec), 0) * 0.25 + np.stack([rl, rr]) * 0.05
    specs = [((h, v, f), pan(sustain_line(sec, [f, f * 1.12, f * 1.26], bright=h, vib=v) * 0.2, -0.5), bed)
             for f in (220, 880, 3520) for h in HARMONICS for v in VIBRATO]
    res = run_grid(render, args, out, "h", specs, sec)
    grid = {k: m for k, (m, _) in res.items()}
    with open(os.path.join(out, "sweep_timbre.csv"), "w") as fh:
        fh.write("harmonics,vibrato,pitch,median_rear,steady_std\n")
        for (h, v, f), m in grid.items():
            fh.write(f"{h},{v},{f},{m['median_rear']:.3f},{m['steady_std']:.3f}\n")
    findings = []
    for f in (220, 880, 3520):
        for v in VIBRATO:
            r = [grid[(h, v, f)]["median_rear"] for h in HARMONICS]
            if np.nanmax(r) - np.nanmin(r) > 0.25:
                findings.append(f"depth follows harmonics   {f:5d} Hz vib {v}: "
                                + " ".join(f"{h}:{x:.2f}" for h, x in zip(HARMONICS, r)))
        for h in HARMONICS:
            r = [grid[(h, v, f)]["median_rear"] for v in VIBRATO]
            if np.nanmax(r) - np.nanmin(r) > 0.20:
                findings.append(f"depth follows vibrato     {f:5d} Hz {h:2d} harm: "
                                + " ".join(f"{v}:{x:.2f}" for v, x in zip(VIBRATO, r)))
        for (h, v, ff), m in grid.items():
            if ff == f and m["steady_std"] > 0.10:
                findings.append(f"unstable in time          {f:5d} Hz {h:2d} harm vib {v}: std {m['steady_std']:.2f}")
    return findings


def _heat(out, fname, panels, rows, cols, xlabel, ylabel, extra):
    import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
    fig, axs = plt.subplots(1, len(panels), figsize=(6 * len(panels), 4))
    for ax, (title, grid, get, vmax) in zip(np.atleast_1d(axs), panels):
        img = np.array([[get(grid[(r, c)]) if get(grid[(r, c)]) is not None else extra[(r, c)]
                         for c in cols] for r in rows], float)
        ax.imshow(img, origin="lower", aspect="auto", cmap="magma", vmin=-vmax if title == "note_flip" else 0, vmax=vmax)
        ax.set_xticks(range(len(cols)), cols); ax.set_yticks(range(len(rows)), rows)
        ax.set_title(title); ax.set_xlabel(xlabel); ax.set_ylabel(ylabel)
        for (i, j), v in np.ndenumerate(img):
            ax.text(j, i, f"{v:+.2f}" if title == "note_flip" else f"{v:.2f}", ha="center", va="center", fontsize=7, color="w" if abs(v) < vmax / 2 else "k")
    fig.tight_layout(); fig.savefig(os.path.join(out, fname), dpi=110)


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--guard")
    ap.add_argument("--out", required=True)
    ap.add_argument("--seconds", type=float, default=20.0)
    ap.add_argument("--variants", default="car_0922,shipped")
    ap.add_argument("--plot", action="store_true", help="write share-over-time PNGs")
    ap.add_argument("--sweep", action="store_true",
                    help="pan x pitch x timbre x background grid on shipped knobs (~25 s)")
    ap.add_argument("--sweep-time", action="store_true",
                    help="note length x lead level grid: in-note flips, level dependence")
    ap.add_argument("--sweep-timbre", action="store_true",
                    help="harmonics x vibrato x pitch grid")
    ap.add_argument("--check", action="store_true",
                    help="shipped knobs only, assert CHECKS, exit 1 on FAIL/XPASS")
    a = ap.parse_args()
    if a.check:
        a.variants = "shipped"
    os.makedirs(a.out, exist_ok=True)
    model = ["--model", a.model] + (["--guard-model", a.guard] if a.guard else [])

    for flag, fn, label in ((a.sweep, sweep, "sweep.csv / sweep.png"),
                            (a.sweep_time, sweep_time, "sweep_time.csv / sweep_time.png"),
                            (a.sweep_timbre, sweep_timbre, "sweep_timbre.csv")):
        if flag:
            findings = fn(a.render, VARIANTS["shipped"] + model, a.out)
            print(f"{len(findings)} findings ({label})")
            for line in findings:
                print("  " + line)
    if a.sweep or a.sweep_time or a.sweep_timbre:
        return 0

    sc = scenes(a.seconds)
    sc["long_run"] = long_scene() + ("90 s steady lead: slow drift / oscillation",)
    items = [(a.render, VARIANTS[v] + model, a.out, name, lead, rest, on, v)
             for name, (lead, rest, on, _) in sc.items() for v in a.variants.split(",")]
    for name in sc:                       # the source WAV is shared by all variants
        lead, rest, _, _ = sc[name]
        mix = lead + rest
        write_wav(os.path.join(a.out, f"{name}.wav"), mix / np.abs(mix).max() * 0.7)
    rows, traces = [], {}
    for it, (m, tr) in zip(items, pmap(render_one, items)):
        rows.append((it[3], it[7], m))
        traces[(it[3], it[7])] = tr

    if a.check:
        bad = check({n: m for n, _, m in rows})
        rb = robustness(a.render, VARIANTS["shipped"] + model, a.out)
        for line in rb:
            print("  FAIL  robustness: " + line)
        return 1 if bad or rb else 0

    hdr = (f"{'scene':18s} {'variant':11s} {'rear':>6s} {'maxstep':>8s} {'p95step':>8s} {'std':>6s} "
           f"{'onsetjmp':>8s} {'pitchgap':>8s} {'worst.5s':>8s} {'range1s':>8s}")
    print(hdr); print("-" * len(hdr))
    for name, v, m in rows:
        print(f"{name:18s} {v:11s} {m['median_rear']:6.2f} {m['max_step']:8.2f} "
              f"{m['p95_step']:8.2f} {m['steady_std']:6.3f} {m['onset_jump']:8.2f} {m['pitch_gap']:+8.2f} "
              f"{m['worst_half_s']:8.2f} {m['range_1s']:8.2f}")
    print("\nrear = median share of the lead's energy in Ls+Rs; maxstep/p95step = biggest "
          "change of that share between 53 ms frames; onsetjmp = largest step in the first 0.3 s of each entry; pitchgap = rear share 1760 Hz minus 440 Hz notes (octave_jump).")

    if a.plot:
        import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
        names = sorted({n for n, _ in traces})
        fig, axs = plt.subplots(len(names), 1, figsize=(11, 2.2 * len(names)), sharex=True)
        for ax, n in zip(axs, names):
            for (nn, v), (s, dt) in traces.items():
                if nn == n:
                    ax.plot(np.arange(len(s)) * dt, s, label=v, lw=1)
            ax.set_ylabel(n, rotation=0, ha="right"); ax.set_ylim(0, 1); ax.grid(alpha=.3)
        axs[0].legend(loc="upper right", fontsize=8); axs[-1].set_xlabel("s")
        fig.suptitle("lead rear share over time"); fig.tight_layout()
        fig.savefig(os.path.join(a.out, "rear_share.png"), dpi=110)
    return 0


if __name__ == "__main__":
    sys.exit(main())
