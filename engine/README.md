# Loop Draft engine

The native audio core, in portable C++17 with no Android dependencies. The Android app will wrap it with Oboe and JNI; on a desktop it builds and tests on its own.

## What is in it

- **Clock:** every step starts on its exact sample, swing included. No drift.
- **Tracks:** drums (8 pads, sampler with pitch, decay, pan, choke groups), bass synth, lead/chord synth.
- **Synth:** two band-limited oscillators, resonant low-pass filter, attack/decay/sustain/release, one LFO to cutoff, pitch or amp, chords built from the scale.
- **FX:** per track drive, low and high shelf EQ, delay send, reverb send. Tempo-synced ping-pong delay, algorithmic reverb, brickwall limiter on the master.
- **Patterns:** 4 scenes, each 1 to 8 bars. Scene changes land on the bar line. Song mode plays the chain.
- **Live play:** pads and notes can be triggered at any time; while recording they are snapped to the grid and reported back.
- **Export:** offline render of a scene or the whole song, one stem per track, and a format-1 MIDI file with General MIDI drum notes.

## Threading

The UI thread owns the `Project` (plain data). Every edit sends a snapshot with `Engine::setProject()`. The audio thread swaps a pointer at the next block and hands the old one back; `collectGarbage()` frees it on the UI thread. Commands travel through a lock-free queue. `render()` never allocates, locks or touches files.

## Build and test

```sh
cmake -S . -B build -G Ninja && cmake --build build
./build/ld_tests                  # 17 tests
./build/ld_render some/folder     # demo song: master WAV, stems, MIDI
```

Sanitizer builds: `-DLD_SANITIZE=ON` (address + undefined behaviour) and `-DLD_TSAN=ON` (threads).

## Not in yet

Project files (JSON), sample loading from the app's storage, the Android/Oboe wrapper and the UI.

`third_party/dr_wav.h` is dr_wav by David Reid (public domain or MIT-0), as chosen in the spec.
