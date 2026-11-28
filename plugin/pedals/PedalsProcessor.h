// Loop Pedals: the pedal collection of Loop Master as an effect plugin and a standalone app.
// Plug a guitar, a bass or a synth into the audio interface, and this is the pedalboard: ten slots in a row,
// an amp and a speaker among the pedals, a tuner, boards to load and to save, and three LFOs that move knobs.
// Every knob is also a parameter of the plugin, so a DAW can record and draw its movements.
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <loopdraft/pedalboard.h>

constexpr int kSlots = ld::kBoardSlots;
constexpr int kKnobs = 5;                 // amount, rate, third, fourth, mix
constexpr int kLfos = 3;

struct BoardLfo { int slot = -1, knob = 0, shape = 0; bool sync = true; float rate = 0.4f, depth = 0.5f; };
struct BoardLayout { int type[kSlots] = {}, voice[kSlots] = {}, flags[kSlots] = {}; BoardLfo lfos[kLfos]; };

struct FactorySlot { int type, voice; float a, r, p3, p4, mix; int flags; };
struct FactoryBoard { const char* group; const char* name; const char* about; FactorySlot slots[8]; };
extern const FactoryBoard kFactoryBoards[];
extern const int kNumFactoryBoards;

class LoopPedalsProcessor : public juce::AudioProcessor {
 public:
  LoopPedalsProcessor();
  ~LoopPedalsProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "Loop Pedals"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 8.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}
  void getStateInformation(juce::MemoryBlock& dest) override;
  void setStateInformation(const void* data, int size) override;

  // ---- the board, from the window (message thread) ----
  BoardLayout layout() const { const juce::ScopedLock sl(lock_); return layout_; }
  float knob(int slot, int k) const { return knobs_[slot][k]->get(); }
  void setKnob(int slot, int k, float v) { knobs_[slot][k]->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, v)); }
  juce::AudioParameterFloat* knobParam(int slot, int k) { return knobs_[slot][k]; }
  bool isOn(int slot) const { return on_[slot]->get(); }
  void setOn(int slot, bool on) { on_[slot]->setValueNotifyingHost(on ? 1.0f : 0.0f); }
  void setType(int slot, int type);                    // another pedal: its knobs go to the middle, it is switched on
  void setVoice(int slot, int voice);
  void setFlags(int slot, int flags);
  void swapSlots(int a, int b);
  void setLfo(int i, const BoardLfo& l);
  void clearBoard();
  void loadFactory(int index);
  std::unique_ptr<juce::XmlElement> boardXml() const;
  void applyBoardXml(const juce::XmlElement& xml);
  static juce::File userDir();
  juce::StringArray userBoards() const;
  bool saveUser(const juce::String& name);
  bool loadUser(const juce::String& name);
  bool deleteUser(const juce::String& name);
  juce::String boardName{"Empty board"};

  juce::AudioParameterFloat *inGain = nullptr, *outGain = nullptr, *tempo = nullptr;
  juce::AudioParameterChoice* inputMode = nullptr;

  // ---- for the display ----
  std::atomic<float> inPeak{0.0f}, outPeak{0.0f}, lfoNow[kLfos];
  std::atomic<bool> tunerOn{false};
  std::atomic<float> bpmNow{120.0f};
  float tunerHz();                                     // the note that comes in, or 0 (message thread)
  int touchSerial = 0, lastSlot = 0, lastKnob = 0;     // the knob that was touched last (the LFOs pick their target this way)

 private:
  juce::CriticalSection lock_;
  BoardLayout layout_, rt_;
  juce::AudioParameterFloat* knobs_[kSlots][kKnobs] = {};
  juce::AudioParameterBool* on_[kSlots] = {};
  std::unique_ptr<ld::PedalChain> chain_;
  std::vector<float> l_, r_;
  double sr_ = 48000.0, lfoPhase_[kLfos] = {}, lfoWhole_[kLfos] = {};
  float lfoRand_[kLfos] = {}, gIn_ = 1.0f, gOut_ = 1.0f;
  uint32_t seed_ = 0x2468aceu;
  static constexpr int kRing = 8192;
  float ring_[kRing] = {};
  std::atomic<int> ringW_{0};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopPedalsProcessor)
};
