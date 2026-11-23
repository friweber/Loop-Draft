// Loop Draft engine - Session: what the app's UI thread talks to.
// Owns the Project, keeps undo history, saves and loads, and forwards every change to the Engine.
// All methods are for one thread (the UI thread). The audio thread only ever touches engine().render*.
#pragma once
#include <vector>

#include "loopdraft/engine.h"

namespace ld {

// Parameter ids for getParam / setParam. `index` is the pad, synth or track number where it applies.
enum Param : int {
  P_BPM = 0, P_SWING, P_MASTER, P_DELAY_FB, P_ROOT, P_SCALE, P_OCTAVE, P_ACTIVE_SYNTHS,
  P_PAD_VOL = 10, P_PAD_PAN, P_PAD_PITCH, P_PAD_DECAY, P_PAD_CHOKE, P_PAD_SLICE_START, P_PAD_SLICE_END, P_PAD_REVERSE,
  P_SYN_WAVE1 = 20, P_SYN_WAVE2, P_SYN_DETUNE, P_SYN_CUTOFF, P_SYN_RES, P_SYN_ATTACK, P_SYN_DECAY, P_SYN_SUSTAIN,
  P_SYN_RELEASE, P_SYN_LEVEL, P_SYN_LFO_RATE, P_SYN_LFO_DEPTH, P_SYN_LFO_TARGET, P_SYN_OCTAVE, P_SYN_CHORD,
  P_SYN_MODE, P_SYN_FM_RATIO, P_SYN_FM_INDEX, P_SYN_FM_DECAY, P_SYN_FM_FB,
  P_SYN_INSTRUMENT = 50, P_SYN_WARMTH = 51, P_SYN_MOTION = 52,
  P_FX_LEVEL = 40, P_FX_DRIVE, P_FX_EQ_LOW, P_FX_EQ_HIGH, P_FX_DELAY, P_FX_REVERB, P_FX_MUTE
};

// Forces every field of a project into its valid range (used after loading and before sending).
void sanitize(Project& p);

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
  void setFxSlot(int track, int slot, int type, float amount, float rate, bool bypass);
  void applyInstrument(int synth, int preset);
  void setActiveSynths(int count);
  void loadJungleKit();
  void loadStandardKit();
  bool toggleNote(int scene, int synth, int step, int degree, bool accent, int len = 1);   // true if the note now exists
  void setSceneBars(int scene, int bars);
  void copyScene(int from, int to);
  void clearScene(int scene);
  void chainAdd(int scene);
  void chainRemove(int pos);
  void resetToDemo();

  // Call regularly (every UI frame is fine). Writes live-recorded hits into the project.
  // Returns true if the project changed.
  bool poll();

  bool dirty() const { return dirty_; }
  bool save(const char* path);     // atomic: writes a temp file, then renames
  bool load(const char* path);     // false if the file is missing or not a valid project of this version

 private:
  void commit();
  Engine engine_;
  Project proj_;
  std::vector<Project> undo_, redo_;
  bool dirty_ = false;
};

}  // namespace ld
