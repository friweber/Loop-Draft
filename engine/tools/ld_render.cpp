// Renders the demo project with the native engine: master WAV, one stem per track, and MIDI.
// Usage: ld_render <output-directory>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "loopdraft/engine.h"

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";
  const ld::Project p = ld::makeDemoProject();
  ld::RenderOptions o;
  o.songMode = true;

  const auto t0 = std::chrono::steady_clock::now();
  const ld::RenderResult master = ld::renderOffline(p, o);
  const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  const double secs = double(master.l.size()) / master.sampleRate;
  float pk = 0;
  for (size_t i = 0; i < master.l.size(); ++i) pk = std::fmax(pk, std::fmax(std::fabs(master.l[i]), std::fabs(master.r[i])));
  if (!ld::writeWav16((dir + "/loopdraft_song.wav").c_str(), master.l.data(), master.r.data(), master.l.size(), master.sampleRate)) {
    std::fprintf(stderr, "cannot write to %s\n", dir.c_str());
    return 1;
  }
  static const char* names[] = {"drums", "bass", "lead"};
  for (int t = 0; t < ld::kNumTracks; ++t) {
    o.soloTrack = t;
    const ld::RenderResult stem = ld::renderOffline(p, o);
    ld::writeWav16((dir + "/loopdraft_stem_" + names[t] + ".wav").c_str(), stem.l.data(), stem.r.data(), stem.l.size(), stem.sampleRate);
  }
  const std::vector<uint8_t> mid = ld::buildMidi(p, true, 0);
  if (FILE* f = std::fopen((dir + "/loopdraft_song.mid").c_str(), "wb")) { std::fwrite(mid.data(), 1, mid.size(), f); std::fclose(f); }
  std::printf("song: %.2f s of audio rendered in %.3f s (%.0fx real time on this machine), peak %.3f\n", secs, took, secs / took, pk);
  return 0;
}
