# Loop Draft

A mobile groovebox for getting a loop going quickly and finishing it later: sketch on the phone, then export MIDI and audio and keep editing in a DAW. Working title in the older documents: **PocketMod**.

## Direction

Version 1 is a native Android app (Kotlin and Compose for the screen, C++ with Oboe for sound), so it responds as fast as the phone allows. The HTML prototype stays as the design reference.

1. Engine in portable C++, tested on a desktop. **Done:** see `engine/`.
2. Android app with Oboe output and the four screens (Drums, Synth, Mix, Song), autosave and undo. **Done:** see `android/`.
3. Still to do: loading your own WAV samples, export of MIDI and stems from the app, several projects, note lengths in the piano roll, playing on with the screen off.
4. Polish and tests on real devices.

## Install on your phone

Every push to `main` builds the app on GitHub, runs it on an emulator, and publishes the result:

**https://github.com/friweber/Loop-Draft/releases/latest** then tap `LoopDraft.apk`.

Android asks once to allow installs from your browser. New builds install over the old one and keep your project.
The line under the app title shows the audio path in use (API, sample rate, burst size, output latency as the phone reports it).

## Try the prototype

Open `prototype/index.html` in a browser (phone or desktop) and press **PLAY**. It is one self-contained file with no dependencies.

What v0.2 does:

- **Stays in time.** Notes are scheduled on the audio clock, with swing and accents.
- **Never loses work.** Autosave after every change, undo/redo for everything, and it reopens where you left off.
- **Edits while playing.** Step grids, a piano roll with chords, live recording from the pads (quantized to the grid), scene changes on the bar line.
- **Stays in key.** Root, scale and octave are selectable; chord mode builds chords from the scale.
- **Per-track FX.** Drums and synth each have drive, EQ, delay send and reverb send.
- **Exports what you hear.** WAV is rendered offline by the same engine as live playback. MIDI is a format-1 file with General MIDI drum notes, for one scene or the whole song chain.

Known limits of v0.2: loaded WAV samples are not kept after a reload, patterns are one bar of 16 steps, there are two tracks (drums and one synth), and there is no stem export yet.

## Layout

| Path | Contents |
| :-- | :-- |
| `engine/` | Native C++ audio engine with desktop tests. See `engine/README.md` |
| `android/` | The Android app: Kotlin/Compose UI, JNI bridge, Oboe audio. `android/ci/` holds the APK checks and the emulator test |
| `prototype/index.html` | Interactive prototype v0.2. The reference for look and behaviour |
| `tests/` | Unit tests for the core logic, and an end-to-end browser test |
| `docs/SPEC.md` | Master development spec (imported from the Google Doc) |
| `specs/` | Project file schema |
| `assets/kits/` | Drum kit manifests. v1.0 is the original; v1.1 uses General MIDI notes like the prototype |

The v0.1 prototype (`pocketmod_groovebox.html`), the task board and the decision log remain in the Google Drive folder "PocketMod - Multi-Agent Dev Hub".

## Tests

```sh
npm test                                        # core logic, Node only
NODE_PATH=$(npm root -g) npm run test:browser   # needs playwright and Chrome
```
