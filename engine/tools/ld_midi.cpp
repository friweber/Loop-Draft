// ld_midi - writes grooves of the content format as Standard MIDI Files.
//   ld_midi <output folder> <groove.json>...
// One .mid per groove, named like the .json. Prints one line per file: file, tempo, bars, key, name.
// The MIDI holds the patterns only (tempo track, drums on channel 10, one track per synth on channels 1 to 4);
// the sounds stay in the .json.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "loopdraft/content.h"
#include "loopdraft/engine.h"
#include "loopdraft/session.h"

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: ld_midi <output folder> <groove.json>...\n"); return 2; }
  const std::string out = argv[1];
  static const char* const keys[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  int bad = 0;
  for (int i = 2; i < argc; ++i) {
    std::ifstream in(argv[i], std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    auto p = std::make_unique<ld::Project>(ld::makeDemoProject());
    for (auto& sc : p->scenes) sc = ld::Scene();
    p->activeSynths = ld::kNumSynths;
    std::string err;
    ld::ContentInfo info;
    if (!in || !ld::applyContent(text.str(), *p, ld::ContentTarget{}, err, &info) || info.kind != "groove") {
      std::fprintf(stderr, "%s: %s\n", argv[i], err.empty() ? "not a groove" : err.c_str());
      ++bad;
      continue;
    }
    std::string base = argv[i];
    const size_t slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const size_t dot = base.rfind('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    const std::vector<uint8_t> midi = ld::buildMidi(*p, false, 0);
    std::ofstream f(out + "/" + base + ".mid", std::ios::binary);
    f.write(reinterpret_cast<const char*>(midi.data()), std::streamsize(midi.size()));
    if (!f) { std::fprintf(stderr, "%s: cannot write\n", base.c_str()); ++bad; continue; }
    std::printf("%s.mid\t%d\t%d\t%s\t%s\n", base.c_str(), int(p->bpm), int(p->scenes[0].bars), keys[((p->root % 12) + 12) % 12], info.name.c_str());
  }
  return bad ? 1 : 0;
}
