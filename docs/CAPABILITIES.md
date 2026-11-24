# Loop Draft: what the app can do (v0.8)

Reference for anyone designing sounds, presets and grooves for Loop Draft.
Everything here is taken from the engine code on `main`. If something is not listed, the app cannot do it.

## 1. Shape of a project

| Thing | Limit |
|---|---|
| Tracks | 1 drum track + 2 to 4 synth tracks (Bass, Lead, Syn 3, Syn 4) |
| Scenes | 4 (A, B, C, D). Each holds one drum pattern and one pattern per synth |
| Scene length | 1 to 8 bars, 16 steps per bar (16th notes), 4/4 only |
| Song | a chain of up to 64 slots, each slot plays one scene once |
| Tempo | 40 to 240 BPM, one tempo for the whole project |
| Swing | 0 to 0.5, delays every second 16th |
| Key | one root (C..B) and one scale for the whole project: Minor, Major, Dorian, Minor Pentatonic |
| Polyphony | 16 drum voices, 8 voices per synth |

There are no triplets, no other time signatures, no tempo or key changes inside a project.

## 2. Drums

8 pads. Default names and General MIDI notes used in the MIDI export:

| Pad | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|---|---|---|---|---|---|---|---|---|
| Name | Kick | Snare | Clap | Closed hat | Open hat | Rim | Tom | Perc |
| GM note | 36 | 38 | 39 | 42 | 46 | 37 | 45 | 64 |

**Step values:** 0 off, 1 normal, 2 accent, 3/4/5 = roll of 2/3/4 hits inside the step.
MIDI velocity is 96 for normal and 127 for accent. There are only these two levels.

**Per pad:** volume, pan, pitch (-12..+12 semitones), decay (0.03..1.5 s), choke group (off, 1, 2; by default the hats share group 1).

**Kits** (all synthesized, no samples shipped):

| # | Kit | Character |
|---|---|---|
| 0 | 808 Classic | round, long kick, soft hats |
| 1 | 909 Punch | punchy house/techno kit |
| 2 | Lo-Fi Acoustic | dusty, boom-bap style |
| 3 | FM Industrial | metallic, harsh |
| 4 | Jungle Break | break slicer, see below |
| 5 | Hardcore Gabber | hard-clipped kick with a "tok" attack |
| 6 | Warehouse Rumble | deep techno kick with a rumble tail, dark rides |
| 7 | Neuro Techstep | very tight kick, two-layer crack snare, sharp hats |
| 8 | EBM Electro | dry 80s drums, ring-mod cowbell, zaps |
| 9 | Glitch Breakcore | click kicks, crushed snares, digital bursts |

**Hardness** (0..1): one control that drives the whole drum track.

**Drum synth:** any pad can play a drum synthesized from these controls instead of the kit sound.

| Control | Range |
|---|---|
| Pitch (base frequency) | 25..1200 Hz |
| Drop (pitch sweep at the start) | 0..600 Hz |
| Drop time | 5..300 ms |
| Length | 0.05..1.5 s |
| Tone / Noise mix | 0..1 each |
| Noise frequency | 300..10000 Hz |
| FM amount / FM ratio | 0..8 / 0.5..8 |
| Drive | 0..1 |
| Click | 0..1 |

Starting points: Kick, Snare, Hat, Gabber kick, Zap, Clang.

**Break slicer (kit 4):** the 8 pads are 8 slices of one loop. The loop is the built-in synthesized break (made for 168 BPM), a WAV the user loads, or a scene's drums bounced with their effects. At most 24 seconds. Per slice: start, end, reverse. Global: repitch, tightness, crunch. The slicer keeps its own patterns and pad settings, separate from the hit kits.

## 3. Synths

Notes are stored as **scale degrees**, not as MIDI notes: degree 0 is the root, 7 is one octave up in a 7-note scale (5 in pentatonic), negative degrees go below. Notes outside the scale cannot be written.
MIDI note = 12 x (project octave + synth octave shift + 1) + root + scale interval.

**Per note:** start step, length in 16ths, accent (louder and the filter opens briefly), slide (glides in from the note before with no new attack, like a 303).
**Chord switch** per synth: every note plays as a triad stacked in thirds inside the scale.
Up to 256 notes per synth per scene.

**Four engines**, chosen per synth:

