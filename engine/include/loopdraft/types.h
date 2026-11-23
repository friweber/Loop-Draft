// Loop Draft engine - project data model.
// Everything here is plain data (trivially copyable), so a whole Project can be
// handed to the audio thread as one snapshot without locks.
#pragma once
#include <cstdint>

namespace ld {

constexpr int kNumScenes   = 4;
constexpr int kNumPads     = 8;
constexpr int kStepsPerBar = 16;
constexpr int kMaxBars     = 8;
constexpr int kMaxSteps    = kStepsPerBar * kMaxBars;   // 128
constexpr int kNumSynths   = 2;                         // 0 = bass, 1 = lead/chords
constexpr int kNumTracks   = 1 + kNumSynths;            // track 0 = drums
constexpr int kMaxNotes    = 256;                       // per synth pattern
constexpr int kMaxChain    = 64;
constexpr int kDrumVoices  = 16;
constexpr int kSynthVoices = 8;

enum class Scale     : uint8_t { Minor, Major, Dorian, Pentatonic };
enum class Wave      : uint8_t { Saw, Square, Triangle, Sine };
enum class LfoTarget : uint8_t { Off, Cutoff, Pitch, Amp };
enum class SynthMode : uint8_t { Subtractive, Fm };
enum class AutoParam : uint8_t { Off, BassCutoff, LeadCutoff, DrumsDrive, LeadDelay };

// Step and note velocity: 0 = off, 1 = normal, 2 = accent.
struct PadParams {
  float vol = 0.9f;
  float pan = 0.0f;        // -1 left .. +1 right
  float pitch = 0.0f;      // semitones
  float decay = 0.35f;     // seconds to fade to silence
  uint8_t choke = 0;       // 0 = none, pads in the same group cut each other
  uint8_t gmNote = 36;     // General MIDI drum note for export
  float sliceStart = 0.0f; // 0 .. 1 (Jungle breakbeat slicer start)
  float sliceEnd = 1.0f;   // 0 .. 1 (Jungle breakbeat slicer end)
  bool reverse = false;    // play sample/slice backwards
};

struct SynthParams {
  Wave wave1 = Wave::Saw;
  Wave wave2 = Wave::Triangle;
  float detune = 7.0f;     // osc 2, cents
  float cutoff = 1800.0f;  // Hz
  float res = 3.2f;        // filter Q, 0.5 .. 15
  float attack = 0.008f;   // seconds
  float decay = 0.35f;     // seconds
  float sustain = 0.0f;    // 0 .. 1
  float release = 0.08f;   // seconds
  float level = 0.8f;
  float lfoRate = 3.0f;    // Hz
  float lfoDepth = 0.45f;  // 0 .. 1
  LfoTarget lfoTarget = LfoTarget::Cutoff;
  int8_t octaveShift = 0;  // relative to Project::octave
  bool chord = false;      // play each note as a triad built from the scale
  SynthMode mode = SynthMode::Subtractive;
  float fmRatio = 2.0f;    // modulator/carrier ratio (0.5 .. 8.0)
  float fmIndex = 2.5f;    // modulation depth (0 .. 8)
  float fmModDecay = 0.25f;// modulator envelope decay (seconds)
  float fmFeedback = 0.15f;// modulator self-feedback (0 .. 1)
};

struct TrackFx {
  float level = 0.8f;
  float drive = 0.1f;      // 0 .. 1
  float eqLow = 0.0f;      // dB at 180 Hz shelf
  float eqHigh = 0.0f;     // dB at 4.2 kHz shelf
  float delaySend = 0.0f;
  float revSend = 0.0f;
  bool mute = false;
};

struct Note {
  uint16_t step = 0;       // start, in 16th notes from the start of the scene
  int8_t degree = 0;       // scale degree, 0 = root; may go below 0 or above one octave
  uint8_t len = 1;         // length in 16th notes
  uint8_t vel = 1;
};

struct DrumPattern  { uint8_t steps[kNumPads][kMaxSteps] = {}; };
struct SynthPattern { Note notes[kMaxNotes] = {}; int32_t count = 0; };

struct AutoLane {
  AutoParam param = AutoParam::Off;
  float startVal = 0.0f;
  float endVal = 0.0f;
};

struct ChainAuto {
  AutoLane lanes[2];
};

struct Scene {
  int32_t bars = 1;        // 1 .. kMaxBars, shared by all tracks of the scene
  DrumPattern drums;
  SynthPattern synth[kNumSynths];
};

struct Project {
  float bpm = 122.0f;
  float swing = 0.0f;      // 0 = straight .. 0.5 = hard shuffle (delay of every 2nd 16th, in steps)
  int32_t root = 2;        // 0 = C .. 11 = B
  Scale scale = Scale::Minor;
  int32_t octave = 3;
  PadParams pads[kNumPads];
  SynthParams synths[kNumSynths];
  TrackFx fx[kNumTracks];
  float delayFeedback = 0.38f;
  float master = 0.85f;
  Scene scenes[kNumScenes];
  int32_t chain[kMaxChain] = {0, 1, 2, 3};
  ChainAuto chainAuto[kMaxChain];
  int32_t chainLen = 4;
  uint8_t kit = 0;         // which built-in drum kit the pads play: 0 = standard, 1 = jungle break slices
};

// Fills in the factory groove (the same one as prototype v0.2, plus a bass line).
Project makeDemoProject();

// Helpers for editing patterns from the UI thread.
bool addNote(SynthPattern& p, int step, int degree, int len = 1, int vel = 1);
bool removeNote(SynthPattern& p, int step, int degree);
inline int sceneSteps(const Scene& s) {
  int b = s.bars < 1 ? 1 : (s.bars > kMaxBars ? kMaxBars : s.bars);
  return b * kStepsPerBar;
}
// Drum step values: 0 off, 1 normal, 2 accent, 3/4/5 = roll of 2/3/4 hits inside the step (always accented).
inline float velGain(int v) { return v >= 2 ? 1.0f : 0.72f; }
inline int   velMidi(int v) { return v >= 2 ? 127 : 96; }
inline int   rollHits(int v) { return v >= 3 && v <= 5 ? v - 1 : 1; }
inline float swingOffset(float swing, int step) { return (step & 1) ? swing : 0.0f; }

// Scales and chords.
int scaleSize(Scale s);
int degreeToMidi(const Project& p, int octaveShift, int degree);
// Writes 1 note, or 3 when chord is on. Returns the count.
int chordMidis(const Project& p, const SynthParams& sp, int degree, int out[3]);

}  // namespace ld
