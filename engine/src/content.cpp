// Presets and grooves as JSON text. See content.h and docs/CONTENT_FORMAT.md.
#include "loopdraft/content.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace ld {
namespace {

// ------------------------------------------------------------------ a small JSON reader
struct JV {
  enum T { Null, Bool, Num, Str, Arr, Obj };
  T t = Null;
  bool b = false;
  double n = 0;
  std::string s;
  std::vector<JV> a;
  std::vector<std::pair<std::string, JV>> o;
  const JV* get(const char* k) const {
    for (const auto& kv : o) if (kv.first == k) return &kv.second;
    return nullptr;
  }
};

struct Parser {
  const std::string& s;
  size_t i = 0;
  std::string err;
  explicit Parser(const std::string& text) : s(text) {}

  bool fail(const char* msg) {
    if (err.empty()) {
      int line = 1;
      for (size_t k = 0; k < i && k < s.size(); ++k) if (s[k] == '\n') ++line;
      err = "line " + std::to_string(line) + ": " + msg;
    }
    return false;
  }
  void ws() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
  bool lit(const char* w) {
    const size_t n = std::strlen(w);
    if (s.compare(i, n, w) != 0) return false;
    i += n;
    return true;
  }
  bool string(std::string& out) {
    if (i >= s.size() || s[i] != '"') return fail("expected a string in double quotes");
    ++i;
    out.clear();
    while (i < s.size() && s[i] != '"') {
      char c = s[i++];
      if (c != '\\') { out.push_back(c); continue; }
      if (i >= s.size()) return fail("unfinished string");
      c = s[i++];
      switch (c) {
        case 'n': out.push_back('\n'); break;
        case 't': out.push_back('\t'); break;
        case 'r': out.push_back('\r'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'u': {
          if (i + 4 > s.size()) return fail("bad \\u escape");
          unsigned cp = 0;
          for (int k = 0; k < 4; ++k) {
            const char h = s[i++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= unsigned(h - '0');
            else if (h >= 'a' && h <= 'f') cp |= unsigned(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp |= unsigned(h - 'A' + 10);
            else return fail("bad \\u escape");
          }
          if (cp < 0x80) out.push_back(char(cp));
          else if (cp < 0x800) { out.push_back(char(0xC0 | (cp >> 6))); out.push_back(char(0x80 | (cp & 0x3F))); }
          else { out.push_back(char(0xE0 | (cp >> 12))); out.push_back(char(0x80 | ((cp >> 6) & 0x3F))); out.push_back(char(0x80 | (cp & 0x3F))); }
          break;
        }
        default: out.push_back(c); break;          // \" \\ \/
      }
    }
    if (i >= s.size()) return fail("unfinished string");
    ++i;
    return true;
  }
  bool value(JV& v, int depth) {
    if (depth > 24) return fail("nested too deeply");
    ws();
    if (i >= s.size()) return fail("unexpected end of file");
    const char c = s[i];
    if (c == '{') {
      ++i; v.t = JV::Obj;
      ws();
      if (i < s.size() && s[i] == '}') { ++i; return true; }
      while (true) {
        ws();
        std::string k;
        if (!string(k)) return false;
        ws();
        if (i >= s.size() || s[i] != ':') return fail("expected ':' after a key");
        ++i;
        JV child;
        if (!value(child, depth + 1)) return false;
        v.o.emplace_back(std::move(k), std::move(child));
        ws();
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') { ++i; return true; }
        return fail("expected ',' or '}'");
      }
    }
    if (c == '[') {
      ++i; v.t = JV::Arr;
      ws();
      if (i < s.size() && s[i] == ']') { ++i; return true; }
      while (true) {
        JV child;
        if (!value(child, depth + 1)) return false;
        v.a.push_back(std::move(child));
        ws();
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == ']') { ++i; return true; }
        return fail("expected ',' or ']'");
      }
    }
    if (c == '"') { v.t = JV::Str; return string(v.s); }
    if (lit("true")) { v.t = JV::Bool; v.b = true; return true; }
    if (lit("false")) { v.t = JV::Bool; v.b = false; return true; }
    if (lit("null")) { v.t = JV::Null; return true; }
    if (c == '-' || (c >= '0' && c <= '9')) {
      const size_t st = i;
      while (i < s.size() && (std::strchr("+-.eE", s[i]) || (s[i] >= '0' && s[i] <= '9'))) ++i;
      char* end = nullptr;
      const std::string tok = s.substr(st, i - st);
      v.n = std::strtod(tok.c_str(), &end);
      if (!end || *end != 0 || !(v.n == v.n) || std::fabs(v.n) > 1e12) { i = st; return fail("not a valid number"); }
      v.t = JV::Num;
      return true;
    }
    return fail("unexpected character");
  }
};

bool parseJson(const std::string& text, JV& out, std::string& err) {
  if (text.size() > 2u * 1024u * 1024u) { err = "the file is larger than 2 MB"; return false; }
  Parser p(text);
  if (!p.value(out, 0)) { err = p.err; return false; }
  p.ws();
  if (p.i != text.size()) { p.fail("extra text after the end"); err = p.err; return false; }
  return true;
}

// ------------------------------------------------------------------ checked access
struct Rd {
  std::string err;
  std::vector<std::string> warn;
  bool fail(const std::string& path, const std::string& msg) {
    if (err.empty()) err = (path.empty() ? std::string("file") : path) + ": " + msg;
    return false;
  }
};

std::string fmt(double v) {
  char b[40];
  std::snprintf(b, sizeof(b), "%g", v);
  return b;
}
// "909 Punch", "909-punch" and "909punch" are the same name.
std::string norm(const std::string& in) {
  std::string o;
  for (char c : in) {
    if (c >= 'A' && c <= 'Z') o.push_back(char(c - 'A' + 'a'));
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '#') o.push_back(c);
  }
  return o;
}
std::string sub(const std::string& path, const char* key) { return path.empty() ? std::string(key) : path + "." + key; }
std::string idx(const std::string& path, size_t i) { return path + "[" + std::to_string(i) + "]"; }

void unknownKeys(Rd& r, const JV& o, const std::string& path, std::initializer_list<const char*> allowed) {
  for (const auto& kv : o.o) {
    bool ok = false;
    for (const char* a : allowed) if (kv.first == a) ok = true;
    if (!ok) r.warn.push_back(sub(path, kv.first.c_str()) + ": unknown key, ignored");
  }
}
bool num(Rd& r, const JV& o, const std::string& path, const char* key, double lo, double hi, float& out) {
  const JV* v = o.get(key);
  if (!v) return true;
  if (v->t != JV::Num) return r.fail(sub(path, key), "must be a number");
  if (v->n < lo || v->n > hi) return r.fail(sub(path, key), fmt(v->n) + " is outside " + fmt(lo) + " .. " + fmt(hi));
  out = float(v->n);
  return true;
}
bool integer(Rd& r, const JV& o, const std::string& path, const char* key, int lo, int hi, int& out, bool* present = nullptr) {
  const JV* v = o.get(key);
  if (present) *present = v != nullptr;
  if (!v) return true;
  if (v->t != JV::Num || std::floor(v->n) != v->n) return r.fail(sub(path, key), "must be a whole number");
  if (v->n < lo || v->n > hi) return r.fail(sub(path, key), fmt(v->n) + " is outside " + fmt(lo) + " .. " + fmt(hi));
  out = int(v->n);
  return true;
}
bool boolean(Rd& r, const JV& o, const std::string& path, const char* key, bool& out) {
  const JV* v = o.get(key);
  if (!v) return true;
  if (v->t != JV::Bool) return r.fail(sub(path, key), "must be true or false");
  out = v->b;
  return true;
}
struct Choice { const char* name; int value; };
// A value from a fixed list of names. Returns false on error; `out` is untouched when the key is absent.
bool choice(Rd& r, const JV& o, const std::string& path, const char* key, std::initializer_list<Choice> list, int& out, bool* present = nullptr) {
  const JV* v = o.get(key);
  if (present) *present = v != nullptr;
  if (!v) return true;
  if (v->t != JV::Str) return r.fail(sub(path, key), "must be a name in double quotes");
  const std::string want = norm(v->s);
  for (const Choice& c : list) if (want == norm(c.name)) { out = c.value; return true; }
  std::string names;
  for (const Choice& c : list) {
    bool dup = false;                                // list each value once, by its first name
    for (const Choice& d : list) { if (&d == &c) break; if (d.value == c.value) dup = true; }
    if (!dup) names += std::string(names.empty() ? "" : ", ") + c.name;
  }
  return r.fail(sub(path, key), "\"" + v->s + "\" is not one of: " + names);
}

const std::initializer_list<Choice> kWaves = {{"saw", 0}, {"square", 1}, {"triangle", 2}, {"sine", 3}, {"sqr", 1}, {"tri", 2}, {"sin", 3}};
const std::initializer_list<Choice> kEngines = {{"subtractive", 0}, {"fm", 1}, {"pluck", 2}, {"vector", 3}, {"sub", 0}};
const std::initializer_list<Choice> kLfoTargets = {{"off", 0}, {"cutoff", 1}, {"pitch", 2}, {"volume", 3}, {"amp", 3}};
const std::initializer_list<Choice> kFxTypes = {{"off", 0}, {"overdrive", 1}, {"bitcrush", 2}, {"chorus", 3}, {"phaser", 4}, {"filter", 5}, {"compressor", 6}, {"distortion", 7}, {"fuzz", 8}, {"booster", 9}, {"tremolo", 10}, {"vibrato", 11}, {"flanger", 12}, {"rotary", 13}, {"wah", 14}, {"autowah", 15}, {"octaver", 16}, {"ringmod", 17}, {"delay", 18}, {"tapeecho", 19}, {"reverb", 20}, {"gate", 21},
    {"punch", 22}, {"crusher", 23}, {"slicer", 24}, {"gatedverb", 25}, {"boom", 26}, {"cloud", 27}, {"lofi", 28}, {"dubdelay", 29}, {"freqshift", 30}, {"resonator", 31}, {"shimmer", 32}};
const std::initializer_list<Choice> kScales = {{"minor", 0}, {"major", 1}, {"dorian", 2}, {"pentatonic", 3}};
const std::initializer_list<Choice> kRoots = {{"C", 0}, {"C#", 1}, {"D", 2}, {"D#", 3}, {"E", 4}, {"F", 5}, {"F#", 6}, {"G", 7}, {"G#", 8}, {"A", 9}, {"A#", 10}, {"B", 11},
                                              {"Db", 1}, {"Eb", 3}, {"Gb", 6}, {"Ab", 8}, {"Bb", 10}};
const std::initializer_list<Choice> kKits = {
    {"808 Classic", 0}, {"909 Punch", 1}, {"Lo-Fi Acoustic", 2}, {"FM Industrial", 3}, {"Jungle Break", 4},
    {"Hardcore Gabber", 5}, {"Warehouse Rumble", 6}, {"Neuro Techstep", 7}, {"EBM Electro", 8}, {"Glitch Breakcore", 9},
    {"808", 0}, {"909", 1}, {"lofi", 2}, {"industrial", 3}, {"jungle", 4}, {"gabber", 5}, {"rumble", 6}, {"neuro", 7}, {"ebm", 8}, {"glitch", 9}};
const std::initializer_list<Choice> kPresets = {{"Custom", 0}, {"Analog Bass", 1}, {"Acid 303", 2}, {"Reese DnB", 3}, {"FM Bell", 4},
                                                {"FM E-Piano", 5}, {"Super Pad", 6}, {"Pluck Lead", 7}};
const std::initializer_list<Choice> kPads = {{"kick", 0}, {"snare", 1}, {"clap", 2}, {"ch", 3}, {"oh", 4}, {"rim", 5}, {"tom", 6}, {"perc", 7},
                                             {"closedhat", 3}, {"openhat", 4}, {"hat", 3},
                                             {"pad1", 0}, {"pad2", 1}, {"pad3", 2}, {"pad4", 3}, {"pad5", 4}, {"pad6", 5}, {"pad7", 6}, {"pad8", 7}};
const std::initializer_list<Choice> kTracks = {{"bass", 0}, {"lead", 1}, {"syn3", 2}, {"syn4", 3}, {"synth1", 0}, {"synth2", 1}, {"synth3", 2}, {"synth4", 3}};
const char* const kWaveNames[] = {"saw", "square", "triangle", "sine"};
const char* const kEngineNames[] = {"subtractive", "fm", "pluck", "vector"};
const char* const kLfoNames[] = {"off", "cutoff", "pitch", "volume"};
const char* const kFxNames[] = {"off", "overdrive", "bitcrush", "chorus", "phaser", "filter", "compressor", "distortion", "fuzz", "booster", "tremolo", "vibrato", "flanger", "rotary", "wah", "autowah", "octaver", "ringmod", "delay", "tapeecho", "reverb", "gate",
                               "punch", "crusher", "slicer", "gatedverb", "boom", "cloud", "lofi", "dubdelay", "freqshift", "resonator", "shimmer"};
static_assert(sizeof(kFxNames) / sizeof(kFxNames[0]) == size_t(kNumFxTypes), "a name for every effect type");
const char* const kScaleNames[] = {"minor", "major", "dorian", "pentatonic"};
const char* const kRootNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const kKitNames[] = {"808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break",
                                 "Hardcore Gabber", "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore"};
const char* const kPresetNames[] = {"Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead"};
const char* const kPadNames[] = {"kick", "snare", "clap", "ch", "oh", "rim", "tom", "perc"};
const char* const kTrackNames[] = {"bass", "lead", "syn3", "syn4"};

int lookup(const std::string& name, std::initializer_list<Choice> list) {
  const std::string want = norm(name);
  for (const Choice& c : list) if (want == norm(c.name)) return c.value;
  return -1;
}

// ------------------------------------------------------------------ reading the three kinds
// The mixer strip of a track: level, drive, EQ, sends and the three effect slots.
bool readTrack(Rd& r, const JV& o, const std::string& path, TrackFx& fx) {
  if (o.t != JV::Obj) return r.fail(path, "must be an object { ... }");
  unknownKeys(r, o, path, {"level", "drive", "eqLow", "eqHigh", "delay", "reverb", "fx"});
  if (!num(r, o, path, "level", 0, 1, fx.level) || !num(r, o, path, "drive", 0, 1, fx.drive) ||
      !num(r, o, path, "eqLow", -12, 12, fx.eqLow) || !num(r, o, path, "eqHigh", -12, 12, fx.eqHigh) ||
      !num(r, o, path, "delay", 0, 1, fx.delaySend) || !num(r, o, path, "reverb", 0, 1, fx.revSend)) return false;
  if (const JV* a = o.get("fx")) {
    const std::string fp = sub(path, "fx");
    if (a->t != JV::Arr) return r.fail(fp, "must be a list [ ... ]");
    if (a->a.size() > size_t(kFxSlots)) return r.fail(fp, "a track has " + std::to_string(kFxSlots) + " effect slots, the file lists " + std::to_string(a->a.size()));
    for (int i = 0; i < kFxSlots; ++i) fx.slots[i] = FxSlot{};       // slots that are not listed are empty
    for (size_t i = 0; i < a->a.size(); ++i) {
      const JV& e = a->a[i];
      const std::string ep = idx(fp, i);
      if (e.t != JV::Obj) return r.fail(ep, "must be an object { ... }");
      unknownKeys(r, e, ep, {"type", "amount", "rate", "bypass", "voice", "p3", "p4", "mix", "keepSub", "sync"});
      int type = 0;
      bool has = false;
      if (!choice(r, e, ep, "type", kFxTypes, type, &has)) return false;
      if (!has) return r.fail(ep, "needs a \"type\"");
      FxSlot& sl = fx.slots[i];
      sl.type = FxType(type);
      if (!num(r, e, ep, "amount", 0, 1, sl.amount) || !num(r, e, ep, "rate", 0, 1, sl.rate) || !boolean(r, e, ep, "bypass", sl.bypass)) return false;
      float voice = 0.0f;
      if (!num(r, e, ep, "voice", 0, kMaxFxVoices - 1, voice) || !num(r, e, ep, "p3", 0, 1, sl.p3) || !num(r, e, ep, "p4", 0, 1, sl.p4)) return false;
      sl.voice = uint8_t(voice + 0.5f);
      bool keepSub = false, sync = false;
      if (!num(r, e, ep, "mix", 0, 1, sl.mix) || !boolean(r, e, ep, "keepSub", keepSub) || !boolean(r, e, ep, "sync", sync)) return false;
      sl.flags = uint8_t((keepSub ? kFxKeepSub : 0) | (sync ? kFxSync : 0));
    }
  }
  return true;
}

// The sound of one synth. Starts from a neutral sound (or a built-in preset), so a file says everything that matters.
bool readInstrument(Rd& r, const JV& o, const std::string& path, SynthParams& sp, TrackFx* fx) {
  if (o.t != JV::Obj) return r.fail(path, "must be an object { ... }");
  unknownKeys(r, o, path, {"kind", "name", "style", "author", "comment", "format", "preset", "engine", "osc1", "osc2", "detune", "cutoff", "res",
                           "attack", "decay", "sustain", "release", "level", "lfo", "fm", "pluckBright", "vector", "fold", "slideTime",
                           "warmth", "motion", "octave", "chord", "track"});
  SynthParams s;
  int v = 0;
  bool has = false;
  if (!choice(r, o, path, "preset", kPresets, v, &has)) return false;
  if (has) setInstrumentPreset(s, v);
  v = int(s.mode);   if (!choice(r, o, path, "engine", kEngines, v)) return false;  s.mode = SynthMode(v);
  v = int(s.wave1);  if (!choice(r, o, path, "osc1", kWaves, v)) return false;      s.wave1 = Wave(v);
  v = int(s.wave2);  if (!choice(r, o, path, "osc2", kWaves, v)) return false;      s.wave2 = Wave(v);
  if (!num(r, o, path, "detune", -50, 50, s.detune) || !num(r, o, path, "cutoff", 60, 12000, s.cutoff) || !num(r, o, path, "res", 0.5, 15, s.res) ||
      !num(r, o, path, "attack", 0.001, 1, s.attack) || !num(r, o, path, "decay", 0.02, 3, s.decay) || !num(r, o, path, "sustain", 0, 1, s.sustain) ||
      !num(r, o, path, "release", 0.01, 3, s.release) || !num(r, o, path, "level", 0, 1, s.level) ||
      !num(r, o, path, "pluckBright", 0, 1, s.pluckBright) || !num(r, o, path, "fold", 0, 1, s.fold) ||
      !num(r, o, path, "slideTime", 0.005, 0.5, s.slideTime) || !num(r, o, path, "warmth", 0, 1, s.feelWarmth) ||
      !num(r, o, path, "motion", 0, 1, s.feelMotion) || !boolean(r, o, path, "chord", s.chord)) return false;
  int oct = s.octaveShift;
  if (!integer(r, o, path, "octave", -2, 2, oct)) return false;
  s.octaveShift = int8_t(oct);
  if (const JV* l = o.get("lfo")) {
    const std::string lp = sub(path, "lfo");
    if (l->t != JV::Obj) return r.fail(lp, "must be an object { ... }");
    unknownKeys(r, *l, lp, {"target", "rate", "depth"});
    v = int(s.lfoTarget);
    if (!choice(r, *l, lp, "target", kLfoTargets, v)) return false;
    s.lfoTarget = LfoTarget(v);
    if (!num(r, *l, lp, "rate", 0.05, 20, s.lfoRate) || !num(r, *l, lp, "depth", 0, 1, s.lfoDepth)) return false;
  }
  if (const JV* f = o.get("fm")) {
    const std::string fp = sub(path, "fm");
    if (f->t != JV::Obj) return r.fail(fp, "must be an object { ... }");
    unknownKeys(r, *f, fp, {"ratio", "index", "decay", "feedback"});
    if (!num(r, *f, fp, "ratio", 0.5, 8, s.fmRatio) || !num(r, *f, fp, "index", 0, 8, s.fmIndex) ||
        !num(r, *f, fp, "decay", 0.01, 3, s.fmModDecay) || !num(r, *f, fp, "feedback", 0, 1, s.fmFeedback)) return false;
  }
  if (const JV* vc = o.get("vector")) {
    const std::string vp = sub(path, "vector");
    if (vc->t != JV::Obj) return r.fail(vp, "must be an object { ... }");
    unknownKeys(r, *vc, vp, {"x", "y"});
    if (!num(r, *vc, vp, "x", 0, 1, s.vecX) || !num(r, *vc, vp, "y", 0, 1, s.vecY)) return false;
  }
  if (const JV* t = o.get("track")) {
    TrackFx scratch = fx ? *fx : TrackFx{};
    if (!readTrack(r, *t, sub(path, "track"), scratch)) return false;
    if (fx) *fx = scratch;
  }
  sp = s;
  return true;
}

bool readDrumkit(Rd& r, const JV& o, const std::string& path, Project& p) {
  if (o.t != JV::Obj) return r.fail(path, "must be an object { ... }");
  unknownKeys(r, o, path, {"kind", "name", "style", "author", "comment", "format", "kit", "hardness", "pads", "track", "slicer"});
  int kit = int(p.drumEngine);
  bool hasKit = false;
  if (!choice(r, o, path, "kit", kKits, kit, &hasKit)) return false;
  if (hasKit) {
    if (kit != int(p.drumEngine)) setDrumEngine(p, kit);   // naming the kit that is already on changes nothing (an own break stays)
    if (kit != int(DrumEngine::JungleSlicer)) {      // a kit file describes all eight pads: the rest play the kit's own sound
      const Project def = makeDemoProject();
      for (int i = 0; i < kNumPads; ++i) { p.pads[i] = def.pads[i]; p.customDrums[i] = DrumSynthParams{}; }
    }
  }
  const bool jungle = p.drumEngine == DrumEngine::JungleSlicer;
  if (const JV* sl = o.get("slicer")) {
    const std::string sp = sub(path, "slicer");
    if (sl->t != JV::Obj) return r.fail(sp, "must be an object { ... }");
    unknownKeys(r, *sl, sp, {"pitch", "tight", "crunch"});
    if (!num(r, *sl, sp, "pitch", -12, 12, p.junglePitch) || !num(r, *sl, sp, "tight", 0.08, 1.5, p.jungleTight) ||
        !num(r, *sl, sp, "crunch", 0, 1, p.jungleCrunch)) return false;
    if (jungle) {
      for (auto& d : p.pads) { d.pitch = p.junglePitch; d.decay = p.jungleTight; }
      p.fx[0].drive = p.jungleCrunch;
    }
  }
  float hard = p.kitHardness;
  if (o.get("hardness")) {
    if (!num(r, o, path, "hardness", 0, 1, hard)) return false;
    p.kitHardness = hard;
    p.fx[0].drive = hard;                            // same as turning the Hardness knob; "track.drive" below can still override it
  }
  if (const JV* t = o.get("track")) if (!readTrack(r, *t, sub(path, "track"), p.fx[0])) return false;
  if (const JV* pads = o.get("pads")) {
    const std::string pp = sub(path, "pads");
    if (pads->t != JV::Obj) return r.fail(pp, "must be an object { \"kick\": { ... }, ... }");
    for (const auto& kv : pads->o) {
      const std::string dp = sub(pp, kv.first.c_str());
      const int pad = lookup(kv.first, kPads);
      if (pad < 0) return r.fail(dp, "is not a pad. Pads are: kick, snare, clap, ch, oh, rim, tom, perc");
      const JV& d = kv.second;
      if (d.t != JV::Obj) return r.fail(dp, "must be an object { ... }");
      unknownKeys(r, d, dp, {"vol", "pan", "pitch", "decay", "choke", "reverse", "sliceStart", "sliceEnd", "synth"});
      PadParams& pd = p.pads[pad];
      int choke = pd.choke;
      if (!num(r, d, dp, "vol", 0, 1, pd.vol) || !num(r, d, dp, "pan", -1, 1, pd.pan) || !num(r, d, dp, "pitch", -24, 24, pd.pitch) ||
          !num(r, d, dp, "decay", 0.02, 2, pd.decay) || !integer(r, d, dp, "choke", 0, 2, choke) || !boolean(r, d, dp, "reverse", pd.reverse) ||
          !num(r, d, dp, "sliceStart", 0, 0.99, pd.sliceStart) || !num(r, d, dp, "sliceEnd", 0.01, 1, pd.sliceEnd)) return false;
      pd.choke = uint8_t(choke);
      if (pd.sliceEnd <= pd.sliceStart) return r.fail(dp, "sliceEnd must be larger than sliceStart");
      if (const JV* sy = d.get("synth")) {
        const std::string yp = sub(dp, "synth");
        if (jungle) return r.fail(yp, "the break slicer has no synthesized pads");
        if (sy->t != JV::Obj) return r.fail(yp, "must be an object { ... }");
        unknownKeys(r, *sy, yp, {"pitch", "drop", "dropTime", "length", "tone", "noise", "noiseFreq", "fm", "fmRatio", "drive", "click"});
        DrumSynthParams ds;
        if (!num(r, *sy, yp, "pitch", 25, 1200, ds.baseFreq) || !num(r, *sy, yp, "drop", 0, 600, ds.pitchDrop) ||
            !num(r, *sy, yp, "dropTime", 0.005, 0.3, ds.pitchDecay) || !num(r, *sy, yp, "length", 0.05, 1.5, ds.length) ||
            !num(r, *sy, yp, "tone", 0, 1, ds.toneMix) || !num(r, *sy, yp, "noise", 0, 1, ds.noiseMix) ||
            !num(r, *sy, yp, "noiseFreq", 300, 10000, ds.noiseFreq) || !num(r, *sy, yp, "fm", 0, 8, ds.fmAmt) ||
            !num(r, *sy, yp, "fmRatio", 0.5, 8, ds.fmRatio) || !num(r, *sy, yp, "drive", 0, 1, ds.drive) || !num(r, *sy, yp, "click", 0, 1, ds.click)) return false;
        ds.enabled = true;
        p.customDrums[pad] = ds;
      }
    }
  }
  return true;
}

// The scale degree whose pitch is nearest to a MIDI note, for a given synth.
int degreeForMidi(const Project& p, int octaveShift, int midi) {
  int best = 0, bestDist = 1000;
  for (int d = -21; d <= 35; ++d) {
    const int dist = std::abs(degreeToMidi(p, octaveShift, d) - midi);
    if (dist < bestDist) { bestDist = dist; best = d; }
  }
  return best;
}

bool readGroove(Rd& r, const JV& o, const std::string& path, Project& p, const ContentTarget& tg) {
  if (o.t != JV::Obj) return r.fail(path, "must be an object { ... }");
  unknownKeys(r, o, path, {"kind", "name", "style", "author", "comment", "format", "bpm", "swing", "root", "scale", "octave", "bars", "drums", "synths"});
  if (tg.scene < 0 || tg.scene >= kNumScenes) return r.fail(path, "no such scene");
  // Read tempo and key even when they are not taken, so that mistakes in them are still reported.
  float bpm = p.bpm, swing = p.swing;
  int root = p.root, scale = int(p.scale), octave = p.octave;
  if (!num(r, o, path, "bpm", 40, 240, bpm) || !num(r, o, path, "swing", 0, 0.5, swing)) return false;
  if (const JV* rt = o.get("root")) {
    if (rt->t == JV::Num) { if (!integer(r, o, path, "root", 0, 11, root)) return false; }
    else if (!choice(r, o, path, "root", kRoots, root)) return false;
  }
  if (!choice(r, o, path, "scale", kScales, scale) || !integer(r, o, path, "octave", 1, 5, octave)) return false;
  if (tg.tempoAndKey) { p.bpm = bpm; p.swing = swing; p.root = root; p.scale = Scale(scale); p.octave = octave; }

  Scene& sc = p.scenes[tg.scene];
  int bars = sc.bars;
  if (!integer(r, o, path, "bars", 1, kMaxBars, bars)) return false;
  sc.bars = bars;
  const int steps = bars * kStepsPerBar;

  if (const JV* d = o.get("drums")) {
    const std::string dp = sub(path, "drums");
    if (d->t != JV::Obj) return r.fail(dp, "must be an object { ... }");
    unknownKeys(r, *d, dp, {"kit", "drumkit", "steps"});
    if (d->get("kit") && d->get("drumkit")) return r.fail(dp, "give either \"kit\" (a name) or \"drumkit\" (a full kit), not both");
    if (d->get("kit")) {
      int kit = 0;
      if (!choice(r, *d, dp, "kit", kKits, kit)) return false;
      if (kit != int(p.drumEngine)) setDrumEngine(p, kit);   // only the kit changes: pads keep what the user set
    }
    if (const JV* dk = d->get("drumkit")) if (!readDrumkit(r, *dk, sub(dp, "drumkit"), p)) return false;
    if (const JV* st = d->get("steps")) {
      const std::string sp = sub(dp, "steps");
      if (st->t != JV::Obj) return r.fail(sp, "must be an object { \"kick\": \"x...x...\", ... }");
      sc.drums = DrumPattern{};                      // pads that are not listed stay silent in this scene
      for (const auto& kv : st->o) {
        const std::string rp = sub(sp, kv.first.c_str());
        const int pad = lookup(kv.first, kPads);
        if (pad < 0) return r.fail(rp, "is not a pad. Pads are: kick, snare, clap, ch, oh, rim, tom, perc");
        if (kv.second.t != JV::Str) return r.fail(rp, "must be a text of steps like \"x...x...x...x...\"");
        std::vector<uint8_t> row;
        for (char c : kv.second.s) {
          if (c == ' ' || c == '|') continue;        // for readability only
          if (c == '.' || c == '-') row.push_back(0);
          else if (c == 'x') row.push_back(1);
          else if (c == 'X') row.push_back(2);
          else if (c == '2' || c == '3' || c == '4') row.push_back(uint8_t(c - '2' + 3));
          else return r.fail(rp, std::string("'") + c + "' is not a step. Use . off, x hit, X accent, 2 3 4 for a roll");
        }
        if (row.empty()) continue;
        if (int(row.size()) > steps) return r.fail(rp, std::to_string(row.size()) + " steps, but " + std::to_string(bars) + " bar(s) have only " + std::to_string(steps));
        if (row.size() % size_t(kStepsPerBar) != 0) return r.fail(rp, std::to_string(row.size()) + " steps is not a whole number of bars (16 steps each)");
        for (int i = 0; i < steps; ++i) sc.drums.steps[pad][i] = row[size_t(i) % row.size()];   // a shorter row repeats
      }
    }
  }

  if (const JV* sy = o.get("synths")) {
    const std::string yp = sub(path, "synths");
    if (sy->t != JV::Arr) return r.fail(yp, "must be a list [ ... ]");
    bool used[kNumSynths] = {};
    for (size_t i = 0; i < sy->a.size(); ++i) {
      const JV& e = sy->a[i];
      const std::string ep = idx(yp, i);
      if (e.t != JV::Obj) return r.fail(ep, "must be an object { ... }");
      unknownKeys(r, e, ep, {"track", "preset", "instrument", "notes"});
      int track = -1;
      bool has = false;
      if (const JV* tv = e.get("track")) {
        if (tv->t == JV::Num) { if (!integer(r, e, ep, "track", 1, kNumSynths, track, &has)) return false; --track; }
        else if (!choice(r, e, ep, "track", kTracks, track, &has)) return false;
      }
      if (!has) return r.fail(ep, "needs a \"track\": bass, lead, syn3 or syn4");
      if (used[track]) return r.fail(sub(ep, "track"), "this track is listed twice");
      used[track] = true;
      if (track >= p.activeSynths) p.activeSynths = track + 1;
      if (e.get("preset") && e.get("instrument")) return r.fail(ep, "give either \"preset\" (a built-in name) or \"instrument\" (a full sound), not both");
      if (e.get("preset")) {
        int pr = 0;
        if (!choice(r, e, ep, "preset", kPresets, pr)) return false;
        setInstrumentPreset(p.synths[track], pr);
      }
      if (const JV* in = e.get("instrument")) if (!readInstrument(r, *in, sub(ep, "instrument"), p.synths[track], &p.fx[track + 1])) return false;
      if (const JV* ns = e.get("notes")) {
        const std::string np = sub(ep, "notes");
        if (ns->t != JV::Arr) return r.fail(np, "must be a list [ ... ]");
        if (ns->a.size() > size_t(kMaxNotes)) return r.fail(np, std::to_string(ns->a.size()) + " notes, a pattern holds at most " + std::to_string(kMaxNotes));
        SynthPattern& pat = sc.synth[track];
        pat = SynthPattern{};
        for (size_t k = 0; k < ns->a.size(); ++k) {
          const JV& n = ns->a[k];
          const std::string kp = idx(np, k);
          if (n.t != JV::Obj) return r.fail(kp, "must be an object { ... }");
          unknownKeys(r, n, kp, {"step", "degree", "midi", "len", "accent", "slide"});
          int step = 0, degree = 0, midi = 0, len = 1;
          bool hasStep = false, hasDeg = false, hasMidi = false, accent = false, slide = false;
          if (!integer(r, n, kp, "step", 0, steps - 1, step, &hasStep) || !integer(r, n, kp, "degree", -21, 35, degree, &hasDeg) ||
              !integer(r, n, kp, "midi", 0, 127, midi, &hasMidi) || !integer(r, n, kp, "len", 1, steps, len) ||
              !boolean(r, n, kp, "accent", accent) || !boolean(r, n, kp, "slide", slide)) return false;
          if (!hasStep) return r.fail(kp, "needs a \"step\"");
          if (hasDeg == hasMidi) return r.fail(kp, "needs either \"degree\" (steps of the scale from the root) or \"midi\" (a note number)");
          if (hasMidi) {
            const int d = degreeForMidi(p, p.synths[track].octaveShift, midi);
            if (degreeToMidi(p, p.synths[track].octaveShift, d) != midi)
              r.warn.push_back(sub(kp, "midi") + ": " + std::to_string(midi) + " is not in the scale, moved to " +
                               std::to_string(degreeToMidi(p, p.synths[track].octaveShift, d)));
            degree = d;
          }
          if (step + len > steps) len = steps - step;
          if (len > 255) len = 255;
          for (int q = 0; q < pat.count; ++q)
            if (pat.notes[q].step == step && pat.notes[q].degree == degree) return r.fail(kp, "the same note is listed twice at this step");
          Note& nt = pat.notes[pat.count++];
          nt.step = uint16_t(step); nt.degree = int8_t(degree); nt.len = uint8_t(len); nt.vel = uint8_t(accent ? 2 : 1);
          nt.flags = slide ? kNoteSlide : 0;
        }
      }
    }
  }
  return true;
}

bool header(Rd& r, const JV& root, ContentInfo* info, std::string& kind) {
  if (root.t != JV::Obj) return r.fail("", "must be one JSON object { ... }");
  const JV* k = root.get("kind");
  if (!k || k->t != JV::Str) return r.fail("kind", "is missing. It must be \"instrument\", \"drumkit\" or \"groove\"");
  kind = norm(k->s);
  if (kind != "instrument" && kind != "drumkit" && kind != "groove") return r.fail("kind", "\"" + k->s + "\" is not one of: instrument, drumkit, groove");
  const JV* n = root.get("name");
  if (!n || n->t != JV::Str || n->s.empty()) return r.fail("name", "is missing. Every file needs a name");
  if (n->s.size() > 48) return r.fail("name", "is longer than 48 characters");
  if (const JV* f = root.get("format")) if (f->t != JV::Num || f->n != 1) return r.fail("format", "only format 1 exists");
  const JV* st = root.get("style");
  if (st && st->t != JV::Str) return r.fail("style", "must be a text");
  if (info) { info->kind = kind; info->name = n->s; info->style = st ? st->s : std::string(); }
  return true;
}

// ------------------------------------------------------------------ writing
struct W {
  std::string s;
  int depth = 0;
  bool first = true;
  void nl() { s += "\n"; s.append(size_t(depth) * 2, ' '); }
  void sep() { if (!first) s += ","; first = false; nl(); }
  static std::string quote(const std::string& t) {
    std::string o = "\"";
    for (char c : t) {
      if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back(c); }
      else if (c == '\n') o += "\\n";
      else if (static_cast<unsigned char>(c) < 0x20) o += " ";
      else o.push_back(c);
    }
    return o + "\"";
  }
  static std::string number(double v) {
    char b[40];
    std::snprintf(b, sizeof(b), "%.6g", v);
    return b;
  }
  void key(const char* k) { sep(); s += quote(k) + ": "; }
  void str(const char* k, const std::string& v) { key(k); s += quote(v); }
  void num(const char* k, double v) { key(k); s += number(v); }
  void boolean(const char* k, bool v) { key(k); s += v ? "true" : "false"; }
  void open(const char* k, char br = '{') { if (k) key(k); else sep(); s.push_back(br); ++depth; first = true; }
  void close(char br = '}') { --depth; if (!first) nl(); s.push_back(br); first = false; }
};

void writeHeader(W& w, const char* kind, const std::string& name, const std::string& style) {
  w.str("kind", kind);
  w.num("format", 1);
  w.str("name", name.empty() ? std::string("Untitled") : name.substr(0, 48));
  if (!style.empty()) w.str("style", style);
}
void writeTrack(W& w, const TrackFx& f) {
  w.open("track");
  w.num("level", f.level); w.num("drive", f.drive); w.num("eqLow", f.eqLow); w.num("eqHigh", f.eqHigh);
  w.num("delay", f.delaySend); w.num("reverb", f.revSend);
  int last = -1;
  for (int i = 0; i < kFxSlots; ++i) if (f.slots[i].type != FxType::Off) last = i;
  w.open("fx", '[');
  for (int i = 0; i <= last; ++i) {
    const FxSlot& sl = f.slots[i];
    w.sep();
    w.s += "{ \"type\": " + W::quote(kFxNames[int(sl.type) < kNumFxTypes ? int(sl.type) : 0]) + ", \"amount\": " + W::number(sl.amount) +
           ", \"rate\": " + W::number(sl.rate) +
           (sl.voice ? ", \"voice\": " + std::to_string(int(sl.voice)) : std::string()) +
           (sl.p3 != 0.5f ? ", \"p3\": " + W::number(sl.p3) : std::string()) + (sl.p4 != 0.5f ? ", \"p4\": " + W::number(sl.p4) : std::string()) +
           (sl.mix != 1.0f ? ", \"mix\": " + W::number(sl.mix) : std::string()) +
           ((sl.flags & kFxKeepSub) ? ", \"keepSub\": true" : "") + ((sl.flags & kFxSync) ? ", \"sync\": true" : "") +
           (sl.bypass ? ", \"bypass\": true }" : " }");
  }
  w.close(']');
  w.close();
}
void writeInstrumentBody(W& w, const SynthParams& s, const TrackFx* fx) {
  w.str("engine", kEngineNames[int(s.mode) <= 3 ? int(s.mode) : 0]);
  w.str("osc1", kWaveNames[int(s.wave1) & 3]); w.str("osc2", kWaveNames[int(s.wave2) & 3]);
  w.num("detune", s.detune); w.num("cutoff", s.cutoff); w.num("res", s.res);
  w.num("attack", s.attack); w.num("decay", s.decay); w.num("sustain", s.sustain); w.num("release", s.release);
  w.num("level", s.level);
  w.open("lfo"); w.str("target", kLfoNames[int(s.lfoTarget) & 3]); w.num("rate", s.lfoRate); w.num("depth", s.lfoDepth); w.close();
  w.open("fm"); w.num("ratio", s.fmRatio); w.num("index", s.fmIndex); w.num("decay", s.fmModDecay); w.num("feedback", s.fmFeedback); w.close();
  w.num("pluckBright", s.pluckBright);
  w.open("vector"); w.num("x", s.vecX); w.num("y", s.vecY); w.close();
  w.num("fold", s.fold); w.num("slideTime", s.slideTime); w.num("warmth", s.feelWarmth); w.num("motion", s.feelMotion);
  w.num("octave", s.octaveShift); w.boolean("chord", s.chord);
  if (fx) writeTrack(w, *fx);
}
void writeDrumkitBody(W& w, const Project& p) {
  const int kit = int(p.drumEngine) <= 9 ? int(p.drumEngine) : 0;
  const bool jungle = p.drumEngine == DrumEngine::JungleSlicer;
  w.str("kit", kKitNames[kit]);
  if (jungle) { w.open("slicer"); w.num("pitch", p.junglePitch); w.num("tight", p.jungleTight); w.num("crunch", p.jungleCrunch); w.close(); }
  writeTrack(w, p.fx[0]);
  if (!jungle) w.num("hardness", p.kitHardness);
  w.open("pads");
  for (int i = 0; i < kNumPads; ++i) {
    const PadParams& d = p.pads[i];
    w.open(kPadNames[i]);
    w.num("vol", d.vol); w.num("pan", d.pan); w.num("pitch", d.pitch); w.num("decay", d.decay); w.num("choke", d.choke > 2 ? 0 : d.choke);
    if (jungle) { w.num("sliceStart", d.sliceStart); w.num("sliceEnd", d.sliceEnd); w.boolean("reverse", d.reverse); }
    const DrumSynthParams& ds = p.customDrums[i];
    if (!jungle && ds.enabled) {
      w.open("synth");
      w.num("pitch", ds.baseFreq); w.num("drop", ds.pitchDrop); w.num("dropTime", ds.pitchDecay); w.num("length", ds.length);
      w.num("tone", ds.toneMix); w.num("noise", ds.noiseMix); w.num("noiseFreq", ds.noiseFreq); w.num("fm", ds.fmAmt);
      w.num("fmRatio", ds.fmRatio); w.num("drive", ds.drive); w.num("click", ds.click);
      w.close();
    }
    w.close();
  }
  w.close();
}

}  // namespace

