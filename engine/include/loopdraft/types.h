// Loop Draft engine - project data model.
// Everything here is plain data (trivially copyable), so a whole Project can be
// handed to the audio thread as one snapshot without locks.
#pragma once
#include <cstdint>

namespace ld {

constexpr int kNumScenes    = 8;                        // grooves A .. H (4 until Loop Master 0.3)
constexpr int kNumPads      = 8;
constexpr int kStepsPerBar  = 16;
constexpr int kMaxBars      = 16;                       // (8 until then)
constexpr int kMaxSteps     = kStepsPerBar * kMaxBars;  // 256
constexpr int kNumSynths    = 4;                        // 0=Bass, 1=Lead, 2=Synth 3, 3=Synth 4
constexpr int kNumTracks    = 1 + kNumSynths;           // track 0 = Drums, 1..4 = Synths
constexpr int kMaxNotes     = 256;                      // per synth pattern
constexpr int kMaxChain     = 64;
constexpr int kFxSlots      = 6;                        // modular per-track FX chain slots
constexpr int kAutoPerTrack = 3;                        // up to 3 free-choice automations per track per chain slot
constexpr int kDrumVoices   = 16;
constexpr int kSynthVoices  = 8;

enum class Scale     : uint8_t { Minor, Major, Dorian, Pentatonic };
enum class Wave      : uint8_t { Saw, Square, Triangle, Sine };
enum class LfoTarget : uint8_t { Off, Cutoff, Pitch, Amp };
// The synths of the collection. Each is a different instrument that reuses the same set of stored controls:
//   Subtractive  two oscillators into a filter
//   Fm           two sine operators
//   Pluck        a plucked string
//   Vector       four sources, blended by X and Y
//   Unison       seven detuned saws and a sub (detune = spread, vecX = how much of the stack, vecY = sub)
//   Organ        drawbar harmonics (vecX = dark .. bright, vecY = hollow, fmIndex = percussion, fmModDecay = its length)
//   Phase        phase distortion (osc1 = shape, fmIndex = how far it is bent, fmModDecay = how fast that fades,
//                fmFeedback = how much of it stays, fmRatio = the resonant shape's sweep)
enum class SynthMode : uint8_t { Subtractive, Fm, Pluck, Vector, Unison, Organ, Phase };
constexpr int kNumSynthModes = 7;
enum class FxType     : uint8_t {
  Off, Overdrive, Bitcrush, Chorus, Phaser, Filter, Compressor,          // the standard pedals
  // the stompbox collection (pedals.h)
  Distortion, Fuzz, Booster, Tremolo, Vibrato, Flanger, Rotary, Wah, AutoWah, Octaver, RingMod, Delay, TapeEcho, Reverb, Gate,
  // made for drums and synths
  Punch, Crusher, Slicer, GatedVerb, Boom, Cloud, LoFi, DubDelay, FreqShift, Resonator, Shimmer,
  Amp, Cab, Eq, Amp2                     // what a guitar plugged straight in needs: an amplifier, its speaker, a tone control
};
constexpr int kNumFxTypes = int(FxType::Amp2) + 1;
enum class DrumEngine : uint8_t {
  Classic808, Punch909, LoFiAcoustic, FmIndustrial, JungleSlicer,
  HardcoreGabber, WarehouseRumble, NeuroTechstep, EbmElectro, GlitchBreakcore
};
enum class InstrumentPreset : uint8_t {
  Custom, AnalogBass, Acid303, ReeseDnB, FmBell, FmEPiano, SuperPad, PluckLead
};

// Parameter ids for getParam / setParam. `index` is the pad, synth or track number where it applies.
enum Param : int {
  P_BPM = 0, P_SWING, P_MASTER, P_DELAY_FB, P_ROOT, P_SCALE, P_OCTAVE,
  P_ACTIVE_SYNTHS = 8,   // read-only: use setActiveSynths()
  P_DRUM_ENGINE = 53,    // read-only: use setDrumEngine()
  P_JUNGLE_PITCH = 54, P_JUNGLE_TIGHT = 55, P_JUNGLE_CRUNCH = 56,
  P_PAD_VOL = 10, P_PAD_PAN, P_PAD_PITCH, P_PAD_DECAY, P_PAD_CHOKE, P_PAD_SLICE_START, P_PAD_SLICE_END, P_PAD_REVERSE,
  P_SYN_WAVE1 = 20, P_SYN_WAVE2, P_SYN_DETUNE, P_SYN_CUTOFF, P_SYN_RES, P_SYN_ATTACK, P_SYN_DECAY, P_SYN_SUSTAIN,
  P_SYN_RELEASE, P_SYN_LEVEL, P_SYN_LFO_RATE, P_SYN_LFO_DEPTH, P_SYN_LFO_TARGET, P_SYN_OCTAVE, P_SYN_CHORD,
  P_SYN_MODE, P_SYN_FM_RATIO, P_SYN_FM_INDEX, P_SYN_FM_DECAY, P_SYN_FM_FB,
  P_SYN_INSTRUMENT = 50, P_SYN_WARMTH = 51, P_SYN_MOTION = 52,
  P_SYN_FOLD = 57, P_SYN_SLIDE_TIME = 58, P_SYN_PLUCK_BRIGHT = 59, P_SYN_VEC_X = 60, P_SYN_VEC_Y = 61,
  P_KIT_HARDNESS = 62,   // also sets the drum track's drive
  // the drum synth of one pad (index = pad). Changing one makes that pad a synthesized drum.
  P_DSYN_FREQ = 70, P_DSYN_DROP, P_DSYN_PDEC, P_DSYN_TONE, P_DSYN_NOISE, P_DSYN_NFREQ,
  P_DSYN_FM, P_DSYN_FMRATIO, P_DSYN_DRIVE, P_DSYN_CLICK, P_DSYN_LEN,
  // the two controls of each effect slot of a track (index = track)
  P_FX_SLOT1_AMOUNT = 81, P_FX_SLOT1_RATE, P_FX_SLOT2_AMOUNT, P_FX_SLOT2_RATE, P_FX_SLOT3_AMOUNT, P_FX_SLOT3_RATE,
  // the third and fourth knob of each slot
  P_FX_SLOT1_P3 = 87, P_FX_SLOT1_P4, P_FX_SLOT2_P3, P_FX_SLOT2_P4, P_FX_SLOT3_P3, P_FX_SLOT3_P4,
  // slots 4 .. 6: four ids each, in the order amount, rate, p3, p4
  P_FX_SLOT4_AMOUNT = 93, P_FX_SLOT4_RATE, P_FX_SLOT4_P3, P_FX_SLOT4_P4,
  P_FX_SLOT5_AMOUNT = 97, P_FX_SLOT5_RATE, P_FX_SLOT5_P3, P_FX_SLOT5_P4,
  P_FX_SLOT6_AMOUNT = 101, P_FX_SLOT6_RATE, P_FX_SLOT6_P3, P_FX_SLOT6_P4,
  P_FX_SLOT1_MIX = 105, P_FX_SLOT2_MIX, P_FX_SLOT3_MIX, P_FX_SLOT4_MIX, P_FX_SLOT5_MIX, P_FX_SLOT6_MIX,   // dry/wet of each slot
  P_LAST = P_FX_SLOT6_MIX,
  P_FX_LEVEL = 40, P_FX_DRIVE, P_FX_EQ_LOW, P_FX_EQ_HIGH, P_FX_DELAY, P_FX_REVERB, P_FX_MUTE
};

// Step and note velocity: 0 = off, 1 = normal, 2 = accent, 3..5 = 2x/3x/4x ratchet roll.
struct DrumSynthParams {
  float baseFreq   = 52.0f;   // 25 .. 1200 Hz
  float pitchDrop  = 140.0f;  // 0 .. 600 Hz
  float pitchDecay = 0.035f;  // 0.005 .. 0.30 s
  float toneMix    = 0.85f;   // 0 .. 1
  float noiseMix   = 0.15f;   // 0 .. 1
  float noiseFreq  = 2800.0f; // 300 .. 10000 Hz
  float fmAmt      = 0.0f;    // 0 .. 8
  float fmRatio    = 2.3f;    // 0.5 .. 8.0
  float drive      = 0.25f;   // 0 .. 1 (multi-stage soft/hard clip)
  float click      = 0.25f;   // 0 .. 1 (transient click)
  float length     = 0.45f;   // 0.05 .. 1.5 s
  bool  enabled    = false;   // true when pad uses custom synthesized drum
};

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
  float fold = 0.0f;         // wavefolder before the filter, 0 (off) .. 1
  float slideTime = 0.06f;   // seconds a slide note takes to reach its pitch
  float pluckBright = 0.6f;  // Pluck: 0 (soft, felt) .. 1 (bright, metallic)
  float vecX = 0.5f;         // Vector: position between the four sources. x: Osc 1 / FM  ->  Osc 2 / Sub
  float vecY = 0.5f;         //         y: Osc 1 / Osc 2 (0)  ->  FM / Sub (1)
};

