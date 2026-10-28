#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Sill's drum kit: seven one-shots and the crowd's three shouts synthesized here, written as 16-bit mono WAVs at 44.1 kHz to
assets/samples-cc0/DRUMS/. No recording and no third-party sample goes into them: every sound is made by the code
below, and the WAVs are dedicated to the public domain (CC0 1.0, see assets/samples-cc0/ATTRIBUTION.txt).

    python3 tools/si_drums.py            writes the WAVs (needs numpy)

The kit is built for crispness: every hit starts at full level on its first sample (no fade-in), its transient is
shaped separately from its body, and the hats and cymbal keep their air up to 20 kHz (stored at 44.1 kHz, as
tools/gen_samples.py keeps the DRUMS set). The files' order is their key on the FM-1 (F3 upward), as
tools/gen_samples.py maps a kit: 00 KICK PUNCH, 01 KICK ROUND, 02 SNARE, 03 CLAP, 04 HAT, 05 OPEN HAT, 06 CRASH,
07 HEY, 08 HO, 09 WHOA. The shouts are a crowd made here too: eight voices (glottal pulses at men's and women's
pitches, each a little late and out of tune), an aspirated onset, through formant filters gliding from vowel to
vowel; no recording of anyone's voice.
Run it again and the same bytes come out (the noise is seeded).
"""
import struct
from pathlib import Path

import numpy as np

SR = 44100
OUT = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0" / "DRUMS"
rng = np.random.default_rng(0x0DA0)


def t_axis(sec):
    return np.arange(int(sec * SR)) / SR


def noise(n):
    return rng.uniform(-1.0, 1.0, n)


def onepole_hp(x, fc):
    """a one-pole high-pass at fc (Hz)"""
    a = np.exp(-2 * np.pi * fc / SR)
    y = np.zeros_like(x)
    prev_x = prev_y = 0.0
    for i, v in enumerate(x):
        prev_y = a * (prev_y + v - prev_x)
        prev_x = v
        y[i] = prev_y
    return y


def biquad(x, kind, fc, q):
    """RBJ cookbook biquad: 'lp', 'hp' or 'bp' (constant peak gain)"""
    w = 2 * np.pi * fc / SR
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    if kind == "lp":
        b = [(1 - c) / 2, 1 - c, (1 - c) / 2]
    elif kind == "hp":
        b = [(1 + c) / 2, -(1 + c), (1 + c) / 2]
    else:
        b = [al, 0.0, -al]
    a = [1 + al, -2 * c, 1 - al]
    b = [v / a[0] for v in b]
    a = [1.0, a[1] / a[0], a[2] / a[0]]
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b[0] * v + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def env_exp(t, tau):
    return np.exp(-t / tau)


def norm(x, peak=0.98):
    return x * (peak / max(1e-9, np.max(np.abs(x))))


def fade_tail(x, sec):
    n = min(len(x), int(sec * SR))
    x[-n:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, n))
    return x


def sine_sweep(t, f0, f1, tau):
    """a sine whose pitch falls from f0 to f1 with time constant tau; starts at phase 0, so the body itself does
    not click: the beater (kick's tick) is the transient, shaped on its own"""
    f = f1 + (f0 - f1) * np.exp(-t / tau)
    ph = 2 * np.pi * np.cumsum(f) / SR
    return np.sin(ph - ph[0])


def kick(f0, f1, tau, decay, click, sec, drive):
    t = t_axis(sec)
    body = sine_sweep(t, f0, f1, tau) * env_exp(t, decay)
    # the beater: a 1.6 ms burst of high-passed noise and a 3 kHz tick, both gone at once
    n = noise(len(t))
    tick = onepole_hp(n, 2500) * env_exp(t, 0.0016) * click
    tick += np.sin(2 * np.pi * 3000 * t) * env_exp(t, 0.0009) * click * 0.6
    x = np.tanh((body + tick) * drive) / np.tanh(drive)
    return fade_tail(x, 0.04)


def snare():
    t = t_axis(0.30)
    tone = (np.sin(2 * np.pi * 186 * t) * 0.9 + np.sin(2 * np.pi * 334 * t) * 0.5) * env_exp(t, 0.055)
    wires = biquad(noise(len(t)), "bp", 4200, 0.7) * 2.2
    wires = onepole_hp(wires, 1400) * env_exp(t, 0.085)
    snap = onepole_hp(noise(len(t)), 5000) * env_exp(t, 0.004) * 1.4      # the stick: a sharp edge on top
    x = np.tanh((tone * 0.75 + wires + snap) * 1.6)
    return fade_tail(x, 0.05)


def clap():
    t = t_axis(0.36)
    n = biquad(noise(len(t)), "bp", 1250, 1.4)
    n += biquad(noise(len(t)), "bp", 2600, 1.0) * 0.6
    e = np.zeros_like(t)
    for k, at in enumerate((0.0, 0.0105, 0.0205)):   # three hands, ~10 ms apart, then the room
        d = t - at
        e += np.where(d >= 0, np.exp(-np.clip(d, 0, None) / 0.0042), 0.0) * (1.0 - 0.12 * k)
    d = t - 0.029
    e += np.where(d >= 0, np.exp(-np.clip(d, 0, None) / 0.105), 0.0) * 0.55
    x = np.tanh(n * e * 2.6)
    x = onepole_hp(x, 500)
    return fade_tail(x, 0.06)


# the metal: six square waves at inharmonic ratios (the classic analogue cymbal recipe), through a band-pass
METAL_HZ = (205.3, 304.4, 369.6, 522.7, 540.0, 800.0)


def metal(t, scale=1.0):
    x = np.zeros_like(t)
    for f in METAL_HZ:
        x += np.sign(np.sin(2 * np.pi * f * scale * t + f))
    return x / len(METAL_HZ)


def hat(decay, sec, bright):
    t = t_axis(sec)
    m = metal(t, 1.0)
    m = biquad(m, "bp", 10000, 0.9) * 1.6 + biquad(m, "hp", 7600, 0.7)
    air = onepole_hp(noise(len(t)), 9000) * 0.45 * bright
    x = (m + air) * env_exp(t, decay)
    x += onepole_hp(noise(len(t)), 12000) * env_exp(t, 0.0012) * 0.8     # the tip of the stick
    x = biquad(x, "hp", 6000, 0.7)
    return fade_tail(x, min(0.04, sec / 4))


def crash():
    t = t_axis(1.0)
    m = metal(t, 1.37) + metal(t, 0.81) * 0.7
    m = biquad(m, "hp", 4200, 0.6)
    air = onepole_hp(noise(len(t)), 5000) * 0.8
    x = (m + air) * (env_exp(t, 0.42) * 0.85 + env_exp(t, 0.006) * 0.6)
    x = biquad(x, "hp", 3200, 0.7)
    return fade_tail(x, 0.35)


# the vowels as their first three formants (Hz) and bandwidths: a shout's
VOWEL = {"h": (600, 1700, 2600), "e": (560, 1850, 2550), "i": (330, 2250, 2950), "o": (520, 880, 2450),
         "u": (360, 720, 2350), "w": (320, 640, 2300), "a": (760, 1150, 2500)}
BW = (90.0, 120.0, 180.0)


def glide(path, n):
    """formant tracks (3 x n) through path: [(time 0..1, vowel), ...]"""
    tt = np.linspace(0.0, 1.0, n)
    xs = [p[0] for p in path]
    return [np.interp(tt, xs, [VOWEL[p[1]][k] for p in path]) for k in range(3)]


def formant_filter(x, tracks):
    """three resonators in parallel, their centre frequencies moving sample by sample"""
    y = np.zeros_like(x)
    gains = (1.0, 4.0, 4.0)                    # (a shout is bright: the upper formants strong)
    for k in range(3):
        f = tracks[k]
        r = np.exp(-np.pi * BW[k] / SR)
        c1 = 2 * r * np.cos(2 * np.pi * f / SR)
        c2 = -r * r
        y1 = y2 = 0.0
        out = np.zeros_like(x)
        g = (1 - r * r) * gains[k]
        for i in range(len(x)):
            o = g * x[i] + c1[i] * y1 + c2 * y2
            y2, y1 = y1, o
            out[i] = o
        y += out
    return y


def crowd(sec, path, pitch, aspir, env_pts):
    """a crowd's shout: eight voices summed, through the vowels in path; pitch (start, end) as a ratio of each
    voice's own; aspir the breath at its onset; env_pts [(time s, level)] its loudness"""
    n = int(sec * SR)
    t = np.arange(n) / SR
    src = np.zeros(n)
    voices = [(150, 0.000), (172, 0.012), (186, 0.006), (163, 0.021), (198, 0.009),
              (262, 0.004), (290, 0.016), (318, 0.010)]
    for f0, late in voices:
        f = f0 * (1.0 + rng.uniform(-0.03, 0.03)) * np.interp(t / sec, [0, 1], pitch)
        f *= 1.0 + 0.012 * np.sin(2 * np.pi * rng.uniform(4.5, 6.5) * t)          # the voice's own wobble
        ph = np.cumsum(f) / SR
        saw = 2.0 * (ph - np.floor(ph)) - 1.0
        k = int(late * SR)
        src[k:] += np.roll(saw, k)[k:] * (0.85 if f0 > 240 else 1.0)
    src = biquad(src, "lp", 6500, 0.6)                                         # the voices' natural tilt
    src = src - 0.94 * np.concatenate(([0.0], src[:-1]))                      # the mouth's radiation (+6 dB/oct)
    src += onepole_hp(noise(n), 900) * aspir * np.exp(-t / 0.035)            # the breath of the onset
    src += onepole_hp(noise(n), 1500) * 0.06                                  # and a little all through
    y = formant_filter(src, glide(path, n))
    xs = [p[0] for p in env_pts]
    y *= np.interp(t, xs, [p[1] for p in env_pts])
    room = np.zeros_like(y)                                                    # a hall's first reflections
    for d, g in ((0.017, 0.32), (0.029, 0.22), (0.043, 0.15), (0.061, 0.1)):
        k = int(d * SR)
        room[k:] += y[:-k] * g
    return fade_tail(y + room, 0.03)


def hey():
    return crowd(0.42, [(0, "h"), (0.12, "e"), (0.7, "e"), (1.0, "i")], (1.18, 0.92), 2.2,
                 [(0, 0), (0.025, 1.0), (0.30, 0.85), (0.42, 0.0)])


def ho():
    return crowd(0.40, [(0, "h"), (0.12, "o"), (0.75, "o"), (1.0, "u")], (1.12, 0.9), 2.0,
                 [(0, 0), (0.025, 1.0), (0.28, 0.8), (0.40, 0.0)])


def whoa():
    return crowd(0.85, [(0, "w"), (0.18, "o"), (0.55, "a"), (1.0, "a")], (0.95, 1.08), 0.6,
                 [(0, 0), (0.06, 0.9), (0.45, 1.0), (0.85, 0.0)])


KIT = [
    ("00_kick_punch", lambda: kick(170, 41, 0.042, 0.50, 1.0, 0.80, 2.2)),
    ("01_kick_round", lambda: kick(110, 36, 0.075, 0.75, 0.45, 1.00, 1.5)),
    ("02_snare", snare),
    ("03_clap", clap),
    ("04_hat", lambda: hat(0.032, 0.11, 1.0)),
    ("05_open_hat", lambda: hat(0.26, 0.56, 1.1)),
    ("06_crash", crash),
    ("07_hey", hey),
    ("08_ho", ho),
    ("09_whoa", whoa),
]


def write_wav(path, x):
    data = np.clip(np.round(norm(x) * 32767), -32768, 32767).astype("<i2").tobytes()
    fmt = struct.pack("<HHIIHH", 1, 1, SR, SR * 2, 2, 16)
    body = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(body)) + b"WAVE" + body)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.wav"):
        old.unlink()
    for name, fn in KIT:
        x = fn()
        write_wav(OUT / f"{name}.wav", x)
        print(f"si_drums: {name}.wav  {len(x) / SR * 1000:.0f} ms")


if __name__ == "__main__":
    main()
