#!/usr/bin/env python3
"""Turns the black ONY Key logo (transparent cutout, or black on white) into the two masks the UI paints.

    python3 scripts/make_logo_assets.py [Resources/brand/ONY-Key-logo-source.webp]

Writes:
  Resources/ONYKey_LogoMask.png     the "ONY KEY" wordmark (no tagline) as
                                    white + alpha, cropped and downsampled
  Resources/ONYKey_KeyholeMask.png  just the keyhole cut out of the "O", same
                                    size and position, so the UI can light it

The UI fills these masks with its own metallic gradient and accent glow, so
the logo follows the theme rather than baking colours into the bitmap.
Needs only macOS `sips` and numpy.
"""

import os
import struct
import subprocess
import sys
import tempfile
import zlib

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "Resources/brand/ONY-Key-logo-source.webp")
DOWNSAMPLE = 2  # source is ~1500 px wide; half is plenty for a 2x-retina header


def read_ink(path):
    """Ink coverage 0..1 per pixel. Uses the alpha channel if the source has
    one (a transparent cutout), otherwise darkness on a white background."""
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    w, h = struct.unpack_from("<ii", data, 18)
    bpp = struct.unpack_from("<H", data, 28)[0]
    assert bpp in (24, 32), f"unexpected BMP depth {bpp}"
    channels = bpp // 8
    stride = (w * channels + 3) & ~3
    rows = np.frombuffer(data, dtype=np.uint8, count=stride * abs(h), offset=offset).reshape(abs(h), stride)
    pixels = rows[:, : w * channels].reshape(abs(h), w, channels).astype(np.float32) / 255.0
    if h > 0:  # bottom-up
        pixels = pixels[::-1]

    if channels == 4 and pixels[:, :, 3].min() < 0.5:
        alpha = pixels[:, :, 3]
        darkness = 1.0 - pixels[:, :, :3].mean(axis=2)
        return alpha * darkness
    return 1.0 - pixels[:, :, :3].mean(axis=2)


def write_png(path, rgba):
    h, w, _ = rgba.shape
    raw = b"".join(b"\x00" + rgba[y].tobytes() for y in range(h))
    def chunk(tag, payload):
        return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


def flood_fill(open_mask, seed):
    """Connected region of `open_mask` (bool) containing `seed` (y, x)."""
    region = np.zeros_like(open_mask)
    stack = [seed]
    h, w = open_mask.shape
    while stack:
        y, x = stack.pop()
        if y < 0 or x < 0 or y >= h or x >= w or region[y, x] or not open_mask[y, x]:
            continue
        region[y, x] = True
        stack.extend(((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)))
    return region


def dilate(mask, steps):
    out = mask.copy()
    for _ in range(steps):
        grown = out.copy()
        grown[1:] |= out[:-1]; grown[:-1] |= out[1:]
        grown[:, 1:] |= out[:, :-1]; grown[:, :-1] |= out[:, 1:]
        out = grown
    return out


def downsample(a, f):
    h, w = a.shape[0] // f * f, a.shape[1] // f * f
    return a[:h, :w].reshape(h // f, f, w // f, f).mean(axis=(1, 3))


def main():
    with tempfile.TemporaryDirectory() as tmp:
        bmp = os.path.join(tmp, "logo.bmp")
        subprocess.run(["sips", "-s", "format", "bmp", SOURCE, "--out", bmp], check=True, capture_output=True)
        ink = read_ink(bmp)

    ink = np.clip((ink - 0.06) / 0.88, 0, 1)  # paper noise to pure 0, ink to pure 1

    # Rows with ink, grouped into bands separated by blank rows: the first
    # two bands are "ONY" and "KEY", the last is the small tagline.
    rows = ink.max(axis=1) > 0.5
    bands, start = [], None
    for y, on in enumerate(rows):
        if on and start is None:
            start = y
        elif not on and start is not None:
            bands.append((start, y)); start = None
    if start is not None:
        bands.append((start, len(rows)))
    bands = [b for b in bands if b[1] - b[0] > 4]
    assert len(bands) >= 3, f"expected wordmark + tagline bands, got {bands}"
    top, bottom = bands[0][0], bands[-2][1]  # drop the tagline

    word = ink[top:bottom]
    cols = np.where(word.max(axis=0) > 0.5)[0]
    pad = 8
    y0, y1 = max(0, top - pad), min(ink.shape[0], bottom + pad)
    x0, x1 = max(0, cols[0] - pad), min(ink.shape[1], cols[-1] + pad + 1)
    word = ink[y0:y1, x0:x1]

    # Keyhole: the enclosed paper region inside the "O" (the first big blob
    # on the left of the top line), found by flood fill from its centre.
    o_rows = slice(bands[0][0] - y0, bands[0][1] - y0)
    o_cols = np.where(word[o_rows].max(axis=0) > 0.5)[0]
    gaps = np.where(np.diff(o_cols) > 3)[0]
    o_right = o_cols[gaps[0]] if len(gaps) else o_cols[-1]
    o_mid_x = (o_cols[0] + o_right) // 2
    o_mid_y = (bands[0][0] + bands[0][1]) // 2 - y0
    paper = word < 0.5
    # Walk right from the O's centre to the first paper pixel inside it.
    seed = None
    for dx in range(-(o_right - o_cols[0]) // 2, (o_right - o_cols[0]) // 2):
        if paper[o_mid_y, o_mid_x + dx]:
            seed = (o_mid_y, o_mid_x + dx); break
    assert seed is not None, "couldn't find the keyhole"
    hole = flood_fill(paper, seed)
    assert hole.sum() < paper.size * 0.05, "keyhole flood fill leaked outside the O"
    keyhole = np.where(dilate(hole, 2), 1.0 - word, 0.0)  # keep anti-aliased edges

    logo = downsample(word, DOWNSAMPLE)
    key = downsample(keyhole, DOWNSAMPLE)

    for name, alpha in (("ONYKey_LogoMask.png", logo), ("ONYKey_KeyholeMask.png", key)):
        a = (np.clip(alpha, 0, 1) * 255 + 0.5).astype(np.uint8)
        rgba = np.dstack([np.full_like(a, 255)] * 3 + [a])
        write_png(os.path.join(ROOT, "Resources", name), rgba)
        print(f"wrote Resources/{name} ({a.shape[1]}x{a.shape[0]})")


if __name__ == "__main__":
    main()
