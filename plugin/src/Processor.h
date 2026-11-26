// Loop Master: the Loop Draft engine inside a host.
//
// Threads
//   audio thread:   processBlock. Talks to the engine only through its real-time calls.
//   everything else (editor, timer, state from the host): holds `lock()` while it touches the session.
#pragma once
#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_utils/juce_audio_utils.h>

#include "loopdraft/session.h"

struct LibraryEntry {
  juce::String kind, name, style;
  const char* data = nullptr;
  int size = 0;
};

class LoopMasterProcessor : public juce::AudioProcessor, private juce::Timer {
 public:
  LoopMasterProcessor();
  ~LoopMasterProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using juce::AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "Loop Master"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 3.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // ---- for the editor (hold lock() around every use of session()) ----
  juce::CriticalSection& lock() { return sessionLock_; }
  ld::Session& session() { return *session_; }
  const std::vector<LibraryEntry>& library() const { return library_; }
  bool openProjectFile(const juce::File& f);          // undoable
  bool saveProjectFile(const juce::File& f);
  juce::String importLibrary(const LibraryEntry& e, int scene, int synth, bool tempoAndKey);   // "" or what went wrong
  // The techno database. genre 0..9: tempo, key, kit, four sounds and pedals for it; with song, also its patterns
  // stacked into grooves A..F and a song. One undo step. Returns what to tell the user.
  juce::String technoGenre(int genre, bool song);
  int technoCount(int what) const;                     // 0 genres, 1 patterns, 2 songs
  float technoBpm(int genre) const;
  juce::String technoStarter(int song);                // one of the three finished starter songs, into A..D
  // the control that was touched last (the LFOs pick their target this way)
  int touchSerial = 0, lastPid = -1, lastIndex = 0;
  void setSongMode(bool on);
  bool songMode() const { return songMode_.load(); }
  void setSongLoop(bool on);                           // with the host: start the song over after its end, or stay silent
  bool songLoop() const { return songLoop_.load(); }
  // The song may begin later than the host's bar 1.
  void setSongStartBar(int bar) { songStartBar_.store(juce::jlimit(1, 999, bar)); }
  int songStartBar() const { return songStartBar_.load(); }
  // What a control shows: its own value, or, while a groove that moves it is playing, where that movement is now.
  float shownParam(int pid, int index);

  // ---- Loop Master Live ----
  // Everything that can be played live has a number, so a key or knob of a MIDI controller can be tied to it.
  enum LiveTarget { kLiveGroove = 0, kLiveMute = 8, kLiveLevel = 13, kLiveFx = 18, kLiveFilter = 24, kLivePlay = 25, kLiveTargets = 26 };
  static constexpr int kLiveFxCount = 6;
  static const char* liveFxName(int i);
  void liveFx(int i, bool down);                       // held: one effect on the whole mix
  int liveFxHeld() const { return fxHeld_; }
  void setLiveFilter(float v);                         // -1 low-pass .. 0 off .. +1 high-pass
  float liveFilter() const { return djFilter_; }
  void liveToggleMute(int track);
  void liveSolo(int track);                            // -1: nobody. Mutes the others and remembers how they were.
  int liveSoloed() const { return solo_; }
  void livePlayStop();
  void liveLearn(int target) { learn_.store(target); } // the next key or knob that comes in is tied to it; -1 stops waiting
  int liveLearning() const { return learn_.load(); }
  void liveForget(int target);
  juce::String liveMapName(int target) const;          // "CC 21", "C#2", or nothing
  int skin = 0;                                        // which look the synths wear (see kSkins in the editor)
  bool liveMode = false, guideSeen = false, fewerControls = false;   // editor settings kept with the project
  juce::String midiFolder;                             // where the pattern browser looks for .mid files (editor thread)
  // The control the user is holding right now, so its movement can be written into the groove (editor thread).
  struct Touch { int pid = -1, index = 0; float value = 0.0f; } touch;
  void setScene(int scene);                            // through the host parameter, so the host can record it
  bool hostLeads() const { return hostLeads_.load(); } // the host's transport and tempo are in charge right now
  bool hostPlays() const { return hostPlays_.load(); } // ... and it is playing: our own PLAY waits
  double shownBpm() const { return shownBpm_.load(); }
  juce::RangedAudioParameter* hostParam(int paramId, int index) const;   // the host parameter for an engine control, or null

  juce::AudioProcessorValueTreeState params;

 private:
  struct Map { juce::String id; int pid; int index; };
  void timerCallback() override;
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  bool projectToMemory(juce::MemoryBlock& project, juce::MemoryBlock& breakWav);
  bool projectFromMemory(const void* project, size_t projectSize, const void* breakWav, size_t breakSize);
  void renderSpan(juce::AudioBuffer<float>& buffer, int start, int count);
  void handleMidi(const juce::MidiMessage& m);
  void buildLibrary();
  void adoptProjectValues();
  void applyLive(int target, int value, bool isNote, bool down);
  void rebuildNoteMap();
  struct LiveMap { int kind = 0, ch = 0, num = 0; };  // kind: 0 none, 1 controller, 2 note
  struct LiveEvent { uint8_t kind, ch, num, val; };
  LiveMap liveMap_[kLiveTargets];
  std::atomic<bool> noteTaken_[16][128];              // notes that are tied to something do not play an instrument
  juce::AbstractFifo liveFifo_{256};
  LiveEvent liveEvents_[256];
  std::atomic<int> learn_{-1}, songStartBar_{1};
  int fxHeld_ = -1, solo_ = -1;
  bool soloWas_[ld::kNumTracks] = {};
  float djFilter_ = 0.0f;                           // after a load: host parameters take the project's values

  juce::CriticalSection sessionLock_;
  std::unique_ptr<ld::Session> session_;
  juce::SpinLock swapLock_;                            // the audio thread against a session being replaced
  double rate_ = 48000.0;

  std::vector<Map> maps_;
  std::vector<std::atomic<float>*> raw_;
  std::vector<juce::RangedAudioParameter*> mapParam_;
  std::vector<float> lastParam_, lastSess_;
  std::atomic<float>* sceneRaw_ = nullptr;
  std::atomic<float>* syncRaw_ = nullptr;
  std::atomic<float>* midiTrackRaw_ = nullptr;
  int lastScene_ = -1;

  std::atomic<bool> songMode_{false}, songLoop_{false}, hostLeads_{false}, hostPlays_{false};
  std::atomic<double> shownBpm_{120.0};

  std::vector<float> scratch_;                         // for track outputs the host did not connect
  static constexpr int kScratchFrames = 2048;
  std::vector<LibraryEntry> library_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopMasterProcessor)
};
