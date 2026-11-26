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
- **Grooves**: eight of them, A to H, each 1 to 16 bars long. DRUMS and SYNTH are the bar editors: the numbers
  open a bar, `+ BAR`, `- BAR` and `x2` change the length (`x2` repeats what is there). While LOOP BAR is lit, only
  the bar that is open plays, so you can work on it; unlit, the whole groove plays.
- **Patterns**: the list beside both editors holds the library's grooves and the `.mid` files of a folder you choose
  (MIDI FOLDER). A click plays a pattern without changing anything; A to H under the list puts it into that groove.
  If pattern and groove differ in length, the shorter one is repeated. MIDI drum files are read by General MIDI
  drum notes; melodic files are moved to the nearest notes of the project's scale.
- **Pedals**: each track has six slots, the sound runs through them from left to right; the strip on top shows the
  whole board, a click on it shows that half. Twenty-one pedals with up to four knobs and a character switch each
  (table below). The phone apps play all of it and show the first three slots with two knobs.
- **Knob movements**: light REC KNOBS, let it play, and hold and move a knob or a bar: the movement is written into
  the groove that is sounding, one value per beat. Up to eight per groove. The SONG page shows them; you can draw
  them there and CLEAR them. Works for track level, drive, EQ and sends, the synth's sound controls and every pedal
  knob. While a groove moves a knob, that movement wins over the host's automation of the same control.
- **Song**: the SONG page is the timeline: blocks of grooves in a row, as wide as they are long. With PLAY THE SONG
  lit and FOLLOW HOST on, the song is tied to the host's bars from bar 1: wherever the host plays, that part of the
  song sounds. After the last block it is silent, or starts over (AFTER THE END).
- **PLAY** works on its own when no host leads, and also in a host as long as the host stands still.
- **Outputs**: the main output carries the whole mix. Five more stereo outputs carry Drums, Bass, Lead, Syn 3 and
  Syn 4 on their own, after their channel strip and effects, without delay, reverb and the master limiter.
- **Automation**: Master, and per track Level, Drive, Delay Send and Reverb Send; per synth Cutoff and Resonance;
  the groove (A to H).
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
