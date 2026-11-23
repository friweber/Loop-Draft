// Loop Draft engine - project data model.
// Everything here is plain data (trivially copyable), so a whole Project can be
// handed to the audio thread as one snapshot without locks.
#pragma once
#include <cstdint>

namespace ld {

constexpr int kNumScenes    = 4;
constexpr int kNumPads      = 8;
constexpr int kStepsPerBar  = 16;
constexpr int kMaxBars      = 8;
constexpr int kMaxSteps     = kStepsPerBar * kMaxBars;  // 128
constexpr int kNumSynths    = 4;                        // 0=Bass, 1=Lead, 2=Synth 3, 3=Synth 4
constexpr int kNumTracks    = 1 + kNumSynths;           // track 0 = Drums, 1..4 = Synths
constexpr int kMaxNotes     = 256;                      // per synth pattern
constexpr int kMaxChain     = 64;
constexpr int kFxSlots      = 3;                        // modular per-track FX chain slots
constexpr int kAutoPerTrack = 3;                        // up to 3 free-choice automations per track per chain slot
constexpr int kDrumVoices   = 16;
constexpr int kSynthVoices  = 8;

enum class Scale     : uint8_t { Minor, Major, Dorian, Pentatonic };
enum class Wave      : uint8_t { Saw, Square, Triangle, Sine };
enum class LfoTarget : uint8_t { Off, Cutoff, Pitch, Amp };
enum class SynthMode : uint8_t { Subtractive, Fm };
enum class FxType     : uint8_t { Off, Overdrive, Bitcrush, Chorus, Phaser, Filter, Compressor };
enum class DrumEngine : uint8_t { Classic808, Punch909, LoFiAcoustic, FmIndustrial, JungleSlicer };
enum class InstrumentPreset : uint8_t {
  Custom, AnalogBass, Acid303, ReeseDnB, FmBell, FmEPiano, SuperPad, PluckLead
};

// Step and note velocity: 0 = off, 1 = normal, 2 = accent, 3..5 = 2x/3x/4x ratchet roll.
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
  float detune = 7.0f;       // osc 2, cents
  float cutoff = 1800.0f;    // Hz
  float res = 3.2f;          // filter Q, 0.5 .. 15
  float attack = 0.008f;     // seconds
  float decay = 0.35f;       // seconds
  float sustain = 0.0f;      // 0 .. 1
  float release = 0.08f;     // seconds
  float level = 0.8f;
  float lfoRate = 3.0f;      // Hz
  float lfoDepth = 0.45f;    // 0 .. 1
  LfoTarget lfoTarget = LfoTarget::Cutoff;
  int8_t octaveShift = 0;    // relative to Project::octave
  bool chord = false;        // play each note as a triad built from the scale
  SynthMode mode = SynthMode::Subtractive;
  float fmRatio = 2.0f;      // modulator/carrier ratio (0.5 .. 8.0)
  float fmIndex = 2.5f;      // modulation depth (0 .. 8)
  float fmModDecay = 0.25f;  // modulator envelope decay (seconds)
  float fmFeedback = 0.15f;  // modulator self-feedback (0 .. 1)
  InstrumentPreset instrument = InstrumentPreset::Custom;
  float feelWarmth = 0.5f;   // 0 (bright/crisp) .. 1 (warm/saturated)
  float feelMotion = 0.3f;   // 0 (static) .. 1 (animated detune/LFO)
};

struct FxSlot {
  FxType type = FxType::Off;
  float amount = 0.5f;       // 0 .. 1 (drive/depth/mix/cutoff)
  float rate = 0.4f;         // 0 .. 1 (LFO rate / crush rate / resonance)
  bool bypass = false;
};

struct TrackFx {
  float level = 0.8f;
  float drive = 0.1f;        // 0 .. 1 (built-in preamp drive)
  float eqLow = 0.0f;        // dB at 180 Hz shelf
  float eqHigh = 0.0f;       // dB at 4.2 kHz shelf
  float delaySend = 0.0f;
  float revSend = 0.0f;
  bool mute = false;
  FxSlot slots[kFxSlots];    // user-configurable Vital-style FX chain
};

struct Note {
  uint16_t step = 0;         // start, in 16th notes from the start of the scene
  int8_t degree = 0;         // scale degree, 0 = root; may go below 0 or above one octave
  uint8_t len = 1;           // length in 16th notes
  uint8_t vel = 1;
};

struct DrumPattern  { uint8_t steps[kNumPads][kMaxSteps] = {}; };
struct SynthPattern { Note notes[kMaxNotes] = {}; int32_t count = 0; };

struct AutoLane {
  int16_t paramId = -1;      // -1 = off, otherwise any Param id (e.g. P_SYN_CUTOFF, P_FX_DRIVE, P_SYN_FM_INDEX)
  float startVal = 0.0f;
  float endVal = 0.0f;
};

struct ChainAuto {
  AutoLane tracks[kNumTracks][kAutoPerTrack];
};

struct Scene {
  int32_t bars = 1;          // 1 .. kMaxBars, shared by all tracks of the scene
  DrumPattern drums;
  SynthPattern synth[kNumSynths];
};

struct Project {
  float bpm = 122.0f;
  float swing = 0.0f;        // 0 = straight .. 0.5 = hard shuffle
  int32_t root = 2;          // 0 = C .. 11 = B
  Scale scale = Scale::Minor;
  int32_t octave = 3;
  int32_t activeSynths = 2;  // 2 by default (Bass + Lead); user can expand up to 4
  DrumEngine drumEngine = DrumEngine::Classic808;
  float junglePitch = 0.0f;  // global repitch (-12 .. +12 st) for Jungle Slicer
  float jungleTight = 0.65f; // global slice decay (0.08 .. 1.5 s) for Jungle Slicer
  float jungleCrunch = 0.25f;// vintage Akai/E-mu crunch (0 .. 1) for Jungle Slicer
  PadParams pads[kNumPads];
  SynthParams synths[kNumSynths];
  TrackFx fx[kNumTracks];
  float delayFeedback = 0.38f;
  float master = 0.85f;
  Scene scenes[kNumScenes];
  int32_t chain[kMaxChain] = {0, 1, 2, 3};
  ChainAuto chainAuto[kMaxChain];
  int32_t chainLen = 4;
};

Project makeDemoProject();

bool addNote(SynthPattern& p, int step, int degree, int len = 1, int vel = 1);
bool removeNote(SynthPattern& p, int step, int degree);
inline int sceneSteps(const Scene& s) {
  int b = s.bars < 1 ? 1 : (s.bars > kMaxBars ? kMaxBars : s.bars);
  return b * kStepsPerBar;
}
inline float velGain(int v) { return v == 2 ? 1.0f : 0.72f; }
inline int   velMidi(int v) { return v == 2 ? 127 : 96; }
inline float swingOffset(float swing, int step) { return (step & 1) ? swing : 0.0f; }

int scaleSize(Scale s);
int degreeToMidi(const Project& p, int octaveShift, int degree);
int chordMidis(const Project& p, const SynthParams& sp, int degree, int out[3]);

}  // namespace ld
