// Loop Draft engine - presets and grooves as plain JSON text.
// The format is described for authors in docs/CONTENT_FORMAT.md. Three kinds of file:
//   "instrument"  the sound of one synth track
//   "drumkit"     kit, hardness, pad settings, synthesized pads
//   "groove"      patterns for one scene, optionally with tempo, key, a drum kit and instruments
// Everything here works on plain project data. The session wraps it in an undo step.
#pragma once
#include <string>
#include <vector>

#include "loopdraft/types.h"

namespace ld {

struct ContentInfo {
  std::string kind, name, style;
  std::vector<std::string> warnings;   // things that were ignored: unknown keys and the like
};

struct ContentTarget {
  int scene = 0;            // groove: the scene whose patterns are replaced
  int synth = 0;            // instrument: the synth track that gets the sound (0..3)
  bool tempoAndKey = true;  // groove: also take bpm, swing, root and scale (these belong to the whole project)
};

// Reads the text and changes `p`. Returns false and sets `error` (with the place in the file) if anything is
// wrong; `p` is then left as it was. Values outside their range are errors, not silently clamped.
bool applyContent(const std::string& json, Project& p, const ContentTarget& target, std::string& error, ContentInfo* info = nullptr);

// The scale degree a synth track would play nearest to a MIDI note, in the project's key.
int midiToDegree(const Project& p, int synth, int midi);

// Only reads the header (kind, name, style) and checks the whole file against an empty project.
bool checkContent(const std::string& json, std::string& error, ContentInfo* info = nullptr);

// The other direction: what is in the project, as a file of this format.
std::string instrumentToJson(const Project& p, int synth, const std::string& name, const std::string& style = "");
std::string drumkitToJson(const Project& p, const std::string& name, const std::string& style = "");
// withSounds: the groove also carries the drum kit and the instruments of its tracks, so it sounds the same anywhere.
std::string grooveToJson(const Project& p, int scene, const std::string& name, const std::string& style = "", bool withSounds = true);

}  // namespace ld
