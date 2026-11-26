# Writing presets and grooves for Loop Draft

This is the guide for authors of content: instruments, drum kits and grooves.
What the sounds and controls are is described in `docs/CAPABILITIES.md`. Read that first.

## How content gets into the app

1. Write one `.json` file per preset or groove.
2. Put it in the repository under `content/library/`:
   - `content/library/instruments/`
   - `content/library/drumkits/`
   - `content/library/grooves/`
3. Check it (see "Checking your files").
4. Commit to `main`. The build checks every file again and ships them inside the app, where they appear in the libraries on the Drums, Synth and Song screens.

A user can also add a single `.json` file on the phone with the FILE button of a library.

Work from the repository's `main` branch. Do not send changed engine or app code together with content: content needs no code changes.

**Older files:** packs, instruments and drum kits written in the earlier "v0.7" layout (`style_pack`, `drum_kit`, `synth`/`fx` blocks) are not loaded by the app. `content/tools/convert_v07.py <library folder> <files>` turns them into this format; check the result afterwards. New content should be written in this format directly.

## Rules for every file

- Plain JSON, UTF-8, one object per file, at most 2 MB.
- `"kind"`: `"instrument"`, `"drumkit"` or `"groove"`. Required.
- `"name"`: shown in the app. Required, at most 48 characters. Make it unique within its kind.
- `"style"`: a short genre tag, shown under the name. Optional but wanted, for example `"acid techno"`.
- `"comment"`, `"author"`: free text, ignored by the app.
- `"format": 1` optional.
- **A value outside its range is an error and the whole file is refused.** Nothing is clamped silently.
- A key the app does not know is ignored and reported as a warning. A warning usually means a typo.
- Names are matched loosely: `"909 Punch"`, `"909-punch"` and `"909punch"` are the same.
- File names: lower case with dashes, for example `rolling-acid-136.json`.

## Instrument

Replaces the whole sound of one synth track and, if `"track"` is given, its mixer strip and effects. Notes stay.
Every key is optional. What you leave out gets the neutral default, not the value the user had: the same file always gives the same sound.

```json
{
  "kind": "instrument", "name": "Rubber Acid", "style": "acid",
  "engine": "subtractive",
  "osc1": "saw", "osc2": "square", "detune": 0,
  "cutoff": 620, "res": 11,
  "attack": 0.002, "decay": 0.2, "sustain": 0.05, "release": 0.05, "level": 0.8,
  "lfo": { "target": "off", "rate": 3, "depth": 0.4 },
  "fm": { "ratio": 2, "index": 2.5, "decay": 0.25, "feedback": 0.15 },
  "pluckBright": 0.6,
  "vector": { "x": 0.5, "y": 0.5 },
  "fold": 0.15, "slideTime": 0.07, "warmth": 0.6, "motion": 0.3,
  "octave": 0, "chord": false,
  "track": { "level": 0.82, "drive": 0.35, "eqLow": 1, "eqHigh": 0, "delay": 0.18, "reverb": 0.05,
             "fx": [ { "type": "overdrive", "amount": 0.45, "rate": 0.4 } ] }
}
```

| Key | Values | Default |
|---|---|---|
| `preset` | start from a built-in sound, then override: `Analog Bass`, `Acid 303`, `Reese DnB`, `FM Bell`, `FM E-Piano`, `Super Pad`, `Pluck Lead` | none |
| `engine` | `subtractive`, `fm`, `pluck`, `vector` | subtractive |
| `osc1`, `osc2` | `saw`, `square`, `triangle`, `sine` | saw, triangle |
| `detune` | -50 .. 50 cents (oscillator 2) | 7 |
| `cutoff` | 60 .. 12000 Hz | 1800 |
| `res` | 0.5 .. 15 | 3.2 |
| `attack` | 0.001 .. 1 s | 0.008 |
| `decay` | 0.02 .. 3 s. For `pluck` this is how long the string rings (times 4) | 0.35 |
| `sustain` | 0 .. 1 | 0 |
| `release` | 0.01 .. 3 s | 0.08 |
| `level` | 0 .. 1 | 0.8 |
| `lfo.target` | `off`, `cutoff`, `pitch`, `volume` | cutoff |
| `lfo.rate` | 0.05 .. 20 Hz | 3 |
| `lfo.depth` | 0 .. 1 | 0.45 |
| `fm.ratio` | 0.5 .. 8 | 2 |
| `fm.index` | 0 .. 8 | 2.5 |
| `fm.decay` | 0.01 .. 3 s | 0.25 |
| `fm.feedback` | 0 .. 1 | 0.15 |
| `pluckBright` | 0 .. 1 | 0.6 |
| `vector.x`, `vector.y` | 0 .. 1. Corners: x0 y0 = osc 1, x1 y0 = osc 2, x0 y1 = FM, x1 y1 = sub | 0.5 |
| `fold` | 0 .. 1 | 0 |
| `slideTime` | 0.005 .. 0.5 s | 0.06 |
| `warmth`, `motion` | 0 .. 1 | 0.5, 0.3 |
| `octave` | -2 .. 2, relative to the project octave | 0 |
| `chord` | `true`: every note plays as a triad from the scale | false |

