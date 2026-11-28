#include "PedalsProcessor.h"

// ---- the boards that come with it ----
// type numbers and characters as in loopdraft/types.h and pedals.h. flags: 1 keeps the lows clean, 2 runs in time.
// 33 amp (0 clean, 1 crunch, 2 lead, 3 bass), 34 cab (0 4x12, 1 2x12 open, 2 1x10, 3 bass), 35 tone control.
#define S(type, voice, a, r, p3, p4, mix, flags) {type, voice, a, r, p3, p4, mix, flags}
#define NONE {0, 0, 0, 0, 0, 0, 0, 0}
const FactoryBoard kFactoryBoards[] = {
  {"Electric guitar", "Clean Sparkle", "A clean amp with a compressor in front, chorus and a room behind.",
   {S(6, 1, 0.45f, 0.4f, 0.5f, 0.6f, 1, 0), S(33, 0, 0.35f, 0.6f, 0.5f, 0.6f, 1, 0), S(34, 1, 0.5f, 0.6f, 0.5f, 0.3f, 1, 0), S(3, 1, 0.45f, 0.35f, 0.5f, 0.5f, 0.6f, 0),
    S(20, 0, 0.35f, 0.45f, 0.5f, 0.5f, 0.3f, 0), NONE, NONE, NONE}},
  {"Electric guitar", "Edge of Breakup", "A green overdrive pushing a crunch amp: clean when you play softly, grit when you dig in.",
   {S(1, 1, 0.35f, 0.55f, 0.55f, 0.5f, 1, 0), S(33, 1, 0.35f, 0.55f, 0.5f, 0.5f, 1, 0), S(34, 0, 0.5f, 0.55f, 0.5f, 0.35f, 1, 0), S(19, 0, 0.35f, 0.4f, 0.4f, 0.5f, 0.35f, 0),
    S(20, 0, 0.3f, 0.4f, 0.5f, 0.5f, 0.25f, 0), NONE, NONE, NONE}},
  {"Electric guitar", "Rock Crunch", "A booster into a stack. Chords crunch, single notes sing.",
   {S(9, 1, 0.45f, 0.5f, 0.5f, 0.5f, 1, 0), S(33, 1, 0.65f, 0.5f, 0.5f, 0.55f, 1, 0), S(34, 0, 0.55f, 0.5f, 0.5f, 0.4f, 1, 0), S(18, 1, 0.3f, 0.3f, 0.3f, 0.5f, 0.3f, 0),
    NONE, NONE, NONE, NONE}},
  {"Electric guitar", "Heavy Lead", "Gate, a tight boost, the lead channel, a 4x12, then delay and room for solos.",
   {S(21, 0, 0.35f, 0.4f, 0.8f, 0.3f, 1, 0), S(1, 1, 0.2f, 0.6f, 0.65f, 0.3f, 1, 0), S(33, 2, 0.6f, 0.5f, 0.5f, 0.55f, 1, 0), S(34, 0, 0.55f, 0.5f, 0.5f, 0.45f, 1, 0),
    S(35, 0, 0.5f, 0.42f, 0.55f, 0.45f, 1, 0), S(18, 0, 0.35f, 0.45f, 0.35f, 0.5f, 0.3f, 2), S(20, 0, 0.3f, 0.5f, 0.5f, 0.5f, 0.22f, 0), NONE}},
  {"Electric guitar", "Fuzz Garden", "A fuzz into a clean amp, a slow phaser and a tape echo.",
   {S(8, 0, 0.6f, 0.5f, 0.5f, 0.5f, 1, 0), S(33, 0, 0.3f, 0.5f, 0.5f, 0.5f, 1, 0), S(34, 0, 0.5f, 0.5f, 0.5f, 0.35f, 1, 0), S(4, 1, 0.5f, 0.2f, 0.5f, 0.5f, 0.6f, 0),
    S(19, 0, 0.4f, 0.5f, 0.45f, 0.5f, 0.35f, 0), NONE, NONE, NONE}},
  {"Electric guitar", "Funk Wah", "A compressor and a wah that opens with every stroke, into a clean amp.",
   {S(6, 1, 0.5f, 0.3f, 0.5f, 0.5f, 1, 0), S(15, 0, 0.6f, 0.5f, 0.5f, 0.5f, 1, 0), S(33, 0, 0.3f, 0.65f, 0.5f, 0.6f, 1, 0), S(34, 1, 0.5f, 0.6f, 0.5f, 0.3f, 1, 0),
    NONE, NONE, NONE, NONE}},
  {"Electric guitar", "Ambient Wash", "Play one note and let it bloom: shimmer, delay in time, a cloud of grains.",
   {S(6, 1, 0.5f, 0.4f, 0.5f, 0.6f, 1, 0), S(33, 0, 0.25f, 0.5f, 0.5f, 0.5f, 1, 0), S(34, 1, 0.5f, 0.5f, 0.5f, 0.3f, 1, 0), S(18, 0, 0.45f, 0.5f, 0.5f, 0.45f, 0.4f, 2),
    S(32, 0, 0.5f, 0.6f, 0.5f, 0.5f, 0.45f, 0), S(27, 0, 0.5f, 0.4f, 0.5f, 0.5f, 0.3f, 0), NONE, NONE}},
  {"Electric guitar", "Surf Tremolo", "A clean amp, tremolo and a splashing reverb.",
   {S(33, 0, 0.35f, 0.6f, 0.5f, 0.55f, 1, 0), S(34, 1, 0.5f, 0.6f, 0.5f, 0.3f, 1, 0), S(10, 0, 0.6f, 0.55f, 0.5f, 0.5f, 1, 0), S(20, 1, 0.5f, 0.5f, 0.5f, 0.6f, 0.4f, 0),
    NONE, NONE, NONE, NONE}},
  {"Bass guitar", "Clean Bass", "Even and full: compressor, tone control, a tube head and an 8x10.",
   {S(6, 1, 0.5f, 0.4f, 0.5f, 0.5f, 1, 0), S(35, 1, 0.58f, 0.45f, 0.52f, 0.4f, 1, 0), S(33, 3, 0.3f, 0.5f, 0.5f, 0.45f, 1, 0), S(34, 3, 0.5f, 0.55f, 0.5f, 0.4f, 1, 0),
    NONE, NONE, NONE, NONE}},
  {"Bass guitar", "Growl", "An overdrive on the upper half only, the lows stay clean; the head pushed.",
   {S(6, 1, 0.5f, 0.4f, 0.5f, 0.5f, 1, 0), S(1, 3, 0.55f, 0.55f, 0.5f, 0.5f, 0.7f, 1), S(33, 3, 0.55f, 0.55f, 0.5f, 0.5f, 1, 0), S(34, 3, 0.5f, 0.55f, 0.5f, 0.45f, 1, 0),
    NONE, NONE, NONE, NONE}},
  {"Bass guitar", "Fuzz Bass", "A wall of fuzz with the sub left alone underneath.",
   {S(8, 0, 0.65f, 0.45f, 0.5f, 0.5f, 0.8f, 1), S(33, 3, 0.35f, 0.5f, 0.5f, 0.45f, 1, 0), S(34, 3, 0.5f, 0.5f, 0.5f, 0.45f, 1, 0), S(21, 0, 0.3f, 0.4f, 0.8f, 0.3f, 1, 0),
    NONE, NONE, NONE, NONE}},
  {"Bass guitar", "Synth Bass", "An octave below, a filter that opens with every note: a bass guitar as a synth.",
   {S(16, 0, 0.6f, 0.45f, 0.5f, 0.3f, 1, 0), S(15, 0, 0.6f, 0.45f, 0.5f, 0.5f, 1, 0), S(6, 1, 0.55f, 0.3f, 0.5f, 0.5f, 1, 0), S(33, 3, 0.3f, 0.5f, 0.5f, 0.45f, 1, 0),
    S(34, 3, 0.5f, 0.5f, 0.5f, 0.4f, 1, 0), NONE, NONE, NONE}},
  {"Bass guitar", "Dub Bass", "Round and deep, no top at all.",
   {S(35, 1, 0.7f, 0.4f, 0.25f, 0.35f, 1, 0), S(6, 1, 0.55f, 0.5f, 0.5f, 0.5f, 1, 0), S(34, 3, 0.6f, 0.35f, 0.5f, 0.5f, 1, 0), NONE, NONE, NONE, NONE, NONE}},
  {"Synth", "Wide Pad", "Makes a plain pad wide and endless: ensemble chorus, shimmer, a long room.",
   {S(3, 2, 0.55f, 0.3f, 0.5f, 0.5f, 0.7f, 0), S(32, 0, 0.5f, 0.6f, 0.5f, 0.5f, 0.4f, 0), S(20, 0, 0.45f, 0.7f, 0.5f, 0.5f, 0.4f, 0), NONE, NONE, NONE, NONE, NONE}},
  {"Synth", "Dub Chords", "For short chord stabs: a phaser and a dub delay in time that swallows them.",
   {S(4, 1, 0.45f, 0.2f, 0.5f, 0.5f, 0.5f, 0), S(29, 0, 0.55f, 0.45f, 0.5f, 0.5f, 0.5f, 2), S(20, 0, 0.45f, 0.65f, 0.5f, 0.5f, 0.35f, 0), NONE, NONE, NONE, NONE, NONE}},
  {"Synth", "Lo-Fi Keys", "Old tape, a slow tremolo and a tape echo for keys and electric pianos.",
   {S(28, 0, 0.5f, 0.4f, 0.5f, 0.5f, 0.8f, 0), S(10, 0, 0.4f, 0.4f, 0.5f, 0.5f, 1, 0), S(19, 0, 0.35f, 0.45f, 0.4f, 0.5f, 0.3f, 0), NONE, NONE, NONE, NONE, NONE}},
  {"Synth", "Acid Driver", "Distortion with the lows kept clean and a delay in time: for a 303 line.",
   {S(7, 0, 0.55f, 0.5f, 0.5f, 0.5f, 0.6f, 1), S(18, 0, 0.35f, 0.45f, 0.4f, 0.5f, 0.3f, 2), S(20, 0, 0.3f, 0.4f, 0.5f, 0.5f, 0.2f, 0), NONE, NONE, NONE, NONE, NONE}},
  {"Synth", "Space Lead", "A little drive, a flanger sweeping slowly, delay and a long reverb.",
   {S(1, 2, 0.4f, 0.5f, 0.5f, 0.5f, 1, 0), S(12, 0, 0.5f, 0.2f, 0.5f, 0.5f, 0.5f, 0), S(18, 0, 0.4f, 0.5f, 0.45f, 0.5f, 0.35f, 2), S(20, 0, 0.45f, 0.7f, 0.5f, 0.5f, 0.35f, 0),
    NONE, NONE, NONE, NONE}},
  {"Synth", "Drum Machine Crush", "For a drum machine: punch, a crusher mixed in, a gated room.",
   {S(22, 0, 0.5f, 0.4f, 0.5f, 0.5f, 1, 0), S(23, 0, 0.4f, 0.5f, 0.5f, 0.5f, 0.3f, 0), S(25, 0, 0.5f, 0.4f, 0.5f, 0.5f, 0.25f, 0), NONE, NONE, NONE, NONE, NONE}},
};
#undef S
#undef NONE
const int kNumFactoryBoards = int(sizeof(kFactoryBoards) / sizeof(kFactoryBoards[0]));

