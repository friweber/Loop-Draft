// Loop Draft engine - project files.
//
// Current format (version 5): a list of named records, "key, size, bytes". A reader keeps the default for
// any key it does not find and skips keys it does not know. So adding fields, tracks or effect slots later
// does not break old files, and no new converter is needed each time the Project struct grows.
//
// Versions 1 (app build 5) and 4 (app build 7, v0.3) were raw copies of the Project struct. Their layouts
// are frozen below so those files keep opening.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "loopdraft/session.h"

namespace ld {

namespace {

constexpr char kMagic[8] = {'L', 'D', 'P', 'R', 'O', 'J', 0, 1};
constexpr uint32_t kFileVersion = 7;   // 5: no closing "end" record. 6: before notes had flags (that byte was padding)

// ------------------------------------------------------------------ frozen layouts of versions 1 and 4

namespace legacy {
constexpr int kSynths = 2, kTracks = 3;
struct TrackFx { float level, drive, eqLow, eqHigh, delaySend, revSend; bool mute; };
struct Scene { int32_t bars; DrumPattern drums; SynthPattern synth[kSynths]; };

void copyCommon(ld::Project& p, const TrackFx* fx, const Scene* scenes, const int32_t* chain, int32_t chainLen) {
  for (int i = 0; i < kTracks; ++i) {
    ld::TrackFx& d = p.fx[i];
    d.level = fx[i].level; d.drive = fx[i].drive; d.eqLow = fx[i].eqLow; d.eqHigh = fx[i].eqHigh;
    d.delaySend = fx[i].delaySend; d.revSend = fx[i].revSend; d.mute = fx[i].mute;
  }
  for (int i = 0; i < kNumScenes; ++i) {
    p.scenes[i].bars = scenes[i].bars; p.scenes[i].drums = scenes[i].drums;
    for (int y = 0; y < kSynths; ++y) p.scenes[i].synth[y] = scenes[i].synth[y];
  }
  for (int i = 0; i < kMaxChain; ++i) p.chain[i] = chain[i];
  p.chainLen = chainLen;
}

namespace v1 {
struct PadParams { float vol, pan, pitch, decay; uint8_t choke, gmNote; };
struct SynthParams {
  Wave wave1, wave2;
  float detune, cutoff, res, attack, decay, sustain, release, level, lfoRate, lfoDepth;
  LfoTarget lfoTarget; int8_t octaveShift; bool chord;
};
struct Project {
  float bpm, swing; int32_t root; Scale scale; int32_t octave;
  PadParams pads[kNumPads]; SynthParams synths[kSynths]; TrackFx fx[kTracks];
  float delayFeedback, master; Scene scenes[kNumScenes]; int32_t chain[kMaxChain]; int32_t chainLen;
};
static_assert(sizeof(Project) == 17060, "version 1 layout changed");
}  // namespace v1

namespace v4 {
struct PadParams { float vol, pan, pitch, decay; uint8_t choke, gmNote; float sliceStart, sliceEnd; bool reverse; };
struct SynthParams {
  Wave wave1, wave2;
  float detune, cutoff, res, attack, decay, sustain, release, level, lfoRate, lfoDepth;
  LfoTarget lfoTarget; int8_t octaveShift; bool chord; SynthMode mode;
  float fmRatio, fmIndex, fmModDecay, fmFeedback;
};
struct AutoLane { uint8_t param; float startVal, endVal; };   // 1 bass cutoff, 2 lead cutoff, 3 drums drive, 4 lead delay
struct ChainAuto { AutoLane lanes[2]; };
struct Project {
  float bpm, swing; int32_t root; Scale scale; int32_t octave;
  PadParams pads[kNumPads]; SynthParams synths[kSynths]; TrackFx fx[kTracks];
  float delayFeedback, master; Scene scenes[kNumScenes]; int32_t chain[kMaxChain];
  ChainAuto chainAuto[kMaxChain]; int32_t chainLen; uint8_t kit;
};
static_assert(sizeof(Project) == 18728, "version 4 layout changed");
}  // namespace v4

// The other development line also called its raw-struct format "version 5" (drum engines, before named records).
// Its sub-structs are the same as today's; if one of them ever changes size, the assert below says so and
// this needs frozen copies like v1 and v4 have.
namespace v5raw {
struct SynthParams {                               // as it was then; today's struct starts with the same fields
  Wave wave1, wave2; float detune, cutoff, res, attack, decay, sustain, release, level, lfoRate, lfoDepth;
  LfoTarget lfoTarget; int8_t octaveShift; bool chord; SynthMode mode; float fmRatio, fmIndex, fmModDecay, fmFeedback;
  InstrumentPreset instrument; float feelWarmth, feelMotion;
};
static_assert(sizeof(SynthParams) == 76 && sizeof(SynthParams) <= sizeof(ld::SynthParams), "raw version 5 synth layout");
struct Project {
  float bpm, swing; int32_t root; Scale scale; int32_t octave; int32_t activeSynths;
  DrumEngine drumEngine; float junglePitch, jungleTight, jungleCrunch;
  ld::PadParams pads[kNumPads]; SynthParams synths[4]; ld::TrackFx fx[5];
  float delayFeedback, master; ld::Scene scenes[kNumScenes]; int32_t chain[kMaxChain];
  ld::ChainAuto chainAuto[kMaxChain]; int32_t chainLen;
};
static_assert(sizeof(Project) == 41520, "raw version 5 layout changed");
}  // namespace v5raw

ld::Project upgrade(const v5raw::Project& o) {
  ld::Project p;
  p.bpm = o.bpm; p.swing = o.swing; p.root = o.root; p.scale = o.scale; p.octave = o.octave; p.activeSynths = o.activeSynths;
  p.drumEngine = o.drumEngine; p.junglePitch = o.junglePitch; p.jungleTight = o.jungleTight; p.jungleCrunch = o.jungleCrunch;
  std::memcpy(static_cast<void*>(p.pads), o.pads, sizeof(p.pads));
  for (int i = 0; i < 4; ++i) std::memcpy(static_cast<void*>(&p.synths[i]), &o.synths[i], sizeof(v5raw::SynthParams));   // newer fields keep their defaults
  std::memcpy(static_cast<void*>(p.fx), o.fx, sizeof(p.fx));
  p.delayFeedback = o.delayFeedback; p.master = o.master;
  std::memcpy(static_cast<void*>(p.scenes), o.scenes, sizeof(p.scenes));
  std::memcpy(p.chain, o.chain, sizeof(p.chain));
  std::memcpy(static_cast<void*>(p.chainAuto), o.chainAuto, sizeof(p.chainAuto));
  p.chainLen = o.chainLen;
  return p;
}

// Works for both versions: version 4 only added fields to these structs.
template <class Old> void copyShared(ld::Project& p, const Old& o) {
  p.bpm = o.bpm; p.swing = o.swing; p.root = o.root; p.scale = o.scale; p.octave = o.octave;
  p.delayFeedback = o.delayFeedback; p.master = o.master;
  for (int i = 0; i < kNumPads; ++i) {
    p.pads[i].vol = o.pads[i].vol; p.pads[i].pan = o.pads[i].pan; p.pads[i].pitch = o.pads[i].pitch;
    p.pads[i].decay = o.pads[i].decay; p.pads[i].choke = o.pads[i].choke; p.pads[i].gmNote = o.pads[i].gmNote;
  }
  for (int i = 0; i < kSynths; ++i) {
    ld::SynthParams& d = p.synths[i]; const auto& s = o.synths[i];
    d.wave1 = s.wave1; d.wave2 = s.wave2; d.detune = s.detune; d.cutoff = s.cutoff; d.res = s.res; d.attack = s.attack;
    d.decay = s.decay; d.sustain = s.sustain; d.release = s.release; d.level = s.level; d.lfoRate = s.lfoRate;
    d.lfoDepth = s.lfoDepth; d.lfoTarget = s.lfoTarget; d.octaveShift = s.octaveShift; d.chord = s.chord;
  }
  copyCommon(p, o.fx, o.scenes, o.chain, o.chainLen);
}

ld::Project upgrade(const v1::Project& o) {
  ld::Project p;                                 // everything newer keeps its default
  copyShared(p, o);
  return p;
}

ld::Project upgrade(const v4::Project& o) {
  ld::Project p;
  copyShared(p, o);
  for (int i = 0; i < kNumPads; ++i) {
    p.pads[i].sliceStart = o.pads[i].sliceStart; p.pads[i].sliceEnd = o.pads[i].sliceEnd; p.pads[i].reverse = o.pads[i].reverse;
  }
  for (int i = 0; i < kSynths; ++i) {
    p.synths[i].mode = o.synths[i].mode; p.synths[i].fmRatio = o.synths[i].fmRatio; p.synths[i].fmIndex = o.synths[i].fmIndex;
    p.synths[i].fmModDecay = o.synths[i].fmModDecay; p.synths[i].fmFeedback = o.synths[i].fmFeedback;
  }
  p.drumEngine = o.kit == 1 ? DrumEngine::JungleSlicer : DrumEngine::Classic808;
  for (int i = 0; i < kMaxChain; ++i)            // the four fixed automation targets become free-choice lanes
    for (int k = 0; k < 2; ++k) {
      const v4::AutoLane& a = o.chainAuto[i].lanes[k];
      int track = -1, id = -1;
      switch (a.param) {
        case 1: track = 1; id = P_SYN_CUTOFF; break;
        case 2: track = 2; id = P_SYN_CUTOFF; break;
        case 3: track = 0; id = P_FX_DRIVE; break;
        case 4: track = 2; id = P_FX_DELAY; break;
        default: break;
      }
      if (track < 0) continue;
      for (int ln = 0; ln < kAutoPerTrack; ++ln) {
        ld::AutoLane& d = p.chainAuto[i].tracks[track][ln];
        if (d.paramId >= 0) continue;
        d.paramId = int16_t(id); d.startVal = a.startVal; d.endVal = a.endVal;
        break;
      }
    }
  return p;
}
}  // namespace legacy

// ------------------------------------------------------------------ version 5: named records

// One list of every saved field, used for writing and for reading.
template <class V> void fields(V& v, Project& p) {
  char k[64];
  v.f("bpm", p.bpm); v.f("swing", p.swing); v.f("root", p.root); v.f("scale", p.scale); v.f("octave", p.octave);
  v.f("delayFeedback", p.delayFeedback); v.f("master", p.master); v.f("activeSynths", p.activeSynths); v.f("drumEngine", p.drumEngine);
  v.f("junglePitch", p.junglePitch); v.f("jungleTight", p.jungleTight); v.f("jungleCrunch", p.jungleCrunch); v.f("breakId", p.breakId);
  v.f("chainLen", p.chainLen); v.blob("chain", p.chain, sizeof(p.chain), sizeof(p.chain));
#define LD_F(prefix, obj, name) std::snprintf(k, sizeof(k), "%s." #name, prefix); v.f(k, obj.name)
  char pre[40];
  for (int i = 0; i < kNumPads; ++i) {
    std::snprintf(pre, sizeof(pre), "pad%d", i);
    PadParams& d = p.pads[i];
    LD_F(pre, d, vol); LD_F(pre, d, pan); LD_F(pre, d, pitch); LD_F(pre, d, decay); LD_F(pre, d, choke); LD_F(pre, d, gmNote);
    LD_F(pre, d, sliceStart); LD_F(pre, d, sliceEnd); LD_F(pre, d, reverse);
  }
  v.f("altValid", p.altValid);
  if (!v.writing || p.altValid) {                    // the stored set of the other drum mode
    v.f("altDrumDrive", p.altDrumDrive);
    for (int i = 0; i < kNumPads; ++i) {
      std::snprintf(pre, sizeof(pre), "altpad%d", i);
      PadParams& d = p.altPads[i];
      LD_F(pre, d, vol); LD_F(pre, d, pan); LD_F(pre, d, pitch); LD_F(pre, d, decay); LD_F(pre, d, choke); LD_F(pre, d, gmNote);
      LD_F(pre, d, sliceStart); LD_F(pre, d, sliceEnd); LD_F(pre, d, reverse);
    }
    for (int i = 0; i < kNumScenes; ++i) {
      std::snprintf(k, sizeof(k), "alt.sc%d.drums", i);
      v.blob(k, p.altDrums[i].steps, sizeof(p.altDrums[i].steps), sizeof(p.altDrums[i].steps));
    }
  }
  for (int i = 0; i < kNumSynths; ++i) {
    std::snprintf(pre, sizeof(pre), "syn%d", i);
    SynthParams& s = p.synths[i];
    LD_F(pre, s, wave1); LD_F(pre, s, wave2); LD_F(pre, s, detune); LD_F(pre, s, cutoff); LD_F(pre, s, res); LD_F(pre, s, attack);
    LD_F(pre, s, decay); LD_F(pre, s, sustain); LD_F(pre, s, release); LD_F(pre, s, level); LD_F(pre, s, lfoRate); LD_F(pre, s, lfoDepth);
    LD_F(pre, s, lfoTarget); LD_F(pre, s, octaveShift); LD_F(pre, s, chord); LD_F(pre, s, mode); LD_F(pre, s, fmRatio); LD_F(pre, s, fmIndex);
    LD_F(pre, s, fmModDecay); LD_F(pre, s, fmFeedback); LD_F(pre, s, instrument); LD_F(pre, s, feelWarmth); LD_F(pre, s, feelMotion);
    LD_F(pre, s, fold); LD_F(pre, s, slideTime); LD_F(pre, s, pluckBright); LD_F(pre, s, vecX); LD_F(pre, s, vecY);
  }
  for (int t = 0; t < kNumTracks; ++t) {
    std::snprintf(pre, sizeof(pre), "fx%d", t);
    TrackFx& x = p.fx[t];
    LD_F(pre, x, level); LD_F(pre, x, drive); LD_F(pre, x, eqLow); LD_F(pre, x, eqHigh); LD_F(pre, x, delaySend); LD_F(pre, x, revSend); LD_F(pre, x, mute);
    for (int sl = 0; sl < kFxSlots; ++sl) {
      std::snprintf(pre, sizeof(pre), "fx%d.slot%d", t, sl);
      FxSlot& fs = x.slots[sl];
      LD_F(pre, fs, type); LD_F(pre, fs, amount); LD_F(pre, fs, rate); LD_F(pre, fs, bypass);
    }
  }
  for (int i = 0; i < kNumScenes; ++i) {
    Scene& sc = p.scenes[i];
    std::snprintf(k, sizeof(k), "sc%d.bars", i); v.f(k, sc.bars);
    std::snprintf(k, sizeof(k), "sc%d.drums", i); v.blob(k, sc.drums.steps, sizeof(sc.drums.steps), sizeof(sc.drums.steps));
    for (int y = 0; y < kNumSynths; ++y) {
      SynthPattern& pat = sc.synth[y];
      if (v.writing && pat.count <= 0) continue;                 // empty patterns are simply left out
      const int n = pat.count < 0 ? 0 : (pat.count > kMaxNotes ? kMaxNotes : pat.count);
      std::snprintf(k, sizeof(k), "sc%d.syn%d.count", i, y); v.f(k, pat.count);
      std::snprintf(k, sizeof(k), "sc%d.syn%d.notes", i, y); v.blob(k, pat.notes, sizeof(pat.notes), uint32_t(n) * sizeof(Note));
    }
  }
  for (int i = 0; i < kMaxChain; ++i)
    for (int t = 0; t < kNumTracks; ++t)
      for (int ln = 0; ln < kAutoPerTrack; ++ln) {
        AutoLane& al = p.chainAuto[i].tracks[t][ln];
        if (v.writing && al.paramId < 0) continue;               // only lanes that are in use
        std::snprintf(pre, sizeof(pre), "auto%d.%d.%d", i, t, ln);
        LD_F(pre, al, paramId); LD_F(pre, al, startVal); LD_F(pre, al, endVal);
      }
#undef LD_F
}

struct Writer {
  FILE* file;
  bool ok = true;
  const bool writing = true;
  void raw(const char* key, const void* data, uint32_t n) {
    const uint16_t kl = uint16_t(std::strlen(key));
    ok = ok && std::fwrite(&kl, 2, 1, file) == 1 && std::fwrite(key, 1, kl, file) == kl &&
         std::fwrite(&n, 4, 1, file) == 1 && (n == 0 || std::fwrite(data, 1, n, file) == n);
  }
  template <class T> void f(const char* key, T& x) { raw(key, &x, uint32_t(sizeof(T))); }
  void blob(const char* key, void* data, uint32_t, uint32_t used) { raw(key, data, used); }
};

struct Reader {
  struct Slot { void* ptr; uint32_t size; bool exact; };
  std::unordered_map<std::string, Slot> slots;
  const bool writing = false;
  template <class T> void f(const char* key, T& x) { slots[key] = Slot{&x, uint32_t(sizeof(T)), true}; }
  void blob(const char* key, void* data, uint32_t capacity, uint32_t) { slots[key] = Slot{data, capacity, false}; }
};

// needEnd: the file must close with an "end" record, so one that was cut off is never taken for complete.
bool readRecords(FILE* f, Project& p, bool needEnd) {
  Reader r;
  fields(r, p);
  std::vector<char> buf;
  for (;;) {
    uint16_t kl = 0;
    if (std::fread(&kl, 2, 1, f) != 1) return !needEnd && std::feof(f) != 0;
    char key[64];
    uint32_t n = 0;
    if (kl == 0 || kl >= sizeof(key) || std::fread(key, 1, kl, f) != kl || std::fread(&n, 4, 1, f) != 1 || n > (1u << 22)) return false;
    key[kl] = 0;
    if (needEnd && n == 0 && std::strcmp(key, "end") == 0) return true;
    buf.resize(n);
    if (n && std::fread(buf.data(), 1, n, f) != n) return false;
    const auto it = r.slots.find(key);
    if (it == r.slots.end()) continue;                                // written by a newer version: ignore
    const Reader::Slot& s = it->second;
    if (s.exact) { if (n == s.size) std::memcpy(s.ptr, buf.data(), n); }
    else std::memcpy(s.ptr, buf.data(), n < s.size ? n : s.size);
  }
}

}  // namespace

bool writeProjectFile(const char* path, const Project& project) {
  const std::string tmp = std::string(path) + ".tmp";
  FILE* f = std::fopen(tmp.c_str(), "wb");
  if (!f) return false;
  Project p = project;
  Writer w{f};
  const uint32_t head[2] = {kFileVersion, 0};
  w.ok = std::fwrite(kMagic, 1, 8, f) == 8 && std::fwrite(head, sizeof(uint32_t), 2, f) == 2;
  fields(w, p);
  const unsigned char end[9] = {3, 0, 'e', 'n', 'd', 0, 0, 0, 0};
  w.ok = w.ok && std::fwrite(end, 1, sizeof(end), f) == sizeof(end);
  bool ok = (std::fclose(f) == 0) && w.ok;
  if (ok) ok = std::rename(tmp.c_str(), path) == 0;                   // atomic: never leaves a half-written project
  if (!ok) std::remove(tmp.c_str());
  return ok;
}

static bool readProjectStream(FILE* f, Project& out, bool* converted) {
  char magic[8];
  uint32_t head[2] = {0, 0};
  bool ok = std::fread(magic, 1, 8, f) == 8 && std::memcmp(magic, kMagic, 8) == 0 && std::fread(head, sizeof(uint32_t), 2, f) == 2;
  bool conv = false;
  bool clearNoteFlags = false;                     // older files: that byte was never written on purpose
  auto p = std::make_unique<Project>();
  const auto rawStruct = [&](auto& old) {
    std::vector<char> raw(sizeof(old));
    if (std::fread(raw.data(), raw.size(), 1, f) != 1) return false;
    std::memcpy(static_cast<void*>(&old), raw.data(), raw.size());
    conv = true;
    clearNoteFlags = true;
    return true;
  };
  if (ok && head[0] >= 5 && head[0] <= kFileVersion && head[1] == 0) {
    ok = readRecords(f, *p, head[0] >= 6);
    if (head[0] < 7) clearNoteFlags = true;
  } else if (ok && head[0] == 5 && head[1] == sizeof(legacy::v5raw::Project)) {
    auto old = std::make_unique<legacy::v5raw::Project>();
    ok = rawStruct(*old);
    if (ok) *p = legacy::upgrade(*old);
  } else if (ok && head[0] == 1 && head[1] == sizeof(legacy::v1::Project)) {
    auto old = std::make_unique<legacy::v1::Project>();
    ok = rawStruct(*old);
    if (ok) *p = legacy::upgrade(*old);
  } else if (ok && head[0] == 4 && head[1] == sizeof(legacy::v4::Project)) {
    auto old = std::make_unique<legacy::v4::Project>();
    ok = rawStruct(*old);
    if (ok) *p = legacy::upgrade(*old);
  } else {
    ok = false;
  }
  if (!ok) return false;
  if (clearNoteFlags)
    for (auto& sc : p->scenes) for (auto& pat : sc.synth) for (auto& nt : pat.notes) nt.flags = 0;
  sanitize(*p);
  out = *p;
  if (converted) *converted = conv;
  return true;
}

bool readProjectFile(const char* path, Project& out, bool* converted) {
  FILE* f = std::fopen(path, "rb");
  if (!f) return false;
  const bool ok = readProjectStream(f, out, converted);
  std::fclose(f);
  return ok;
}

// ------------------------------------------------------------------ the project a fresh install starts with
namespace {
const unsigned char kStartPreset[] = {
#include "preset_warehouse303.inc"
};
}

Project makeStartProject() {
  Project p;
  FILE* f = fmemopen(const_cast<unsigned char*>(kStartPreset), sizeof(kStartPreset), "rb");
  const bool ok = f && readProjectStream(f, p, nullptr);
  if (f) std::fclose(f);
  return ok ? p : makeDemoProject();
}

}  // namespace ld
