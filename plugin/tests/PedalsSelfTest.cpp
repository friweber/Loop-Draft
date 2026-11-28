// Loop Pedals driven like a host drives it: every board that comes with it, a note through it, state, tuner.
#include "PedalsProcessor.h"

int main() {
  juce::ScopedJuceInitialiser_GUI init;
  int fails = 0;
  const auto check = [&](bool ok, const juce::String& what) { std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8()); if (!ok) ++fails; };
  const double sr = 48000.0;
  const int block = 256;
  LoopPedalsProcessor q;
  q.setRateAndBufferSizeDetails(sr, block);
  q.prepareToPlay(sr, block);
  juce::MidiBuffer midi;
  double phase = 0.0;
  const auto play = [&](LoopPedalsProcessor& pr, double hz, float amp, int blocks, double& rms, float& peak) {
    juce::AudioBuffer<float> buf(2, block);
    double sum = 0.0; peak = 0.0f; bool finite = true;
    for (int b = 0; b < blocks; ++b) {
      for (int i = 0; i < block; ++i) { const float v = amp * float(std::sin(phase)); phase += 6.283185307179586 * hz / sr; buf.setSample(0, i, v); buf.setSample(1, i, 0.0f); }
      pr.processBlock(buf, midi);
      for (int i = 0; i < block; ++i) { const float v = buf.getSample(0, i), w = buf.getSample(1, i); finite = finite && std::isfinite(v) && std::isfinite(w); sum += double(v) * double(v); peak = juce::jmax(peak, std::fabs(v), std::fabs(w)); }
    }
    rms = std::sqrt(sum / double(blocks * block));
    return finite;
  };
  check(kNumFactoryBoards >= 23, "boards for guitar, bass and synth come with it");
  for (int i = 0; i < kNumFactoryBoards; ++i) {
    q.loadFactory(i);
    double rms = 0.0; float peak = 0.0f;
    const bool finite = play(q, 110.0, 0.2f, 190, rms, peak);
    std::printf("      %-22s rms %.3f peak %.2f\n", kFactoryBoards[i].name, rms, double(peak));
    check(finite && rms > 0.01 && rms < 0.75 && peak <= 1.0f, juce::String(kFactoryBoards[i].group) + " / " + kFactoryBoards[i].name + ": a note comes out, not too loud");
  }
  {  // an empty board passes the sound on; input 1 goes to both sides
    q.clearBoard();
    double rms = 0.0; float peak = 0.0f;
    play(q, 110.0, 0.2f, 60, rms, peak);
    check(std::fabs(rms - 0.2 / std::sqrt(2.0)) < 0.01, "an empty board changes nothing");
  }
  {  // a pedal that is off, and a knob that is a parameter
    q.loadFactory(3);
    double on = 0.0, off = 0.0; float peak = 0.0f;
    play(q, 110.0, 0.05f, 120, on, peak);
    for (int s = 0; s < kSlots; ++s) q.setOn(s, false);
    play(q, 110.0, 0.05f, 120, off, peak);
    check(on > off * 1.5 && std::fabs(off - 0.05 / std::sqrt(2.0)) < 0.005, "pedals switched off let the sound through untouched");
    check(q.getParameters().size() == kSlots * (kKnobs + 1) + 4, "every knob and switch is a parameter the host can record");
  }
  {  // state
    q.loadFactory(9);
    q.setKnob(1, 0, 0.83f);
    BoardLfo l; l.slot = 1; l.knob = 0; l.shape = 2; l.sync = false; l.rate = 0.7f; l.depth = 0.4f;
    q.setLfo(2, l);
    juce::MemoryBlock mb; q.getStateInformation(mb);
    LoopPedalsProcessor r2; r2.setStateInformation(mb.getData(), int(mb.getSize()));
    const BoardLayout a = q.layout(), b = r2.layout();
    bool same = r2.boardName == q.boardName;
    for (int s = 0; s < kSlots; ++s) { same = same && a.type[s] == b.type[s] && a.voice[s] == b.voice[s] && a.flags[s] == b.flags[s]; for (int k = 0; k < kKnobs; ++k) same = same && std::fabs(q.knob(s, k) - r2.knob(s, k)) < 1e-4f; }
    same = same && b.lfos[2].slot == 1 && b.lfos[2].shape == 2 && !b.lfos[2].sync && std::fabs(b.lfos[2].depth - 0.4f) < 1e-4f;
    check(same, "a board comes back from the saved state: pedals, characters, knobs, LFOs");
  }
  {  // an LFO moves a knob: a tremolo made from the mix of a silent... simply: the level of the tone control's lows on a low note
    q.clearBoard();
    q.setType(0, 35); q.setKnob(0, 0, 0.5f);
    BoardLfo l; l.slot = 0; l.knob = 0; l.shape = 3; l.sync = false; l.rate = 0.75f; l.depth = 1.0f;       // a square, a few Hz
    q.setLfo(0, l);
    double lo = 1e9, hi = 0.0;
    for (int w = 0; w < 40; ++w) { double rms = 0.0; float peak = 0.0f; play(q, 55.0, 0.1f, 8, rms, peak); lo = juce::jmin(lo, rms); hi = juce::jmax(hi, rms); }
    check(hi > lo * 1.8, "an LFO on a knob is heard");
  }
  {  // the tuner
    q.clearBoard();
    bool all = true;
    for (double hz : {41.2, 82.41, 110.0, 329.63}) {
      double rms = 0.0; float peak = 0.0f;
      play(q, hz, 0.2f, 80, rms, peak);
      const float got = q.tunerHz();
      std::printf("      tuner: %.2f Hz played, %.2f Hz shown\n", hz, double(got));
      all = all && std::fabs(double(got) / hz - 1.0) < 0.004;
    }
    check(all, "the tuner reads a bass's low E, a guitar's low E, A and high E within a few cents");
    q.tunerOn = true;
    double rms = 0.0; float peak = 0.0f;
    play(q, 110.0, 0.2f, 20, rms, peak);
    check(rms < 1e-6, "the output is silent while the tuner is on");
    q.tunerOn = false;
  }
  std::printf("\n%s\n", fails ? "Loop Pedals self-test FAILED" : "Loop Pedals self-test passed");
  return fails ? 1 : 0;
}