struct FxSlot {
  FxType type = FxType::Off;
  float amount = 0.5f;       // 0 .. 1 (drive/depth/mix/cutoff)
  float rate = 0.4f;         // 0 .. 1 (LFO rate / crush rate / resonance)
  bool bypass = false;
  uint8_t voice = 0;         // the pedal's character switch, see pedals.h. 0 on a standard pedal is the sound it always had
  float p3 = 0.5f;           // 0 .. 1, third and fourth knob. What they do depends on the pedal; 0.5 is the neutral setting
  float p4 = 0.5f;
  float mix = 1.0f;          // 0 = only the dry signal .. 1 = only the pedal
  uint8_t flags = 0;         // kFxKeepSub, kFxSync
};
constexpr uint8_t kFxKeepSub = 1;   // everything below about 120 Hz goes round the pedal, clean and in the middle
constexpr uint8_t kFxSync = 2;      // the pedal's speed or time follows the tempo (see fxCanSync)
constexpr int kMaxFxVoices = 4;
// How many characters each pedal has, in the order of FxType.
constexpr uint8_t kFxVoices[] = {1, 4, 1, 4, 4, 3, 4, 4, 4, 3, 4, 2, 3, 2, 3, 3, 2, 2, 3, 3, 4, 2,
                                 2, 3, 2, 2, 2, 4, 3, 3, 3, 3, 3, 4, 4, 3, 4};
