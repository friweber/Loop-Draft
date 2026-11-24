// Loop Draft engine - Session: what the app's UI thread talks to.
// Owns the Project, keeps undo history, saves and loads, and forwards every change to the Engine.
// All methods are for one thread (the UI thread). The audio thread only ever touches engine().render*.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "loopdraft/content.h"
#include "loopdraft/engine.h"

namespace ld {


// Forces every field of a project into its valid range (used after loading and before sending).
void sanitize(Project& p);

// Project files (see project_io.cpp). Writing is atomic. Reading accepts the current format and the
// formats of app builds 5 and 7; `converted` reports that an older file was upgraded.
bool writeProjectFile(const char* path, const Project& p);
bool readProjectFile(const char* path, Project& out, bool* converted = nullptr);

constexpr double kMaxBreakSeconds = 24.0;

class Session {
 public:
  explicit Session(double sampleRate);

  Engine& engine() { return engine_; }
  const Project& project() const { return proj_; }

  // Undo works per gesture: call beginEdit() once, then change things, each change is sent right away.
  void beginEdit();
  bool undo();
  bool redo();
  bool canUndo() const { return !undo_.empty(); }
  bool canRedo() const { return !redo_.empty(); }

  float getParam(int id, int index) const;
  void setParam(int id, int index, float value);            // no undo entry of its own

  // Each of these makes its own undo entry.
  int  toggleDrumStep(int scene, int pad, int step, bool accent);   // returns the new value 0/1/2
  void setDrumStep(int scene, int pad, int step, int value);        // 0=off, 1=norm, 2=acc, 3=2x, 4=3x, 5=4x roll
  void setChainAuto(int pos, int track, int lane, int paramId, float startVal, float endVal, bool undoStep = true);
  void setFxSlot(int track, int slot, int type, float amount, float rate, bool bypass, bool undoStep = true);
  void applyInstrument(int synth, int preset);
  void setActiveSynths(int count);
  void loadJungleKit();            // = setDrumEngine(JungleSlicer)
  void loadStandardKit();          // = setDrumEngine(Classic808)
  void setDrumEngine(int engine);  // 0..4, see DrumEngine. Jungle Slicer: pads become 8 slices of a break
  void applyJungleTuning(float pitch, float tight, float crunch, bool undoStep = true);
  // The Jungle Slicer can slice your own break instead of the built-in one. At most kMaxBreakSeconds long.
  bool loadJungleBreakWav(const char* path);
  bool resampleSceneToJungle(int scene);   // renders the scene's drums (with their effects) and slices that
  std::shared_ptr<const Sample> activeBreak() const;   // the user's break if the project plays one, else null
  void writeBreakPattern(int scene);   // fills the scene's drums with the 8 slices in their original order
  void setKitHardness(float hardness);             // 0..1, drives the whole drum track
  // Drum synth: a pad can play a drum that is synthesized from its DrumSynthParams instead of the kit's sound.
  void synthesizeCustomPad(int pad, bool audition = true);   // switches the pad to its drum synth
  void applyDrumSynthTemplate(int pad, int tmpl);            // 0 kick, 1 snare, 2 hat, 3 gabber kick, 4 zap, 5 clang
  void clearCustomPad(int pad);                              // back to the kit's own sound
  // true if the note now exists. slide: adds the note as a slide note, or switches slide on/off on an existing one.
  bool toggleNote(int scene, int synth, int step, int degree, bool accent, int len = 1, bool slide = false);
  void setSceneBars(int scene, int bars);
  void copyScene(int from, int to);
  void clearScene(int scene);
  void chainAdd(int scene);
  void chainRemove(int pos);
  void resetToDemo();
  void loadStartPreset();          // the built-in "Warehouse 303" project; one undo step

  // Outline of a pad's sound for drawing: n peak values 0..1 across the whole sample. Returns how many were written.
  int padWaveform(int pad, float* out, int n);

  // Call regularly (every UI frame is fine). Writes live-recorded hits into the project.
  // Returns true if the project changed.
  bool poll();

  bool dirty() const { return dirty_; }
  bool save(const char* path);     // atomic: writes a temp file, then renames
  // A preset or groove file (see content.h). One undo step; on an error nothing changes and `error` says where.
  bool importContent(const std::string& json, const ContentTarget& target, std::string& error, ContentInfo* info = nullptr);
  bool openProject(const char* path);      // loads a named project as one undo step: UNDO goes back to what was open
  bool saveCopy(const char* path) const;   // a second copy somewhere else (a named project); the autosave state is untouched
  bool load(const char* path);     // false if the file is missing or not a project. Files from build 5 (version 1) are converted.

 private:
  void commit();
  void switchDrumMode(bool toJungle);   // swaps pads, drum patterns and drive with the stored set of the other mode
  void syncKit();                  // makes the engine's pad samples match the project's drum engine and break
  void useBreak(std::shared_ptr<const Sample> s, uint32_t id = 0);
  std::shared_ptr<const Sample> findBreak(uint32_t id) const;
  struct Break { uint32_t id; std::shared_ptr<const Sample> sample; };
  std::vector<Break> breaks_;      // the last few user breaks, so undo can bring one back
  uint32_t nextBreakId_ = 1, savedBreakId_ = 0;
  int appliedEngine_ = 0;
  uint32_t appliedBreak_ = 0;
  uint32_t appliedPad_[kNumPads] = {};   // which drum-synth settings each pad's sound was made from (0 = kit sound)
  uint32_t wavePad_[kNumPads] = {};
  std::shared_ptr<const Sample> wave_[kNumPads];   // UI-side copies of the kit sounds, only for drawing
  int waveEngine_ = -1;
  uint32_t waveBreak_ = 0;
  Engine engine_;
  Project proj_;
  std::vector<Project> undo_, redo_;
  bool dirty_ = false;
};

}  // namespace ld
