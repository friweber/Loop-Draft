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
#include "loopdraft/pedals.h"
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

  // ---- audio thread, for a host that brings its own clock, MIDI and automation (the Loop Master plugin) ----
  // Call these between render() calls, on the thread that renders.
  // hostTransport: once per block, before render(). quarterNotes is the host's position at the first frame of the block.
  // The sequencer then follows the host: start, stop, jumps, loops and tempo. Play/Stop from the control thread are ignored.
  void hostTransport(bool playing, double quarterNotes, double bpm);
  void hostTransportOff();                 // back to the engine's own transport and the project's tempo
  void rtTriggerPad(int pad, int vel = 1);
  void rtNoteOn(int synth, int midiNote, int vel = 1);     // chromatic, not bound to the project's scale
  void rtNoteOff(int synth, int midiNote);
  void rtAllNotesOff();
  void rtQueueScene(int scene);
  int  rtPadForMidiNote(int midiNote) const;               // the pad a drum note plays (its General MIDI note, or 60..67), else -1
  // A value that replaces the project's own until cleared: host automation. Levels, sends and drive take a track
  // as index, cutoff and resonance a synth; P_MASTER takes 0.
  void rtSetOverride(int paramId, int index, float value);
  void rtClearOverrides();
  // Each track on its own (after its channel strip and effects, before delay, reverb, master and limiter).
  // kNumTracks pointers per side, or nullptr for none. Applies to the next render() call only.
  void setTrackOutputs(float* const* l, float* const* r);

  // ---- status, any thread ----
  bool isPlaying() const    { return aPlaying_.load(std::memory_order_relaxed); }
  int  currentStep() const  { return aStep_.load(std::memory_order_relaxed); }
  int  currentScene() const { return aScene_.load(std::memory_order_relaxed); }
  int  chainPos() const     { return aChainPos_.load(std::memory_order_relaxed); }
  // Only this bar of the groove goes round (-1: the whole groove). Not in song mode.
  void setLoopBar(int bar)  { aLoopBar_.store(bar, std::memory_order_relaxed); }
  int  loopBar() const      { return aLoopBar_.load(std::memory_order_relaxed); }
  // With a host: does the song start over after its last groove (as on the phone), or is it silent from there on?
  void setSongLoop(bool on) { aSongLoop_.store(on, std::memory_order_relaxed); }
  int  queuedScene() const  { return aPending_.load(std::memory_order_relaxed); }
  // For meters: the loudest sample of a track lately (it falls back on its own), after its strip and pedals.
  float trackPeak(int track) const { return track >= 0 && track < kNumTracks ? aTrackPeak_[track].load(std::memory_order_relaxed) : 0.0f; }

  // ---- for playing live: things that act on the whole mix, from any thread ----
  // One pedal on the sum of all tracks, for as long as it is held. It follows the tempo. Pedals that ring (echo,
  // reverb) ring out after they are let go; the music itself is back at once.
  void setPerformFx(int type, int voice, float amount, float rate, float p3, float p4) {
    aPerfK_[0].store(amount); aPerfK_[1].store(rate); aPerfK_[2].store(p3); aPerfK_[3].store(p4);
    aPerfVoice_.store(voice); aPerfType_.store(type);
  }
  void holdPerformFx(bool on) { aPerfOn_.store(on, std::memory_order_relaxed); }
  // The filter of a DJ mixer: 0 is out of the way, towards -1 the highs go, towards +1 the lows.
  void setDjFilter(float v) { aDjFilter_.store(v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v), std::memory_order_relaxed); }
  float djFilter() const { return aDjFilter_.load(std::memory_order_relaxed); }
  float peak(int ch) const  { return (ch ? aPeakR_ : aPeakL_).load(std::memory_order_relaxed); }
  double sampleRate() const { return sr_; }

 private:
  struct Cmd {
    enum Type : uint8_t { SetProject, SetSample, Play, Stop, QueueScene, SongMode, Record, TrigPad, NoteOn, NoteOff } type;
    int a = 0, b = 0, c = 0;
    void* ptr = nullptr;
  };
  struct Trash { bool isProject; void* ptr; };
  struct FxSlotState { Drive drive; Bitcrusher crush; Chorus chorus; Phaser phaser; Svf flt[2]; TrackComp comp; Pedal pedal; float sub[2][2] = {}; };
  struct TrackState { int64_t idle = 0; Smooth level, dSend, rSend; Drive drive; Shelf lo[2], hi[2]; FxSlotState fxSlots[kFxSlots]; float cLow = 1e9f, cHigh = 1e9f, cDrive = -1; };
  struct Suppress { bool on = false; int scene = 0, track = 0, row = 0, step = 0; };
  struct Ratchet { bool active = false; int pad = 0, hits = 0, done = 0; int64_t nextFrame = 0; double base = 0.0, interval = 0.0; };

  void send(const Cmd& c);
  void processCommands();
  void applyProject();
  void fireStep();
  void renderAudio(float* l, float* r, int n);
  void recordHit(int track, int row, int vel);
  bool suppressed(int track, int row);
  const Sample* padSample(int pad) const { return user_[pad] ? user_[pad] : builtin_[pad].get(); }
  double tempo() const { const double b = hostBpm_ > 0.0 ? hostBpm_ : double(proj_->bpm); return b < 20.0 ? 20.0 : b; }
  double stepLen() const { return sr_ * 60.0 / tempo() / 4.0; }
  void stopNow();
  void seekHost(double pos16);
  bool hostAgrees() const;
  int loopStart(int steps) const;
  int64_t songSteps() const;

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
  int64_t frame_ = 0, nextFire_ = 0, lastFire_ = 0;
  // The song slot that is audible right now (the sequencer state above already points at the next step).
  int autoPos_ = 0, autoScene_ = 0;
  int64_t autoStart_ = 0;
  int lastStep_ = 0, lastScene_ = 0;
  double stepBase_ = 0;
  Suppress sup_[8];
  Ratchet ratchets_[kNumPads];
  float peakL_ = 0, peakR_ = 0;

  // host clock (plugin)
  bool hostSync_ = false;
  float scaleHz_[15] = {};   // the project's scale over two octaves, for the pedals that are tuned
  float subK_ = 0.0f;        // the crossover of KEEP SUB
  bool hostPlaying_ = false, ownPlay_ = false;   // with a host that stands still, PLAY starts our own transport
  double hostBpm_ = 0.0, delayBpm_ = 0.0;
  int64_t hostNext_ = -1;          // the host's count of sixteenths of the step that fires next; -1: find it again
  struct Override { int id = -1, index = 0; float value = 0.0f; };
  static constexpr int kMaxOverrides = 48;
  Override ov_[kMaxOverrides];
  float* trkL_[kNumTracks] = {};
  float* trkR_[kNumTracks] = {};
  int trkOff_ = 0;

  std::atomic<bool> aPlaying_{false}, aSongLoop_{true};
  std::atomic<int> aLoopBar_{-1};
  std::atomic<int> aStep_{-1}, aScene_{0}, aChainPos_{0}, aPending_{-1};
  std::atomic<float> aTrackPeak_[kNumTracks] = {};
  std::atomic<int> aPerfType_{0}, aPerfVoice_{0};
  std::atomic<float> aPerfK_[4] = {};
  std::atomic<bool> aPerfOn_{false};
  std::atomic<float> aDjFilter_{0.0f};
  Pedal perf_;
  float perfMix_ = 0.0f, djNow_ = 0.0f, djIc_[2][2] = {};
  int64_t perfTail_ = 0;
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
