# Loop Pedals

The pedal collection of Loop Master as its own effect: plug a guitar, a bass or a synth into your audio interface and
use this instead of a pedalboard. It comes as a plugin (VST3 everywhere, AU on macOS, LV2 on Linux) and as an app that
runs without a DAW.

## Getting sound

**The app:** start *Loop Pedals*, open `Options > Audio/MIDI Settings`, choose your interface as input and output, and
pick the smallest buffer that does not crackle (128 samples or less feels immediate). The app starts with the input
muted to protect you from feedback: switch that off in the same Options menu, and use headphones or keep the speakers
away from a microphone. Set **Input** to the socket the instrument is in (a guitar in the first socket: *Input 1*).

**In a DAW:** copy `Loop Pedals.vst3` into your VST3 folder, put it on an audio track, and switch on the track's input
monitoring.

Turn **In** up until the IN light turns yellow when you play hard, never red.

## The board

- Ten slots from the instrument (left) to the output (right); four are shown at a time. The strip of chips above the
  pedals is the whole chain: click one to bring it into view.
- Every slot has a box to choose its pedal ("Empty slot" removes it), `<` and `>` to move it along the chain, and its
  foot switch.
- Most pedals have up to four knobs, a row of characters (the same effect built another way), **Mix**, **KEEP SUB**
  (lows below about 120 Hz go round the pedal: for bass) and, where it makes sense, **SYNC** to the tempo.
- 35 pedals: everything from Loop Master, and three made for instruments plugged straight in: **Amp** (clean,
  crunch, lead, bass), **Cab** (4x12, 2x12 open, 1x10, bass 8x10) and **Tone Control** (three bands).
  A guitar straight into an interface sounds thin and harsh through a drive alone: put an Amp and a Cab on the board.
- **TUNER**: note, cents and a needle; the output is silent while it is on.

## Boards

The menu at the top holds ready-made boards for electric guitar, bass guitar and synth. **SAVE AS...** stores the
board as it stands under "My boards" (files in `Documents/Loop Pedals/Boards`, shared by plugin and app);
**DELETE** removes the loaded one of yours; **NEW** clears the floor.

## Movement

- **LFOs** (bottom): three slow waves. Click an LFO's button, then touch the knob (or Mix bar) it should move. Shape,
  SYNC, speed, depth. Saved with the board.
- **Your DAW**: every knob, Mix and foot switch is a plugin parameter ("Pedal 3 Knob 1" ...), so the DAW can record
  and draw their movement.

## Not there yet

No MIDI foot controller, no looper, no impulse responses for the Cab, no stereo routing options beyond the input
choice. The amp and cab are simple models, not captures of particular amplifiers.
