#include "loopdraft/session.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace ld {

namespace {
constexpr size_t kMaxUndo = 64;
constexpr char kMagic[8] = {'L', 'D', 'P', 'R', 'O', 'J', 0, 1};
constexpr uint32_t kFileVersion = 3;
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
  p.delayFeedback = clampT(fin(p.delayFeedback, 0.38f), 0.0f, 0.85f);
  p.master = clampT(fin(p.master, 0.85f), 0.0f, 1.0f);
  for (auto& d : p.pads) {
    d.vol = clampT(fin(d.vol, 0.9f), 0.0f, 1.0f); d.pan = clampT(fin(d.pan, 0), -1.0f, 1.0f);
    d.pitch = clampT(fin(d.pitch, 0), -24.0f, 24.0f); d.decay = clampT(fin(d.decay, 0.3f), 0.02f, 2.0f);
    d.choke = uint8_t(clampT<int>(d.choke, 0, 4)); d.gmNote = uint8_t(clampT<int>(d.gmNote, 0, 127));
    d.sliceStart = clampT(fin(d.sliceStart, 0.0f), 0.0f, 0.99f);
    d.sliceEnd = clampT(fin(d.sliceEnd, 1.0f), d.sliceStart + 0.01f > 1.0f ? 1.0f : d.sliceStart + 0.01f, 1.0f);
  }
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
    if (uint8_t(s.mode) > uint8_t(SynthMode::Fm)) s.mode = SynthMode::Subtractive;
    s.fmRatio = clampT(fin(s.fmRatio, 2.0f), 0.5f, 8.0f);
    s.fmIndex = clampT(fin(s.fmIndex, 2.5f), 0.0f, 8.0f);
    s.fmModDecay = clampT(fin(s.fmModDecay, 0.25f), 0.01f, 3.0f);
    s.fmFeedback = clampT(fin(s.fmFeedback, 0.15f), 0.0f, 1.0f);
  }
  for (auto& f : p.fx) {
    f.level = clampT(fin(f.level, 0.8f), 0.0f, 1.0f); f.drive = clampT(fin(f.drive, 0), 0.0f, 1.0f);
    f.eqLow = clampT(fin(f.eqLow, 0), -12.0f, 12.0f); f.eqHigh = clampT(fin(f.eqHigh, 0), -12.0f, 12.0f);
    f.delaySend = clampT(fin(f.delaySend, 0), 0.0f, 1.0f); f.revSend = clampT(fin(f.revSend, 0), 0.0f, 1.0f);
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
    for (int ln = 0; ln < 2; ++ln) {
      auto& al = p.chainAuto[i].lanes[ln];
      if (uint8_t(al.param) > uint8_t(AutoParam::LeadDelay)) al.param = AutoParam::Off;
      al.startVal = fin(al.startVal, 0.0f); al.endVal = fin(al.endVal, 0.0f);
    }
  }
}

Session::Session(double sampleRate) : engine_(sampleRate), proj_(makeDemoProject()) {
  engine_.setProject(proj_);
}

void Session::commit() {
  sanitize(proj_);
  engine_.setProject(proj_);
  dirty_ = true;
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
    case P_FX_LEVEL: return &fx().level;
    case P_FX_DRIVE: return &fx().drive;
    case P_FX_EQ_LOW: return &fx().eqLow;
    case P_FX_EQ_HIGH: return &fx().eqHigh;
    case P_FX_DELAY: return &fx().delaySend;
    case P_FX_REVERB: return &fx().revSend;
    case P_FX_MUTE: kind = 4; return reinterpret_cast<float*>(&fx().mute);
    default: scratch = 0; return &scratch;
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

bool Session::toggleNote(int scene, int synth, int step, int degree, bool accent, int len) {
  if (!sceneOk(scene) || synth < 0 || synth >= kNumSynths || step < 0 || step >= kMaxSteps) return false;
  beginEdit();
  SynthPattern& pat = proj_.scenes[scene].synth[synth];
  bool exists = false; int vel = 0;
  for (int i = 0; i < pat.count; ++i) if (pat.notes[i].step == step && pat.notes[i].degree == degree) { exists = true; vel = pat.notes[i].vel; }
  bool now;
  if (accent) { addNote(pat, step, degree, len, (exists && vel == 2) ? 1 : 2); now = true; }
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
void Session::setChainAuto(int pos, int lane, int param, float startVal, float endVal) {
  if (pos < 0 || pos >= proj_.chainLen || lane < 0 || lane > 1) return;
  beginEdit();
  auto& al = proj_.chainAuto[pos].lanes[lane];
  al.param = AutoParam(clampT(param, 0, int(AutoParam::LeadDelay)));
  al.startVal = startVal; al.endVal = endVal;
  commit();
}
void Session::loadJungleKit() {
  beginEdit();
  auto masterBreak = makeJungleBreakSample(engine_.sampleRate());
  for (int i = 0; i < kNumPads; ++i) {
    engine_.setPadSample(i, std::make_unique<Sample>(*masterBreak));
    proj_.pads[i].sliceStart = float(i) / float(kNumPads);
    proj_.pads[i].sliceEnd = float(i + 1) / float(kNumPads);
    proj_.pads[i].choke = 1;
    proj_.pads[i].decay = 0.65f;
    proj_.pads[i].pitch = 0.0f;
    proj_.pads[i].reverse = false;
  }
  proj_.bpm = 168.0f;
  commit();
}
void Session::loadStandardKit() {
  beginEdit();
  const Project def = makeDemoProject();
  for (int i = 0; i < kNumPads; ++i) {
    engine_.setPadSample(i, nullptr);
    proj_.pads[i] = def.pads[i];
  }
  commit();
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

bool Session::save(const char* path) {
  const std::string tmp = std::string(path) + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) return false;
  const uint32_t head[2] = {kFileVersion, uint32_t(sizeof(Project))};
  bool ok = std::fwrite(kMagic, 1, 8, f) == 8 && std::fwrite(head, sizeof(uint32_t), 2, f) == 2 &&
            std::fwrite(&proj_, sizeof(Project), 1, f) == 1;
  ok = (std::fclose(f) == 0) && ok;
  if (ok) ok = std::rename(tmp.c_str(), path) == 0;
  if (!ok) std::remove(tmp.c_str());
  if (ok) dirty_ = false;
  return ok;
}

bool Session::load(const char* path) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  char magic[8]; uint32_t head[2] = {0, 0};
  Project p;
  bool ok = std::fread(magic, 1, 8, f) == 8 && std::memcmp(magic, kMagic, 8) == 0 &&
            std::fread(head, sizeof(uint32_t), 2, f) == 2 && head[0] == kFileVersion && head[1] == sizeof(Project) &&
            std::fread(&p, sizeof(Project), 1, f) == 1;
  std::fclose(f);
  if (!ok) return false;
  sanitize(p);
  proj_ = p; undo_.clear(); redo_.clear();
  engine_.setProject(proj_);
  dirty_ = false;
  return true;
}

}  // namespace ld