int midiToDegree(const Project& p, int synth, int midi) {
  return degreeForMidi(p, p.synths[synth < 0 || synth >= kNumSynths ? 0 : synth].octaveShift, midi);
}

// ------------------------------------------------------------------ public
bool applyContent(const std::string& json, Project& p, const ContentTarget& target, std::string& error, ContentInfo* info) {
  JV root;
  Rd r;
  if (!parseJson(json, root, error)) { error = "not valid JSON, " + error; return false; }
  std::string kind;
  auto work = std::make_unique<Project>(p);          // all or nothing: the caller's project changes only on success
  bool ok = header(r, root, info, kind);
  if (ok && kind == "instrument") {
    if (target.synth < 0 || target.synth >= kNumSynths) ok = r.fail("", "no such synth track");
    else ok = readInstrument(r, root, "", work->synths[target.synth], &work->fx[target.synth + 1]);
  } else if (ok && kind == "drumkit") {
    ok = readDrumkit(r, root, "", *work);
  } else if (ok) {
    ok = readGroove(r, root, "", *work, target);
  }
  if (info) info->warnings = r.warn;
  if (!ok) { error = r.err.empty() ? std::string("could not be read") : r.err; return false; }
  p = *work;
  error.clear();
  return true;
}

bool checkContent(const std::string& json, std::string& error, ContentInfo* info) {
  auto scratch = std::make_unique<Project>(makeDemoProject());
  scratch->activeSynths = kNumSynths;
  return applyContent(json, *scratch, ContentTarget{}, error, info);
}

