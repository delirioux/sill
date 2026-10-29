#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Sill's choir: a sung OH and a sung AH, two notes each, synthesized here and written as 16-bit mono WAVs at
22,050 Hz to assets/samples-cc0/CHOIR/. No recording and no third-party sample goes into them: every sound is made by
the code below, and the WAVs are dedicated to the public domain (CC0 1.0, see assets/samples-cc0/ATTRIBUTION.txt).

    python3 tools/si_choir.py            writes the WAVs (needs numpy)

A choir, not a crowd: twelve voices singing one note together, each a few cents off and a little late, with its own
slow vibrato; soft glottal pulses (the voice's tilt steep, as singing is, not shouting); the vowel's formants wide and
dark; a breath in the onset, a slow swell, a long fade, a hall's first reflections. tools/gen_samples.py puts the
four into the DRUMS set as pitched zones (si_sounds.h SIS_CHOIR_KEY): OH and AH, each from F3 and from F4, played
across an octave around each. Run it again and the same bytes come out (the noise is seeded).
"""
import struct
from pathlib import Path

import numpy as np

SR = 22050
SEC = 0.9                                  # a note: the swell, the held vowel, the fade (the flash: 4 x 10 KB)
OUT = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0" / "CHOIR"
rng = np.random.default_rng(0x5111)

# the sung vowels: three formants (Hz), their bandwidths (Hz) and levels; a choir's (mixed voices), darker than speech
VOWEL = {
    "oh": ((450, 800, 2830), (80, 90, 140), (1.0, 0.55, 0.5)),
    "ah": ((750, 1150, 2800), (90, 100, 140), (1.0, 0.70, 0.7)),
}
NOTES = (53, 65)                           # each vowel sung at F3 and F4


def noise(n):
    return rng.standard_normal(n)


def onepole_lp(x, fc):
    a = np.exp(-2 * np.pi * fc / SR)
    y = np.zeros_like(x)
    s = 0.0
    for i in range(len(x)):
        s = (1 - a) * x[i] + a * s
        y[i] = s
    return y


def resonator(x, f, bw, g):
    r = np.exp(-np.pi * bw / SR)
    c1, c2 = 2 * r * np.cos(2 * np.pi * f / SR), -r * r
    y = np.zeros_like(x)
    y1 = y2 = 0.0
    k = (1 - r * r) * g
    for i in range(len(x)):
        o = k * x[i] + c1 * y1 + c2 * y2
        y2, y1 = y1, o
        y[i] = o
    return y


def sing(midi, vowel):
    n = int(SEC * SR)
    t = np.arange(n) / SR
    f0 = 440.0 * 2 ** ((midi - 69) / 12)
    src = np.zeros(n)
    for v in range(12):
        cents = rng.uniform(-9, 9)
        late = int(rng.uniform(0, 0.025) * SR)
        rate, depth = rng.uniform(4.6, 5.8), rng.uniform(0.003, 0.006)
        vib = 1 + depth * np.sin(2 * np.pi * rate * t + rng.uniform(0, 6.3)) * np.clip(t / 0.25, 0, 1)
        f = f0 * 2 ** (cents / 1200) * vib
        ph = np.cumsum(f) / SR + rng.uniform(0, 1)
        pulse = 2.0 * (ph - np.floor(ph)) - 1.0                    # the glottis: a saw, its edge softened below
        g = np.zeros(n)
        g[late:] = pulse[:n - late]
        src += g * rng.uniform(0.8, 1.0)
    src = onepole_lp(onepole_lp(src, 1400), 4000)                  # the singing voice's tilt (-12 dB/oct)
    src += onepole_lp(noise(n), 3000) * 0.12 * np.exp(-t / 0.08)   # the breath at the onset
    src += onepole_lp(noise(n), 2500) * 0.02                       # and a little all through
    fs, bws, gs = VOWEL[vowel]
    y = sum(resonator(src, f, b, g) for f, b, g in zip(fs, bws, gs))
    y = y - np.concatenate(([0.0], y[:-1])) * 0.5                  # the mouth's radiation, gently
    env = np.interp(t, [0, 0.09, 0.30, 0.55, SEC], [0, 0.9, 1.0, 0.75, 0])
    y *= env * env
    room = np.zeros_like(y)                                         # a hall's first reflections
    for d, gr in ((0.019, 0.30), (0.031, 0.24), (0.047, 0.18), (0.067, 0.12), (0.089, 0.08)):
        k = int(d * SR)
        room[k:] += y[:-k] * gr
    y = y + room
    k = int(0.06 * SR)
    y[-k:] *= np.linspace(1, 0, k)
    return y


def write_wav(path, x):
    x = x / (np.max(np.abs(x)) + 1e-12) * 0.98
    data = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2").tobytes()
    fmt = struct.pack("<HHIIHH", 1, 1, SR, SR * 2, 2, 16)
    body = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(body)) + b"WAVE" + body)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.wav"):
        old.unlink()
    for vowel in VOWEL:
        for m in NOTES:
            name = f"{vowel}_{m}.wav"
            write_wav(OUT / name, sing(m, vowel))
            print(f"si_choir: {name}  {SEC * 1000:.0f} ms")


if __name__ == "__main__":
    main()
