#!/usr/bin/env python3
"""Renders common chord progressions in all 24 keys, as labelled training and
test material for ONY Key's key model.

    python3 scripts/make_synthetic_progressions.py <out folder>

Real-world labelled music teaches the model the statistics of real tracks,
but it can miss plain functional harmony (it once read every I-IV-V-I in its
subdominant). These files anchor it: textbook progressions, several voicings
and sounds, a bassline, each labelled with its key in the file name:
"<Tonic>_<major|minor>__<progression>__<timbre>.wav". Needs only numpy.
"""

import os
import sys
import wave

import numpy as np

SR = 44100
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# Chords as (semitones above the tonic of the chord root, quality).
MAJOR_PROGRESSIONS = {
    "I-IV-V-I": [(0, "M"), (5, "M"), (7, "M"), (0, "M")],
    "I-V-vi-IV": [(0, "M"), (7, "M"), (9, "m"), (5, "M")],
    "I-vi-IV-V": [(0, "M"), (9, "m"), (5, "M"), (7, "M")],
    "ii-V-I-I": [(2, "m"), (7, "M"), (0, "M"), (0, "M")],
    "I-IV-I-V": [(0, "M"), (5, "M"), (0, "M"), (7, "M")],
    "IV-I-V-I": [(5, "M"), (0, "M"), (7, "M"), (0, "M")],
}
MINOR_PROGRESSIONS = {
    "i-iv-V-i": [(0, "m"), (5, "m"), (7, "M"), (0, "m")],
    "i-VI-III-VII": [(0, "m"), (8, "M"), (3, "M"), (10, "M")],
    "i-VII-VI-VII": [(0, "m"), (10, "M"), (8, "M"), (10, "M")],
    "i-iv-VII-III": [(0, "m"), (5, "m"), (10, "M"), (3, "M")],
    "i-VI-iv-V": [(0, "m"), (8, "M"), (5, "m"), (7, "M")],
    "i-iv-i-v": [(0, "m"), (5, "m"), (0, "m"), (7, "m")],
}


def tone(freq, seconds, timbre, rng):
    t = np.arange(int(seconds * SR)) / SR
    detune = 1.0 + rng.uniform(-0.002, 0.002)
    if timbre == "saw":
        partials = [(h, 1.0 / h) for h in range(1, 13)]
    elif timbre == "square":
        partials = [(h, 1.0 / h) for h in range(1, 13, 2)]
    else:  # "keys": bright attack, few partials
        partials = [(1, 1.0), (2, 0.5), (3, 0.25), (4, 0.12)]
    out = np.zeros_like(t)
    for h, a in partials:
        f = freq * h * detune
        if f < SR * 0.45:
            out += a * np.sin(2 * np.pi * f * t + rng.uniform(0, 2 * np.pi))
    env = np.minimum(1.0, t / 0.01) * np.minimum(1.0, (seconds - t) / 0.03)
    if timbre == "keys":
        env *= np.exp(-t * 1.5)
    return out * env


def midi_to_hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def render(tonic, minor, chords, timbre, rng):
    chord_len = rng.choice([1.0, 1.5, 2.0])
    out = np.zeros(int(chord_len * len(chords) * SR) + SR // 10)
    base = 48 + tonic  # C3..B3
    if base > 54:
        base -= 12
    for i, (offset, quality) in enumerate(chords):
        root = base + offset
        third = 4 if quality == "M" else 3
        notes = [root, root + third, root + 7]
        # Random inversion / spread voicing.
        inv = rng.integers(0, 3)
        notes = sorted(n + (12 if j < inv else 0) for j, n in enumerate(notes))
        start = int(i * chord_len * SR)
        for n in notes:
            seg = tone(midi_to_hz(n), chord_len, timbre, rng) * 0.12
            out[start:start + len(seg)] += seg
        bass = tone(midi_to_hz(root - 12 if root - 12 >= 36 else root), chord_len, "keys" if timbre == "keys" else "saw", rng) * 0.18
        out[start:start + len(bass)] += bass
    out /= max(1e-9, np.max(np.abs(out))) / 0.8
    return out


def main():
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)
    rng = np.random.default_rng(7)
    n = 0
    for tonic in range(12):
        for minor, progressions in ((False, MAJOR_PROGRESSIONS), (True, MINOR_PROGRESSIONS)):
            for name, chords in progressions.items():
                for timbre in ("saw", "square", "keys"):
                    audio = render(tonic, minor, chords, timbre, rng)
                    label = f"{NOTE_NAMES[tonic].replace('#', 's')}_{'minor' if minor else 'major'}"
                    path = os.path.join(out_dir, f"{label}__{name}__{timbre}.wav")
                    with wave.open(path, "wb") as w:
                        w.setnchannels(1)
                        w.setsampwidth(2)
                        w.setframerate(SR)
                        w.writeframes((audio * 32767).astype("<i2").tobytes())
                    n += 1
    print(f"wrote {n} files to {out_dir}")


if __name__ == "__main__":
    main()
