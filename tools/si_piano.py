#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Sill's PIANO: six notes of a Kawai grand from the Versilian Community Sample Library (VCSL, CC0 1.0,
github.com/sgossner/VCSL, "Chordophones/Zithers/Grand Piano, Kawai/Sustains"), its loudest layer (v4), made into the
bright, hard club piano of the 90s: summed to mono, cut short (a stab does not need the long tail), a high shelf
and a presence lift for the brightness, a soft clip for the hardness, normalised; written as 16-bit mono WAVs at
44.1 kHz to assets/samples-cc0/PIANO/ (tools/gen_samples.py stores them at 22.05 kHz).

    python3 tools/si_piano.py VCSL_DIR      VCSL_DIR: a checkout of VCSL (only that folder is needed:
                                            git clone --depth 1 --filter=blob:none --sparse
                                            https://github.com/sgossner/VCSL, then git sparse-checkout set
                                            "Chordophones/Zithers/Grand Piano, Kawai/Sustains")

The same source gives the same bytes. The results stay CC0 (assets/samples-cc0/ATTRIBUTION.txt).
"""
import struct
import sys
import wave
from pathlib import Path

import numpy as np

SR = 44100
OUT = Path(__file__).resolve().parents[1] / "assets" / "samples-cc0" / "PIANO"
FOLDER = "Chordophones/Zithers/Grand Piano, Kawai/Sustains"
# the notes (VCSL's names, C3 = middle C) and how long each is kept (s): the stabs' and the hook's range
NOTES = [("C2", 1.15), ("F#2", 1.05), ("C3", 1.0), ("F#3", 0.95), ("C4", 0.9), ("F#4", 0.85)]


def read(path):
    with wave.open(str(path)) as w:
        n, ch, sw, sr = w.getnframes(), w.getnchannels(), w.getsampwidth(), w.getframerate()
        raw = w.readframes(n)
    assert sw == 2 and sr == SR, (path, sw, sr)
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64).reshape(-1, ch).mean(axis=1) / 32768.0
    return x


def biquad(x, b, a):
    y = np.zeros_like(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        o = b[0] * v + b[1] * x1 + b[2] * x2 - a[1] * y1 - a[2] * y2
        x2, x1, y2, y1 = x1, v, y1, o
        y[i] = o
    return y


def shelf_high(x, fc, db):
    """RBJ high shelf, slope 1"""
    A = 10 ** (db / 40)
    w = 2 * np.pi * fc / SR
    c, s = np.cos(w), np.sin(w)
    al = s / 2 * np.sqrt(2)
    b = [A * ((A + 1) + (A - 1) * c + 2 * np.sqrt(A) * al), -2 * A * ((A - 1) + (A + 1) * c),
         A * ((A + 1) + (A - 1) * c - 2 * np.sqrt(A) * al)]
    a = [(A + 1) - (A - 1) * c + 2 * np.sqrt(A) * al, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - 2 * np.sqrt(A) * al]
    return biquad(x, [v / a[0] for v in b], [1.0, a[1] / a[0], a[2] / a[0]])


def peak_eq(x, fc, q, db):
    A = 10 ** (db / 40)
    w = 2 * np.pi * fc / SR
    al = np.sin(w) / (2 * q)
    c = np.cos(w)
    b = [1 + al * A, -2 * c, 1 - al * A]
    a = [1 + al / A, -2 * c, 1 - al / A]
    return biquad(x, [v / a[0] for v in b], [1.0, a[1] / a[0], a[2] / a[0]])


def make(src, keep):
    x = read(src)
    on = int(np.argmax(np.abs(x) > 0.02 * np.max(np.abs(x))))   # the hammer: start just before it
    x = x[max(0, on - 32):]
    x = x[:int(keep * SR)]
    x = x / np.max(np.abs(x))
    x = shelf_high(x, 2000, 10.0)                   # the brightness
    x = peak_eq(x, 5000, 0.9, 3.0)                  # and its presence
    x = peak_eq(x, 250, 0.8, -3.0)                  # less body: a stab, not a ballad
    x = np.tanh(x * 1.8) / np.tanh(1.8)             # the hardness: the hammer flattened a little, the tail lifted
    n = len(x)
    fade = int(0.12 * SR)                           # a raised-cosine fade over the last 120 ms
    x[n - fade:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, fade))
    return x / np.max(np.abs(x)) * 0.98


def write_wav(path, x):
    data = np.clip(np.round(x * 32767), -32768, 32767).astype("<i2").tobytes()
    fmt = struct.pack("<HHIIHH", 1, 1, SR, SR * 2, 2, 16)
    body = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(body)) + b"WAVE" + body)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    folder = Path(sys.argv[1]) / FOLDER
    OUT.mkdir(parents=True, exist_ok=True)
    for old in OUT.glob("*.wav"):
        old.unlink()
    for k, (note, keep) in enumerate(NOTES):
        src = folder / f"GPiano_sus_{note}_v4_rr1_Player.wav"
        x = make(src, keep)
        write_wav(OUT / f"{k:02d}_Piano_{note}_v4.wav", x)
        print(f"si_piano: {k:02d}_Piano_{note}_v4.wav  {len(x) / SR * 1000:.0f} ms  <-  VCSL {src.name}")


if __name__ == "__main__":
    main()
