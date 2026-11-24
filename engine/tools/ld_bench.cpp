// Measures what the engine costs per audio callback, the way the phone calls it (small interleaved blocks).
// Usage: ld_bench [frames-per-callback]     prints mean / worst time per callback as a share of real time
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "loopdraft/engine.h"

using namespace ld;

static Project heavy(SynthMode mode, bool fx) {
  Project p = makeStartProject();
  p.activeSynths = 4;
  for (int s = 0; s < kNumSynths; ++s) {
    SynthParams& sp = p.synths[s];
    sp.mode = mode; sp.chord = true; sp.sustain = 0.8f; sp.release = 0.6f; sp.decay = 0.8f; sp.fold = 0.4f; sp.lfoDepth = 0.5f;
    SynthPattern& pt = p.scenes[0].synth[s];
    pt.count = 0;
    for (int st = 0; st < 16 && pt.count < kMaxNotes; ++st) { Note n; n.step = uint16_t(st); n.degree = int8_t((st * 3 + s) % 9); n.len = 2; pt.notes[pt.count++] = n; }
  }
  for (int t = 0; t < kNumTracks; ++t) {
    p.fx[t].delaySend = 0.3f; p.fx[t].revSend = 0.3f; p.fx[t].drive = 0.3f;
    if (fx) {
      p.fx[t].slots[0].type = FxType::Chorus; p.fx[t].slots[1].type = FxType::Phaser; p.fx[t].slots[2].type = FxType::Compressor;
      for (auto& sl : p.fx[t].slots) { sl.bypass = false; sl.amount = 0.6f; sl.rate = 0.5f; }
    }
  }
  return p;
}

static void run(const char* name, const Project& p, int frames, double seconds) {
  const double sr = 48000.0;
  const int blocks = int(seconds * sr / frames);
  // The engine is deterministic, so the same callback costs the same in every pass. Taking the fastest of several
  // passes per callback removes what the machine did in between, and leaves what the engine itself needs.
  std::vector<double> t(size_t(blocks), 1e9);
  std::vector<float> out(size_t(frames) * 2);
  for (int pass = 0; pass < 5; ++pass) {
    Engine e(sr);
    e.setProject(p);
    e.play(0);
    for (int b = 0; b < blocks; ++b) {
      const auto t0 = std::chrono::steady_clock::now();
      e.renderInterleaved(out.data(), frames);
      t[size_t(b)] = std::min(t[size_t(b)], std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
    }
  }
  const double budget = frames / sr;
  double sum = 0; for (double x : t) sum += x;
  const size_t worstAt = size_t(std::max_element(t.begin(), t.end()) - t.begin());
  std::sort(t.begin(), t.end());
  std::printf("%-34s mean %5.1f%%   99%% %5.1f%%   worst %5.1f%% (callback %zu)   of real time, %d frames\n", name,
              100.0 * sum / double(t.size()) / budget, 100.0 * t[size_t(double(t.size()) * 0.99)] / budget, 100.0 * t.back() / budget, worstAt, frames);
}

int main(int argc, char** argv) {
  const int frames = argc > 1 ? std::max(16, std::atoi(argv[1])) : 192;
  const double secs = argc > 2 ? std::atof(argv[2]) : 20.0;
  const int only = argc > 3 ? std::atoi(argv[3]) : -1;      // one scenario, for profiling
  struct { const char* name; Project p; } sc[] = {
    {"start project", makeStartProject()},
    {"demo project", makeDemoProject()},
    {"4 synths chords, subtractive", heavy(SynthMode::Subtractive, false)},
    {"4 synths chords, FM", heavy(SynthMode::Fm, false)},
    {"4 synths chords, pluck", heavy(SynthMode::Pluck, false)},
    {"4 synths chords, vector", heavy(SynthMode::Vector, false)},
    {"4 synths chords, vector + 15 FX", heavy(SynthMode::Vector, true)},
  };
  int i = 0;
  for (auto& s : sc) { if (only < 0 || only == i) run(s.name, s.p, frames, secs); ++i; }
  return 0;
}
