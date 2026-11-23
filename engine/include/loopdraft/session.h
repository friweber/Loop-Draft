// Loop Draft engine - Session: what the app's UI thread talks to.
// Owns the Project, keeps undo history, saves and loads, and forwards every change to the Engine.
// All methods are for one thread (the UI thread). The audio thread only ever touches engine().render*.
#pragma once
#include <vector>

#include "loopdraft/engine.h"

namespace ld {


// Forces every field of a project into its valid range (used after loading and before sending).
void sanitize(Project& p);

// Project files (see project_io.cpp). Writing is atomic. Reading accepts the current format and the
// formats of app builds 5 and 7; `converted` reports that an older file was upgraded.
bool writeProjectFile(const char* path, const Project& p);
bool readProjectFile(const char* path, Project& out, bool* converted = nullptr);

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
  void setChainAuto(int pos, int track, int lane, int paramId, float startVal, float endVal);
  void setFxSlot(int track, int slot, int type, float amount, float rate, bool bypass, bool undoStep = true);
  void applyInstrument(int synth, int preset);
  void setActiveSynths(int count);
  void loadJungleKit();            // pads become 8 slices of the built-in break; tempo goes to 168
  void loadStandardKit();
  void writeBreakPattern(int scene);   // fills the scene's drums with the 8 slices in their original order
  bool toggleNote(int scene, int synth, int step, int degree, bool accent, int len = 1);   // true if the note now exists
  void setSceneBars(int scene, int bars);
  void copyScene(int from, int to);
  void clearScene(int scene);
  void chainAdd(int scene);
  void chainRemove(int pos);
  void resetToDemo();

  // Outline of a pad's sound for drawing: n peak values 0..1 across the whole sample. Returns how many were written.
  int padWaveform(int pad, float* out, int n);

  // Call regularly (every UI frame is fine). Writes live-recorded hits into the project.
  // Returns true if the project changed.
  bool poll();

  bool dirty() const { return dirty_; }
  bool save(const char* path);     // atomic: writes a temp file, then renames
  bool load(const char* path);     // false if the file is missing or not a project. Files from build 5 (version 1) are converted.

 private:
  void commit();
  void syncKit();                  // makes the engine's pad samples match project().kit
  int appliedKit_ = 0;
  std::unique_ptr<Sample> wave_[kNumPads];   // UI-side copies of the kit sounds, only for drawing
  int waveKit_ = -1;
  Engine engine_;
  Project proj_;
  std::vector<Project> undo_, redo_;
  bool dirty_ = false;
};

}  // namespace ld
