// Loop Draft engine - the real-time core.
//
// Threading model
//   control thread (UI): setProject, setPadSample, play, stop, queueScene, triggerPad, noteOn/off,
//                        pollRecorded, collectGarbage. One thread only.
//   audio thread:        render / renderInterleaved. Never allocates, locks, or does IO.
//   any thread:          the status getters (currentStep, peak, ...). They read atomics.
//
// The UI owns the Project. Every edit sends a fresh snapshot with setProject(); the engine swaps a
// pointer at the next audio block and hands the old one back for deletion in collectGarbage().
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "loopdraft/dsp.h"
#include "loopdraft/spsc_queue.h"
#include "loopdraft/types.h"
#include "loopdraft/voices.h"

namespace ld {

struct RecordedHit {
  int track = 0;   // 0 = drums, 1.. = synths
  int row = 0;     // pad index, or scale degree
  int scene = 0;
  int step = 0;
  int vel = 1;
};

class Engine {
 public:
  explicit Engine(double sampleRate);
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // ---- control thread ----
  void setProject(const Project& p);
  void setPadSample(int pad, std::unique_ptr<Sample> s);   // nullptr restores the built-in sound
  void play(int scene);            // starts from the top; in song mode the chain decides the scene
  void stop();
  void queueScene(int scene);      // while playing: switches at the next bar line; stopped: immediately
  void setSongMode(bool on);
  void setRecording(bool on);
  void triggerPad(int pad, int vel = 1);
  void noteOn(int synth, int degree, int vel = 1);
  void noteOff(int synth, int degree);
  bool pollRecorded(RecordedHit& out);   // notes played live while recording, already snapped to the grid
  void collectGarbage();                 // call now and then; frees projects and samples the audio thread gave back

  // ---- audio thread ----
  void render(float* outL, float* outR, int frames);
  void renderInterleaved(float* out, int frames);   // stereo, LRLR... (what Oboe delivers)

  // ---- status, any thread ----
  bool isPlaying() const    { return aPlaying_.load(std::memory_order_relaxed); }
  int  currentStep() const  { return aStep_.load(std::memory_order_relaxed); }
  int  currentScene() const { return aScene_.load(std::memory_order_relaxed); }
  int  chainPos() const     { return aChainPos_.load(std::memory_order_relaxed); }
  int  queuedScene() const  { return aPending_.load(std::memory_order_relaxed); }
  float peak(int ch) const  { return (ch ? aPeakR_ : aPeakL_).load(std::memory_order_relaxed); }
  double sampleRate() const { return sr_; }

 private:
  struct Cmd {
    enum Type : uint8_t { SetProject, SetSample, Play, Stop, QueueScene, SongMode, Record, TrigPad, NoteOn, NoteOff } type;
    int a = 0, b = 0, c = 0;
    void* ptr = nullptr;
  };
  struct Trash { bool isProject; void* ptr; };
  struct FxSlotState { Drive drive; Bitcrusher crush; Chorus chorus; Phaser phaser; Svf flt[2]; TrackComp comp; };
  struct TrackState { Smooth level, dSend, rSend; Drive drive; Shelf lo[2], hi[2]; FxSlotState fxSlots[kFxSlots]; float cLow = 1e9f, cHigh = 1e9f, cDrive = -1; };
  struct Suppress { bool on = false; int scene = 0, track = 0, row = 0, step = 0; };
  struct Ratchet { bool active = false; int pad = 0; int remaining = 0; int64_t nextFrame = 0; double interval = 0.0; };

  void send(const Cmd& c);
  void processCommands();
  void applyProject();
  void fireStep();
  void renderAudio(float* l, float* r, int n);
  void recordHit(int track, int row, int vel);
  bool suppressed(int track, int row);
  const Sample* padSample(int pad) const { return user_[pad] ? user_[pad] : builtin_[pad].get(); }
  double stepLen() const { return sr_ * 60.0 / double(proj_->bpm < 20 ? 20 : proj_->bpm) / 4.0; }

  double sr_;
  SpscQueue<Cmd> cmds_{1024};
  SpscQueue<Trash> trash_{2048};   // larger than cmds_, and emptied before every send, so it cannot fill up
  SpscQueue<RecordedHit> recorded_{256};

  // owned by the audio thread after construction
  Project* proj_ = nullptr;
  const Sample* user_[kNumPads] = {};
  std::unique_ptr<Sample> builtin_[kNumPads];
  Sampler sampler_;
  Synth synth_[kNumSynths];
  TrackState tr_[kNumTracks];
  Delay delay_;
  Reverb reverb_;
  Limiter limiter_;
  Smooth master_;

  bool playing_ = false, songMode_ = false, recording_ = false;
  int scene_ = 0, step_ = 0, pending_ = -1, chainPos_ = 0;
  int64_t frame_ = 0, nextFire_ = 0, lastFire_ = 0, slotStartFrame_ = 0;
  int lastStep_ = 0, lastScene_ = 0;
  double stepBase_ = 0;
  Suppress sup_[8];
  Ratchet ratchets_[kNumPads];
  float peakL_ = 0, peakR_ = 0;

  std::atomic<bool> aPlaying_{false};
  std::atomic<int> aStep_{-1}, aScene_{0}, aChainPos_{0}, aPending_{-1};
  std::atomic<float> aPeakL_{0}, aPeakR_{0};
};

// ---- offline rendering and export (control thread, may allocate) ----
struct RenderResult {
  std::vector<float> l, r;
  double sampleRate = 48000.0;
};
struct RenderOptions {
  double sampleRate = 48000.0;
  bool songMode = false;      // true: play the chain once; false: play `scene` once
  int scene = 0;
  int soloTrack = -1;         // 0..kNumTracks-1 renders a stem
  double tailSeconds = 2.5;   // room for delay and reverb to ring out
  int blockSize = 256;
  const Sample* samples[kNumPads] = {};   // optional user samples (copied)
};
RenderResult renderOffline(const Project& p, const RenderOptions& opt);
// Standard MIDI File, format 1: tempo track, drums (channel 10, General MIDI notes), then one track per synth.
std::vector<uint8_t> buildMidi(const Project& p, bool songMode, int scene);

}  // namespace ld