std::string instrumentToJson(const Project& p, int synth, const std::string& name, const std::string& style) {
  if (synth < 0 || synth >= kNumSynths) synth = 0;
  W w;
  w.open(nullptr);
  w.first = true; w.s = "{";
  writeHeader(w, "instrument", name, style);
  writeInstrumentBody(w, p.synths[synth], &p.fx[synth + 1]);
  w.close();
  return w.s + "\n";
}

std::string drumkitToJson(const Project& p, const std::string& name, const std::string& style) {
  W w;
  w.open(nullptr);
  w.first = true; w.s = "{";
  writeHeader(w, "drumkit", name, style);
  writeDrumkitBody(w, p);
  w.close();
  return w.s + "\n";
}

std::string grooveToJson(const Project& p, int scene, const std::string& name, const std::string& style, bool withSounds) {
  if (scene < 0 || scene >= kNumScenes) scene = 0;
  const Scene& sc = p.scenes[scene];
  const int bars = sc.bars < 1 ? 1 : (sc.bars > kMaxBars ? kMaxBars : sc.bars);
  const int steps = bars * kStepsPerBar;
  W w;
  w.open(nullptr);
  w.first = true; w.s = "{";
  writeHeader(w, "groove", name, style);
  w.num("bpm", p.bpm); w.num("swing", p.swing);
  w.str("root", kRootNames[((p.root % 12) + 12) % 12]); w.str("scale", kScaleNames[int(p.scale) & 3]); w.num("octave", p.octave);
  w.num("bars", bars);
  w.open("drums");
  if (withSounds) { w.open("drumkit"); writeDrumkitBody(w, p); w.close(); }
  w.open("steps");
  for (int pad = 0; pad < kNumPads; ++pad) {
    bool any = false;
    std::string row;
    for (int i = 0; i < steps; ++i) {
      const int v = sc.drums.steps[pad][i];
      any = any || v != 0;
      if (i > 0 && i % kStepsPerBar == 0) row.push_back('|');
      row.push_back(v == 0 ? '.' : v == 1 ? 'x' : v == 2 ? 'X' : char('2' + (v - 3)));
    }
    if (any) w.str(kPadNames[pad], row);
  }
  w.close();
  w.close();
  w.open("synths", '[');
  const int nSyn = p.activeSynths < 1 ? 1 : (p.activeSynths > kNumSynths ? kNumSynths : p.activeSynths);
  for (int y = 0; y < nSyn; ++y) {
    const SynthPattern& pat = sc.synth[y];
    if (pat.count <= 0 && !withSounds) continue;
    w.open(nullptr);
    w.str("track", kTrackNames[y]);
    if (withSounds) { w.open("instrument"); writeInstrumentBody(w, p.synths[y], &p.fx[y + 1]); w.close(); }
    w.open("notes", '[');
    for (int i = 0; i < pat.count && i < kMaxNotes; ++i) {
      const Note& n = pat.notes[i];
      if (n.step >= steps) continue;
      w.sep();
      w.s += "{ \"step\": " + std::to_string(n.step) + ", \"degree\": " + std::to_string(int(n.degree)) + ", \"len\": " + std::to_string(int(n.len)) +
             (n.vel >= 2 ? ", \"accent\": true" : "") + ((n.flags & kNoteSlide) ? ", \"slide\": true" : "") + " }";
    }
    w.close(']');
    w.close();
  }
  w.close(']');
  w.close();
  return w.s + "\n";
}

}  // namespace ld
