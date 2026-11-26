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
  void setSongMode(bool on);
  bool songMode() const { return songMode_.load(); }
  void setSongLoop(bool on);                           // with the host: start the song over after its end, or stay silent
  bool songLoop() const { return songLoop_.load(); }
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
  void adoptProjectValues();                           // after a load: host parameters take the project's values

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
