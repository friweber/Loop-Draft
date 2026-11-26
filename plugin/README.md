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
- **Outputs**: the main output carries the whole mix. Five more stereo outputs carry Drums, Bass, Lead, Syn 3 and
  Syn 4 on their own, after their channel strip and effects, without delay, reverb and the master limiter.
- **Automation**: Master, and per track Level, Drive, Delay Send and Reverb Send; per synth Cutoff and Resonance;
  the Scene.
- **The project is stored in the host's session.** OPEN and SAVE exchange `.ldp` project files with the phone apps.

## Build

    cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
    cmake --build plugin/build --config Release

Linux needs the usual JUCE packages (ALSA, X11, FreeType, Fontconfig development files).

JUCE is fetched while configuring. It is dual-licensed under the AGPLv3 and the JUCE licence.