static_assert(int(sizeof(kFxVoices)) == kNumFxTypes, "one voice count per pedal");

// Which slot and which of its knobs (0 amount, 1 rate, 2 p3, 3 p4) a parameter id stands for. False if it is not a slot knob.
inline bool fxSlotParam(int id, int& slot, int& knob);
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
  uint8_t vel = 1;           // 1 = normal, 2 = accent (louder, and the filter opens for a moment)
  uint8_t flags = 0;         // kNoteSlide: glide into this note from the one before, without a new attack
};
constexpr uint8_t kNoteSlide = 1;
static_assert(sizeof(Note) == 6, "Note is stored in project files as it is");

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

// A knob that moves while a groove plays: one value per beat, straight lines in between. The values are in the
// parameter's own unit, as getParam() gives them. The last point is the end of the last bar.
constexpr int kSceneLanes = 8;
constexpr int kLanePoints = kMaxBars * 4 + 1;
struct SceneLane {
  int16_t paramId = -1;      // -1 = not in use
  int16_t index = 0;         // track (P_FX_*), synth (P_SYN_*), or 0
  float points[kLanePoints] = {};
};
// Three slow waves that each move one knob of your choice, all the time, on top of whatever the knob is set to.
constexpr int kNumLfos = 3;
constexpr int kNumLfoShapes = 5;       // sine, triangle, ramp, square, random steps
struct ModLfo {
  int16_t paramId = -1;      // -1 = not connected
  int16_t index = 0;         // track (P_FX_*), synth (P_SYN_*)
  uint8_t shape = 0;
  uint8_t sync = 1;          // 1: the rate is a length in beats; 0: free, in Hz
  uint8_t rsv[2] = {};
  float rate = 0.4f;         // 0 .. 1 (see lfoBeats / lfoHz)
  float depth = 0.5f;        // 0 .. 1: at 1 the knob swings over its whole range
};
// The lengths a synced LFO steps through, in beats: 8 bars down to a sixteenth.
inline float lfoBeats(float rate) {
  static const float k[9] = {32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 1.0f / 3.0f};
  int i = int(rate * 8.0f + 0.5f); if (i < 0) i = 0; if (i > 7) i = 7;
  return k[i];
}
inline float lfoHz(float rate) { float r = rate < 0 ? 0 : (rate > 1 ? 1 : rate); float h = 0.05f; for (int i = 0; i < 8; ++i) h *= 1.0f + r * 1.1146f; return h; }   // 0.05 .. 20 Hz
// What an LFO can move (the same list as in Engine::renderAudio).
inline bool lfoCanMove(int id) {
  int slot, knob;
  if (fxSlotParam(id, slot, knob)) return true;
  switch (id) {
    case P_FX_LEVEL: case P_KIT_HARDNESS: case P_FX_DRIVE: case P_FX_EQ_LOW: case P_FX_EQ_HIGH: case P_FX_DELAY: case P_FX_REVERB:
    case P_SYN_DETUNE: case P_SYN_CUTOFF: case P_SYN_RES: case P_SYN_DECAY: case P_SYN_LEVEL: case P_SYN_FM_RATIO: case P_SYN_FM_INDEX:
    case P_SYN_FM_FB: case P_SYN_WARMTH: case P_SYN_MOTION: case P_SYN_FOLD: return true;
    default: return false;
  }
}
struct Scene {
  int32_t bars = 1;          // 1 .. kMaxBars, shared by all tracks of the scene
  DrumPattern drums;
  SynthPattern synth[kNumSynths];
  SceneLane lanes[kSceneLanes];
};

