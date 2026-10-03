# Progress Log

## Particles (2026-10-02)

- ONY Verb's ParticleOverlay ported (`Source/UI/ParticleOverlay.h`), with the
  wheel's centre disc standing in for Verb's orb:
  - ambient glow particles follow the input level while LISTENing (a gentle
    trickle while a file analyses, nothing at rest);
  - bursts on transients in the input (level jumping well above its
    0.25 s average, at most every 0.22 s), when a key is found or changes
    (at most once a second, not for a result restored when the editor
    opens), on click-to-copy, and small pops on every button;
  - Kush Koma: leaf particles, a curling wisp of smoke rising from the disc,
    and the "hotbox" haze that builds over about a minute and clears over
    8 s after switching away;
  - Canada Eh?: beaver particles; Acid Trip: particles shift hue as they age,
    plus the rotating, breathing hypnotic spiral (fade in 3 s, out 1.5 s).
- Not ported: Verb's Insane/Eco modes and the slider drag trickles (ONY Key
  has no sliders).
- Unlike Verb, the overlay repaints only the area its particles cover
  (current and previous frame) unless the haze or spiral is showing, since
  every overlay repaint also repaints the UI beneath it. On light themes,
  particles keep the accent's own brightness so they don't wash out.
- Leaf and beaver paths moved to `Source/UI/Shapes.h` (shared with the logo
  emblem). Snapshot tool gained `--watch` and `--idle`.

## Themes (2026-10-02)

- ONY Verb's theme system ported: the same 18 palettes (9 dark, 9 light,
  same names and colours), with Violet Dusk as ONY Key's default. Picked
  from a header pill whose menu shows a swatch per theme, grouped Dark and
  Light, with "Hide NSFW themes" at the bottom (Kush Koma and Acid Trip,
  same rule as Verb; hiding the active theme moves to the first safe theme
  of the same kind).
- Saved per user rather than per session (`ONYVA/ONY Key/theme.txt` and
  `hidensfw.txt` in Application Support), stored by name.
- No hard-coded colours remain in the UI. Each palette also derives the
  hardware tones (bezel metal, well, segments, glass disc, LEDs, logo
  metal), so tinted themes tint the instrument and light themes become pale
  anodised hardware with a graphite logo and much softer shadows.
- Novelty extras as in Verb: Kush Koma's leaf and Canada Eh?'s maple leaf
  beside the logo, and Acid Trip's continuous rainbow accent cycle with a
  background wash (whole-panel repaint at 30 Hz while that theme is active).
  Verb's particle effects (smoke, beavers, swirl) are not ported.
- `ONYKeySnapshot --theme <name>` renders any theme without touching the
  saved preference; `docs/themes.png` is a contact sheet of eight.

## New logo (2026-10-02)

- The ONY Key logo (`Resources/brand/ONY-Key-logo-source.webp`, a black
  transparent cutout) replaces the drawn placeholder wordmark.
- `scripts/make_logo_assets.py` crops the wordmark (dropping the tagline,
  which the header now spells out as "KEY DETECTION SOFTWARE" in the UI's
  label type) and splits it into two masks: the wordmark, and the keyhole
  found by flood-filling the enclosed hole in the "O".
- The UI paints both masks in its own materials rather than baking colours
  into the bitmap: the letters get a brushed-metal gradient with a cast
  shadow, and the keyhole is lit from behind as a status light. It's a dim
  breathing ember when idle, flares violet when a key is found, pulses while
  analysing, and follows the input level in red while listening. Its light
  spills slightly onto the metal around it.
- Header enlarged for the logo (172 x 64 design px).

## UI premium pass (2026-10-02)

- Material: Avenir Next type (as in ONY Verb), a top-lit grain-textured
  background with an accent glow behind the wheel that rises with a result,
  and raised glass cards with bevel, sheen and drop shadows.
