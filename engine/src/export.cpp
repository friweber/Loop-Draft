#include <cmath>
#include <algorithm>
#include <string>
#include <vector>

#include "loopdraft/engine.h"

namespace ld {

namespace {
// Scenes to play, in order: the chain in song mode, otherwise one scene.
std::vector<int> playOrder(const Project& p, bool songMode, int scene) {
  std::vector<int> o;
  if (songMode) {
    const int n = std::clamp(p.chainLen, 1, kMaxChain);
    for (int i = 0; i < n; ++i) o.push_back(std::clamp(p.chain[i], 0, kNumScenes - 1));
  } else {
    o.push_back(std::clamp(scene, 0, kNumScenes - 1));
  }
  return o;
}
}  // namespace

RenderResult renderOffline(const Project& project, const RenderOptions& opt) {
  Project p = project;
  if (opt.soloTrack >= 0 && opt.soloTrack < kNumTracks)
    for (int t = 0; t < kNumTracks; ++t) p.fx[t].mute = (t != opt.soloTrack);

  Engine e(opt.sampleRate);
  std::unique_ptr<Sample> brk;
  if (p.kit == 1) brk = makeJungleBreakSample(opt.sampleRate);      // same kit as the app plays
  for (int i = 0; i < kNumPads; ++i) {
    if (opt.samples[i]) e.setPadSample(i, std::make_unique<Sample>(*opt.samples[i]));
    else if (brk) e.setPadSample(i, std::make_unique<Sample>(*brk));
  }
  e.setProject(p);
  e.setSongMode(opt.songMode);
  e.play(opt.scene);

  int steps = 0;
  for (int sc : playOrder(p, opt.songMode, opt.scene)) steps += sceneSteps(p.scenes[sc]);
  const double stepLen = opt.sampleRate * 60.0 / double(p.bpm < 20 ? 20 : p.bpm) / 4.0;
  const int64_t body = std::llround(double(steps) * stepLen);
  const int64_t total = body + int64_t(opt.tailSeconds * opt.sampleRate);

  RenderResult res;
  res.sampleRate = opt.sampleRate;
  res.l.assign(size_t(total), 0.0f); res.r.assign(size_t(total), 0.0f);
  const int bs = std::max(1, opt.blockSize);
  int64_t pos = 0;
  bool stopped = false;
  while (pos < total) {
    if (!stopped && pos >= body) { e.stop(); stopped = true; }   // play the material once, then let the tail ring
    int64_t n = std::min<int64_t>(bs, total - pos);
    if (!stopped) n = std::min<int64_t>(n, body - pos);
    e.render(res.l.data() + pos, res.r.data() + pos, int(n));
    pos += n;
  }
  return res;
}

// ------------------------------------------------------------------ Standard MIDI File

namespace {
struct Ev { int tick; int on; int note; int vel; };
void vlq(std::vector<uint8_t>& b, uint32_t n) {
  uint8_t tmp[5]; int k = 0;
  tmp[k++] = n & 0x7f;
  while ((n >>= 7) > 0) tmp[k++] = uint8_t((n & 0x7f) | 0x80);
  while (k > 0) b.push_back(tmp[--k]);
}
void chunk(std::vector<uint8_t>& out, const char* id, const std::vector<uint8_t>& body) {
  out.insert(out.end(), id, id + 4);
  const uint32_t n = uint32_t(body.size());
  out.push_back(uint8_t(n >> 24)); out.push_back(uint8_t(n >> 16)); out.push_back(uint8_t(n >> 8)); out.push_back(uint8_t(n));
  out.insert(out.end(), body.begin(), body.end());
}
std::vector<uint8_t> track(std::vector<Ev> evs, int ch, const char* name) {
  std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : a.on < b.on; });
  std::vector<uint8_t> b = {0, 0xFF, 0x03};
  const std::string nm(name);
  b.push_back(uint8_t(nm.size())); b.insert(b.end(), nm.begin(), nm.end());
  int last = 0;
  for (const Ev& e : evs) {
    vlq(b, uint32_t(e.tick - last));
    b.push_back(uint8_t((e.on ? 0x90 : 0x80) | ch)); b.push_back(uint8_t(e.note)); b.push_back(uint8_t(e.vel));
    last = e.tick;
  }
  const uint8_t end[] = {0, 0xFF, 0x2F, 0};
  b.insert(b.end(), end, end + 4);
  return b;
}
}  // namespace

std::vector<uint8_t> buildMidi(const Project& p, bool songMode, int scene) {
  constexpr int TPQ = 480, ST = TPQ / 4;
  const auto tickOf = [&](int base, int step) { return int(std::lround((double(base + step) + double(swingOffset(p.swing, step))) * ST)); };

  std::vector<Ev> drums, syn[kNumSynths];
  int base = 0;
  for (int si : playOrder(p, songMode, scene)) {
    const Scene& sc = p.scenes[si];
    const int steps = sceneSteps(sc);
    for (int pad = 0; pad < kNumPads; ++pad)
      for (int s = 0; s < steps; ++s)
        if (const int v = sc.drums.steps[pad][s]) {
          const int t = tickOf(base, s), hits = rollHits(v), gap = ST / hits;     // rolls become separate notes
          for (int h = 0; h < hits; ++h) {
            drums.push_back({t + h * gap, 1, p.pads[pad].gmNote, velMidi(v)});
            drums.push_back({t + h * gap + (gap < 36 ? gap - 6 : 30), 0, p.pads[pad].gmNote, 0});
          }
        }
    const int numSyn = std::clamp(p.activeSynths, 1, kNumSynths);
    for (int y = 0; y < numSyn; ++y) {
      const SynthPattern& pat = sc.synth[y];
      for (int k = 0; k < std::min(pat.count, kMaxNotes); ++k) {
        const Note& nt = pat.notes[k];
        if (nt.step >= steps) continue;
        const int t0 = tickOf(base, nt.step);
        const int endStep = std::min<int>(nt.step + nt.len, steps);
        const int t1 = std::max(t0 + 10, tickOf(base, endStep) - 12);
        int m[3];
        const int n = chordMidis(p, p.synths[y], nt.degree, m);
        for (int i = 0; i < n; ++i) { syn[y].push_back({t0, 1, m[i], velMidi(nt.vel)}); syn[y].push_back({t1, 0, m[i], 0}); }
      }
    }
    base += steps;
  }

  const uint32_t us = uint32_t(std::lround(60000000.0 / double(p.bpm < 20 ? 20 : p.bpm)));
  const std::vector<uint8_t> tempo = {0, 0xFF, 0x51, 3, uint8_t(us >> 16), uint8_t(us >> 8), uint8_t(us),
                                      0, 0xFF, 0x58, 4, 4, 2, 24, 8, 0, 0xFF, 0x2F, 0};
  std::vector<uint8_t> out;
  const int numSyn = std::clamp(p.activeSynths, 1, kNumSynths);
  const int ntrk = 2 + numSyn;
  chunk(out, "MThd", {0, 1, uint8_t(ntrk >> 8), uint8_t(ntrk), uint8_t(TPQ >> 8), uint8_t(TPQ & 255)});
  chunk(out, "MTrk", tempo);
  chunk(out, "MTrk", track(drums, 9, "Drums"));
  static const char* names[] = {"Bass", "Lead", "Synth 3", "Synth 4"};
  for (int y = 0; y < numSyn; ++y) chunk(out, "MTrk", track(syn[y], y, names[y & 3]));
  return out;
}

}  // namespace ld
