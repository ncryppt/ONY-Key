#!/usr/bin/env python3
"""Learns ONY Key's key model from labelled evidence, honestly evaluated.

    python3 scripts/train_key_model.py <features.csv> [options] [--emit header.h [--emit-only]]

The shipped model (Source/DSP/KeyModel.h) is:
    python3 scripts/train_key_model.py features.csv --balance 0.25 --hidden 16 --extra \
        --emit Source/DSP/KeyModel.h --emit-only

Input: the CSV written by ONYKeyFeatures (one row per labelled file, with the
detector's raw 10-cent pitch-class histograms for all / bass / mid).

The model is rotation-invariant: for each mode (major, minor) it learns one
weight profile over the per-register chroma (bass, mid, high, all), and a
key's score is that profile dotted with the chroma rotated to the key's
tonic. That's a multinomial logistic regression with weights shared across
the 12 tonics, i.e. "learned key profiles" that also know which register a
note was heard in. Small (2 x 48 weights + 2 biases), and trivial to run in
the plugin.

Every number printed is leave-one-pack-out: the model scoring a pack never
saw that pack in training, so the accuracy isn't memorised. --emit trains
on everything and writes the weights as a C++ header.
"""

import argparse
import csv
import math
import sys

import numpy as np

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
BINS_PER_SEMI = 10
NB = 12 * BINS_PER_SEMI


def parse_key(label):
    tonic, mode = label.split()
    return NOTE_NAMES.index(tonic) + (12 if mode == "minor" else 0)


def load(path):
    rows = list(csv.reader(open(path)))
    header, rows = rows[0], rows[1:]
    idx = {h: i for i, h in enumerate(header)}
    a0 = idx["all0"]
    nbands = (len(header) - a0) // NB
    data = []
    for r in rows:
        hist = np.array(r[a0:a0 + nbands * NB], dtype=np.float64).reshape(nbands, NB)
        data.append({
            "path": r[idx["path"]], "label": r[idx["label"]], "group": r[idx["group"]], "kind": r[idx["kind"]],
            "pred_key": int(r[idx["pred_key"]]), "pred_kind": int(r[idx["pred_kind"]]), "pred_root": int(r[idx["pred_root"]]),
            "hist": hist,
        })
    return data


def fold_all(hist):
    """10-cent histograms -> tuning-corrected 12-bin chroma for every band
    (same folding as KeyDetector::computeResult)."""
    b = np.arange(NB)
    phasor = np.sum(hist[0] * np.exp(2j * np.pi * b / BINS_PER_SEMI))
    tuning = np.angle(phasor) / (2 * np.pi)
    pc = (np.round(b / BINS_PER_SEMI - tuning).astype(int)) % 12
    out = np.zeros((hist.shape[0], 12))
    for band in range(hist.shape[0]):
        np.add.at(out[band], pc, hist[band])
    return out


def fold(hist):
    out = fold_all(hist)
    high = np.clip(out[0] - out[1] - out[2], 0, None)
    return out[0], out[1], out[2], high


def features(item, transform):
    folded = fold_all(item["hist"])
    all_, bass, mid = folded[0], folded[1], folded[2]
    high = np.clip(all_ - bass - mid, 0, None)
    chosen = [bass, mid, high, all_]
    bands = []
    for v in chosen:
        s = v.max()
        v = v / s if s > 0 else v
        if transform == "sqrt":
            v = np.sqrt(v)
        elif transform == "log":
            v = np.log1p(9 * v) / np.log(10)
        bands.append(v - v.mean())
    return np.concatenate(bands)  # 12 per band


SHAATH = np.array([[6.6, 2.0, 3.5, 2.3, 4.6, 4.0, 2.5, 5.2, 2.4, 3.7, 2.3, 3.4],
                   [6.5, 2.7, 3.5, 5.4, 2.6, 3.5, 2.5, 5.2, 4.0, 2.7, 4.3, 3.2]])


def corr(a, b):
    a = a - a.mean(axis=-1, keepdims=True)
    b = b - b.mean()
    return (a * b).sum(axis=-1) / (np.sqrt((a * a).sum(axis=-1) * (b * b).sum()) + 1e-12)