- Wheel: machined bezel with a 72-dot track. The dots light around the
  detected key, sweep while analysing, and shimmer with the input level
  while listening. Glass centre disc. Segments are radially shaded, and the
  key highlight eases from one key to the next with a bloom that flares on
  arrival. The result text settles in. Hovering a key outlines the keys it
  mixes with; clicking the centre copies the result.
- Cards: 12-LED confidence meter with an animated figure, runner-up key with
  its relationship ("relative major", "parallel minor"...), and a tuning
  strip with a needle.
- Note bars: segmented LEDs, spelled for the key, with the tonic marked.
- Buttons: raised and pressed states with drawn icons. LISTEN has an LED that
  follows the input level (the processor now meters its input).
- Frosted drag-and-drop overlay: blurred snapshot, marching-ants border, and
  the dragged file's name.
- Resizable 70-160% at a fixed aspect ratio, saved with the session.
- Flat and sharp signs are drawn as vector shapes, because Avenir Next has
  no ♭/♯ glyphs and the fallback font's look out of place.
- Verified with `ONYKeySnapshot` (now also `--drop`). Unit tests and `auval`
  pass.

## v0.1.0: first build (2026-10-02)

- Scaffold copied from ONY Verb's conventions: JUCE 9.0.2 submodule, CMake,
  VST3 + AU + Standalone (AAX behind `AAX_SDK_PATH`), universal macOS binary,
  static MSVC runtime, Windows GitHub Action, signing scripts.
- `KeyDetector` (headless) plus file and live (LISTEN) analysis paths,
  Camelot-wheel UI, and state saved with the session.

### Detector tuning

Tuned against 1,580 key-labelled loops from the sample packs in
`~/Desktop/SAMPLE PACKS : RACKS` using `ONYKeyEval`. Roughly 90% of these are
labelled minor, so major and minor accuracy were tracked separately so the
detector wasn't just learning the label skew.

| Step | Exact | Major | Minor |
|---|---|---|---|
| Naive: Hann FFT, every peak within 40 dB, KK+Temperley profiles | 25.2% | n/a | n/a |
| Sha'ath profiles + overtone suppression | 39.8% | n/a | n/a |
| Blackman-Harris window, -30 dB floor, 6 dB local prominence | 43.8% | 72.0% | 40.8% |
| Tonic = main bass note bonus (0.2) + minor prior (0.04) | **47.9%** | **75.3%** | **45.0%** |

What made the biggest difference:
- **Flat chroma.** Hann sidelobes (-31 dB) next to strong low notes, plus hat
  and noise peaks, spread energy across all 12 notes. Fixed with the
  Blackman-Harris window and a local-prominence test.
- **Overtones.** The 5th harmonic of a bass note is a major third and the 3rd
  is a fifth. Removing non-octave overtones of a louder lower peak was the
  single largest gain.

Tried with no gain: 16384-point FFT, a wider overtone range (up to the 16th),
linear rather than square-root peak weighting, loudness-weighted frames,
Faraldo's EDMA profiles, Albrecht-Shanahan profiles, and gating the tuning
estimate.

The confidence mapping was calibrated on the same set: High is right 88% of
the time, Good 70%, Fair 44%.

### Verified

- Unit tests pass (`ONYKeyTests`).
- `auval -v aufx Onky Onyv`: AU validation succeeded.
- `ONYKeySnapshot` renders the editor and runs a loop through
  `processBlock()` with LISTEN on. The live result matched the file result
  (G minor), and output was bit-identical to input.

### Not done yet

- Ears-on testing in real hosts (Ableton / Logic / Pro Tools / FL), and
  pluginval (not installed on this machine).
- Dragging clips from a DAW's arrangement view. This works where the host
  hands over a file path (e.g. Ableton's browser). Hosts that drag a region
  rather than a file won't trigger it; LISTEN covers those.
- A dedicated ONY Key logo. The wordmark is currently drawn in code.
- Notarized release build and a GitHub release.
