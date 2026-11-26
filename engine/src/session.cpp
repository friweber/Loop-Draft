#include "loopdraft/session.h"
#include "files.h"
#include "loopdraft/content.h"

#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace ld {

namespace {
constexpr size_t kMaxUndo = 64;

template <typename T> T clampT(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
float fin(float v, float fallback) { return v == v && v > -1e30f && v < 1e30f ? v : fallback; }
bool sceneOk(int s) { return s >= 0 && s < kNumScenes; }
}  // namespace

void sanitize(Project& p) {
  p.bpm = clampT(fin(p.bpm, 122), 40.0f, 240.0f);
  p.swing = clampT(fin(p.swing, 0), 0.0f, 0.5f);
  p.root = clampT(p.root, 0, 11);
  if (uint8_t(p.scale) > uint8_t(Scale::Pentatonic)) p.scale = Scale::Minor;
  p.octave = clampT(p.octave, 1, 5);
  p.activeSynths = clampT(p.activeSynths, 2, kNumSynths);
  if (uint8_t(p.drumEngine) > uint8_t(DrumEngine::GlitchBreakcore)) p.drumEngine = DrumEngine::Classic808;
  p.kitHardness = clampT(fin(p.kitHardness, 0.0f), 0.0f, 1.0f);
  for (auto& ds : p.customDrums) {
    ds.baseFreq   = clampT(fin(ds.baseFreq, 52.0f), 25.0f, 1200.0f);
    ds.pitchDrop  = clampT(fin(ds.pitchDrop, 140.0f), 0.0f, 600.0f);
    ds.pitchDecay = clampT(fin(ds.pitchDecay, 0.035f), 0.005f, 0.30f);
    ds.toneMix    = clampT(fin(ds.toneMix, 0.85f), 0.0f, 1.0f);
    ds.noiseMix   = clampT(fin(ds.noiseMix, 0.15f), 0.0f, 1.0f);
    ds.noiseFreq  = clampT(fin(ds.noiseFreq, 2800.0f), 300.0f, 10000.0f);
    ds.fmAmt      = clampT(fin(ds.fmAmt, 0.0f), 0.0f, 8.0f);
    ds.fmRatio    = clampT(fin(ds.fmRatio, 2.3f), 0.5f, 8.0f);
    ds.drive      = clampT(fin(ds.drive, 0.25f), 0.0f, 1.0f);
    ds.click      = clampT(fin(ds.click, 0.25f), 0.0f, 1.0f);
    ds.length     = clampT(fin(ds.length, 0.45f), 0.05f, 1.5f);
  }
  p.junglePitch = clampT(fin(p.junglePitch, 0.0f), -12.0f, 12.0f);
  p.jungleTight = clampT(fin(p.jungleTight, 0.65f), 0.08f, 1.5f);
  p.jungleCrunch = clampT(fin(p.jungleCrunch, 0.25f), 0.0f, 1.0f);
  p.delayFeedback = clampT(fin(p.delayFeedback, 0.38f), 0.0f, 0.85f);
  p.master = clampT(fin(p.master, 0.85f), 0.0f, 1.0f);
  const auto fixPad = [](PadParams& d) {
    d.vol = clampT(fin(d.vol, 0.9f), 0.0f, 1.0f); d.pan = clampT(fin(d.pan, 0), -1.0f, 1.0f);
    d.pitch = clampT(fin(d.pitch, 0), -24.0f, 24.0f); d.decay = clampT(fin(d.decay, 0.3f), 0.02f, 2.0f);
    d.choke = uint8_t(clampT<int>(d.choke, 0, 4)); d.gmNote = uint8_t(clampT<int>(d.gmNote, 0, 127));
    d.sliceStart = clampT(fin(d.sliceStart, 0.0f), 0.0f, 0.99f);
    d.sliceEnd = clampT(fin(d.sliceEnd, 1.0f), d.sliceStart + 0.01f > 1.0f ? 1.0f : d.sliceStart + 0.01f, 1.0f);
  };
  for (auto& d : p.pads) fixPad(d);
  for (auto& d : p.altPads) fixPad(d);
  p.altValid = p.altValid ? 1 : 0;
  p.altDrumDrive = clampT(fin(p.altDrumDrive, 0.0f), 0.0f, 1.0f);
  for (auto& s : p.synths) {
    if (uint8_t(s.wave1) > uint8_t(Wave::Sine)) s.wave1 = Wave::Saw;
    if (uint8_t(s.wave2) > uint8_t(Wave::Sine)) s.wave2 = Wave::Saw;
    if (uint8_t(s.lfoTarget) > uint8_t(LfoTarget::Amp)) s.lfoTarget = LfoTarget::Off;
    s.detune = clampT(fin(s.detune, 0), -50.0f, 50.0f); s.cutoff = clampT(fin(s.cutoff, 1800), 60.0f, 12000.0f);
    s.res = clampT(fin(s.res, 2), 0.5f, 15.0f); s.attack = clampT(fin(s.attack, 0.008f), 0.001f, 1.0f);
    s.decay = clampT(fin(s.decay, 0.35f), 0.02f, 3.0f); s.sustain = clampT(fin(s.sustain, 0), 0.0f, 1.0f);
    s.release = clampT(fin(s.release, 0.08f), 0.01f, 3.0f); s.level = clampT(fin(s.level, 0.8f), 0.0f, 1.0f);
    s.lfoRate = clampT(fin(s.lfoRate, 3), 0.05f, 20.0f); s.lfoDepth = clampT(fin(s.lfoDepth, 0), 0.0f, 1.0f);
    s.octaveShift = int8_t(clampT<int>(s.octaveShift, -2, 2));
    if (uint8_t(s.mode) > uint8_t(SynthMode::Vector)) s.mode = SynthMode::Subtractive;
    s.fold = clampT(fin(s.fold, 0.0f), 0.0f, 1.0f); s.slideTime = clampT(fin(s.slideTime, 0.06f), 0.005f, 0.5f);
    s.pluckBright = clampT(fin(s.pluckBright, 0.6f), 0.0f, 1.0f);
    s.vecX = clampT(fin(s.vecX, 0.5f), 0.0f, 1.0f); s.vecY = clampT(fin(s.vecY, 0.5f), 0.0f, 1.0f);
    s.fmRatio = clampT(fin(s.fmRatio, 2.0f), 0.5f, 8.0f);
    s.fmIndex = clampT(fin(s.fmIndex, 2.5f), 0.0f, 8.0f);
    s.fmModDecay = clampT(fin(s.fmModDecay, 0.25f), 0.01f, 3.0f);
    s.fmFeedback = clampT(fin(s.fmFeedback, 0.15f), 0.0f, 1.0f);
    if (uint8_t(s.instrument) > uint8_t(InstrumentPreset::PluckLead)) s.instrument = InstrumentPreset::Custom;
    s.feelWarmth = clampT(fin(s.feelWarmth, 0.5f), 0.0f, 1.0f);
    s.feelMotion = clampT(fin(s.feelMotion, 0.3f), 0.0f, 1.0f);
  }
  for (auto& f : p.fx) {
    f.level = clampT(fin(f.level, 0.8f), 0.0f, 1.0f); f.drive = clampT(fin(f.drive, 0), 0.0f, 1.0f);
    f.eqLow = clampT(fin(f.eqLow, 0), -12.0f, 12.0f); f.eqHigh = clampT(fin(f.eqHigh, 0), -12.0f, 12.0f);
    f.delaySend = clampT(fin(f.delaySend, 0), 0.0f, 1.0f); f.revSend = clampT(fin(f.revSend, 0), 0.0f, 1.0f);
    for (auto& sl : f.slots) {
      if (int(sl.type) >= kNumFxTypes) sl.type = FxType::Off;
      sl.amount = clampT(fin(sl.amount, 0.5f), 0.0f, 1.0f);
      sl.rate = clampT(fin(sl.rate, 0.4f), 0.0f, 1.0f);
    }
  }
  for (auto& sc : p.scenes) {
    sc.bars = clampT(sc.bars, 1, kMaxBars);
    for (auto& row : sc.drums.steps) for (auto& v : row) if (v > 5) v = 1;
    for (auto& pat : sc.synth) {
      pat.count = clampT(pat.count, 0, kMaxNotes);
      for (int i = 0; i < pat.count; ++i) {
        Note& n = pat.notes[i];
        if (n.step >= kMaxSteps) n.step = kMaxSteps - 1;
        n.degree = int8_t(clampT<int>(n.degree, -21, 35));
        n.len = uint8_t(clampT<int>(n.len, 1, kMaxSteps)); n.vel = uint8_t(n.vel == 2 ? 2 : 1);
      }
    }
  }
  p.chainLen = clampT(p.chainLen, 1, kMaxChain);
  for (int i = 0; i < kMaxChain; ++i) {
    p.chain[i] = clampT(p.chain[i], 0, kNumScenes - 1);
    for (int t = 0; t < kNumTracks; ++t) {
      for (int ln = 0; ln < kAutoPerTrack; ++ln) {
        auto& al = p.chainAuto[i].tracks[t][ln];
        if (al.paramId < -1 || al.paramId > P_FX_SLOT3_RATE) al.paramId = -1;   // the highest id that can be automated
        al.startVal = fin(al.startVal, 0.0f); al.endVal = fin(al.endVal, 0.0f);
      }
    }
  }
}

Session::Session(double sampleRate) : engine_(sampleRate), proj_(makeDemoProject()) {
  engine_.setProject(proj_);
}

void Session::commit() {
  sanitize(proj_);
  syncKit();
  engine_.setProject(proj_);
  dirty_ = true;
}

// What a pad's drum synth is set to, as one number. 0 means the pad plays the kit's own sound.
static uint32_t drumSynthKey(const Project& p, int pad) {
  const DrumSynthParams& d = p.customDrums[pad];
  if (!d.enabled || p.drumEngine == DrumEngine::JungleSlicer) return 0;
  const float v[11] = {d.baseFreq, d.pitchDrop, d.pitchDecay, d.toneMix, d.noiseMix, d.noiseFreq, d.fmAmt, d.fmRatio, d.drive, d.click, d.length};
  uint32_t h = 2166136261u;
  const unsigned char* b = reinterpret_cast<const unsigned char*>(v);
  for (size_t i = 0; i < sizeof(v); ++i) { h ^= b[i]; h *= 16777619u; }
  return h ? h : 1u;
}

// The drum sounds follow the project, so undo, redo, reset and loading all bring the right ones back.
void Session::syncKit() {
  const DrumEngine de = proj_.drumEngine;
  const uint32_t bid = de == DrumEngine::JungleSlicer && findBreak(proj_.breakId) ? proj_.breakId : 0;
  const bool kitChanged = appliedEngine_ != int(de) || appliedBreak_ != bid;
  const double sr = engine_.sampleRate();
  std::shared_ptr<const Sample> brk;
  if (kitChanged && de == DrumEngine::JungleSlicer) {
    brk = findBreak(bid);
    if (!brk) brk = makeJungleBreakSample(sr);
  }
  for (int i = 0; i < kNumPads; ++i) {
    const uint32_t key = drumSynthKey(proj_, i);
    if (!kitChanged && key == appliedPad_[i]) continue;
    if (key != 0) engine_.setPadSample(i, makeCustomDrumSample(proj_.customDrums[i], sr));
    else if (de == DrumEngine::JungleSlicer) {
      if (!brk) { brk = findBreak(bid); if (!brk) brk = makeJungleBreakSample(sr); }
      engine_.setPadSample(i, std::make_unique<Sample>(*brk));
    }
    else engine_.setPadSample(i, de == DrumEngine::Classic808 ? nullptr : makeDrumKitSample(de, i, sr));
    appliedPad_[i] = key;
  }
  appliedEngine_ = int(de);
  appliedBreak_ = bid;
}

void Session::beginEdit() {
  undo_.push_back(proj_);
  if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
  redo_.clear();
}

bool Session::undo() {
  if (undo_.empty()) return false;
  redo_.push_back(proj_); proj_ = undo_.back(); undo_.pop_back();
  commit();
  return true;
}

bool Session::redo() {
  if (redo_.empty()) return false;
  undo_.push_back(proj_); proj_ = redo_.back(); redo_.pop_back();
  commit();
  return true;
}

// One place that maps a parameter id to its field. Used for reading and writing.
static float* field(Project& p, int id, int index, bool& isInt, int& kind) {
  // kind: 0 float, 1 int32, 2 uint8/enum, 3 int8, 4 bool
  static float scratch;
  isInt = false; kind = 0;
  const auto pad = [&]() -> PadParams& { return p.pads[clampT(index, 0, kNumPads - 1)]; };
  const auto dsyn = [&]() -> DrumSynthParams& { return p.customDrums[clampT(index, 0, kNumPads - 1)]; };
  const auto syn = [&]() -> SynthParams& { return p.synths[clampT(index, 0, kNumSynths - 1)]; };
  const auto fx = [&]() -> TrackFx& { return p.fx[clampT(index, 0, kNumTracks - 1)]; };
  switch (id) {
    case P_BPM: return &p.bpm;
    case P_SWING: return &p.swing;
    case P_MASTER: return &p.master;
    case P_DELAY_FB: return &p.delayFeedback;
    case P_ROOT: kind = 1; return reinterpret_cast<float*>(&p.root);
    case P_SCALE: kind = 2; return reinterpret_cast<float*>(&p.scale);
    case P_OCTAVE: kind = 1; return reinterpret_cast<float*>(&p.octave);
    case P_ACTIVE_SYNTHS: kind = 1; return reinterpret_cast<float*>(&p.activeSynths);
    case P_DRUM_ENGINE: kind = 2; return reinterpret_cast<float*>(&p.drumEngine);
    case P_JUNGLE_PITCH: return &p.junglePitch;
    case P_JUNGLE_TIGHT: return &p.jungleTight;
    case P_JUNGLE_CRUNCH: return &p.jungleCrunch;
    case P_KIT_HARDNESS: return &p.kitHardness;
    case P_DSYN_FREQ: return &dsyn().baseFreq;
    case P_DSYN_DROP: return &dsyn().pitchDrop;
    case P_DSYN_PDEC: return &dsyn().pitchDecay;
    case P_DSYN_TONE: return &dsyn().toneMix;
    case P_DSYN_NOISE: return &dsyn().noiseMix;
    case P_DSYN_NFREQ: return &dsyn().noiseFreq;
    case P_DSYN_FM: return &dsyn().fmAmt;
    case P_DSYN_FMRATIO: return &dsyn().fmRatio;
    case P_DSYN_DRIVE: return &dsyn().drive;
    case P_DSYN_CLICK: return &dsyn().click;
    case P_DSYN_LEN: return &dsyn().length;
    case P_PAD_VOL: return &pad().vol;
    case P_PAD_PAN: return &pad().pan;
    case P_PAD_PITCH: return &pad().pitch;
    case P_PAD_DECAY: return &pad().decay;
    case P_PAD_CHOKE: kind = 2; return reinterpret_cast<float*>(&pad().choke);
    case P_PAD_SLICE_START: return &pad().sliceStart;
    case P_PAD_SLICE_END: return &pad().sliceEnd;
    case P_PAD_REVERSE: kind = 4; return reinterpret_cast<float*>(&pad().reverse);
    case P_SYN_WAVE1: kind = 2; return reinterpret_cast<float*>(&syn().wave1);
    case P_SYN_WAVE2: kind = 2; return reinterpret_cast<float*>(&syn().wave2);
    case P_SYN_DETUNE: return &syn().detune;
    case P_SYN_CUTOFF: return &syn().cutoff;
    case P_SYN_RES: return &syn().res;
    case P_SYN_ATTACK: return &syn().attack;
    case P_SYN_DECAY: return &syn().decay;
    case P_SYN_SUSTAIN: return &syn().sustain;
    case P_SYN_RELEASE: return &syn().release;
    case P_SYN_LEVEL: return &syn().level;
    case P_SYN_LFO_RATE: return &syn().lfoRate;
    case P_SYN_LFO_DEPTH: return &syn().lfoDepth;
    case P_SYN_LFO_TARGET: kind = 2; return reinterpret_cast<float*>(&syn().lfoTarget);
    case P_SYN_OCTAVE: kind = 3; return reinterpret_cast<float*>(&syn().octaveShift);
    case P_SYN_CHORD: kind = 4; return reinterpret_cast<float*>(&syn().chord);
    case P_SYN_MODE: kind = 2; return reinterpret_cast<float*>(&syn().mode);
    case P_SYN_FM_RATIO: return &syn().fmRatio;
    case P_SYN_FM_INDEX: return &syn().fmIndex;
    case P_SYN_FM_DECAY: return &syn().fmModDecay;
    case P_SYN_FM_FB: return &syn().fmFeedback;
    case P_SYN_INSTRUMENT: kind = 2; return reinterpret_cast<float*>(&syn().instrument);
    case P_SYN_WARMTH: return &syn().feelWarmth;
    case P_SYN_MOTION: return &syn().feelMotion;
    case P_SYN_FOLD: return &syn().fold;
    case P_SYN_SLIDE_TIME: return &syn().slideTime;
    case P_SYN_PLUCK_BRIGHT: return &syn().pluckBright;
    case P_SYN_VEC_X: return &syn().vecX;
    case P_SYN_VEC_Y: return &syn().vecY;
    case P_FX_LEVEL: return &fx().level;
    case P_FX_DRIVE: return &fx().drive;
    case P_FX_EQ_LOW: return &fx().eqLow;
    case P_FX_EQ_HIGH: return &fx().eqHigh;
    case P_FX_DELAY: return &fx().delaySend;
    case P_FX_REVERB: return &fx().revSend;
    case P_FX_SLOT1_AMOUNT: return &fx().slots[0].amount;
    case P_FX_SLOT1_RATE: return &fx().slots[0].rate;
    case P_FX_SLOT2_AMOUNT: return &fx().slots[1].amount;
    case P_FX_SLOT2_RATE: return &fx().slots[1].rate;
    case P_FX_SLOT3_AMOUNT: return &fx().slots[2].amount;
    case P_FX_SLOT3_RATE: return &fx().slots[2].rate;
    case P_FX_MUTE: kind = 4; return reinterpret_cast<float*>(&fx().mute);
    default: scratch = 0; return &scratch;
  }
}

float Session::defaultParam(int id, int index) {
  static const std::unique_ptr<Project> def = std::make_unique<Project>(makeDemoProject());
  bool isInt; int kind;
  const void* f = field(*def, id, index, isInt, kind);     // field() only reads here
  switch (kind) {
    case 1: return float(*static_cast<const int32_t*>(f));
    case 2: return float(*static_cast<const uint8_t*>(f));
    case 3: return float(*static_cast<const int8_t*>(f));
    case 4: return *static_cast<const bool*>(f) ? 1.0f : 0.0f;
    default: return *static_cast<const float*>(f);
  }
}

float Session::getParam(int id, int index) const {
  bool isInt; int kind;
  Project& p = const_cast<Project&>(proj_);
  const void* f = field(p, id, index, isInt, kind);
  switch (kind) {
    case 1: return float(*static_cast<const int32_t*>(f));
    case 2: return float(*static_cast<const uint8_t*>(f));
    case 3: return float(*static_cast<const int8_t*>(f));
    case 4: return *static_cast<const bool*>(f) ? 1.0f : 0.0f;
    default: return *static_cast<const float*>(f);
  }
}

void Session::setParam(int id, int index, float value) {
  if (id == P_DRUM_ENGINE || id == P_ACTIVE_SYNTHS) return;   // these change more than one field: use their own calls
  if (id == P_KIT_HARDNESS) {
    proj_.kitHardness = clampT(fin(value, 0.0f), 0.0f, 1.0f);
    proj_.fx[0].drive = proj_.kitHardness;
    commit();
    return;
  }
  if (id >= P_DSYN_FREQ && id <= P_DSYN_LEN && index >= 0 && index < kNumPads) proj_.customDrums[index].enabled = true;
  if (id == P_JUNGLE_PITCH || id == P_JUNGLE_TIGHT || id == P_JUNGLE_CRUNCH) {
    applyJungleTuning(id == P_JUNGLE_PITCH ? value : proj_.junglePitch, id == P_JUNGLE_TIGHT ? value : proj_.jungleTight,
                      id == P_JUNGLE_CRUNCH ? value : proj_.jungleCrunch, false);
    return;
  }
  bool isInt; int kind;
  void* f = field(proj_, id, index, isInt, kind);
  const int iv = int(value < 0 ? value - 0.5f : value + 0.5f);
  switch (kind) {
    case 1: *static_cast<int32_t*>(f) = iv; break;
    case 2: *static_cast<uint8_t*>(f) = uint8_t(clampT(iv, 0, 255)); break;
    case 3: *static_cast<int8_t*>(f) = int8_t(clampT(iv, -128, 127)); break;
    case 4: *static_cast<bool*>(f) = value > 0.5f; break;
    default: *static_cast<float*>(f) = value;
  }
  commit();
}

int Session::toggleDrumStep(int scene, int pad, int step, bool accent) {
  if (!sceneOk(scene) || pad < 0 || pad >= kNumPads || step < 0 || step >= kMaxSteps) return 0;
  beginEdit();
  uint8_t& v = proj_.scenes[scene].drums.steps[pad][step];
  v = accent ? (v == 2 ? 1 : 2) : (v ? 0 : 1);
  const int out = v;
  commit();
  return out;
}

bool Session::toggleNote(int scene, int synth, int step, int degree, bool accent, int len, bool slide) {
  if (!sceneOk(scene) || synth < 0 || synth >= kNumSynths || step < 0 || step >= kMaxSteps) return false;
  beginEdit();
  SynthPattern& pat = proj_.scenes[scene].synth[synth];
  bool exists = false; int vel = 0;
  for (int i = 0; i < pat.count; ++i) if (pat.notes[i].step == step && pat.notes[i].degree == degree) { exists = true; vel = pat.notes[i].vel; }
  bool now;
  if (slide) {
    if (!exists) addNote(pat, step, degree, len, accent ? 2 : 1);
    now = false;
    for (int i = 0; i < pat.count; ++i) if (pat.notes[i].step == step && pat.notes[i].degree == degree) {
      pat.notes[i].flags = exists ? uint8_t(pat.notes[i].flags ^ kNoteSlide) : kNoteSlide;
      now = true;
    }
  }
  else if (accent) { addNote(pat, step, degree, len, (exists && vel == 2) ? 1 : 2); now = true; }
  else if (exists) { removeNote(pat, step, degree); now = false; }
  else now = addNote(pat, step, degree, len, 1);
  commit();
  return now;
}

void Session::setSceneBars(int scene, int bars) {
  if (!sceneOk(scene)) return;
  beginEdit(); proj_.scenes[scene].bars = bars; commit();
}
void Session::copyScene(int from, int to) {
  if (!sceneOk(from) || !sceneOk(to) || from == to) return;
  beginEdit(); proj_.scenes[to] = proj_.scenes[from]; commit();
}
void Session::clearScene(int scene) {
  if (!sceneOk(scene)) return;
  beginEdit(); const int bars = proj_.scenes[scene].bars; proj_.scenes[scene] = Scene{}; proj_.scenes[scene].bars = bars; commit();
}
void Session::setDrumStep(int scene, int pad, int step, int value) {
  if (!sceneOk(scene) || pad < 0 || pad >= kNumPads || step < 0 || step >= kMaxSteps) return;
  beginEdit();
  proj_.scenes[scene].drums.steps[pad][step] = uint8_t(clampT(value, 0, 5));
  commit();
}
void Session::setChainAuto(int pos, int track, int lane, int paramId, float startVal, float endVal, bool undoStep) {
  if (pos < 0 || pos >= proj_.chainLen || track < 0 || track >= kNumTracks || lane < 0 || lane >= kAutoPerTrack) return;
  if (undoStep) beginEdit();
  auto& al = proj_.chainAuto[pos].tracks[track][lane];
  al.paramId = int16_t(paramId);
  al.startVal = startVal; al.endVal = endVal;
  commit();
}
void Session::setFxSlot(int track, int slot, int type, float amount, float rate, bool bypass, bool undoStep) {
  if (track < 0 || track >= kNumTracks || slot < 0 || slot >= kFxSlots) return;
  if (undoStep) beginEdit();
  auto& sl = proj_.fx[track].slots[slot];
  sl.type = FxType(clampT(type, 0, kNumFxTypes - 1));
  sl.amount = clampT(amount, 0.0f, 1.0f);
  sl.rate = clampT(rate, 0.0f, 1.0f);
  sl.bypass = bypass;
  commit();
}
void Session::setActiveSynths(int count) {
  beginEdit();
  proj_.activeSynths = clampT(count, 2, kNumSynths);
  commit();
}
void setInstrumentPreset(SynthParams& sp, int preset) {
  const auto pr = InstrumentPreset(clampT(preset, 0, int(InstrumentPreset::PluckLead)));
  sp.instrument = pr;
  switch (pr) {
    case InstrumentPreset::AnalogBass:
      sp.mode = SynthMode::Subtractive; sp.wave1 = Wave::Saw; sp.wave2 = Wave::Square;
      sp.detune = 5.0f; sp.cutoff = 520.0f; sp.res = 2.4f; sp.attack = 0.004f; sp.decay = 0.28f; sp.sustain = 0.55f; sp.release = 0.06f;
      sp.lfoTarget = LfoTarget::Off; sp.feelWarmth = 0.72f; sp.feelMotion = 0.20f;
      break;
    case InstrumentPreset::Acid303:
      sp.mode = SynthMode::Subtractive; sp.wave1 = Wave::Saw; sp.wave2 = Wave::Saw;
      sp.detune = 0.0f; sp.cutoff = 950.0f; sp.res = 10.5f; sp.attack = 0.002f; sp.decay = 0.22f; sp.sustain = 0.1f; sp.release = 0.05f;
      sp.lfoTarget = LfoTarget::Cutoff; sp.lfoRate = 2.0f; sp.lfoDepth = 0.35f; sp.feelWarmth = 0.65f; sp.feelMotion = 0.45f;
      break;
    case InstrumentPreset::ReeseDnB:
      sp.mode = SynthMode::Subtractive; sp.wave1 = Wave::Saw; sp.wave2 = Wave::Saw;
      sp.detune = 24.0f; sp.cutoff = 780.0f; sp.res = 3.5f; sp.attack = 0.02f; sp.decay = 0.5f; sp.sustain = 0.85f; sp.release = 0.18f;
      sp.lfoTarget = LfoTarget::Cutoff; sp.lfoRate = 0.8f; sp.lfoDepth = 0.55f; sp.feelWarmth = 0.80f; sp.feelMotion = 0.75f;
      break;
    case InstrumentPreset::FmBell:
      sp.mode = SynthMode::Fm; sp.fmRatio = 3.5f; sp.fmIndex = 4.5f; sp.fmModDecay = 0.45f; sp.fmFeedback = 0.1f;
      sp.cutoff = 7200.0f; sp.res = 1.0f; sp.attack = 0.002f; sp.decay = 0.85f; sp.sustain = 0.0f; sp.release = 0.45f;
      sp.lfoTarget = LfoTarget::Off; sp.feelWarmth = 0.25f; sp.feelMotion = 0.25f;
      break;
    case InstrumentPreset::FmEPiano:
      sp.mode = SynthMode::Fm; sp.fmRatio = 2.0f; sp.fmIndex = 2.2f; sp.fmModDecay = 0.22f; sp.fmFeedback = 0.08f;
      sp.cutoff = 4200.0f; sp.res = 1.2f; sp.attack = 0.004f; sp.decay = 0.65f; sp.sustain = 0.25f; sp.release = 0.25f;
      sp.lfoTarget = LfoTarget::Amp; sp.lfoRate = 4.5f; sp.lfoDepth = 0.22f; sp.feelWarmth = 0.60f; sp.feelMotion = 0.40f;
      break;
    case InstrumentPreset::SuperPad:
      sp.mode = SynthMode::Subtractive; sp.wave1 = Wave::Saw; sp.wave2 = Wave::Triangle;
      sp.detune = 18.0f; sp.cutoff = 1900.0f; sp.res = 1.8f; sp.attack = 0.22f; sp.decay = 0.6f; sp.sustain = 0.75f; sp.release = 0.55f;
      sp.lfoTarget = LfoTarget::Cutoff; sp.lfoRate = 0.5f; sp.lfoDepth = 0.35f; sp.feelWarmth = 0.65f; sp.feelMotion = 0.70f;
      break;
    case InstrumentPreset::PluckLead:
      sp.mode = SynthMode::Subtractive; sp.wave1 = Wave::Square; sp.wave2 = Wave::Saw;
      sp.detune = 9.0f; sp.cutoff = 3100.0f; sp.res = 2.8f; sp.attack = 0.003f; sp.decay = 0.18f; sp.sustain = 0.0f; sp.release = 0.09f;
      sp.lfoTarget = LfoTarget::Off; sp.feelWarmth = 0.40f; sp.feelMotion = 0.30f;
      break;
    default: break;
  }
}
void Session::applyInstrument(int synth, int preset) {
  if (synth < 0 || synth >= kNumSynths) return;
  beginEdit();
  setInstrumentPreset(proj_.synths[synth], preset);
  commit();
}
namespace {
void sliceAcrossPads(Project& p) {
  for (int i = 0; i < kNumPads; ++i) {
    PadParams& d = p.pads[i];
    d.sliceStart = float(i) / float(kNumPads);
    d.sliceEnd = float(i + 1) / float(kNumPads);
    d.choke = 1;
    d.decay = p.jungleTight;
    d.pitch = p.junglePitch;
    d.reverse = false;
  }
}
}  // namespace

void Session::loadJungleKit() { setDrumEngine(int(DrumEngine::JungleSlicer)); }
void Session::loadStandardKit() { setDrumEngine(int(DrumEngine::Classic808)); }

// Only project fields change here. commit() -> syncKit() then loads the matching sounds, and does the
// same after undo, redo and loading a file.
void switchDrumMode(Project& p, bool toJungle) {
  const bool isJungle = p.drumEngine == DrumEngine::JungleSlicer;
  if (isJungle == toJungle) return;
  PadParams pads[kNumPads];
  DrumPattern drums[kNumScenes];
  const float drive = p.fx[0].drive;
  for (int i = 0; i < kNumPads; ++i) pads[i] = p.pads[i];
  for (int i = 0; i < kNumScenes; ++i) drums[i] = p.scenes[i].drums;
  if (p.altValid) {                            // been in that mode before: everything comes back as it was left
    for (int i = 0; i < kNumPads; ++i) p.pads[i] = p.altPads[i];
    for (int i = 0; i < kNumScenes; ++i) p.scenes[i].drums = p.altDrums[i];
    p.fx[0].drive = p.altDrumDrive;
  } else if (toJungle) {                           // first visit: slice the break, keep the pattern as a start
    sliceAcrossPads(p);
    p.fx[0].drive = p.jungleCrunch;
  } else {                                         // an old project that was saved inside the slicer
    const Project def = makeDemoProject();
    for (int i = 0; i < kNumPads; ++i) p.pads[i] = def.pads[i];
  }
  for (int i = 0; i < kNumPads; ++i) p.altPads[i] = pads[i];
  for (int i = 0; i < kNumScenes; ++i) p.altDrums[i] = drums[i];
  p.altDrumDrive = drive;
  p.altValid = 1;
  p.drumEngine = toJungle ? DrumEngine::JungleSlicer : DrumEngine::Classic808;
}
void setDrumEngine(Project& p, int engine) {
  const auto de = DrumEngine(clampT(engine, 0, int(DrumEngine::GlitchBreakcore)));
  const bool wasJungle = p.drumEngine == DrumEngine::JungleSlicer;
  const bool toJungle = de == DrumEngine::JungleSlicer;
  if (wasJungle && toJungle) {                     // choosing the slicer again inside it: back to the built-in break
    p.breakId = 0;
    sliceAcrossPads(p);
  }
  // The built-in break is made for 168 BPM. An own break keeps the tempo it was made at.
  if (toJungle && !wasJungle && !p.altValid && p.breakId == 0) p.bpm = 168.0f;
  switchDrumMode(p, toJungle);                        // the own break (breakId) stays with the project
  p.drumEngine = de;
}
void Session::switchDrumMode(bool toJungle) { ld::switchDrumMode(proj_, toJungle); }
void Session::setDrumEngine(int engine) { beginEdit(); ld::setDrumEngine(proj_, engine); commit(); }
void Session::applyJungleTuning(float pitch, float tight, float crunch, bool undoStep) {
  if (undoStep) beginEdit();
  proj_.junglePitch = clampT(fin(pitch, 0.0f), -12.0f, 12.0f);
  proj_.jungleTight = clampT(fin(tight, 0.65f), 0.08f, 1.5f);
  proj_.jungleCrunch = clampT(fin(crunch, 0.25f), 0.0f, 1.0f);
  if (proj_.drumEngine == DrumEngine::JungleSlicer) {
    for (int i = 0; i < kNumPads; ++i) {
      proj_.pads[i].pitch = proj_.junglePitch;
      proj_.pads[i].decay = proj_.jungleTight;
    }
    proj_.fx[0].drive = proj_.jungleCrunch;
  }
  commit();
}
std::shared_ptr<const Sample> Session::findBreak(uint32_t id) const {
  if (id == 0) return nullptr;
  for (const Break& b : breaks_) if (b.id == id) return b.sample;
  return nullptr;
}
std::shared_ptr<const Sample> Session::activeBreak() const {
  return proj_.drumEngine == DrumEngine::JungleSlicer ? findBreak(proj_.breakId) : nullptr;
}
void Session::useBreak(std::shared_ptr<const Sample> s, uint32_t id) {
  if (id == 0) id = nextBreakId_;
  if (id >= nextBreakId_) nextBreakId_ = id + 1;
  for (size_t i = 0; i < breaks_.size(); ++i) if (breaks_[i].id == id) { breaks_.erase(breaks_.begin() + long(i)); break; }
  breaks_.push_back(Break{id, std::move(s)});
  if (breaks_.size() > 3) breaks_.erase(breaks_.begin());
  proj_.breakId = id;
}
bool Session::loadJungleBreakWav(const char* path) {
  std::shared_ptr<const Sample> loaded = loadWav(path);
  if (!loaded || loaded->frames() < 16 || loaded->rate < 1000.0) return false;
  if (double(loaded->frames()) / loaded->rate > kMaxBreakSeconds) return false;
  beginEdit();
  switchDrumMode(true);
  useBreak(std::move(loaded));
  sliceAcrossPads(proj_);
  commit();
  return true;
}
bool Session::resampleSceneToJungle(int scene) {
  if (!sceneOk(scene)) return false;
  // Render this scene's drums, through their effects, exactly as they sound now.
  RenderOptions opt;
  opt.sampleRate = engine_.sampleRate();
  opt.songMode = false;
  opt.scene = scene;
  opt.soloTrack = 0;
  opt.tailSeconds = 0.0;
  const std::shared_ptr<const Sample> cur = activeBreak();
  if (cur) for (int i = 0; i < kNumPads; ++i) opt.samples[i] = cur.get();
  const double seconds = double(sceneSteps(proj_.scenes[scene])) * 15.0 / double(proj_.bpm < 20 ? 20 : proj_.bpm);
  if (seconds > kMaxBreakSeconds) return false;
  RenderResult rr = renderOffline(proj_, opt);
  float pk = 0.0f;
  for (size_t i = 0; i < rr.l.size(); ++i) pk = std::fmax(pk, std::fmax(std::fabs(rr.l[i]), std::fabs(rr.r[i])));
  if (rr.l.size() < 16 || pk < 1e-4f) return false;          // an empty pattern has nothing to slice
  const float norm = 0.92f / pk;
  for (size_t i = 0; i < rr.l.size(); ++i) { rr.l[i] *= norm; rr.r[i] *= norm; }
  auto bounced = std::make_shared<Sample>();
  bounced->rate = rr.sampleRate;
  bounced->l = std::move(rr.l);
  bounced->r = std::move(rr.r);

  const double sliceSeconds = seconds / double(kNumPads);
  beginEdit();
  switchDrumMode(true);
  useBreak(std::move(bounced));
  sliceAcrossPads(proj_);
  // The slices in their order, long enough to play through: the scene sounds like the loop that was bounced.
  Scene& bs = proj_.scenes[scene];
  bs.drums = DrumPattern{};
  for (int i = 0; i < kNumPads; ++i) {
    bs.drums.steps[i][i * sceneSteps(bs) / kNumPads] = 2;
    proj_.pads[i].pitch = 0.0f;
    proj_.pads[i].decay = clampT(float(sliceSeconds * 1.05), proj_.jungleTight, 2.0f);
  }
  proj_.junglePitch = 0.0f;
  // The effects are now part of the recording, so they must not be applied a second time.
  proj_.fx[0].drive = 0.0f;
  proj_.jungleCrunch = 0.0f;
  for (auto& sl : proj_.fx[0].slots) sl.type = FxType::Off;
  commit();
  return true;
}
void Session::writeBreakPattern(int scene) {
  if (!sceneOk(scene)) return;
  beginEdit();
  Scene& sc = proj_.scenes[scene];
  sc.drums = DrumPattern{};
  for (int bar = 0; bar < sceneSteps(sc) / kStepsPerBar; ++bar)
    for (int i = 0; i < kNumPads; ++i) sc.drums.steps[i][bar * kStepsPerBar + i * 2] = uint8_t(i % 4 == 0 ? 2 : 1);
  commit();
}

void Session::setKitHardness(float hardness) {
  beginEdit();
  proj_.kitHardness = clampT(fin(hardness, 0.0f), 0.0f, 1.0f);
  proj_.fx[0].drive = proj_.kitHardness;
  commit();
}

// These only change the project. commit() -> syncKit() builds the sound, also after undo, redo and loading.
void Session::synthesizeCustomPad(int pad, bool audition) {
  if (pad < 0 || pad >= kNumPads) return;
  beginEdit();
  proj_.customDrums[pad].enabled = true;
  commit();
  if (audition) engine_.triggerPad(pad, 2);
}
void Session::clearCustomPad(int pad) {
  if (pad < 0 || pad >= kNumPads) return;
  beginEdit();
  proj_.customDrums[pad].enabled = false;
  commit();
}
void Session::applyDrumSynthTemplate(int pad, int tmpl) {
  if (pad < 0 || pad >= kNumPads) return;
  beginEdit();
  DrumSynthParams& d = proj_.customDrums[pad];
  switch (tmpl) {
    case 0: d = DrumSynthParams{48.0f, 180.0f, 0.025f, 0.95f, 0.05f, 2200.0f, 0.0f, 1.5f, 0.30f, 0.35f, 0.48f, true}; break;    // deep kick
    case 1: d = DrumSynthParams{195.0f, 80.0f, 0.015f, 0.45f, 0.85f, 5200.0f, 0.8f, 2.3f, 0.35f, 0.40f, 0.30f, true}; break;    // crisp snare
    case 2: d = DrumSynthParams{620.0f, 0.0f, 0.010f, 0.35f, 0.85f, 8800.0f, 5.5f, 3.71f, 0.20f, 0.15f, 0.16f, true}; break;    // metallic hat
    case 3: d = DrumSynthParams{46.0f, 320.0f, 0.012f, 1.0f, 0.08f, 3500.0f, 0.4f, 2.0f, 0.92f, 0.85f, 0.52f, true}; break;     // gabber kick
    case 4: d = DrumSynthParams{95.0f, 580.0f, 0.030f, 0.95f, 0.0f, 4000.0f, 1.2f, 2.0f, 0.25f, 0.20f, 0.32f, true}; break;     // electro zap
    default: d = DrumSynthParams{260.0f, 40.0f, 0.020f, 0.85f, 0.20f, 6000.0f, 6.8f, 2.71f, 0.55f, 0.30f, 0.50f, true}; break;  // industrial clang
  }
  commit();
  engine_.triggerPad(pad, 2);
}

void Session::chainAdd(int scene) {
  if (!sceneOk(scene) || proj_.chainLen >= kMaxChain) return;
  beginEdit();
  proj_.chain[proj_.chainLen] = scene;
  proj_.chainAuto[proj_.chainLen] = ChainAuto{};
  ++proj_.chainLen;
  commit();
}
void Session::chainRemove(int pos) {
  if (pos < 0 || pos >= proj_.chainLen || proj_.chainLen <= 1) return;
  beginEdit();
  for (int i = pos; i + 1 < proj_.chainLen; ++i) {
    proj_.chain[i] = proj_.chain[i + 1];
    proj_.chainAuto[i] = proj_.chainAuto[i + 1];
  }
  --proj_.chainLen;
  commit();
}
void Session::resetToDemo() { beginEdit(); proj_ = makeDemoProject(); commit(); }
void Session::loadStartPreset() { beginEdit(); proj_ = makeStartProject(); commit(); }

int Session::padWaveform(int pad, float* out, int n) {
  if (pad < 0 || pad >= kNumPads || !out || n <= 0) return 0;
  const DrumEngine de = proj_.drumEngine;
  const std::shared_ptr<const Sample> brk = activeBreak();
  const uint32_t bid = brk ? proj_.breakId : 0;
  const uint32_t key = drumSynthKey(proj_, pad);
  if (waveEngine_ != int(de) || waveBreak_ != bid) {
    const double sr = engine_.sampleRate();
    if (de == DrumEngine::JungleSlicer) {
      const std::shared_ptr<const Sample> one = brk ? brk : std::shared_ptr<const Sample>(makeJungleBreakSample(sr));
      for (auto& w : wave_) w = one;
    } else {
      for (int i = 0; i < kNumPads; ++i) wave_[i] = makeDrumKitSample(de, i, sr);
    }
    for (auto& k : wavePad_) k = 0;
    waveEngine_ = int(de);
    waveBreak_ = bid;
  }
  if (wavePad_[pad] != key) {
    if (key != 0) wave_[pad] = makeCustomDrumSample(proj_.customDrums[pad], engine_.sampleRate());
    else if (de != DrumEngine::JungleSlicer) wave_[pad] = makeDrumKitSample(de, pad, engine_.sampleRate());
    wavePad_[pad] = key;
  }
  const Sample& s = *wave_[pad];
  const size_t frames = s.frames();
  for (int b = 0; b < n; ++b) {
    const size_t a = frames * size_t(b) / size_t(n), e = std::max(a + 1, frames * size_t(b + 1) / size_t(n));
    float pk = 0;
    for (size_t i = a; i < e && i < frames; ++i) pk = std::max(pk, std::fabs(s.l[i]));
    out[b] = pk > 1.0f ? 1.0f : pk;
  }
  return n;
}

bool Session::poll() {
  engine_.collectGarbage();
  RecordedHit h;
  bool changed = false;
  while (engine_.pollRecorded(h)) {
    if (!sceneOk(h.scene) || h.step < 0 || h.step >= kMaxSteps) continue;
    Scene& sc = proj_.scenes[h.scene];
    if (h.track == 0) {
      if (h.row < 0 || h.row >= kNumPads || sc.drums.steps[h.row][h.step]) continue;
      if (!changed) beginEdit();
      sc.drums.steps[h.row][h.step] = uint8_t(h.vel == 2 ? 2 : 1); changed = true;
    } else if (h.track - 1 < kNumSynths) {
      SynthPattern& pat = sc.synth[h.track - 1];
      bool exists = false;
      for (int i = 0; i < pat.count; ++i) if (pat.notes[i].step == h.step && pat.notes[i].degree == h.row) exists = true;
      if (exists) continue;
      if (!changed) beginEdit();
      changed = addNote(pat, h.step, h.row, 1, h.vel) || changed;
    }
  }
  if (changed) commit();
  return changed;
}

// A user break is audio, not settings, so it lives in its own file next to the project.
static std::string breakPath(const char* path) { return std::string(path) + ".break.wav"; }

bool Session::save(const char* path) {
  if (const std::shared_ptr<const Sample> brk = findBreak(proj_.breakId)) {   // also while a hit kit is selected
    if (savedBreakId_ != proj_.breakId) {
      const float* r = brk->r.size() == brk->l.size() ? brk->r.data() : brk->l.data();
      if (!writeWav16(breakPath(path).c_str(), brk->l.data(), r, brk->frames(), brk->rate)) return false;
      savedBreakId_ = proj_.breakId;
    }
  }
  if (!writeProjectFile(path, proj_)) return false;
  dirty_ = false;
  return true;
}

bool Session::importContent(const std::string& json, const ContentTarget& target, std::string& error, ContentInfo* info) {
  auto work = std::make_unique<Project>(proj_);
  if (!applyContent(json, *work, target, error, info)) return false;
  beginEdit();
  proj_ = *work;
  commit();
  return true;
}

bool Session::openProject(const char* path) {
  Project p;
  if (!readProjectFile(path, p, nullptr)) return false;
  std::shared_ptr<const Sample> brk;
  if (p.breakId != 0) {
    brk = loadWav(breakPath(path).c_str());
    if (!brk || brk->frames() < 16) { brk.reset(); p.breakId = 0; }
  }
  beginEdit();
  proj_ = p;
  if (brk) useBreak(std::move(brk));               // gets a fresh id, so it cannot be mixed up with a break of this session
  appliedEngine_ = -1;
  waveEngine_ = -1;
  commit();
  return true;
}

bool Session::saveCopy(const char* path) const {
  if (const std::shared_ptr<const Sample> brk = findBreak(proj_.breakId)) {
    const float* r = brk->r.size() == brk->l.size() ? brk->r.data() : brk->l.data();
    if (!writeWav16(breakPath(path).c_str(), brk->l.data(), r, brk->frames(), brk->rate)) return false;
  } else {
    removeFile(breakPath(path).c_str());          // no stale audio from an earlier save under this name
  }
  return writeProjectFile(path, proj_);
}

bool Session::load(const char* path) {
  Project p;
  bool converted = false;
  if (!readProjectFile(path, p, &converted)) return false;
  proj_ = p; undo_.clear(); redo_.clear();
  breaks_.clear();
  savedBreakId_ = 0;
  if (proj_.breakId != 0) {
    std::shared_ptr<const Sample> brk = loadWav(breakPath(path).c_str());
    if (brk && brk->frames() >= 16) {
      useBreak(std::move(brk), proj_.breakId);
      savedBreakId_ = proj_.breakId;
    } else {
      proj_.breakId = 0;                           // the audio is gone: fall back to the built-in break
    }
  }
  appliedEngine_ = -1;                             // the same id may now mean different audio
  waveEngine_ = -1;
  syncKit();
  engine_.setProject(proj_);
  dirty_ = converted;                            // a converted project gets written back in the current format
  return true;
}

}  // namespace ld