def rotations(x, extra=False):
    """x: (N, 48) -> (N, 12, D): features rotated so tonic t sits at index 0.
    With extra=True, appends the Sha'ath-profile correlations (major, minor)
    of the rotated all-band and bass+all chroma, the hand-tuned detector's
    main evidence, so the model can build on it."""
    n = x.shape[0]
    nb = x.shape[1] // 12
    xb = x.reshape(n, nb, 12)
    out = np.empty((n, 12, nb, 12))
    for t in range(12):
        out[:, t] = np.roll(xb, -t, axis=2)
    flat = out.reshape(n, 12, nb * 12)
    if not extra:
        return flat
    allc = out[:, :, 3, :]
    mix = out[:, :, 3, :] + 0.35 * out[:, :, 0, :]
    ex = np.stack([corr(allc, SHAATH[0]), corr(allc, SHAATH[1]), corr(mix, SHAATH[0]), corr(mix, SHAATH[1])], axis=2)
    return np.concatenate([flat, ex], axis=2)


def init_params(d, hidden, rng):
    if hidden == 0:
        return {"W": np.zeros((2, d)), "b": np.zeros(2)}
    return {"W1": rng.normal(0, 1 / np.sqrt(d), (2, d, hidden)), "b1": np.zeros((2, hidden)),
            "w2": rng.normal(0, 1 / np.sqrt(hidden), (2, hidden)), "b2": np.zeros(2)}


def forward(P, xr):
    """Logits (N, 24) and a cache for backprop."""
    if "W" in P:
        return np.concatenate([xr @ P["W"][0] + P["b"][0], xr @ P["W"][1] + P["b"][1]], axis=1), None
    cache, logits = [], []
    for m in range(2):
        z = xr @ P["W1"][m] + P["b1"][m]          # (N, 12, H)
        h = np.maximum(z, 0)
        logits.append(h @ P["w2"][m] + P["b2"][m])  # (N, 12)
        cache.append((z, h))
    return np.concatenate(logits, axis=1), cache


def grads(P, xr, g, cache):
    if "W" in P:
        return {"W": np.stack([np.einsum("nt,ntd->d", g[:, :12], xr), np.einsum("nt,ntd->d", g[:, 12:], xr)]),
                "b": np.array([g[:, :12].sum(), g[:, 12:].sum()])}
    G = {k: np.zeros_like(v) for k, v in P.items()}
    for m in range(2):
        z, h = cache[m]
        gm = g[:, m * 12:(m + 1) * 12]               # (N, 12)
        G["w2"][m] = np.einsum("nt,nth->h", gm, h)
        G["b2"][m] = gm.sum()
        gh = gm[:, :, None] * P["w2"][m][None, None, :] * (z > 0)
        G["W1"][m] = np.einsum("ntd,nth->dh", xr, gh)
        G["b1"][m] = gh.sum(axis=(0, 1))
    return G


def train(xr, y, sample_w, l2=0.01, iters=1500, lr=0.05, hidden=0, seed=0):
    """Softmax over 24 keys with weights shared across the 12 tonics."""
    rng = np.random.default_rng(seed)
    P = init_params(xr.shape[2], hidden, rng)
    M = {k: np.zeros_like(v) for k, v in P.items()}
    V = {k: np.zeros_like(v) for k, v in P.items()}
    n = xr.shape[0]
    onehot = np.zeros((n, 24)); onehot[np.arange(n), y] = 1
    lr = lr if hidden == 0 else lr * 0.4
    for it in range(1, iters + 1):
        logits, cache = forward(P, xr)
        logits -= logits.max(axis=1, keepdims=True)
        p = np.exp(logits); p /= p.sum(axis=1, keepdims=True)
        g = (p - onehot) * sample_w[:, None] / sample_w.sum()
        G = grads(P, xr, g, cache)
        for k in P:
            if k in ("W", "W1", "w2"):
                G[k] = G[k] + l2 * P[k]
            M[k] *= 0.9; M[k] += 0.1 * G[k]
            V[k] *= 0.999; V[k] += 0.001 * G[k] * G[k]
            P[k] -= lr * (M[k] / (1 - 0.9 ** it)) / (np.sqrt(V[k] / (1 - 0.999 ** it)) + 1e-8)
    return P


def predict(xr, P):
    return forward(P, xr)[0]


def mirex(truth, guess):
    if truth == guess: return 1.0
    same = (truth >= 12) == (guess >= 12)
    d = (guess % 12 - truth % 12) % 12
    if same and d in (5, 7): return 0.5
    rel = (truth % 12 + 3) % 12 if truth >= 12 else 12 + (truth % 12 + 9) % 12
    if guess == rel: return 0.3
    if not same and truth % 12 == guess % 12: return 0.2
    return 0.0


