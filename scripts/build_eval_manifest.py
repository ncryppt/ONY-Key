#!/usr/bin/env python3
"""Builds the labelled evaluation set for ONY Key's key detector from a local
sample library (nothing here is committed; only the manifest format is).

    python3 scripts/build_eval_manifest.py <out.csv> <library folder>... [--giantsteps <dataset dir>]
                                           [--synthetic <dir from make_synthetic_progressions.py>]

Each row: path, label, group, kind
  kind  = "key"  -> label is a key ("G minor"), from the file or folder name
  kind  = "note" -> label is a single pitch class ("C#"), for pitched one-shots
  group = the sample pack it came from; cross-validation holds out whole
          groups, so a pack is never both trained and tested on.

--giantsteps adds the GiantSteps Key benchmark (Knees et al., ISMIR 2015:
604 two-minute EDM previews with checked key annotations; clone
github.com/GiantSteps/giantsteps-key-dataset and run its audio_dl.sh). Its
tracks are split into 5 fixed folds ("GiantSteps fold N"), so it is always
scored held out like everything else.

Files whose names say they aren't tonal (drums, percussion, FX, vocals) are
skipped even when their kit carries a key label: they'd only add label noise.
"""

import csv
import os
import re
import sys

AUDIO = (".wav", ".aif", ".aiff", ".flac", ".ogg", ".mp3")
LETTERS = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# Not tonal (or not reliably in the kit's key).
SKIP = re.compile(r"(?<![a-z])(kicks?|snares?|hats?|hh|claps?|perc\w*|drums?\w*|tops?|rides?|crash\w*|cymbals?|"
                  r"shakers?|toms?|rims?\w*|fx|sfx|risers?|impacts?|noise|sweeps?|vox|vocal\w*|voices?|adlibs?|"
                  r"foley|breaks?|grooves?|full ?mix)(?![a-z])", re.I)

KEY_TOKEN = re.compile(r"^([A-G])(#|b|s|sharp|flat)?(maj|major|min|minor|m)$", re.I)
KEY_PHRASE = re.compile(r"(?:^|[ _\-])([A-G])(#|b)? ?(Major|Minor|major|minor)(?=$|[ _\-\d])")
NOTE_OCTAVE = re.compile(r"(?:^|[ _\-])([A-G])(#|b)?(-?[0-7])(?=$|[ _\-])")


def pitch_class(letter, accidental):
    pc = LETTERS[letter.upper()]
    acc = (accidental or "").strip().lower()
    if acc in ("#", "s", "sharp"):
        pc += 1
    elif acc in ("b", "flat"):
        pc -= 1
    return pc % 12


def key_name(pc, minor):
    return f"{NOTE_NAMES[pc]} {'minor' if minor else 'major'}"


def key_from_text(text):
    """Key stated in a file/folder name, or None."""
    found = None
    for tok in re.split(r"[ _\-\.\(\)\[\]]+", text):
        m = KEY_TOKEN.match(tok)
        if m and tok[0].isupper():  # "am"/"em" are usually words
            quality = m.group(3)
            minor = quality == "m" or quality.lower().startswith("min")
            found = key_name(pitch_class(m.group(1), m.group(2)), minor)
    m = KEY_PHRASE.search(text)
    if m:
        found = key_name(pitch_class(m.group(1), m.group(2)), m.group(3).lower().startswith("min"))
    return found


def group_of(path, root):
    rel = os.path.relpath(path, root).split(os.sep)
    # ".../Packs/<pack>/..." or "<product>/..."
    if "Packs" in rel:
        i = rel.index("Packs")
        return rel[i + 1] if i + 1 < len(rel) else rel[0]
    return rel[0]


def giantsteps_rows(root):
    rows = []
    key_dir = os.path.join(root, "annotations", "key")
    for name in sorted(os.listdir(key_dir)):
        track = name.replace(".key", "")
        audio = os.path.join(root, "audio", track + ".mp3")
        if not os.path.exists(audio):
            continue
        tonic, mode = open(os.path.join(key_dir, name)).read().split()[:2]
        pc = pitch_class(tonic[0], tonic[1:] if len(tonic) > 1 else None)
        fold = int(track.split(".")[0]) % 5
        rows.append((os.path.abspath(audio), key_name(pc, mode == "minor"), f"GiantSteps fold {fold}", "key"))
    return rows


def synthetic_rows(root):
    """Files named "<Tonic>_<major|minor>__<progression>__<timbre>.wav"."""
    rows = []
    for f in sorted(os.listdir(root)):
        if not f.endswith(".wav"):
            continue
        tonic, mode = f.split("__")[0].split("_")
        pc = pitch_class(tonic[0], tonic[1:] or None)
        progression = f.split("__")[1]
        # One group per progression type, so held-out scores mean "a
        # progression the model never heard", not just an unseen voicing.
        rows.append((os.path.abspath(os.path.join(root, f)), key_name(pc, mode == "minor"), f"Synthetic {progression}", "key"))
    return rows


def take_option(args, name):
    if name in args:
        i = args.index(name)
        value = args[i + 1]
        del args[i:i + 2]
        return value
    return None


def main():
    args = sys.argv[1:]
    giantsteps = take_option(args, "--giantsteps")
    synthetic = take_option(args, "--synthetic")
    out, roots = args[0], args[1:]
    rows = giantsteps_rows(giantsteps) if giantsteps else []
    if synthetic:
        rows += synthetic_rows(synthetic)
    for root in roots:
        for dirpath, _, files in os.walk(root):
            folder = os.path.basename(dirpath)
            for f in sorted(files):
                if not f.lower().endswith(AUDIO):
                    continue
                path = os.path.abspath(os.path.join(dirpath, f))
                stem = os.path.splitext(f)[0]
                group = group_of(path, root)

                # Pitched single-note one-shots: only the sustained string
                # notes are reliably one clean note (runs/phrases aren't).
                if "Disco Strings" in path and os.sep + "Sustains" in path:
                    m = NOTE_OCTAVE.search(stem)
                    if m:
                        rows.append((path, NOTE_NAMES[pitch_class(m.group(1), m.group(2))], group, "note"))
                    continue

                if SKIP.search(stem):
                    continue

                label = key_from_text(stem) or key_from_text(folder)
                if label and not re.search(r"maj7|min7|m7|maj9|sus|dim|aug", stem, re.I):
                    rows.append((path, label, group, "key"))

    with open(out, "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["path", "label", "group", "kind"])
        w.writerows(rows)

    keys = [r for r in rows if r[3] == "key"]
    notes = [r for r in rows if r[3] == "note"]
    print(f"{len(keys)} key-labelled files in {len({r[2] for r in keys})} packs "
          f"({sum('major' in r[1] for r in keys)} major), {len(notes)} note-labelled one-shots")


if __name__ == "__main__":
    main()