`track` (the mixer strip), all optional:

| Key | Values |
|---|---|
| `level`, `drive`, `delay`, `reverb` | 0 .. 1 |
| `eqLow`, `eqHigh` | -12 .. 12 dB |
| `fx` | a list of up to 3 slots: `{ "type", "amount" 0..1, "rate" 0..1, "bypass" }`. Types: `overdrive`, `bitcrush`, `chorus`, `phaser`, `filter`, `compressor` (the standard pedals), and the stompbox collection: `distortion` (gain, tone), `fuzz` (fuzz, tone), `booster` (boost, treble), `tremolo` (depth, speed), `vibrato` (depth, speed), `flanger` (feedback, speed), `rotary` (depth, speed), `wah` (depth, speed), `autowah` (sensitivity, peak), `octaver` (sub level, tone), `ringmod` (mix, pitch), `delay` (level, time), `tapeecho` (level, time), `reverb` (level, size), `gate` (threshold, decay); the two words are what `amount` and `rate` do. If `fx` is given, slots you do not list are emptied |

## Drum kit

Sets the kit, the eight pads and the drum track's strip. Patterns stay.
If `"kit"` is given, all pads first go back to that kit's own sound and default settings, then your `"pads"` are applied.

```json
{
  "kind": "drumkit", "name": "Warehouse Hard", "style": "hard techno",
  "kit": "Warehouse Rumble", "hardness": 0.35,
  "track": { "level": 0.85, "eqLow": 2, "reverb": 0.08, "fx": [ { "type": "compressor", "amount": 0.5 } ] },
  "pads": {
    "kick": { "vol": 0.95, "decay": 0.5,
              "synth": { "pitch": 47, "drop": 260, "dropTime": 0.02, "length": 0.5, "tone": 1.0, "noise": 0.05,
                         "noiseFreq": 3000, "fm": 0.3, "fmRatio": 2, "drive": 0.7, "click": 0.6 } },
    "ch":   { "vol": 0.6, "decay": 0.06, "choke": 1 }
  }
}
```

- `kit`: `808 Classic`, `909 Punch`, `Lo-Fi Acoustic`, `FM Industrial`, `Jungle Break`, `Hardcore Gabber`, `Warehouse Rumble`, `Neuro Techstep`, `EBM Electro`, `Glitch Breakcore`.
- `hardness`: 0 .. 1. It sets the drum track's drive; a `track.drive` in the same file wins.
- Pads: `kick`, `snare`, `clap`, `ch`, `oh`, `rim`, `tom`, `perc`.
- Per pad: `vol` 0..1, `pan` -1..1, `pitch` -24..24 semitones, `decay` 0.02..2 s, `choke` 0, 1 or 2.
- `synth`: makes the pad a synthesized drum. `pitch` 25..1200 Hz, `drop` 0..600 Hz, `dropTime` 0.005..0.3 s, `length` 0.05..1.5 s, `tone` 0..1, `noise` 0..1, `noiseFreq` 300..10000 Hz, `fm` 0..8, `fmRatio` 0.5..8, `drive` 0..1, `click` 0..1.
- Only for `Jungle Break`: `"slicer": { "pitch" -12..12, "tight" 0.08..1.5, "crunch" 0..1 }` and per pad `sliceStart` 0..0.99, `sliceEnd` 0.01..1, `reverse`. The slicer has no synthesized pads. A file cannot carry the break audio itself.

## Groove

Replaces the patterns of the scene the user is on, and brings its sounds.