LoopPedalsProcessor::LoopPedalsProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
  static const char* const names[kKnobs] = {"Knob 1", "Knob 2", "Knob 3", "Knob 4", "Mix"};
  for (int s = 0; s < kSlots; ++s) {
    for (int k = 0; k < kKnobs; ++k) {
      knobs_[s][k] = new juce::AudioParameterFloat(juce::ParameterID{"s" + juce::String(s) + "k" + juce::String(k), 1},
                                                   "Pedal " + juce::String(s + 1) + " " + names[k], juce::NormalisableRange<float>(0.0f, 1.0f), k == 4 ? 1.0f : 0.5f);
      addParameter(knobs_[s][k]);
    }
    on_[s] = new juce::AudioParameterBool(juce::ParameterID{"s" + juce::String(s) + "on", 1}, "Pedal " + juce::String(s + 1) + " On", true);
    addParameter(on_[s]);
  }
  addParameter(inGain = new juce::AudioParameterFloat(juce::ParameterID{"in", 1}, "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f));
  addParameter(outGain = new juce::AudioParameterFloat(juce::ParameterID{"out", 1}, "Output Level", juce::NormalisableRange<float>(-24.0f, 12.0f), 0.0f));
  addParameter(tempo = new juce::AudioParameterFloat(juce::ParameterID{"tempo", 1}, "Tempo (when the host has none)", juce::NormalisableRange<float>(40.0f, 240.0f), 120.0f));
  addParameter(inputMode = new juce::AudioParameterChoice(juce::ParameterID{"input", 1}, "Input", juce::StringArray{"Input 1 (mono)", "Input 2 (mono)", "Stereo"}, 0));
  for (auto& v : lfoNow) v.store(0.0f);
  chain_ = std::make_unique<ld::PedalChain>();
  chain_->prepare(48000.0);
  loadFactory(0);
}

bool LoopPedalsProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
  return out == juce::AudioChannelSet::stereo() && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void LoopPedalsProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  sr_ = sampleRate > 1000.0 ? sampleRate : 48000.0;
  chain_->prepare(sr_);
  const size_t n = size_t(juce::jmax(64, samplesPerBlock));
  l_.assign(n, 0.0f); r_.assign(n, 0.0f);
  for (auto& p : lfoPhase_) p = 0.0;
}

void LoopPedalsProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  juce::ScopedNoDenormals noDenormals;
  const int total = buffer.getNumSamples(), nIn = getTotalNumInputChannels(), nOut = buffer.getNumChannels();
  if (total <= 0 || nOut <= 0) return;
  { const juce::ScopedTryLock tl(lock_); if (tl.isLocked()) rt_ = layout_; }
  float bpm = tempo->get();
  bool haveBeat = false;
  double beat = 0.0;
  if (auto* ph = getPlayHead())
    if (const auto pos = ph->getPosition()) {
      if (const auto b = pos->getBpm()) if (*b > 20.0) bpm = float(*b);
      if (pos->getIsPlaying()) if (const auto q = pos->getPpqPosition()) { haveBeat = true; beat = *q; }
    }
  bpmNow.store(bpm, std::memory_order_relaxed);
  const float gInTo = juce::Decibels::decibelsToGain(inGain->get()), gOutTo = juce::Decibels::decibelsToGain(outGain->get());
  const int mode = inputMode->getIndex();
  const bool muted = tunerOn.load(std::memory_order_relaxed);
  float peakIn = 0.0f, peakOut = 0.0f;
  int w = ringW_.load(std::memory_order_relaxed);
  const int cap = int(l_.size());
  for (int off = 0; off < total;) {
    const int n = juce::jmin(64, total - off, cap);
    // what comes in
    const float* inL = buffer.getReadPointer(0, off);
    const float* inR = nIn > 1 && nOut > 1 ? buffer.getReadPointer(1, off) : inL;
    for (int i = 0; i < n; ++i) {
      gIn_ += 0.002f * (gInTo - gIn_);
      float a = inL[i], b = inR[i];
      if (mode == 0 || nIn < 2) b = a; else if (mode == 1) a = b;
      a *= gIn_; b *= gIn_;
      l_[size_t(i)] = a; r_[size_t(i)] = b;
      const float m = 0.5f * (a + b);
      ring_[w] = m; w = (w + 1) & (kRing - 1);
      peakIn = juce::jmax(peakIn, std::fabs(m));
    }
    // the board as it stands now: the knobs, and the LFOs on top of them
    ld::FxSlot slots[kSlots];
    for (int s = 0; s < kSlots; ++s) {
      ld::FxSlot& fs = slots[s];
      const int type = juce::jlimit(0, ld::kNumFxTypes - 1, rt_.type[s]);
      fs.type = ld::FxType(type);
      fs.voice = uint8_t(juce::jlimit(0, int(ld::kFxVoices[type]) - 1, rt_.voice[s]));
      fs.flags = uint8_t(rt_.flags[s] & (ld::fxCanSync(fs.type, fs.voice) ? 3 : 1));
      fs.bypass = !on_[s]->get();
      fs.amount = knobs_[s][0]->get(); fs.rate = knobs_[s][1]->get(); fs.p3 = knobs_[s][2]->get(); fs.p4 = knobs_[s][3]->get(); fs.mix = knobs_[s][4]->get();
    }
    for (int i = 0; i < kLfos; ++i) {
      const BoardLfo& lf = rt_.lfos[i];
      const double hz = lf.sync ? double(bpm) / 60.0 / double(ld::lfoBeats(lf.rate)) : double(ld::lfoHz(lf.rate));
      const bool locked = lf.sync && haveBeat;                      // with the host running: locked to its bars
      double ph = locked ? beat / double(ld::lfoBeats(lf.rate)) : lfoPhase_[i] + hz * double(n) / sr_;
      const double whole = std::floor(ph);
      if (whole != lfoWhole_[i]) {                                  // a new cycle: a new random step
        lfoWhole_[i] = locked ? whole : 0.0;
        seed_ = seed_ * 1664525u + 1013904223u;
        lfoRand_[i] = float(int32_t(seed_)) / 2147483648.0f;
      }
      const double fr = ph - whole;
      lfoPhase_[i] = fr;
      float v = 0.0f;
      switch (lf.shape) {
        case 1: v = float(fr < 0.5 ? 4.0 * fr - 1.0 : 3.0 - 4.0 * fr); break;
        case 2: v = float(1.0 - 2.0 * fr); break;
        case 3: v = fr < 0.5 ? 1.0f : -1.0f; break;
        case 4: v = lfoRand_[i]; break;
        default: v = float(std::sin(6.283185307179586 * fr)); break;
      }
      lfoNow[i].store(v, std::memory_order_relaxed);
      if (lf.slot < 0 || lf.slot >= kSlots || lf.depth <= 0.0f) continue;
      float& target = ld::fxSlotKnob(slots[lf.slot], juce::jlimit(0, kKnobs - 1, lf.knob));
      target = juce::jlimit(0.0f, 1.0f, target + v * juce::jmin(1.0f, lf.depth) * 0.5f);
    }
    chain_->process(l_.data(), r_.data(), n, slots, kSlots, bpm, haveBeat, beat);
    if (haveBeat) beat += double(n) / sr_ * double(bpm) / 60.0;
    // what goes out
    float* outL = buffer.getWritePointer(0, off);
    float* outR = nOut > 1 ? buffer.getWritePointer(1, off) : nullptr;
    for (int i = 0; i < n; ++i) {
      gOut_ += 0.002f * (gOutTo - gOut_);
      float a = l_[size_t(i)] * gOut_, b = r_[size_t(i)] * gOut_;
      if (!std::isfinite(a) || !std::isfinite(b)) { a = b = 0.0f; }
      a = a > 1.0f ? 1.0f : (a < -1.0f ? -1.0f : a);                 // nothing louder than full scale leaves the board
      b = b > 1.0f ? 1.0f : (b < -1.0f ? -1.0f : b);
      peakOut = juce::jmax(peakOut, std::fabs(a), std::fabs(b));
      if (muted) a = b = 0.0f;
      if (outR) { outL[i] = a; outR[i] = b; } else outL[i] = 0.5f * (a + b);
    }
    off += n;
  }
  for (int c = 2; c < nOut; ++c) buffer.clear(c, 0, total);
  ringW_.store(w, std::memory_order_relaxed);
  inPeak.store(juce::jmax(peakIn, inPeak.load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
  outPeak.store(juce::jmax(peakOut, outPeak.load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
}

// The note that is coming in: the strongest repeat of the last sixth of a second, from a low B on a bass up to
// the top of a guitar's neck. 0 when it is too quiet or nothing repeats.
float LoopPedalsProcessor::tunerHz() {
  static constexpr int kWin = 4096;
  float x[kRing];
  const int w = ringW_.load(std::memory_order_relaxed);
  for (int i = 0; i < kRing; ++i) x[i] = ring_[(w + i) & (kRing - 1)];
  double energy = 0.0;
  for (int i = 0; i < kWin; ++i) energy += double(x[i]) * double(x[i]);
  if (energy / double(kWin) < 1e-6) return 0.0f;
  const int minLag = juce::jmax(8, int(sr_ / 1400.0)), maxLag = juce::jmin(kRing - kWin - 1, int(sr_ / 27.0));
  std::vector<float> nsdf(size_t(maxLag + 2), 0.0f);
  for (int lag = minLag - 1; lag <= maxLag + 1; ++lag) {
    double ac = 0.0, m = 0.0;
    for (int i = 0; i < kWin; i += 2) { ac += double(x[i]) * double(x[i + lag]); m += double(x[i]) * double(x[i]) + double(x[i + lag]) * double(x[i + lag]); }
    nsdf[size_t(lag)] = m > 1e-12 ? float(2.0 * ac / m) : 0.0f;
  }
  float best = 0.0f;
  for (int lag = minLag; lag <= maxLag; ++lag) best = juce::jmax(best, nsdf[size_t(lag)]);
  if (best < 0.6f) return 0.0f;
  bool below = false;                                          // the first peak nearly as high as the highest, after the first dip
  for (int lag = minLag; lag <= maxLag; ++lag) {
    const float v = nsdf[size_t(lag)];
    if (v < 0.0f) below = true;
    if (below && v > best * 0.9f && v >= nsdf[size_t(lag - 1)] && v >= nsdf[size_t(lag + 1)]) {
      const float a = nsdf[size_t(lag - 1)], c = nsdf[size_t(lag + 1)], d = a - 2.0f * v + c;
      const float shift = std::fabs(d) > 1e-9f ? 0.5f * (a - c) / d : 0.0f;
      return float(sr_) / (float(lag) + juce::jlimit(-1.0f, 1.0f, shift));
    }
  }
  return 0.0f;
}

// ---- the board ----

void LoopPedalsProcessor::setType(int slot, int type) {
  if (slot < 0 || slot >= kSlots) return;
  { const juce::ScopedLock sl(lock_); layout_.type[slot] = juce::jlimit(0, ld::kNumFxTypes - 1, type); layout_.voice[slot] = 0; layout_.flags[slot] = 0; }
  for (int k = 0; k < kKnobs; ++k) setKnob(slot, k, k == 4 ? 1.0f : 0.5f);
  setOn(slot, true);
}
void LoopPedalsProcessor::setVoice(int slot, int voice) {
  if (slot < 0 || slot >= kSlots) return;
  const juce::ScopedLock sl(lock_);
  layout_.voice[slot] = juce::jlimit(0, int(ld::kFxVoices[layout_.type[slot]]) - 1, voice);
}
void LoopPedalsProcessor::setFlags(int slot, int flags) {
  if (slot < 0 || slot >= kSlots) return;
  const juce::ScopedLock sl(lock_);
  layout_.flags[slot] = flags & 3;
}
void LoopPedalsProcessor::swapSlots(int a, int b) {
  if (a < 0 || b < 0 || a >= kSlots || b >= kSlots || a == b) return;
  {
    const juce::ScopedLock sl(lock_);
    std::swap(layout_.type[a], layout_.type[b]); std::swap(layout_.voice[a], layout_.voice[b]); std::swap(layout_.flags[a], layout_.flags[b]);
    for (auto& lf : layout_.lfos) { if (lf.slot == a) lf.slot = b; else if (lf.slot == b) lf.slot = a; }
  }
  for (int k = 0; k < kKnobs; ++k) { const float va = knob(a, k), vb = knob(b, k); setKnob(a, k, vb); setKnob(b, k, va); }
  const bool oa = isOn(a), ob = isOn(b);
  setOn(a, ob); setOn(b, oa);
}
void LoopPedalsProcessor::setLfo(int i, const BoardLfo& l) {
  if (i < 0 || i >= kLfos) return;
  const juce::ScopedLock sl(lock_);
  layout_.lfos[i] = l;
  layout_.lfos[i].shape = juce::jlimit(0, ld::kNumLfoShapes - 1, l.shape);
  layout_.lfos[i].rate = juce::jlimit(0.0f, 1.0f, l.rate); layout_.lfos[i].depth = juce::jlimit(0.0f, 1.0f, l.depth);
  if (l.slot >= kSlots) layout_.lfos[i].slot = -1;
}
void LoopPedalsProcessor::clearBoard() {
  { const juce::ScopedLock sl(lock_); layout_ = BoardLayout(); }
  for (int s = 0; s < kSlots; ++s) { for (int k = 0; k < kKnobs; ++k) setKnob(s, k, k == 4 ? 1.0f : 0.5f); setOn(s, true); }
  boardName = "Empty board";
}
void LoopPedalsProcessor::loadFactory(int index) {
  if (index < 0 || index >= kNumFactoryBoards) return;
  const FactoryBoard& fb = kFactoryBoards[index];
  clearBoard();
  for (int s = 0; s < 8; ++s) {
    const FactorySlot& f = fb.slots[s];
    if (f.type <= 0 || f.type >= ld::kNumFxTypes) continue;
    { const juce::ScopedLock sl(lock_); layout_.type[s] = f.type; layout_.voice[s] = juce::jlimit(0, int(ld::kFxVoices[f.type]) - 1, f.voice); layout_.flags[s] = f.flags & 3; }
    setKnob(s, 0, f.a); setKnob(s, 1, f.r); setKnob(s, 2, f.p3); setKnob(s, 3, f.p4); setKnob(s, 4, f.mix);
  }
  boardName = fb.name;
}

std::unique_ptr<juce::XmlElement> LoopPedalsProcessor::boardXml() const {
  auto xml = std::make_unique<juce::XmlElement>("Board");
  xml->setAttribute("name", boardName);
  const BoardLayout lay = layout();
  for (int s = 0; s < kSlots; ++s) {
    auto* e = xml->createNewChildElement("Pedal");
    e->setAttribute("type", lay.type[s]); e->setAttribute("character", lay.voice[s]); e->setAttribute("flags", lay.flags[s]); e->setAttribute("on", isOn(s) ? 1 : 0);
    for (int k = 0; k < kKnobs; ++k) e->setAttribute("k" + juce::String(k), double(knob(s, k)));
  }
  for (int i = 0; i < kLfos; ++i) {
    auto* e = xml->createNewChildElement("Lfo");
    const BoardLfo& l = lay.lfos[i];
    e->setAttribute("pedal", l.slot); e->setAttribute("knob", l.knob); e->setAttribute("shape", l.shape); e->setAttribute("sync", l.sync ? 1 : 0);
    e->setAttribute("rate", double(l.rate)); e->setAttribute("depth", double(l.depth));
  }
  return xml;
}
void LoopPedalsProcessor::applyBoardXml(const juce::XmlElement& xml) {
  if (!xml.hasTagName("Board")) return;
  clearBoard();
  int s = 0, i = 0;
  for (auto* e : xml.getChildWithTagNameIterator("Pedal")) {
    if (s >= kSlots) break;
    const int type = juce::jlimit(0, ld::kNumFxTypes - 1, e->getIntAttribute("type"));
    { const juce::ScopedLock sl(lock_); layout_.type[s] = type; layout_.voice[s] = juce::jlimit(0, int(ld::kFxVoices[type]) - 1, e->getIntAttribute("character")); layout_.flags[s] = e->getIntAttribute("flags") & 3; }
    for (int k = 0; k < kKnobs; ++k) setKnob(s, k, float(e->getDoubleAttribute("k" + juce::String(k), k == 4 ? 1.0 : 0.5)));
    setOn(s, e->getIntAttribute("on", 1) != 0);
    ++s;
  }
  for (auto* e : xml.getChildWithTagNameIterator("Lfo")) {
    if (i >= kLfos) break;
    BoardLfo l;
    l.slot = juce::jlimit(-1, kSlots - 1, e->getIntAttribute("pedal", -1)); l.knob = juce::jlimit(0, kKnobs - 1, e->getIntAttribute("knob"));
    l.shape = e->getIntAttribute("shape"); l.sync = e->getIntAttribute("sync", 1) != 0;
    l.rate = float(e->getDoubleAttribute("rate", 0.4)); l.depth = float(e->getDoubleAttribute("depth", 0.5));
    setLfo(i++, l);
  }
  boardName = xml.getStringAttribute("name", "Board");
}

juce::File LoopPedalsProcessor::userDir() {
  return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Loop Pedals").getChildFile("Boards");
}
juce::StringArray LoopPedalsProcessor::userBoards() const {
  juce::StringArray names;
  for (const auto& f : userDir().findChildFiles(juce::File::findFiles, false, "*.lpboard")) names.add(f.getFileNameWithoutExtension());
  names.sort(true);
  return names;
}
bool LoopPedalsProcessor::saveUser(const juce::String& name) {
  const juce::String clean = juce::File::createLegalFileName(name.trim());
  if (clean.isEmpty() || !userDir().createDirectory()) return false;
  boardName = clean;
  return boardXml()->writeTo(userDir().getChildFile(clean + ".lpboard"));
}
bool LoopPedalsProcessor::loadUser(const juce::String& name) {
  const auto xml = juce::parseXML(userDir().getChildFile(juce::File::createLegalFileName(name) + ".lpboard"));
  if (!xml || !xml->hasTagName("Board")) return false;
  applyBoardXml(*xml);
  boardName = name;
  return true;
}
bool LoopPedalsProcessor::deleteUser(const juce::String& name) {
  return userDir().getChildFile(juce::File::createLegalFileName(name) + ".lpboard").deleteFile();
}

void LoopPedalsProcessor::getStateInformation(juce::MemoryBlock& dest) {
  juce::XmlElement root("LoopPedals");
  root.setAttribute("version", 1);
  root.setAttribute("in", double(inGain->get())); root.setAttribute("out", double(outGain->get()));
  root.setAttribute("tempo", double(tempo->get())); root.setAttribute("input", inputMode->getIndex());
  root.addChildElement(boardXml().release());
  copyXmlToBinary(root, dest);
}
void LoopPedalsProcessor::setStateInformation(const void* data, int size) {
  const auto root = getXmlFromBinary(data, size);
  if (!root || !root->hasTagName("LoopPedals")) return;
  inGain->setValueNotifyingHost(inGain->convertTo0to1(float(root->getDoubleAttribute("in", 0.0))));
  outGain->setValueNotifyingHost(outGain->convertTo0to1(float(root->getDoubleAttribute("out", 0.0))));
  tempo->setValueNotifyingHost(tempo->convertTo0to1(float(root->getDoubleAttribute("tempo", 120.0))));
  inputMode->setValueNotifyingHost(inputMode->convertTo0to1(float(juce::jlimit(0, 2, root->getIntAttribute("input", 0)))));
  if (const auto* b = root->getChildByName("Board")) applyBoardXml(*b);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LoopPedalsProcessor(); }
