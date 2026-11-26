# Loop Master

The Loop Draft engine as an instrument plugin. A project sounds the same here as on the phone.

## Install

Copy the plugin into the folder your host scans, then let the host rescan.

| System  | File                    | Folder                                             |
|---------|-------------------------|----------------------------------------------------|
| Windows | `Loop Master.vst3`      | `C:\Program Files\Common Files\VST3`               |
| macOS   | `Loop Master.vst3`      | `~/Library/Audio/Plug-Ins/VST3`                    |
| macOS   | `Loop Master.component` | `~/Library/Audio/Plug-Ins/Components` (Logic, GarageBand) |
| Linux   | `Loop Master.vst3`      | `~/.vst3`                                          |
| Linux   | `Loop Master.lv2`       | `~/.lv2`                                           |

The builds are not signed. On macOS, remove the download mark once after copying:

    xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Loop Master.vst3"
    xattr -cr ~/Library/Audio/Plug-Ins/Components/"Loop Master.component"

`Loop Master` (the app next to the plugins) runs without a host.

## In the host

- **Tempo and transport** come from the host while FOLLOW HOST is lit. The pattern is locked to the host's bars.
- **MIDI**: channel 10 plays the drum pads (General MIDI notes, or C4 to G4 for pad 1 to 8), channels 1 to 4 play
  Bass, Lead, Syn 3 and Syn 4. "MIDI: ..." in the top right sends every channel to one track instead.
- **Pedals**: each track has three slots, the sound runs through them from left to right. Twenty-one pedals:
  overdrive, distortion, fuzz, booster, bitcrush, compressor, gate, chorus, flanger, phaser, tremolo, vibrato,
  rotary, ring mod, low-pass, wah, auto-wah, octaver, delay, tape echo and reverb. Each has up to four knobs and
  a character switch (table below). The phone apps play all of it and offer the six standard ones with two knobs.
- **Outputs**: the main output carries the whole mix. Five more stereo outputs carry Drums, Bass, Lead, Syn 3 and
  Syn 4 on their own, after their channel strip and effects, without delay, reverb and the master limiter.
- **Automation**: Master, and per track Level, Drive, Delay Send and Reverb Send; per synth Cutoff and Resonance;
  the Scene.
- **The project is stored in the host's session.** OPEN and SAVE exchange `.ldp` project files with the phone apps.

## The pedals

A character is the way that kind of pedal is built in the real world: where it clips, what it does to the mids, how
its sweep moves. They are voicings in that spirit, not copies of one circuit. "Classic" is the pedal as Loop Draft
has always had it, so old projects sound the same. In files: `voice` 0..3 in the order below, knobs `amount`, `rate`,
`p3`, `p4`.

| Pedal | Knobs | Characters |
|---|---|---|
| Overdrive | Drive, Tone, Level, Body | Classic, Green (mid hump), Gold (clean blend), Blue (flat, low gain) |
| Distortion | Gain, Tone, Level, Mids | Rodent, Orange, Stack, Metal |
| Fuzz | Fuzz, Tone, Level, Bias | Round, Wall, Bender, Octave |
| Booster | Boost, Treble, Level, Bass | Clean, Treble, Preamp |
| Bitcrush | Crush, Rate, Level | - |
| Compressor | Sustain, Attack, Level, Blend | Classic, Squash, Optical, Studio |
| Gate | Threshold, Decay, Range, Hold | Smooth, Hard |
| Chorus | Depth, Speed, Mix, Tone | Classic, Ensemble, Wide, Tri |
| Flanger | Feedback, Speed, Depth, Manual | Jet, Mistress, Zero |
| Phaser | Depth, Speed, Feedback, Mix | Classic, Script (Feedback past noon: the later one), Stone, Vibe |
| Tremolo | Depth, Speed, Shape, Pan | Bias, Optical, Harmonic, Chop |
| Vibrato | Depth, Speed, Mix, Tone | Pitch, Tape |
| Rotary | Depth, Speed, Balance, Drive | Cabinet, Simple |
| Ring Mod | Mix, Pitch, Wobble, Wobble speed | Sine, Square |
| Low-Pass | Cutoff, Resonance, Envelope, Drive | Classic, Ladder, Band |
| Wah | Depth, Speed, Range, Sharpness | Cry, Clyde, Bass |
| Auto-Wah | Sensitivity, Peak, Range, Decay | Up, Down, Low-pass |
| Octaver | Octave, Tone, Dry, Second | Sub, Up |
| Delay | Level, Time, Repeats, Tone (Analog: Wobble) | Digital, Analog, Ping-pong |
| Tape Echo | Level, Time, Repeats, Age | Single, Multi-head, Worn |
| Reverb | Level, Decay, Tone, Pre-delay (Spring: Dwell) | Spring, Plate, Hall, Cloud |

## Build

    cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
    cmake --build plugin/build --config Release

Linux needs the usual JUCE packages (ALSA, X11, FreeType, Fontconfig development files).

JUCE is fetched while configuring. It is dual-licensed under the AGPLv3 and the JUCE licence.