def report(name, truth, guess, groups):
    truth, guess = np.array(truth), np.array(guess)
    exact = truth == guess
    maj, mnr = truth < 12, truth >= 12
    mx = np.mean([mirex(t, g) for t, g in zip(truth, guess)])
    per_pack = {}
    for gname in sorted(set(groups)):
        m = np.array([g == gname for g in groups])
        per_pack[gname] = exact[m].mean()
    pack_mean = np.mean(list(per_pack.values()))
    rel = np.where(truth >= 12, (truth % 12 + 3) % 12, 12 + (truth % 12 + 9) % 12)
    same_scale = exact | (guess == rel)
    balanced = 0.5 * (exact[maj].mean() + exact[mnr].mean())
    print(f"{name:34s} exact {100*exact.mean():5.1f}%  balanced {100*balanced:5.1f}%  major {100*exact[maj].mean():5.1f}%  "
          f"minor {100*exact[mnr].mean():5.1f}%  key-or-relative {100*same_scale.mean():5.1f}%  mirex {100*mx:5.1f}%  per-pack {100*pack_mean:5.1f}%")
    return per_pack


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("features")
    ap.add_argument("--transform", default="sqrt")
    ap.add_argument("--l2", type=float, default=0.01)
    ap.add_argument("--balance", type=float, default=0.5, help="0 = natural major/minor mix, 1 = fully balanced")
    ap.add_argument("--emit", help="write the trained model (all data) as a C++ header")
    ap.add_argument("--emit-only", action="store_true", help="skip the held-out evaluation, just train on everything and emit")
    ap.add_argument("--exclude", default="", help="comma-separated group prefixes to leave out entirely")
    ap.add_argument("--packs", action="store_true", help="print per-pack accuracy")
    ap.add_argument("--hidden", type=int, default=0, help="0 = linear profiles, >0 = small MLP")
    ap.add_argument("--extra", action="store_true", help="add the Sha'ath correlation features")
    ap.add_argument("--iters", type=int, default=1500)
    args = ap.parse_args()

    data = load(args.features)
    excluded = [e for e in args.exclude.split(",") if e]
    keys = [d for d in data if d["kind"] == "key" and not any(d["group"].startswith(e) for e in excluded)]
    notes = [d for d in data if d["kind"] == "note"]
    y = np.array([parse_key(d["label"]) for d in keys])
    groups = [d["group"] for d in keys]
    print(f"{len(keys)} key-labelled files, {len(set(groups))} packs, {np.sum(y < 12)} major; {len(notes)} one-shot notes\n")

    x = np.stack([features(d, args.transform) for d in keys])
    xr = rotations(x, args.extra)

    # Sample weights: blend between the natural mix and equal major/minor mass.
    n_maj, n_min = np.sum(y < 12), np.sum(y >= 12)
    w_maj = (1 - args.balance) + args.balance * (len(y) / (2 * n_maj))
    w_min = (1 - args.balance) + args.balance * (len(y) / (2 * n_min))
    sw = np.where(y < 12, w_maj, w_min)

    if args.emit_only:
        P = train(xr, y, sw, l2=args.l2, hidden=args.hidden, iters=args.iters)
        emit_header(args.emit, P, args)
        print(f"wrote {args.emit}")
        return

    base_guess = np.array([d["pred_key"] for d in keys])
    gs = np.array([g.startswith("GiantSteps") for g in groups])
    syn = np.array([g.startswith("Synthetic") for g in groups])
    packs = ~gs & ~syn

    def report_split(name, guess_all):
        """Whole set, then GiantSteps (the benchmark), the sample packs and the
        synthetic progressions separately."""
        out = report(name, y, guess_all, groups)
        for label, m in (("  - GiantSteps", gs), ("  - sample packs", packs), ("  - synthetic progressions", syn)):
            if m.any() and (~m).any():
                report(label, y[m], guess_all[m], [g for g, k in zip(groups, m) if k])
        return out

    base = report_split("current detector (baseline)", base_guess)

    guess = np.zeros_like(y)
    probs = np.zeros((len(y), 24))
    for gname in sorted(set(groups)):
        test = np.array([g == gname for g in groups])
        P = train(xr[~test], y[~test], sw[~test], l2=args.l2, hidden=args.hidden, iters=args.iters)
        logits = predict(xr[test], P)
        guess[test] = logits.argmax(axis=1)
        e = np.exp(logits - logits.max(axis=1, keepdims=True))
        probs[test] = e / e.sum(axis=1, keepdims=True)
    learned = report_split(f"learned ({args.transform}, bal {args.balance}, l2 {args.l2}, h {args.hidden}{', +corr' if args.extra else ''})", guess)

    # Calibration of the model's own probability for its top answer.
    top = probs.max(axis=1)
    print("\n  held-out calibration (model probability -> exact rate):")
    for lo, hi in ((0, .3), (.3, .5), (.5, .7), (.7, .85), (.85, 1.01)):
        m = (top >= lo) & (top < hi)
        if m.sum():
            print(f"    {lo:.2f}-{min(hi,1):.2f}: {m.sum():4d} files, {100*np.mean(y[m] == guess[m]):5.1f}% exact")

    if args.packs:
        print("\n  per pack (baseline -> learned):")
        for gname in sorted(base):
            n = sum(g == gname for g in groups)
            print(f"    {n:4d}  {100*base[gname]:5.1f}% -> {100*learned[gname]:5.1f}%  {gname}")

    # One-shots: is the root note right? (Model trained on all key data.)
    if notes:
        P = train(xr, y, sw, l2=args.l2, hidden=args.hidden, iters=args.iters)
        truth_pc = np.array([NOTE_NAMES.index(d["label"]) for d in notes])
        base_root = np.array([d["pred_root"] for d in notes])
        base_flagged = np.mean([d["pred_kind"] == 2 for d in notes])
        xn = rotations(np.stack([features(d, args.transform) for d in notes]), args.extra)
        model_root = predict(xn, P).argmax(axis=1) % 12
        print(f"\none-shot notes ({len(notes)}): baseline root {100*np.mean(base_root == truth_pc):.1f}% "
              f"(flagged as root note {100*base_flagged:.1f}%), learned-key tonic {100*np.mean(model_root == truth_pc):.1f}%")

    if args.emit:
        P = train(xr, y, sw, l2=args.l2, hidden=args.hidden, iters=args.iters)
        emit_header(args.emit, P, args)
        print(f"\nwrote {args.emit}")


