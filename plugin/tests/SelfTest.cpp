// Runs the plugin the way a host does, without a host: transport, outputs, automation, MIDI, state.
#include <cstdio>

#include "../src/Processor.h"

namespace {

struct Head : juce::AudioPlayHead {
  bool playing = false;
  double ppq = 0.0, bpm = 120.0;
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo info;
    info.setIsPlaying(playing);
    info.setPpqPosition(ppq);
    info.setBpm(bpm);
    return info;
  }
};

double level(const juce::AudioBuffer<float>& b, int channel) {
  double sum = 0.0;
  if (channel < b.getNumChannels()) {
    const float* x = b.getReadPointer(channel);
    for (int i = 0; i < b.getNumSamples(); ++i) sum += std::abs(double(x[i]));
  }
  return sum;
}

}  // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI init;
  int fails = 0;
  const auto check = [&fails](bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++fails;
  };
  const double sr = 44100.0;
  const int block = 512;

  LoopMasterProcessor p;
  Head head;
  p.setPlayHead(&head);
  auto layout = p.getBusesLayout();
  for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference(b) = juce::AudioChannelSet::stereo();
  check(p.setBusesLayout(layout), "all six stereo outputs can be switched on");
  p.setRateAndBufferSizeDetails(sr, block);
  p.prepareToPlay(sr, block);
  juce::AudioBuffer<float> buf(p.getTotalNumOutputChannels(), block);
  juce::MidiBuffer midi;
  check(buf.getNumChannels() == 12, "twelve output channels");

  double mainOut = 0.0, drums = 0.0, bass = 0.0;
  const auto run = [&](int blocks) {
    mainOut = drums = bass = 0.0;
    for (int i = 0; i < blocks; ++i) {
      buf.clear();
      p.processBlock(buf, midi);
      midi.clear();
      mainOut += level(buf, 0); drums += level(buf, 2); bass += level(buf, 4);
      if (head.playing) head.ppq += double(block) * head.bpm / (60.0 * sr);
    }
  };

  run(50);
  check(mainOut < 1e-6, "silent while the host is stopped");
  head.playing = true;
  run(400);
  const double playingLevel = mainOut / 400.0;                 // per block
  check(mainOut > 1.0 && drums > 1.0, "plays with the host: main and drum output carry sound");
  check(p.hostLeads(), "the host leads");
  check(std::abs(p.shownBpm() - 120.0) < 1e-6, "shows the host's tempo");
  {
    const juce::ScopedLock sl(p.lock());
    check(p.session().engine().isPlaying(), "the sequencer runs");
    const int expect = int(std::floor(head.ppq * 4.0)) % (16 * int(p.session().project().scenes[0].bars));
    const int step = p.session().engine().currentStep();
    check(step == expect || step == (expect + 15) % 16 || step == expect - 1, "the step is where the host's position is");
  }

  const double drumsBefore = drums;
  p.params.getParameter("lvl0")->setValueNotifyingHost(0.0f);
  run(60);
  run(400);
  check(drums < drumsBefore * 0.001, "a host parameter pulls the drums down");
  p.params.getParameter("lvl0")->setValueNotifyingHost(0.8f);
  run(60);
  run(400);
  check(drums > drumsBefore * 0.5, "and up again");

  head.playing = false;
  run(600);                                                    // delay and reverb ring out
  run(50);
  check(mainOut / 50.0 < playingLevel * 1e-4, "quiet again after the host stopped");
  midi.addEvent(juce::MidiMessage::noteOn(1, 45, 0.9f), 0);
  run(40);
  check(bass > 0.5, "a MIDI note on channel 1 plays the bass");
  midi.addEvent(juce::MidiMessage::noteOff(1, 45), 0);
  run(300);
  midi.addEvent(juce::MidiMessage::noteOn(10, 36, 0.9f), 100);
  run(20);
  check(drums > 0.5, "a MIDI note on channel 10 plays the kick");

  {
    const juce::ScopedLock sl(p.lock());
    p.session().beginEdit();
    p.session().setDrumStep(0, 5, 3, 2);
    p.session().setParam(ld::P_BPM, 0, 97.0f);
    p.session().setSceneBars(7, 16);                       // groove H, sixteen bars, a pedal in slot six, a knob movement
    p.session().setDrumStep(7, 2, 255, 1);
    p.session().setFxSlot(1, 5, int(ld::FxType::TapeEcho), 0.4f, 0.5f, false);
    p.session().writeLanePoint(7, ld::fxSlotParamId(5, 2), 1, 64, 0.8f);
  }
  p.setSongMode(true);
  p.setSongLoop(true);
  p.midiFolder = "/somewhere/midi";
  juce::MemoryBlock state;
  p.getStateInformation(state);
  check(state.getSize() > 1000, "state is written");

  LoopMasterProcessor q;
  q.setStateInformation(state.getData(), int(state.getSize()));
  {
    const juce::ScopedLock sl(q.lock());
    check(q.session().project().scenes[0].drums.steps[5][3] == 2 && std::abs(q.session().getParam(ld::P_BPM, 0) - 97.0f) < 0.01f,
          "the host's session brings the project back");
  }
  check(q.songMode(), "song mode comes back");
  check(q.songLoop() && q.midiFolder == "/somewhere/midi", "and so do the song's end and the MIDI folder");
  {
    const juce::ScopedLock sl(q.lock());
    const ld::Project& pr = q.session().project();
    check(pr.scenes[7].bars == 16 && pr.scenes[7].drums.steps[2][255] == 1, "groove H keeps its sixteen bars");
    check(pr.fx[1].slots[5].type == ld::FxType::TapeEcho, "the sixth pedal slot is kept");
    check(pr.scenes[7].lanes[0].paramId == ld::fxSlotParamId(5, 2) && pr.scenes[7].lanes[0].points[64] == 0.8f, "a knob movement inside a groove is kept");
  }
  if (auto* sc = dynamic_cast<juce::AudioParameterChoice*>(q.params.getParameter("scene")))
    check(sc->choices.size() == 8, "eight grooves for the host to choose from");
  q.setRateAndBufferSizeDetails(96000.0, 256);
  q.prepareToPlay(96000.0, 256);
  {
    const juce::ScopedLock sl(q.lock());
    check(q.session().project().scenes[0].drums.steps[5][3] == 2, "the project survives a change of sample rate");
    check(std::abs(q.session().engine().sampleRate() - 96000.0) < 1.0, "the engine runs at the new rate");
  }
  check(!p.library().empty() && p.library().size() >= 100, "the library is inside the plugin");
  {
    const juce::String err = q.importLibrary(q.library().front(), 1, 0, true);
    check(err.isEmpty(), "a library entry loads");
  }

  std::printf("\n%s\n", fails ? "SELF-TEST FAILED" : "self-test passed");
  return fails ? 1 : 0;
}