| Engine | What it is | Its own controls |
|---|---|---|
| Subtractive | two oscillators (Saw, Square, Triangle, Sine), second one detunable (-30..+30 cents) | Wave 1, Wave 2, Detune |
| FM 2-Op | one sine carrier, one sine modulator with its own decay | Ratio 0.5..8, Index 0..8, Mod decay, Feedback 0..1 |
| Pluck | plucked string (Karplus-Strong) | Bright 0..1, Ring (how long it sounds) |
| Vector | X/Y pad mixing four sources: Osc 1, Osc 2, FM, Sub (square one octave down) | X, Y, plus the Osc and FM controls |

**Shared by all engines:**
- low-pass filter: cutoff 80..8000 Hz, resonance 0.5..15
- envelope: attack, decay, sustain, release (one envelope, for volume)
- one LFO: rate, depth, target = cutoff, pitch or volume
- Fold: wavefolder before the filter, 0..1
- Slide time: 10..400 ms
- Warmth (darker, slightly saturated) and Motion (more detune and LFO movement), 0..1
- level, octave shift

**Built-in instrument presets:** Analog Bass, Acid 303, Reese DnB, FM Bell, FM E-Piano, Super Pad, Pluck Lead.

There is no filter envelope, no second LFO, no noise oscillator, no unison, no portamento outside slide notes, no pitch bend, and only a low-pass filter.

## 4. Effects and mix

Per track:
- level, mute, drive, low shelf (180 Hz) and high shelf (4.2 kHz) in dB
- send to the shared delay (dotted eighth, synced to tempo, ping-pong) and to the shared reverb
- 3 effect slots, each one of: Overdrive, Bitcrush, Chorus, Phaser, Filter, Compressor. Each slot has two controls (amount, rate) and bypass

Master: volume, delay feedback, a limiter that is always on.
Delay time and reverb size cannot be changed. There is no sidechain.

## 5. Song automation

In each chain slot, each track has 3 lanes. A lane moves one control in a straight line from a start value to an end value while the slot plays, then the control returns to its stored value.

Controls that can be automated:
- drum track: level, drive, low EQ, high EQ, delay send, reverb send
- synth tracks: cutoff, resonance, FM index, warmth, motion, LFO depth, fold, vector X, vector Y, pluck bright, drive, delay send, reverb send, level

There are no curves and no automation inside a scene outside the song chain.

## 6. In and out

**Out:**
- MIDI file (480 ticks per quarter): one drum track on the GM notes above and one track per synth. Rolls are written as separate notes, slides as overlapping notes.
- WAV of the mix, and one WAV per track (stems).

**In:**
- a WAV as the break for the slicer.
- presets and grooves as JSON files, see section 8.
- there is no MIDI import yet.

**Saving:** the open project is kept automatically. Named projects can be saved, opened and deleted in the app. One built-in start project ("Warehouse 303"). The current instrument, drum kit or groove can be saved to the library under a name.

## 7. Styles this covers well, and where it falls short

| Fits well | Why |
|---|---|
| Acid, techno, hard techno | 303 preset with slide and accent, 909 / Warehouse / Gabber kits, drive, hardness |
| Jungle, drum and bass, breakcore | break slicer, rolls, Reese preset, Neuro and Glitch kits |
| Electro, EBM, synthwave | 808 / EBM kits, FM bass and bells, chorus, vector pads |
| House, lo-fi, boom-bap | 909 and Lo-Fi kits, swing, FM E-Piano, pluck |
| Ambient sketches | pads with long attack, reverb, vector automation |

Falls short for: anything needing real instrument samples (acoustic drums, piano, vocals), chords outside the scale or key changes, triplet feels (trap hat rolls only as 2/3/4 rolls per step), sidechain pumping, long evolving filter envelopes.

## 8. Presets and grooves: how to deliver them

Three kinds of content, each a plain JSON file:

1. **Instrument:** the sound of one synth track and its effects.
2. **Drum kit:** kit, hardness, pad settings, synthesized pads, drum track effects.
3. **Groove:** drum and note patterns for one scene (1 to 8 bars), with tempo, key and its sounds.

Files go into `content/library/` in the repository and ship inside the app, where they show up in the libraries on the Drums, Synth and Song screens. Loading one is a single undo step.

The exact format, every key and range, and the tool that checks and renders files are in **`docs/CONTENT_FORMAT.md`**.

## 9. Software gaps worth closing next

1. MIDI import for grooves.
2. Tags and search in the library once it holds many files.
3. Grooves that fill several scenes at once (verse, break, drop).
4. Loading own WAV one-shots onto pads (today only a whole break can be loaded).
5. Filter envelope for the synths. This is the biggest missing sound-design tool.
6. Adjustable delay time and reverb size.
7. Sidechain ducking from the kick.
8. Copying one track's pattern to another scene or track (today only a whole scene can be copied), and transposing a pattern.