struct Project {
  float bpm = 122.0f;
  float swing = 0.0f;        // 0 = straight .. 0.5 = hard shuffle
  int32_t root = 2;          // 0 = C .. 11 = B
  Scale scale = Scale::Minor;
  int32_t octave = 3;
  int32_t activeSynths = 2;  // 2 by default (Bass + Lead); user can expand up to 4
  DrumEngine drumEngine = DrumEngine::Classic808;   // which sounds the 8 pads play
  float junglePitch = 0.0f;  // global repitch (-12 .. +12 st) for Jungle Slicer
  float jungleTight = 0.65f; // global slice decay (0.08 .. 1.5 s) for Jungle Slicer
  float jungleCrunch = 0.25f;// vintage Akai/E-mu crunch (0 .. 1) for Jungle Slicer
  float kitHardness = 0.0f;  // global drum kit hardness/drive macro (0 .. 1)
  DrumSynthParams customDrums[kNumPads];
  PadParams pads[kNumPads];
  SynthParams synths[kNumSynths];
  TrackFx fx[kNumTracks];
  float delayFeedback = 0.38f;
  float master = 0.85f;
  Scene scenes[kNumScenes];
  int32_t chain[kMaxChain] = {0, 1, 2, 3};
  ChainAuto chainAuto[kMaxChain];
  int32_t chainLen = 4;
  uint32_t breakId = 0;    // Jungle Slicer: 0 = built-in break, otherwise the user's own break (a WAV or a bounced loop)
  // The break slicer and the hit kits each keep their own pad settings and drum patterns, so going
  // to another kit and back loses nothing. alt* holds the set that is not in use right now.
  uint8_t altValid = 0;
  float altDrumDrive = 0.0f;
  PadParams altPads[kNumPads];
  DrumPattern altDrums[kNumScenes];
  ModLfo lfos[kNumLfos];
};

// Building blocks that work on plain project data (no undo, no audio): shared by the session and the content importer.
void setDrumEngine(Project& p, int engine);            // 0..9; entering or leaving the break slicer swaps pads and patterns
void switchDrumMode(Project& p, bool toJungle);
void setInstrumentPreset(SynthParams& sp, int preset); // one of the built-in instruments, see InstrumentPreset
Project makeDemoProject();
// The "Warehouse 303" preset built into the app: what a fresh install opens with.
Project makeStartProject();

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

int scaleSize(Scale s);
int degreeToMidi(const Project& p, int octaveShift, int degree);
int chordMidis(const Project& p, const SynthParams& sp, int degree, int out[3]);

inline bool fxSlotParam(int id, int& slot, int& knob) {
  if (id >= P_FX_SLOT1_AMOUNT && id <= P_FX_SLOT3_RATE) { slot = (id - P_FX_SLOT1_AMOUNT) / 2; knob = (id - P_FX_SLOT1_AMOUNT) % 2; return true; }
  if (id >= P_FX_SLOT1_P3 && id <= P_FX_SLOT3_P4) { slot = (id - P_FX_SLOT1_P3) / 2; knob = 2 + (id - P_FX_SLOT1_P3) % 2; return true; }
  if (id >= P_FX_SLOT4_AMOUNT && id <= P_FX_SLOT6_P4) { slot = 3 + (id - P_FX_SLOT4_AMOUNT) / 4; knob = (id - P_FX_SLOT4_AMOUNT) % 4; return true; }
  if (id >= P_FX_SLOT1_MIX && id <= P_FX_SLOT6_MIX) { slot = id - P_FX_SLOT1_MIX; knob = 4; return true; }
  return false;
}
// The id of knob 0 .. 3 of slot 0 .. kFxSlots - 1.
inline int fxSlotParamId(int slot, int knob) {
  if (knob == 4) return P_FX_SLOT1_MIX + slot;
  if (slot >= 3) return P_FX_SLOT4_AMOUNT + (slot - 3) * 4 + knob;
  return knob < 2 ? P_FX_SLOT1_AMOUNT + slot * 2 + knob : P_FX_SLOT1_P3 + slot * 2 + (knob - 2);
}
// Is `index` of this parameter a synth (true) or a track / nothing (false)?
inline bool isSynthParam(int id) { return (id >= P_SYN_WAVE1 && id < P_FX_LEVEL) || id == P_SYN_INSTRUMENT || id == P_SYN_WARMTH || id == P_SYN_MOTION || (id >= P_SYN_FOLD && id <= P_SYN_VEC_Y); }
inline float& fxSlotKnob(FxSlot& s, int knob) { return knob == 0 ? s.amount : (knob == 1 ? s.rate : (knob == 2 ? s.p3 : (knob == 3 ? s.p4 : s.mix))); }
// Can this pedal's speed or time follow the tempo when asked to? (The slicer and the dub delay always do.)
inline bool fxCanSync(FxType t, int voice) {
  switch (t) {
    case FxType::Chorus: case FxType::Phaser: return voice > 0;
    case FxType::Flanger: case FxType::Tremolo: case FxType::Vibrato: case FxType::Wah: case FxType::Delay: case FxType::TapeEcho: return true;
    default: return false;
  }
}

}  // namespace ld