def emit_header(path, P, args):
    """Writes the trained model as a C++ header (KeyModel.h). Always emitted in
    MLP form; a linear model becomes a 1-unit identity layer."""
    if "W" in P:
        d = P["W"].shape[1]
        W1 = P["W"][:, :, None]                     # (2, D, 1)
        b1 = np.zeros((2, 1))
        w2 = np.ones((2, 1))
        b2 = P["b"]
        linear = True
    else:
        W1, b1, w2, b2 = P["W1"], P["b1"], P["w2"], P["b2"]
        d, linear = W1.shape[1], False
    h = W1.shape[2]

    def arr(v):
        """Nested C++ initialiser matching the array's shape."""
        v = np.asarray(v)
        if v.ndim == 1:
            return ", ".join(f"{x:.7g}f" for x in v)
        return ", ".join("{ " + arr(sub) + " }" for sub in v)

    lines = [
        "#pragma once",
        "",
        "// Generated by scripts/train_key_model.py -- do not edit by hand.",
        f"// transform={args.transform} balance={args.balance} l2={args.l2} hidden={args.hidden} extra={args.extra}",
        "",
        "namespace onykey::model",
        "{",
        "",
        "/** Learned, rotation-invariant key scorer. For each candidate tonic the",
        "    per-register chroma (bass, mid, high, all: each max-normalised,",
        "    square-rooted, mean-centred) is rotated so the tonic sits at index 0;",
        f"    {'plus the Sha' + chr(39) + 'ath correlations (major/minor, all and all+bass); ' if args.extra else ''}"
        "then per mode (0 major, 1 minor):",
        "        score = w2 . relu (W1^T x + b1) + b2",
        "    and the 24 scores go through a softmax. */",
        f"inline constexpr int inputs = {d};",
        f"inline constexpr int hidden = {h};",
        f"inline constexpr bool linear = {'true' if linear else 'false'};",
        f"inline constexpr bool usesCorrelations = {'true' if args.extra else 'false'};",
        "",
        f"inline constexpr float W1[2][{d}][{h}] = {{ {arr(W1)} }};",
        f"inline constexpr float b1[2][{h}] = {{ {arr(b1)} }};",
        f"inline constexpr float w2[2][{h}] = {{ {arr(w2)} }};",
        f"inline constexpr float b2[2] = {{ {arr(b2)} }};",
        "",
        "} // namespace onykey::model",
        "",
    ]
    open(path, "w").write("\n".join(lines))


if __name__ == "__main__":
    main()