```json
{
  "kind": "groove", "name": "Rolling Acid 136", "style": "acid techno",
  "bpm": 136, "swing": 0, "root": "D", "scale": "minor", "bars": 2,
  "drums": {
    "kit": "909 Punch",
    "steps": {
      "kick": "x...x...x...x...",
      "clap": "....X.......X...",
      "ch":   "..x...x...x...x.|..x...x...x..3x."
    }
  },
  "synths": [
    { "track": "bass", "preset": "Acid 303",
      "notes": [ { "step": 0, "degree": 0, "accent": true },
                 { "step": 3, "degree": 7, "slide": true },
                 { "step": 6, "degree": 0, "len": 2 } ] }
  ]
}
```

**Tempo and key:** `bpm` 40..240, `swing` 0..0.5, `root` `"C"` .. `"B"` (sharps or flats), `scale` `minor`, `major`, `dorian`, `pentatonic`, `octave` 1..5. They belong to the whole project. The user can switch off taking them, so a groove must also make sense in another key and tempo.

**`bars`:** 1 .. 8. Sets the length of the scene.

**Drum steps:** one text per pad, 16 characters per bar.

| Character | Meaning |
|---|---|
| `.` or `-` | silent |
| `x` | hit |
| `X` | accent |
| `2` `3` `4` | a roll of that many hits inside the step |
| space, `\|` | ignored, use them to make bars readable |

A row must be a whole number of bars. A row shorter than the groove repeats: a one-bar kick row fills all bars. If `steps` is given, pads you do not list are silent in this scene.

**Drum sounds:** either `"kit": "<name>"` (only the kit changes, the user's pad settings stay) or `"drumkit": { ... }` with a full drum kit as described above (without its `kind` and `name`). Not both.

**Synths:** a list, one entry per track: `bass`, `lead`, `syn3`, `syn4`. Using `syn3` or `syn4` switches those tracks on.
- Sound: either `"preset": "<built-in name>"` or `"instrument": { ... }` with a full instrument as described above. Not both. Without either, the user's sound stays.
- `notes`: if given, it replaces the track's pattern in this scene. At most 256 notes.

| Note key | Values |
|---|---|
| `step` | 0 .. bars x 16 - 1. Required |
| `degree` | -21 .. 35, steps of the scale from the root. 0 is the root, 7 is an octave up (5 in pentatonic), -1 is the note below the root |
| `midi` | instead of `degree`: a MIDI note number. It is moved to the nearest note of the scale, with a warning if it was not in it |
| `len` | length in 16ths, default 1. Cut at the end of the groove |
| `accent` | `true`: louder and the filter opens for a moment |
| `slide` | `true`: glides in from the note that ends where this one starts, without a new attack. Put it on the second note |

Prefer `degree`: it keeps working when the user changes key or scale.

## Checking your files

Build the tool once:

```
cmake -S engine -B engine/build -DCMAKE_BUILD_TYPE=Release
cmake --build engine/build -j 4
```

Check files. It prints `ok` or the error with its place in the file, and the warnings:

```
./engine/build/ld_content check content/library/*/*.json
```

```
ERROR my-groove.json: synths[0].notes[3].step: 40 is outside 0 .. 31
ok    other.json: instrument "Glass Kalimba", pluck
      warning: filterEnv: unknown key, ignored
```

Listen to a file without the app. It is rendered on top of the start project: a groove plays its scene, an instrument plays the start project's bass line alone, a drum kit plays the start project's drums alone.

```
./engine/build/ld_content render content/library/grooves/rolling-acid-136.json out.wav
./engine/build/ld_content midi   content/library/grooves/rolling-acid-136.json out.mid
```

A file is ready when `check` prints `ok` with no warnings and the rendered peak is above 0.3 (not too quiet). The master limiter keeps the peak below 1.

## What makes good content

- **Levels:** keep `level` of the tracks around 0.6 to 0.85 so that a groove does not hit the limiter all the time.
- **Bass:** keep `octave` at 0 or -1 for bass sounds; say so in `comment` if a sound is meant for one register.
- **Grooves bring their sounds.** A groove with patterns only will play on whatever the user has loaded.
- **One idea per groove**, 1 to 4 bars. Variations are separate files with related names, so the user can put them on scenes A to D.
- **Automation is not part of a groove.** It belongs to the song chain. Mention useful moves in `comment`, for example "automate cutoff 400 to 3000 in the build".
- Describe in `comment` how the content is meant to be used.
