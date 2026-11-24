// Checks preset and groove files, and renders them to WAV so they can be listened to without the app.
//   ld_content check <file.json>...          exit code 1 if any file has an error
//   ld_content render <file.json> <out.wav>  what the file sounds like on top of the start project
//   ld_content midi <file.json> <out.mid>    a groove as a MIDI file
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "loopdraft/content.h"
#include "loopdraft/engine.h"

static bool readFile(const char* path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::ostringstream ss;
  ss << f.rdbuf();
  out = ss.str();
  return true;
}

int main(int argc, char** argv) {
  const std::string cmd = argc > 1 ? argv[1] : "";
  if (cmd == "check" && argc > 2) {
    int bad = 0;
    for (int i = 2; i < argc; ++i) {
      std::string text, err;
      ld::ContentInfo info;
      if (!readFile(argv[i], text)) { std::printf("ERROR %s: cannot open\n", argv[i]); ++bad; continue; }
      if (!ld::checkContent(text, err, &info)) { std::printf("ERROR %s: %s\n", argv[i], err.c_str()); ++bad; continue; }
      std::printf("ok    %s: %s \"%s\"%s%s\n", argv[i], info.kind.c_str(), info.name.c_str(), info.style.empty() ? "" : ", ", info.style.c_str());
      for (const auto& w : info.warnings) std::printf("      warning: %s\n", w.c_str());
    }
    return bad ? 1 : 0;
  }
  if ((cmd == "render" || cmd == "midi") && argc == 4) {
    std::string text, err;
    ld::ContentInfo info;
    if (!readFile(argv[2], text)) { std::printf("ERROR %s: cannot open\n", argv[2]); return 1; }
    auto p = std::make_unique<ld::Project>(ld::makeStartProject());
    ld::ContentTarget tg;                            // scene A; an instrument goes on the bass track
    if (!ld::applyContent(text, *p, tg, err, &info)) { std::printf("ERROR %s: %s\n", argv[2], err.c_str()); return 1; }
    for (const auto& w : info.warnings) std::printf("warning: %s\n", w.c_str());
    if (cmd == "midi") {
      const std::vector<uint8_t> mid = ld::buildMidi(*p, false, 0);
      FILE* f = std::fopen(argv[3], "wb");
      if (!f) { std::printf("ERROR cannot write %s\n", argv[3]); return 1; }
      std::fwrite(mid.data(), 1, mid.size(), f);
      std::fclose(f);
      std::printf("wrote %s\n", argv[3]);
      return 0;
    }
    ld::RenderOptions o;
    o.songMode = false; o.scene = 0; o.tailSeconds = 1.5;
    if (info.kind == "instrument") o.soloTrack = 1;  // hear the instrument alone, playing the start project's bass line
    if (info.kind == "drumkit") o.soloTrack = 0;
    const ld::RenderResult rr = ld::renderOffline(*p, o);
    float pk = 0;
    for (size_t i = 0; i < rr.l.size(); ++i) pk = std::fmax(pk, std::fmax(std::fabs(rr.l[i]), std::fabs(rr.r[i])));
    if (!ld::writeWav16(argv[3], rr.l.data(), rr.r.data(), rr.l.size(), rr.sampleRate)) { std::printf("ERROR cannot write %s\n", argv[3]); return 1; }
    std::printf("wrote %s: %s \"%s\", %.1f s, peak %.2f\n", argv[3], info.kind.c_str(), info.name.c_str(), double(rr.l.size()) / rr.sampleRate, double(pk));
    return 0;
  }
  std::printf("usage:\n  ld_content check <file.json>...\n  ld_content render <file.json> <out.wav>\n  ld_content midi <file.json> <out.mid>\n");
  return 2;
}
