// The Loop Master editor: scenes, drum grid, piano roll, sound and mix controls, and the library.
// Everything that touches the session holds the processor's lock.
#include "Editor.h"

namespace {

namespace col {
const juce::Colour chassis(0xff17181c), faceplate(0xff202227), recess(0xff131418), line(0xff353942);
const juce::Colour silk(0xffebe5d5), muted(0xff969185), ink(0xff1b1c20);
const juce::Colour orange(0xffff6d1f), cyan(0xff29d3e8), yellow(0xffffca28), green(0xff3be083), red(0xffe53935);
const juce::Colour pads[8] = {juce::Colour(0xffe84738), juce::Colour(0xffff8a24), juce::Colour(0xffffc929), juce::Colour(0xff29d4e8),
                              juce::Colour(0xff5ea8ff), juce::Colour(0xff4ae08a), juce::Colour(0xffbf6bff), juce::Colour(0xffff5c94)};
const juce::Colour synths[4] = {juce::Colour(0xffff9426), juce::Colour(0xff29d4e8), juce::Colour(0xffff4da6), juce::Colour(0xff5ee870)};
}  // namespace col

const char* const kPadNames[8] = {"Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc"};
const char* const kTrackNames[5] = {"Drums", "Bass", "Lead", "Syn 3", "Syn 4"};
const char* const kSceneNames[8] = {"A", "B", "C", "D", "E", "F", "G", "H"};
const char* const kKits[10] = {"808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break", "Hardcore Gabber",
                               "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore"};
const char* const kInstruments[8] = {"Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead"};
const char* const kEngines[4] = {"Subtractive", "FM 2-Op", "Pluck", "Vector"};
const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Every pedal: the six standard ones of Loop Draft and the stompbox collection.
// A pedal has up to four knobs and a character switch; the names here follow engine/include/loopdraft/pedals.h.
struct PedalSpec {
  int type;
  const char* code;
  const char* name;
  juce::uint32 top, bottom;             // the enclosure
  bool lightInk;
  juce::uint32 cap;                     // the knob caps
  const char* knobs[4];
  const char* voices[4];
  const char* about[4];                 // one line about each character
};
const PedalSpec kPedals[] = {
  {1, "OD", "Overdrive", 0xffe0971b, 0xffb56e0c, false, 0xff1d1b18, {"Drive", "Tone", "Level", "Body"},
   {"Classic", "Green", "Gold", "Blue"},
   {"The Loop Draft drive: one knob", "Mids pushed forward, lows kept clean", "Clean signal underneath, lots of headroom", "Flat and open, low gain, follows your touch"}},
  {2, "BIT", "Bitcrush", 0xff218c9c, 0xff135b68, true, 0xffe9e4d6, {"Crush", "Rate", "Level", ""}, {"Classic"}, {"Fewer bits, lower sample rate"}},
  {3, "CHO", "Chorus", 0xff2b6ec2, 0xff1a4682, true, 0xffe9e4d6, {"Depth", "Speed", "Mix", "Tone"},
   {"Classic", "Ensemble", "Wide", "Tri"},
   {"The Loop Draft chorus", "One dark voice, the bucket-brigade sound", "Two voices against each other, hardly any wobble", "Three voices, like a string machine"}},
  {4, "PHA", "Phaser", 0xffd95621, 0xffa1350d, true, 0xff1d1b18, {"Depth", "Speed", "Feedbk", "Mix"},
   {"Classic", "Script", "Stone", "Vibe"},
   {"The Loop Draft phaser", "Smooth, no feedback. FEEDBK past noon: the later, sharper one", "Deep and resonant", "Uneven stages and a lamp that throbs"}},
  {5, "LPF", "Low-Pass", 0xff1f7a52, 0xff124d33, true, 0xffe9e4d6, {"Cutoff", "Reso", "Env", "Drive"},
   {"Classic", "Ladder", "Band"},
   {"The Loop Draft filter", "Four stages with feedback, fat. ENV lets your playing open it", "Only a slice around the cutoff"}},
  {6, "COMP", "Compressor", 0xffc22d32, 0xff871b1f, true, 0xffe9e4d6, {"Sustain", "Attack", "Level", "Blend"},
   {"Classic", "Squash", "Optical", "Studio"},
   {"The Loop Draft compressor: one knob", "Hard squeeze with a push in the mids", "Slow and gentle, lets go lazily", "Fast and a little gritty"}},
  {7, "DST", "Distortion", 0xffe0661b, 0xffa8430c, true, 0xff1d1b18, {"Gain", "Tone", "Level", "Mids"},
   {"Rodent", "Orange", "Stack", "Metal"},
   {"Hard clipping behind a slow chip: thick, rounded edges", "Hard clipping, TONE scoops the middle", "An amplifier pushed hard, mids forward", "Tight lows, huge gain, middle scooped out"}},
  {8, "FUZZ", "Fuzz", 0xff7a3fb5, 0xff4f2480, true, 0xffd8d2c0, {"Fuzz", "Tone", "Level", "Bias"},
   {"Round", "Wall", "Bender", "Octave"},
   {"Warm, cleans up when you play softly. BIAS makes it sputter", "Two stages in a row: endless sustain, scooped", "Bright and biting, cuts off like a gate", "An octave above rings on top"}},
  {9, "BST", "Booster", 0xffe8e2d0, 0xffbdb59c, false, 0xff1d1b18, {"Boost", "Treble", "Level", "Bass"},
   {"Clean", "Treble", "Preamp"},
   {"Just more, with two tone knobs", "Mostly the top, pushed into soft clipping", "The front end of a tape echo: thicker, brighter"}},
  {10, "TREM", "Tremolo", 0xff3e9a45, 0xff25692b, true, 0xffe9e4d6, {"Depth", "Speed", "Shape", "Pan"},
   {"Bias", "Optical", "Harmonic", "Chop"},
   {"The smooth throb of an amplifier", "Quick up, lazy down", "Lows and highs take turns: it swirls", "On or off"}},
  {11, "VIB", "Vibrato", 0xff2f8fc9, 0xff1b5f8c, true, 0xff1d1b18, {"Depth", "Speed", "Mix", "Tone"},
   {"Pitch", "Tape"}, {"Pitch wobble. MIX at noon turns it into a chorus", "Slow wow with a quick flutter on top"}},
  {12, "FLG", "Flanger", 0xff8a5ad6, 0xff5b3699, true, 0xffe9e4d6, {"Feedbk", "Speed", "Depth", "Manual"},
   {"Jet", "Mistress", "Zero"},
   {"The classic take-off", "Feedback turned round: hollow and glassy", "Sweeps through the dry signal, which cancels for a moment"}},
  {13, "ROTO", "Rotary", 0xff8c5a32, 0xff5e3a1d, true, 0xffe9e4d6, {"Depth", "Speed", "Balance", "Drive"},
   {"Cabinet", "Simple"}, {"Horn for the highs, drum for the lows, at two speeds", "One speaker turning"}},
  {14, "WAH", "Wah", 0xff33363d, 0xff1c1e22, true, 0xffc9ccd2, {"Depth", "Speed", "Range", "Sharp"},
   {"Cry", "Clyde", "Bass"}, {"Narrow and sharp", "Lower and rounder, growls a little", "Keeps everything below the sweep"}},
  {15, "AWAH", "Auto-Wah", 0xffe3c22b, 0xffb09210, false, 0xff1d1b18, {"Sens", "Peak", "Range", "Decay"},
   {"Up", "Down", "Low-pass"}, {"Opens when you dig in", "Closes when you dig in", "The funk filter with the lows left in"}},
  {16, "OCT", "Octaver", 0xff6b4a2e, 0xff45301d, true, 0xffe9e4d6, {"Octave", "Tone", "Dry", "Second"},
   {"Sub", "Up"}, {"One octave down. SECOND adds two down", "One octave up. SECOND adds the one below"}},
  {17, "RING", "Ring Mod", 0xff7d8794, 0xff525b66, true, 0xff1d1b18, {"Mix", "Pitch", "Wobble", "W-Speed"},
   {"Sine", "Square"}, {"Bells and robots", "Harsher, more overtones"}},
  {18, "DLY", "Delay", 0xffdce6f2, 0xffa9b8cc, false, 0xff1d1b18, {"Level", "Time", "Repeats", "Tone"},
   {"Digital", "Analog", "Ping-pong"},
   {"Every repeat a clean copy", "Every repeat duller and softer than the last", "Repeats jump from side to side"}},
  {19, "TAPE", "Tape Echo", 0xff2e5e46, 0xff1b3b2b, true, 0xffe9e4d6, {"Level", "Time", "Repeats", "Age"},
   {"Single", "Multi-head", "Worn"},
   {"One head. REPEATS all the way up feeds itself", "Three heads along the tape", "An old machine: dark and unsteady"}},
  {20, "VERB", "Reverb", 0xff5e7fa3, 0xff3b5575, true, 0xffe9e4d6, {"Level", "Decay", "Tone", "Pre-dly"},
   {"Spring", "Plate", "Hall", "Cloud"},
   {"Band-limited, with the drip of the springs", "Bright and dense", "Darker and longer", "Nearly endless, slowly turning"}},
  {21, "GATE", "Gate", 0xff26282d, 0xff121316, true, 0xffc9ccd2, {"Thresh", "Decay", "Range", "Hold"},
   {"Smooth", "Hard"}, {"Fades the noise out", "Slams shut"}},
};
// What knob `i` of a pedal is called on a given character. "" = this knob does nothing there.
const char* knobName(const PedalSpec& sp, int voice, int i) {
  const bool classic = voice == 0 && sp.type <= 6;               // the standard pedals as Loop Draft has them
  if (classic) {
    if (i == 2) return "Level";
    if (i == 3) return "";
    if (i == 1 && (sp.type == 1 || sp.type == 6)) return "";
  }
  if (i == 3) {
    if (sp.type == 8 && (voice == 1 || voice == 3)) return "";
    if (sp.type == 18 && voice == 1) return "Wobble";
    if (sp.type == 20 && voice == 0) return "Dwell";
  }
  if (i == 2 && sp.type == 13 && voice == 1) return "";
  return sp.knobs[i];
}
// What the pedal does to a wave, as a little scope trace: x 0 .. 1, result -1 .. 1.
float pedalTrace(int type, float x) {
  const float t = juce::MathConstants<float>::twoPi * x;
  const auto pulses = [x](float width, float wobble) {
    float y = 0.0f;
    for (int n = 0; n < 5; ++n) {
      const float c = 0.08f + 0.2f * float(n) + wobble * std::sin(float(n) * 2.1f), d = (x - c) / width;
      y += std::exp(-d * d) * std::pow(0.62f, float(n));
    }
    return y * 1.8f - 0.9f;
  };
  switch (type) {
    case 1: return std::tanh(2.5f * std::sin(t * 3.0f)) * 0.85f;
    case 2: return std::round(std::sin(t * 2.0f) * 3.0f) / 3.0f * 0.85f;
    case 3: return (std::sin(t * 3.0f) + 0.6f * std::sin(t * 3.4f + 0.6f)) * 0.55f;
    case 4: return std::sin(t * 3.0f + 2.2f * std::sin(t)) * 0.85f;
    case 5: { const float d = (x - 0.62f) / 0.07f; return x < 0.62f ? 0.1f + 0.75f * std::exp(-d * d) : 0.85f * std::exp(-d * d) - 0.9f * (1.0f - std::exp(-(x - 0.62f) * 9.0f)) + 0.1f * std::exp(-(x - 0.62f) * 9.0f); }
    case 6: return std::sin(t * 7.0f) * juce::jmin(0.55f, 1.6f * std::exp(-2.2f * x));
    case 7: return juce::jlimit(-0.6f, 0.6f, 2.5f * std::sin(t * 3.0f)) * 1.3f;
    case 8: return std::tanh(9.0f * (std::sin(t * 3.0f) + 0.35f)) * 0.8f;
    case 9: return std::sin(t * 5.0f) * (0.2f + 0.7f * x);
    case 10: return std::sin(t * 14.0f) * (0.5f + 0.4f * std::sin(t * 2.0f));
    case 11: return std::sin(t * 5.0f + 2.5f * std::sin(t * 1.5f)) * 0.8f;
    case 12: return 0.85f - 1.7f * std::fabs(std::sin(juce::MathConstants<float>::pi * (1.0f + 5.0f * x * x)));
    case 13: return std::sin(t * 9.0f) * std::cos(t) * 0.85f;
    case 14: { const float d = (x - 0.5f - 0.25f * std::sin(t)) / 0.09f; return 1.7f * std::exp(-d * d) - 0.85f; }
    case 15: return std::sin(t * 10.0f * (1.0f - 0.5f * x)) * std::exp(-2.5f * x) * 0.9f;
    case 16: return (std::sin(t * 6.0f) * 0.4f + std::sin(t * 3.0f) * 0.55f);
    case 17: return std::sin(t * 2.0f) * std::sin(t * 17.0f) * 0.85f;
    case 18: return pulses(0.018f, 0.0f);
    case 19: return pulses(0.035f, 0.015f);
    case 20: return std::sin(t * 23.0f + 3.0f * std::sin(t * 7.0f)) * std::exp(-3.5f * x) * 0.9f;
    case 21: return x < 0.55f ? std::sin(t * 9.0f) * 0.8f : 0.0f;
    default: return 0.0f;
  }
}
const PedalSpec* pedalSpec(int type) {
  for (const auto& sp : kPedals) if (sp.type == type) return &sp;
  return nullptr;
}
// how the menu is ordered: families, each with its pedals (by type number)
struct PedalFamily { const char* name; int types[8]; };
const PedalFamily kPedalFamilies[] = {
  {"Drive", {1, 7, 8, 9, 2, 0, 0, 0}},
  {"Dynamics", {6, 21, 0, 0, 0, 0, 0, 0}},
  {"Modulation", {3, 12, 4, 10, 11, 13, 17, 0}},
  {"Filter and pitch", {5, 14, 15, 16, 0, 0, 0, 0}},
  {"Echo and space", {18, 19, 20, 0, 0, 0, 0, 0}},
};

// ---- knob movements inside a groove: what can move, what it is called, and its range ----
struct AutoParam { int pid; const char* name; float lo, hi; };
const AutoParam kAutoParams[] = {
  {ld::P_FX_LEVEL, "Level", 0.0f, 1.0f}, {ld::P_FX_DRIVE, "Drive", 0.0f, 1.0f}, {ld::P_FX_EQ_LOW, "Low EQ", -12.0f, 12.0f},
  {ld::P_FX_EQ_HIGH, "High EQ", -12.0f, 12.0f}, {ld::P_FX_DELAY, "Delay send", 0.0f, 0.8f}, {ld::P_FX_REVERB, "Reverb send", 0.0f, 0.8f},
  {ld::P_SYN_CUTOFF, "Cutoff", 60.0f, 12000.0f}, {ld::P_SYN_RES, "Resonance", 0.5f, 15.0f}, {ld::P_SYN_DETUNE, "Detune", -50.0f, 50.0f},
  {ld::P_SYN_ATTACK, "Attack", 0.001f, 1.0f}, {ld::P_SYN_DECAY, "Decay", 0.02f, 3.0f}, {ld::P_SYN_SUSTAIN, "Sustain", 0.0f, 1.0f},
  {ld::P_SYN_RELEASE, "Release", 0.01f, 3.0f}, {ld::P_SYN_LEVEL, "Synth level", 0.0f, 1.0f}, {ld::P_SYN_LFO_RATE, "LFO rate", 0.05f, 20.0f},
  {ld::P_SYN_LFO_DEPTH, "LFO depth", 0.0f, 1.0f}, {ld::P_SYN_FM_RATIO, "FM ratio", 0.5f, 8.0f}, {ld::P_SYN_FM_INDEX, "FM index", 0.0f, 8.0f},
  {ld::P_SYN_FM_DECAY, "FM decay", 0.01f, 3.0f}, {ld::P_SYN_FM_FB, "FM feedback", 0.0f, 1.0f}, {ld::P_SYN_WARMTH, "Warmth", 0.0f, 1.0f},
  {ld::P_SYN_MOTION, "Motion", 0.0f, 1.0f}, {ld::P_SYN_FOLD, "Fold", 0.0f, 1.0f}, {ld::P_SYN_VEC_X, "Vector X", 0.0f, 1.0f},
  {ld::P_SYN_VEC_Y, "Vector Y", 0.0f, 1.0f}, {ld::P_SYN_PLUCK_BRIGHT, "Pluck bright", 0.0f, 1.0f},
};
// False for controls that cannot move inside a groove (tempo, pad settings, switches).
bool autoRange(int pid, float& lo, float& hi) {
  int slot, knob;
  if (ld::fxSlotParam(pid, slot, knob)) { lo = 0.0f; hi = 1.0f; return true; }
  for (const auto& a : kAutoParams) if (a.pid == pid) { lo = a.lo; hi = a.hi; return true; }
  return false;
}
// 0 .. 1 across the control's range; the cutoff runs in octaves.
float autoToUnit(int pid, float v) {
  float lo = 0.0f, hi = 1.0f;
  autoRange(pid, lo, hi);
  v = juce::jlimit(lo, hi, v);
  return pid == ld::P_SYN_CUTOFF ? std::log(v / lo) / std::log(hi / lo) : (v - lo) / (hi - lo);
}
float autoFromUnit(int pid, float u) {
  float lo = 0.0f, hi = 1.0f;
  autoRange(pid, lo, hi);
  u = juce::jlimit(0.0f, 1.0f, u);
  return pid == ld::P_SYN_CUTOFF ? lo * std::pow(hi / lo, u) : lo + u * (hi - lo);
}

// ---- a MIDI file as a pattern ----
// part 0: every note becomes a drum hit (General MIDI drum notes, or the pads' own notes).
// part 1..: the notes go to that synth, moved to the nearest notes of the project's scale.
bool midiToScene(const juce::File& file, const ld::Project& pr, int part, ld::Scene& out) {
  juce::FileInputStream in(file);
  juce::MidiFile mf;
  if (!in.openedOk() || !mf.readFrom(in)) return false;
  const int tpq = mf.getTimeFormat();
  if (tpq <= 0) return false;                                // SMPTE time: no bars to go by
  const double perStep = double(tpq) / 4.0;
  out = ld::Scene();
  int last = -1;
  bool tenOnly = false;
  if (part == 0)                                             // a file with a drum channel: only that channel is drums
    for (int t = 0; t < mf.getNumTracks() && !tenOnly; ++t)
      for (const auto* h : *mf.getTrack(t)) if (h->message.isNoteOn() && h->message.getChannel() == 10) { tenOnly = true; break; }
  for (int t = 0; t < mf.getNumTracks(); ++t) {
    for (const auto* h : *mf.getTrack(t)) {
      const juce::MidiMessage& m = h->message;
      if (!m.isNoteOn()) continue;
      const int step = int(std::floor(m.getTimeStamp() / perStep + 0.5));
      if (step < 0 || step >= ld::kMaxSteps) continue;
      const int note = m.getNoteNumber();
      const bool strong = m.getVelocity() >= 100;
      if (part == 0) {
        if (tenOnly && m.getChannel() != 10) continue;
        int pad = -1;
        for (int i = 0; i < ld::kNumPads && pad < 0; ++i) if (int(pr.pads[i].gmNote) == note) pad = i;
        if (pad < 0) {
          switch (note) {
            case 35: case 36: pad = 0; break;
            case 38: case 40: pad = 1; break;
            case 39: pad = 2; break;
            case 42: case 44: pad = 3; break;
            case 46: case 49: case 51: case 52: case 55: case 57: case 59: pad = 4; break;
            case 37: pad = 5; break;
            case 41: case 43: case 45: case 47: case 48: case 50: pad = 6; break;
            default: pad = 7; break;
          }
        }
        if (out.drums.steps[pad][step] < 2) out.drums.steps[pad][step] = strong ? 2 : 1;
      } else {
        if (m.getChannel() == 10) continue;
        ld::SynthPattern& pat = out.synth[part - 1];
        if (pat.count >= ld::kMaxNotes) continue;
        const int deg = ld::midiToDegree(pr, part - 1, note);
        bool twice = false;
        for (int i = 0; i < pat.count && !twice; ++i) twice = pat.notes[i].step == step && pat.notes[i].degree == deg;
        if (twice) continue;
        ld::Note n;
        n.step = uint16_t(step); n.degree = int8_t(juce::jlimit(-21, 35, deg)); n.vel = strong ? 2 : 1;
        const double end = h->noteOffObject ? h->noteOffObject->message.getTimeStamp() : m.getTimeStamp() + perStep;
        n.len = uint8_t(juce::jlimit(1, 255, int(std::floor((end - m.getTimeStamp()) / perStep + 0.5))));
        pat.notes[pat.count++] = n;
      }
      last = juce::jmax(last, step);
    }
  }
  if (last < 0) return false;
  out.bars = juce::jlimit(1, ld::kMaxBars, last / 16 + 1);
  return true;
}

juce::String noteName(int midi) { return juce::String(kNoteNames[((midi % 12) + 12) % 12]) + juce::String(midi / 12 - 1); }

class Look : public juce::LookAndFeel_V4 {
 public:
  Look() {
    setColour(juce::ResizableWindow::backgroundColourId, col::chassis);
    setColour(juce::TextButton::buttonColourId, col::faceplate);
    setColour(juce::TextButton::buttonOnColourId, col::orange);
    setColour(juce::TextButton::textColourOffId, col::silk);
    setColour(juce::TextButton::textColourOnId, col::ink);
    setColour(juce::ComboBox::backgroundColourId, col::recess);
    setColour(juce::ComboBox::outlineColourId, col::line);
    setColour(juce::ComboBox::textColourId, col::silk);
    setColour(juce::ComboBox::arrowColourId, col::muted);
    setColour(juce::PopupMenu::backgroundColourId, col::faceplate);
    setColour(juce::PopupMenu::textColourId, col::silk);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, col::orange);
    setColour(juce::PopupMenu::highlightedTextColourId, col::ink);
    setColour(juce::Slider::backgroundColourId, col::recess);
    setColour(juce::Slider::trackColourId, col::orange.withAlpha(0.85f));
    setColour(juce::Slider::textBoxTextColourId, col::silk);
    setColour(juce::Slider::textBoxOutlineColourId, col::line);
    setColour(juce::Label::textColourId, col::silk);
    setColour(juce::ListBox::backgroundColourId, col::recess);
    setColour(juce::ListBox::outlineColourId, col::line);
    setColour(juce::ScrollBar::thumbColourId, col::line);
  }
  // The knob of a pedal: a cap in the pedal's own colour with a pointer, on a dark skirt.
  void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl) override {
    const float size = float(juce::jmin(w, h)) - 4.0f, cx = float(x) + float(w) * 0.5f, cy = float(y) + float(h) * 0.5f, rad = size * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const juce::Colour cap = sl.findColour(juce::Slider::rotarySliderFillColourId), ink = sl.findColour(juce::Slider::rotarySliderOutlineColourId);
    g.setColour(ink.withAlpha(0.55f));
    for (int i = 0; i <= 10; ++i) {                              // the scale
      const float a = a0 + float(i) / 10.0f * (a1 - a0);
      g.fillEllipse(cx + std::sin(a) * rad - 1.2f, cy - std::cos(a) * rad - 1.2f, 2.4f, 2.4f);
    }
    const float r1 = rad * 0.8f, r2 = rad * 0.62f;
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(cx - r1, cy - r1 + 2.0f, r1 * 2.0f, r1 * 2.0f);
    g.setColour(juce::Colour(0xff0e0e10));
    g.fillEllipse(cx - r1, cy - r1, r1 * 2.0f, r1 * 2.0f);
    g.setGradientFill(juce::ColourGradient(cap.brighter(0.25f), cx, cy - r2, cap.darker(0.25f), cx, cy + r2, false));
    g.fillEllipse(cx - r2, cy - r2, r2 * 2.0f, r2 * 2.0f);
    g.setColour(cap.getBrightness() > 0.5f ? juce::Colour(0xff141414) : juce::Colour(0xfff4f1e8));
    g.drawLine(cx + std::sin(ang) * r2 * 0.25f, cy - std::cos(ang) * r2 * 0.25f, cx + std::sin(ang) * r1 * 0.95f, cy - std::cos(ang) * r1 * 0.95f, 2.6f);
  }
};

// One control of the project: a bar with its name and value. Controls the host can automate go through the host's
// parameter, so the host sees the movement; the rest go straight to the session.
class ParamBar : public juce::Component {
 public:
  ParamBar(LoopMasterProcessor& proc, juce::String label, int paramId, int index, float lo, float hi,
           std::function<juce::String(float)> format, float centre = 0.0f)
      : p_(proc), label_(std::move(label)), pid_(paramId), index_(index), format_(std::move(format)) {
    slider_.setSliderStyle(juce::Slider::LinearBar);
    slider_.setRange(double(lo), double(hi), 0.0);
    if (centre > lo && centre < hi) slider_.setSkewFactorFromMidPoint(double(centre));
    slider_.setTextBoxIsEditable(false);
    slider_.textFromValueFunction = [this](double v) { return label_ + "  " + format_(float(v)); };
    slider_.setDoubleClickReturnValue(true, double(ld::Session::defaultParam(paramId, index)));
    slider_.onDragStart = [this] {
      dragging_ = true;
      p_.touch.pid = pid_; p_.touch.index = index_; p_.touch.value = float(slider_.getValue());
      if (auto* hp = p_.hostParam(pid_, index_)) hp->beginChangeGesture();
      else { const juce::ScopedLock sl(p_.lock()); p_.session().beginEdit(); }
    };
    slider_.onDragEnd = [this] {
      dragging_ = false;
      p_.touch.pid = -1;
      if (auto* hp = p_.hostParam(pid_, index_)) hp->endChangeGesture();
    };
    slider_.onValueChange = [this] {
      if (setting_) return;
      const float v = float(slider_.getValue());
      if (dragging_) p_.touch.value = v;
      if (auto* hp = p_.hostParam(pid_, index_)) {
        hp->setValueNotifyingHost(hp->convertTo0to1(v));
      } else {
        const juce::ScopedLock sl(p_.lock());
        if (!dragging_) p_.session().beginEdit();           // a double-click reset or a wheel step is one undo step
        p_.session().setParam(pid_, index_, v);
      }
    };
    addAndMakeVisible(slider_);
    slider_.updateText();                               // the name is part of the text from the start
  }
  void setIndex(int index) {
    if (index == index_) return;
    index_ = index;
    slider_.setDoubleClickReturnValue(true, double(ld::Session::defaultParam(pid_, index_)));
    slider_.updateText();
  }
  void setLabel(const juce::String& label) { if (label != label_) { label_ = label; slider_.updateText(); } }
  // call with the lock held
  void refresh() {
    if (dragging_) return;
    const double v = double(p_.session().getParam(pid_, index_));
    if (std::abs(v - slider_.getValue()) > 1e-6) {
      setting_ = true;
      slider_.setValue(v, juce::dontSendNotification);
      setting_ = false;
    }
  }
  void resized() override { slider_.setBounds(getLocalBounds()); }

 private:
  LoopMasterProcessor& p_;
  juce::Slider slider_;
  juce::String label_;
  int pid_, index_;
  std::function<juce::String(float)> format_;
  bool dragging_ = false, setting_ = false;
};

// One knob of a pedal, with its name underneath. While it is turned the name gives way to the value.
class Knob : public juce::Component {
 public:
  Knob(LoopMasterProcessor& proc, int paramId) : p_(proc), pid_(paramId) {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    slider_.setRange(0.0, 1.0, 0.0);
    slider_.setDoubleClickReturnValue(true, double(ld::Session::defaultParam(paramId, 0)));
    slider_.onDragStart = [this] {
      dragging_ = true;
      p_.touch.pid = pid_; p_.touch.index = index_; p_.touch.value = float(slider_.getValue());
      const juce::ScopedLock sl(p_.lock()); p_.session().beginEdit(); repaint();
    };
    slider_.onDragEnd = [this] { dragging_ = false; p_.touch.pid = -1; repaint(); };
    slider_.onValueChange = [this] {
      if (setting_) return;
      const juce::ScopedLock sl(p_.lock());
      if (!dragging_) p_.session().beginEdit(); else p_.touch.value = float(slider_.getValue());
      p_.session().setParam(pid_, index_, float(slider_.getValue()));
      repaint();
    };
    addAndMakeVisible(slider_);
  }
  void set(int paramId, int index, const juce::String& label, juce::Colour cap, juce::Colour ink) {
    pid_ = paramId; index_ = index;
    if (label != label_ || ink != ink_ || cap != cap_) {
      label_ = label; ink_ = ink; cap_ = cap;
      slider_.setColour(juce::Slider::rotarySliderFillColourId, cap);
      slider_.setColour(juce::Slider::rotarySliderOutlineColourId, ink);
      repaint();
    }
  }
  // call with the lock held
  void refresh() {
    if (dragging_) return;
    const double v = double(p_.session().getParam(pid_, index_));
    if (std::abs(v - slider_.getValue()) > 1e-6) { setting_ = true; slider_.setValue(v, juce::dontSendNotification); setting_ = false; }
  }
  void resized() override { slider_.setBounds(getLocalBounds().withTrimmedBottom(16)); }
  void paint(juce::Graphics& g) override {
    g.setColour(ink_);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    const juce::String text = dragging_ ? juce::String(juce::roundToInt(slider_.getValue() * 100.0)) : label_.toUpperCase();
    g.drawText(text, getLocalBounds().removeFromBottom(16), juce::Justification::centred, false);
  }

 private:
  LoopMasterProcessor& p_;
  juce::Slider slider_;
  juce::String label_;
  juce::Colour ink_, cap_;
  int pid_, index_ = 0;
  bool dragging_ = false, setting_ = false;
};

juce::String fmtPct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + "%"; }
juce::String fmtInt(float v) { return juce::String(juce::roundToInt(v)); }
juce::String fmtOne(float v) { return juce::String(v, 1); }
juce::String fmtSecs(float v) { return juce::String(v, 2) + " s"; }
juce::String fmtMs(float v) { return juce::String(juce::roundToInt(v * 1000.0f)) + " ms"; }
juce::String fmtSemis(float v) { return (v >= 0 ? "+" : "") + juce::String(juce::roundToInt(v)) + " st"; }
juce::String fmtCents(float v) { return (v >= 0 ? "+" : "") + juce::String(juce::roundToInt(v)) + " c"; }
juce::String fmtDb(float v) { return (v >= 0 ? "+" : "") + juce::String(juce::roundToInt(v)) + " dB"; }
juce::String fmtPan(float v) {
  if (v < -0.02f) return "L" + juce::String(juce::roundToInt(-v * 100.0f));
  if (v > 0.02f) return "R" + juce::String(juce::roundToInt(v * 100.0f));
  return "centre";
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------

struct LoopMasterEditor::Body : public juce::Component, private juce::Timer, private juce::ListBoxModel {
  LoopMasterProcessor& p;
  Look look;

  // what is being edited
  int tab = 0, pad = 0, synth = 0, bar = 0, rollBase = 0, libKind = 0;
  bool accent = false, slide = false, loopBar = true, recKnobs = false, browse = true;
  juce::String message;

  // header
  juce::TextButton openBtn{"OPEN"}, saveBtn{"SAVE"}, undoBtn{"UNDO"}, redoBtn{"REDO"}, playBtn{"PLAY"}, syncBtn{"FOLLOW HOST"}, songBtn{"SONG"};
  juce::TextButton bpmDown{"-"}, bpmUp{"+"};
  juce::TextButton sceneBtn[8], tabBtn[6], barBtn[16];
  juce::TextButton barAdd{"+ BAR"}, barDel{"- BAR"}, barDouble{"x2"}, loopBtn{"LOOP BAR"}, recBtn{"REC KNOBS"}, browseBtn{"PATTERNS"};

  // the pattern browser beside the two editors: the library's grooves and .mid files from a folder
  struct Pattern { juce::String name; bool midi = false; juce::File file; std::shared_ptr<ld::Scene> scene; };
  struct PatternModel : public juce::ListBoxModel {
    Body* b = nullptr;
    int getNumRows() override { return int(b->patterns.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override {
      if (row < 0 || row >= int(b->patterns.size())) return;
      const Pattern& pt = b->patterns[size_t(row)];
      const bool playing = row == b->previewRow;
      if (playing) g.fillAll(col::green.withAlpha(0.85f)); else if (selected) g.fillAll(col::orange.withAlpha(0.85f));
      g.setColour(selected || playing ? col::ink : col::silk);
      g.setFont(juce::Font(juce::FontOptions(13.0f)));
      g.drawText(pt.name, 6, 0, width - 46, height, juce::Justification::centredLeft, true);
      g.setColour(selected || playing ? col::ink : col::muted);
      g.setFont(juce::Font(juce::FontOptions(10.0f)));
      g.drawText(pt.midi ? "MIDI" : "", width - 40, 0, 36, height, juce::Justification::centredRight, false);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent&) override { b->listen(row); }
  } patternModel;
  std::vector<Pattern> patterns;
  std::vector<std::shared_ptr<ld::Scene>> grooveScenes;      // one per library entry (null where it is not a groove)
  juce::ListBox patternList;
  juce::TextButton putBtn[8], folderBtn{"MIDI FOLDER..."};
  juce::Rectangle<int> browseArea;
  int previewRow = -1, patternsFor = -99;
  bool previewStartedPlay = false;

  // song
  int songSel = 0, laneDrag = -1;
  juce::TextButton songAdd[8], songRemove{"REMOVE"}, songLeft{"<"}, songRight{">"}, songEndBtn, songPlayBtn{"PLAY THE SONG"}, laneClear[ld::kSceneLanes];
  std::vector<juce::Rectangle<int>> songRects;
  juce::Rectangle<int> songArea, laneArea[ld::kSceneLanes], laneTitle;
  juce::ComboBox midiBox;
  std::unique_ptr<juce::FileChooser> chooser;

  // drums
  juce::ComboBox kitBox;
  juce::TextButton accentBtn{"ACCENT"}, padBtn[8];
  std::unique_ptr<ParamBar> hardness, padVol, padPitch, padDecay, padPan;

  // synth
  juce::TextButton synthBtn[4], addSynthBtn{"+ TRACK"}, slideBtn{"SLIDE"}, chordBtn{"CHORD"}, rollDown{"LOWER"}, rollUp{"HIGHER"};
  juce::ComboBox instBox, engineBox;
  juce::OwnedArray<ParamBar> synthBars;

  // mix
  juce::OwnedArray<ParamBar> mixBars;      // 4 per track, then master, swing, delay feedback
  juce::TextButton muteBtn[5];

  // pedals
  int fxTrack = 0, pedalPage = 0;
  juce::Rectangle<int> chipArea[ld::kFxSlots];
  int slotOf(int i) const { return pedalPage * 3 + i; }
  juce::TextButton fxTrackBtn[5], pedalOn[3], pedalLeft[3], pedalRight[3];
  juce::ComboBox pedalBox[3];
  std::unique_ptr<Knob> pedalKnob[3][4];
  juce::TextButton voiceBtn[3][4];
  juce::Rectangle<int> pedalArea[3], traceArea[3], aboutArea[3];

  // library
  juce::TextButton kindBtn[3], loadBtn{"LOAD INTO PROJECT"}, tempoBtn{"TAKE TEMPO AND KEY"};
  juce::ComboBox styleBox;
  juce::ListBox list;
  std::vector<int> shown;                  // indices into p.library()

  // geometry of the two grids, set in resized()
  juce::Rectangle<int> gridArea, rollArea, pageArea;
  static constexpr int kRollRows = 15;

  explicit Body(LoopMasterProcessor& proc) : p(proc), list("library", this) {
    setLookAndFeel(&look);
    tab = juce::jlimit(0, 5, juce::SystemStats::getEnvironmentVariable("LOOPMASTER_TAB", "0").getIntValue());   // for screenshots
    {  // also for screenshots: LOOPMASTER_PEDALS="7:1,19:2,20:3" puts those pedals (type:character) on the bass
      juce::StringArray want;
      want.addTokens(juce::SystemStats::getEnvironmentVariable("LOOPMASTER_PEDALS", ""), ",", "");
      const juce::ScopedLock sl(p.lock());
      for (int i = 0; i < juce::jmin(3, want.size()); ++i) {
        if (want[i].isEmpty()) continue;
        p.session().setFxSlot(1, i, want[i].upToFirstOccurrenceOf(":", false, false).getIntValue(), 0.6f, 0.45f, i == 2 && want.size() > 3);
        p.session().setFxVoice(1, i, want[i].fromFirstOccurrenceOf(":", false, false).getIntValue());
        fxTrack = 1;
      }
      if (juce::SystemStats::getEnvironmentVariable("LOOPMASTER_DEMO", "").isNotEmpty()) {   // a song and a knob movement to look at
        ld::Session& s = p.session();
        s.growScene(0, 4); s.copyScene(0, 4); s.growScene(1, 2);
        s.chainInsert(4, 4); s.chainInsert(5, 1); s.chainInsert(6, 4);
        for (int pt = 0; pt <= 16; ++pt) s.writeLanePoint(0, ld::P_SYN_CUTOFF, 0, pt, 200.0f * std::pow(30.0f, float(pt) / 16.0f), false);
        for (int pt = 0; pt <= 16; ++pt) s.writeLanePoint(0, ld::P_FX_REVERB, 0, pt, pt % 4 == 3 ? 0.6f : 0.05f, false);
        s.setFxSlot(1, 3, 20, 0.5f, 0.5f, false); s.setFxSlot(1, 4, 18, 0.5f, 0.5f, false);
        s.writeLanePoint(0, ld::fxSlotParamId(4, 2), 1, 8, 0.9f, false);
      }
    }
    const auto button = [this](juce::TextButton& b, std::function<void()> fn) {
      b.onClick = std::move(fn);
      addAndMakeVisible(b);
    };

    button(openBtn, [this] { openProject(); });
    button(saveBtn, [this] { saveProject(); });
    button(undoBtn, [this] { const juce::ScopedLock sl(p.lock()); p.session().undo(); });
    button(redoBtn, [this] { const juce::ScopedLock sl(p.lock()); p.session().redo(); });
    button(playBtn, [this] {
      if (p.hostPlays()) return;
      const juce::ScopedLock sl(p.lock());
      if (p.session().engine().isPlaying()) p.session().engine().stop(); else p.session().engine().play(scene());
    });
    button(syncBtn, [this] {
      if (auto* prm = p.params.getParameter("sync")) prm->setValueNotifyingHost(prm->getValue() > 0.5f ? 0.0f : 1.0f);
    });
    button(songBtn, [this] { p.setSongMode(!p.songMode()); });
    button(bpmDown, [this] { nudgeBpm(-1); });
    button(bpmUp, [this] { nudgeBpm(1); });
    for (int i = 0; i < 8; ++i) {
      sceneBtn[i].setButtonText(kSceneNames[i]);
      button(sceneBtn[i], [this, i] { p.setScene(i); bar = 0; });
    }
    for (int i = 0; i < 6; ++i) {
      static const char* const tabs[6] = {"DRUMS", "SYNTH", "PEDALS", "MIX", "SONG", "LIBRARY"};
      tabBtn[i].setButtonText(tabs[i]);
      button(tabBtn[i], [this, i] { stopListening(); tab = i; message.clear(); showTab(); });
    }
    for (int i = 0; i < 16; ++i) {
      barBtn[i].setButtonText(juce::String(i + 1));
      button(barBtn[i], [this, i] { bar = i; });
    }
    // the bar editor: bars are added, removed and doubled here, and the one that is open can go round on its own
    button(barAdd, [this] {
      const juce::ScopedLock sl(p.lock());
      const int bars = int(p.session().project().scenes[scene()].bars);
      if (bars < ld::kMaxBars) { p.session().setSceneBars(scene(), bars + 1); bar = bars; }
    });
    button(barDel, [this] {
      const juce::ScopedLock sl(p.lock());
      const int bars = int(p.session().project().scenes[scene()].bars);
      if (bars > 1) p.session().setSceneBars(scene(), bars - 1);
    });
    button(barDouble, [this] {
      const juce::ScopedLock sl(p.lock());
      const int bars = int(p.session().project().scenes[scene()].bars);
      p.session().growScene(scene(), juce::jmin(int(ld::kMaxBars), bars * 2));
    });
    button(loopBtn, [this] { loopBar = !loopBar; });
    button(browseBtn, [this] { browse = !browse; if (!browse) stopListening(); showTab(); });
    button(recBtn, [this] {
      recKnobs = !recKnobs;
      message = recKnobs ? "REC KNOBS: while it plays, hold a knob or a bar and move it. The movement stays in this groove (see SONG)." : juce::String();
    });
    barAdd.setTooltip("One more bar at the end");
    barDouble.setTooltip("Twice as long: the new bars repeat what is there");

    // pattern browser
    patternModel.b = this;
    patternList.setModel(&patternModel);
    patternList.setRowHeight(22);
    addChildComponent(patternList);
    for (int i = 0; i < 8; ++i) {
      putBtn[i].setButtonText(kSceneNames[i]);
      putBtn[i].onClick = [this, i] { putPattern(i); };
      addChildComponent(putBtn[i]);
    }
    folderBtn.onClick = [this] {
      chooser = std::make_unique<juce::FileChooser>("A folder with MIDI files", juce::File(p.midiFolder));
      chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& fc) {
        const juce::File f = fc.getResult();
        if (f == juce::File()) return;
        p.midiFolder = f.getFullPathName();
        patternsFor = -99;
      });
    };
    addChildComponent(folderBtn);
    {  // every groove of the library, once, as patterns
      auto tmp = std::make_unique<ld::Project>();
      for (const auto& e : p.library()) {
        std::shared_ptr<ld::Scene> sc;
        if (e.kind == "groove") {
          *tmp = ld::Project();
          tmp->activeSynths = ld::kNumSynths;
          ld::ContentTarget tg;
          tg.scene = 0; tg.tempoAndKey = false;
          std::string err;
          if (ld::applyContent(std::string(e.data, size_t(e.size)), *tmp, tg, err, nullptr)) sc = std::make_shared<ld::Scene>(tmp->scenes[0]);
        }
        grooveScenes.push_back(sc);
      }
    }

    // song
    for (int i = 0; i < 8; ++i) {
      songAdd[i].setButtonText(kSceneNames[i]);
      songAdd[i].onClick = [this, i] {
        const juce::ScopedLock sl(p.lock());
        const int len = p.session().project().chainLen;
        if (len >= ld::kMaxChain) { message = "The song is full."; return; }
        songSel = juce::jlimit(0, len - 1, songSel);
        p.session().chainInsert(songSel + 1, i);
        songSel = songSel + 1;
      };
      addChildComponent(songAdd[i]);
    }
    songRemove.onClick = [this] { const juce::ScopedLock sl(p.lock()); p.session().chainRemove(songSel); songSel = juce::jmax(0, songSel - 1); };
    songLeft.onClick = [this] { const juce::ScopedLock sl(p.lock()); if (songSel > 0) { p.session().chainMove(songSel, songSel - 1); --songSel; } };
    songRight.onClick = [this] { const juce::ScopedLock sl(p.lock()); if (songSel + 1 < p.session().project().chainLen) { p.session().chainMove(songSel, songSel + 1); ++songSel; } };
    songEndBtn.onClick = [this] { p.setSongLoop(!p.songLoop()); };
    songPlayBtn.onClick = [this] { p.setSongMode(!p.songMode()); };
    for (auto* b : {&songRemove, &songLeft, &songRight, &songEndBtn, &songPlayBtn}) addChildComponent(b);
    for (int i = 0; i < ld::kSceneLanes; ++i) {
      laneClear[i].setButtonText("CLEAR");
      laneClear[i].onClick = [this, i] { const juce::ScopedLock sl(p.lock()); p.session().clearLane(scene(), i); };
      addChildComponent(laneClear[i]);
    }
    midiBox.addItemList({"MIDI: by channel", "MIDI: drums", "MIDI: bass", "MIDI: lead", "MIDI: syn 3", "MIDI: syn 4"}, 1);
    midiBox.onChange = [this] {
      if (auto* prm = p.params.getParameter("miditrack"))
        prm->setValueNotifyingHost(prm->convertTo0to1(float(juce::jmax(0, midiBox.getSelectedId() - 1))));
    };
    addAndMakeVisible(midiBox);

    // drums
    for (int i = 0; i < 10; ++i) kitBox.addItem(kKits[i], i + 1);
    kitBox.onChange = [this] {
      const int k = kitBox.getSelectedId() - 1;
      const juce::ScopedLock sl(p.lock());
      if (k >= 0 && k != int(p.session().getParam(ld::P_DRUM_ENGINE, 0) + 0.5f)) p.session().setDrumEngine(k);
    };
    addChildComponent(kitBox);
    accentBtn.onClick = [this] { accent = !accent; };
    addChildComponent(accentBtn);
    for (int i = 0; i < 8; ++i) {
      padBtn[i].setButtonText(kPadNames[i]);
      padBtn[i].onClick = [this, i] {
        pad = i;
        const juce::ScopedLock sl(p.lock());
        p.session().engine().triggerPad(i, accent ? 2 : 1);
      };
      addChildComponent(padBtn[i]);
    }
    hardness = std::make_unique<ParamBar>(p, "Hardness", ld::P_KIT_HARDNESS, 0, 0.0f, 1.0f, fmtPct);
    padVol = std::make_unique<ParamBar>(p, "Volume", ld::P_PAD_VOL, 0, 0.0f, 1.0f, fmtPct);
    padPitch = std::make_unique<ParamBar>(p, "Pitch", ld::P_PAD_PITCH, 0, -12.0f, 12.0f, fmtSemis);
    padDecay = std::make_unique<ParamBar>(p, "Decay", ld::P_PAD_DECAY, 0, 0.03f, 1.5f, fmtSecs);
    padPan = std::make_unique<ParamBar>(p, "Pan", ld::P_PAD_PAN, 0, -1.0f, 1.0f, fmtPan);
    for (auto* b : {hardness.get(), padVol.get(), padPitch.get(), padDecay.get(), padPan.get()}) addChildComponent(b);

    // synth
    for (int i = 0; i < 4; ++i) {
      synthBtn[i].setButtonText(kTrackNames[i + 1]);
      synthBtn[i].onClick = [this, i] { synth = i; };
      addChildComponent(synthBtn[i]);
    }
    addSynthBtn.onClick = [this] {
      const juce::ScopedLock sl(p.lock());
      const int n = int(p.session().getParam(ld::P_ACTIVE_SYNTHS, 0) + 0.5f);
      if (n < ld::kNumSynths) { p.session().setActiveSynths(n + 1); synth = n; }
    };
    slideBtn.onClick = [this] { slide = !slide; };
    chordBtn.onClick = [this] {
      const juce::ScopedLock sl(p.lock());
      p.session().beginEdit();
      p.session().setParam(ld::P_SYN_CHORD, synth, p.session().getParam(ld::P_SYN_CHORD, synth) > 0.5f ? 0.0f : 1.0f);
    };
    rollDown.onClick = [this] { rollBase = juce::jmax(-14, rollBase - 7); };
    rollUp.onClick = [this] { rollBase = juce::jmin(14, rollBase + 7); };
    for (auto* b : {&addSynthBtn, &slideBtn, &chordBtn, &rollDown, &rollUp}) addChildComponent(b);
    for (int i = 0; i < 8; ++i) instBox.addItem(kInstruments[i], i + 1);
    instBox.onChange = [this] {
      const int k = instBox.getSelectedId() - 1;
      const juce::ScopedLock sl(p.lock());
      if (k >= 0 && k != int(p.session().getParam(ld::P_SYN_INSTRUMENT, synth) + 0.5f)) p.session().applyInstrument(synth, k);
    };
    for (int i = 0; i < 4; ++i) engineBox.addItem(kEngines[i], i + 1);
    engineBox.onChange = [this] {
      const int k = engineBox.getSelectedId() - 1;
      const juce::ScopedLock sl(p.lock());
      if (k >= 0 && k != int(p.session().getParam(ld::P_SYN_MODE, synth) + 0.5f)) {
        p.session().beginEdit();
        p.session().setParam(ld::P_SYN_MODE, synth, float(k));
      }
    };
    addChildComponent(instBox);
    addChildComponent(engineBox);
    const auto sb = [this](const char* name, int pid, float lo, float hi, std::function<juce::String(float)> f, float centre = 0.0f) {
      addChildComponent(synthBars.add(new ParamBar(p, name, pid, 0, lo, hi, std::move(f), centre)));
    };
    sb("Cutoff", ld::P_SYN_CUTOFF, 80.0f, 8000.0f, fmtInt, 900.0f);
    sb("Resonance", ld::P_SYN_RES, 0.5f, 15.0f, fmtOne);
    sb("Level", ld::P_SYN_LEVEL, 0.0f, 1.0f, fmtPct);
    sb("Detune", ld::P_SYN_DETUNE, -30.0f, 30.0f, fmtCents);
    sb("Attack", ld::P_SYN_ATTACK, 0.002f, 0.4f, fmtMs, 0.05f);
    sb("Decay", ld::P_SYN_DECAY, 0.03f, 1.5f, fmtSecs);
    sb("Sustain", ld::P_SYN_SUSTAIN, 0.0f, 1.0f, fmtPct);
    sb("Release", ld::P_SYN_RELEASE, 0.01f, 1.5f, fmtSecs);
    sb("Warmth", ld::P_SYN_WARMTH, 0.0f, 1.0f, fmtPct);
    sb("Motion", ld::P_SYN_MOTION, 0.0f, 1.0f, fmtPct);
    sb("Fold", ld::P_SYN_FOLD, 0.0f, 1.0f, fmtPct);
    sb("Slide time", ld::P_SYN_SLIDE_TIME, 0.01f, 0.4f, fmtMs);
    sb("LFO rate", ld::P_SYN_LFO_RATE, 0.1f, 12.0f, fmtOne);
    sb("LFO depth", ld::P_SYN_LFO_DEPTH, 0.0f, 1.0f, fmtPct);
    sb("FM ratio", ld::P_SYN_FM_RATIO, 0.5f, 8.0f, fmtOne);
    sb("FM index", ld::P_SYN_FM_INDEX, 0.0f, 8.0f, fmtOne);
    sb("FM decay", ld::P_SYN_FM_DECAY, 0.01f, 2.0f, fmtSecs);
    sb("FM feedback", ld::P_SYN_FM_FB, 0.0f, 1.0f, fmtPct);
    sb("Vector X", ld::P_SYN_VEC_X, 0.0f, 1.0f, fmtPct);
    sb("Vector Y", ld::P_SYN_VEC_Y, 0.0f, 1.0f, fmtPct);
    sb("Pluck bright", ld::P_SYN_PLUCK_BRIGHT, 0.0f, 1.0f, fmtPct);

    // mix
    for (int t = 0; t < 5; ++t) {
      addChildComponent(mixBars.add(new ParamBar(p, "Level", ld::P_FX_LEVEL, t, 0.0f, 1.0f, fmtPct)));
      addChildComponent(mixBars.add(new ParamBar(p, "Drive", ld::P_FX_DRIVE, t, 0.0f, 1.0f, fmtPct)));
      addChildComponent(mixBars.add(new ParamBar(p, "Delay", ld::P_FX_DELAY, t, 0.0f, 0.8f, fmtPct)));
      addChildComponent(mixBars.add(new ParamBar(p, "Reverb", ld::P_FX_REVERB, t, 0.0f, 0.8f, fmtPct)));
      addChildComponent(mixBars.add(new ParamBar(p, "Low EQ", ld::P_FX_EQ_LOW, t, -12.0f, 12.0f, fmtDb)));
      addChildComponent(mixBars.add(new ParamBar(p, "High EQ", ld::P_FX_EQ_HIGH, t, -12.0f, 12.0f, fmtDb)));
      muteBtn[t].setButtonText("MUTE");
      muteBtn[t].onClick = [this, t] {
        const juce::ScopedLock sl(p.lock());
        p.session().beginEdit();
        p.session().setParam(ld::P_FX_MUTE, t, p.session().getParam(ld::P_FX_MUTE, t) > 0.5f ? 0.0f : 1.0f);
      };
      addChildComponent(muteBtn[t]);
    }
    addChildComponent(mixBars.add(new ParamBar(p, "Master", ld::P_MASTER, 0, 0.0f, 1.0f, fmtPct)));
    addChildComponent(mixBars.add(new ParamBar(p, "Swing", ld::P_SWING, 0, 0.0f, 0.5f, fmtPct)));
    addChildComponent(mixBars.add(new ParamBar(p, "Delay feedback", ld::P_DELAY_FB, 0, 0.0f, 0.8f, fmtPct)));

    // pedals
    for (int t = 0; t < 5; ++t) {
      fxTrackBtn[t].setButtonText(kTrackNames[t]);
      fxTrackBtn[t].onClick = [this, t] { fxTrack = t; };
      addChildComponent(fxTrackBtn[t]);
    }
    for (int i = 0; i < 3; ++i) {
      pedalBox[i].addItem("Empty slot", 1);
      for (const auto& fam : kPedalFamilies) {
        pedalBox[i].addSectionHeading(fam.name);
        for (int type : fam.types) if (const PedalSpec* sp = pedalSpec(type)) pedalBox[i].addItem(sp->name, sp->type + 1);
      }
      pedalBox[i].onChange = [this, i] {
        const int type = juce::jmax(0, pedalBox[i].getSelectedId() - 1);
        const juce::ScopedLock sl(p.lock());
        const ld::FxSlot cur = p.session().project().fx[fxTrack].slots[slotOf(i)];
        if (type == int(cur.type)) return;
        // a new pedal starts in the middle of its range, and switched on
        p.session().setFxSlot(fxTrack, slotOf(i), type, 0.5f, 0.4f, false);
      };
      pedalOn[i].onClick = [this, i] {
        const juce::ScopedLock sl(p.lock());
        const ld::FxSlot cur = p.session().project().fx[fxTrack].slots[slotOf(i)];
        p.session().setFxSlot(fxTrack, slotOf(i), int(cur.type), cur.amount, cur.rate, !cur.bypass);
      };
      pedalLeft[i].setButtonText("<");
      pedalRight[i].setButtonText(">");
      pedalLeft[i].onClick = [this, i] { swapPedals(slotOf(i), slotOf(i) - 1); };
      pedalRight[i].onClick = [this, i] { swapPedals(slotOf(i), slotOf(i) + 1); };
      static const int ids[3][4] = {{ld::P_FX_SLOT1_AMOUNT, ld::P_FX_SLOT1_RATE, ld::P_FX_SLOT1_P3, ld::P_FX_SLOT1_P4},
                                    {ld::P_FX_SLOT2_AMOUNT, ld::P_FX_SLOT2_RATE, ld::P_FX_SLOT2_P3, ld::P_FX_SLOT2_P4},
                                    {ld::P_FX_SLOT3_AMOUNT, ld::P_FX_SLOT3_RATE, ld::P_FX_SLOT3_P3, ld::P_FX_SLOT3_P4}};
      for (int k = 0; k < 4; ++k) {
        pedalKnob[i][k] = std::make_unique<Knob>(p, ids[i][k]);
        addChildComponent(pedalKnob[i][k].get());
        voiceBtn[i][k].onClick = [this, i, k] { const juce::ScopedLock sl(p.lock()); p.session().setFxVoice(fxTrack, slotOf(i), k); };
        addChildComponent(voiceBtn[i][k]);
      }
      for (juce::Component* c : {static_cast<juce::Component*>(&pedalBox[i]), static_cast<juce::Component*>(&pedalOn[i]),
                                 static_cast<juce::Component*>(&pedalLeft[i]), static_cast<juce::Component*>(&pedalRight[i])})
        addChildComponent(c);
    }

    // library
    static const char* const kinds[3] = {"GROOVES", "SYNTH SOUNDS", "DRUM KITS"};
    for (int i = 0; i < 3; ++i) {
      kindBtn[i].setButtonText(kinds[i]);
      kindBtn[i].onClick = [this, i] { libKind = i; fillStyles(); filterLibrary(); };
      addChildComponent(kindBtn[i]);
    }
    loadBtn.onClick = [this] { loadSelected(); };
    tempoBtn.setClickingTogglesState(true);
    tempoBtn.setToggleState(true, juce::dontSendNotification);
    styleBox.onChange = [this] { filterLibrary(); };
    list.setRowHeight(24);
    addChildComponent(loadBtn);
    addChildComponent(tempoBtn);
    addChildComponent(styleBox);
    addChildComponent(list);
    fillStyles();
    filterLibrary();

    showTab();
    startTimerHz(20);
  }

  ~Body() override {
    stopTimer();
    {
      const juce::ScopedLock sl(p.lock());
      p.session().preview(nullptr);                             // nothing of the editor stays behind in the sound
      p.session().engine().setLoopBar(-1);
      p.touch.pid = -1;
    }
    setLookAndFeel(nullptr);
  }

  int scene() const { return juce::jlimit(0, ld::kNumScenes - 1, int(p.params.getRawParameterValue("scene")->load() + 0.5f)); }

  void nudgeBpm(int by) {
    if (p.hostLeads()) return;
    const juce::ScopedLock sl(p.lock());
    p.session().beginEdit();
    p.session().setParam(ld::P_BPM, 0, juce::jlimit(40.0f, 240.0f, p.session().getParam(ld::P_BPM, 0) + float(by)));
  }

  void openProject() {
    chooser = std::make_unique<juce::FileChooser>("Open a Loop Draft project", juce::File(), "*.ldp");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
      const juce::File f = fc.getResult();
      if (f == juce::File()) return;
      message = p.openProjectFile(f) ? "Opened " + f.getFileName() : f.getFileName() + " is not a Loop Draft project";
      bar = 0;
    });
  }

  void saveProject() {
    chooser = std::make_unique<juce::FileChooser>("Save as a Loop Draft project", juce::File(), "*.ldp");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this](const juce::FileChooser& fc) {
      juce::File f = fc.getResult();
      if (f == juce::File()) return;
      if (!f.hasFileExtension("ldp")) f = f.withFileExtension("ldp");
      message = p.saveProjectFile(f) ? "Saved " + f.getFileName() : "Could not save " + f.getFileName();
    });
  }

  // ---- pattern browser ----
  // What the list offers depends on the page: drum patterns, or patterns for the synth that is open.
  void rebuildPatterns() {
    const int part = tab == 0 ? 0 : 1 + synth;
    patternsFor = part;
    stopListening();
    patterns.clear();
    const auto& lib = p.library();
    for (size_t i = 0; i < lib.size() && i < grooveScenes.size(); ++i) {
      const auto& src = grooveScenes[i];
      if (!src) continue;
      Pattern pt;
      pt.name = lib[i].name;
      if (part == 0) {
        bool any = false;
        for (const auto& row : src->drums.steps) for (int st = 0; st < src->bars * 16 && !any; ++st) any = row[st] != 0;
        if (!any) continue;
        pt.scene = src;
      } else {
        const int want = part - 1;
        const int from = src->synth[want].count > 0 ? want : (want >= 2 && src->synth[1].count > 0 ? 1 : -1);   // synth 3 and 4 borrow the lead's lines
        if (from < 0) continue;
        if (from == want) pt.scene = src;
        else { auto moved = std::make_shared<ld::Scene>(); moved->bars = src->bars; moved->synth[want] = src->synth[from]; pt.scene = moved; }
      }
      patterns.push_back(pt);
    }
    const juce::File dir(p.midiFolder);
    if (p.midiFolder.isNotEmpty() && dir.isDirectory()) {
      juce::Array<juce::File> files = dir.findChildFiles(juce::File::findFiles, true, "*.mid;*.midi");
      files.sort();
      for (int i = 0; i < files.size() && i < 2000; ++i) {
        Pattern pt;
        pt.name = files[i].getFileNameWithoutExtension(); pt.midi = true; pt.file = files[i];
        patterns.push_back(pt);
      }
    }
    patternList.deselectAllRows();
    patternList.updateContent();
    patternList.repaint();
  }

  // The pattern of a row, read from its file if need be. Null if the file holds nothing usable.
  std::shared_ptr<ld::Scene> patternScene(int row) {
    if (row < 0 || row >= int(patterns.size())) return {};
    Pattern& pt = patterns[size_t(row)];
    if (!pt.scene && pt.midi) {
      auto sc = std::make_shared<ld::Scene>();
      const juce::ScopedLock sl(p.lock());
      if (midiToScene(pt.file, p.session().project(), patternsFor, *sc)) pt.scene = sc;
    }
    return pt.scene;
  }

  // A click plays the pattern in place of the groove, without changing the project. A second click stops it.
  void listen(int row) {
    if (row == previewRow) { stopListening(); return; }
    const auto sc = patternScene(row);
    if (!sc) { message = "Nothing in that file that this track could play."; stopListening(); return; }
    const juce::ScopedLock sl(p.lock());
    ld::Session& s = p.session();
    s.preview(sc.get(), patternsFor);
    if (!p.hostPlays() && !s.engine().isPlaying()) { s.engine().play(scene()); previewStartedPlay = true; }
    previewRow = row;
    message = "Listening to " + patterns[size_t(row)].name + " (" + juce::String(int(sc->bars)) + (sc->bars == 1 ? " bar" : " bars") + "). PUT IN A .. H keeps it; another click stops.";
  }

  void stopListening() {
    const juce::ScopedLock sl(p.lock());
    if (p.session().previewing()) p.session().preview(nullptr);
    if (previewStartedPlay && !p.hostPlays()) p.session().engine().stop();
    previewStartedPlay = false;
    if (previewRow >= 0) { previewRow = -1; message.clear(); patternList.repaint(); }
  }

  void putPattern(int target) {
    const int row = patternList.getSelectedRow();
    const auto sc = patternScene(row);
    if (!sc) { message = "Pick a pattern in the list first."; return; }
    const juce::String name = patterns[size_t(row)].name;
    stopListening();
    {
      const juce::ScopedLock sl(p.lock());
      if (patternsFor == 0) p.session().putDrums(target, sc->drums, sc->bars);
      else p.session().putSynth(target, patternsFor - 1, sc->synth[patternsFor - 1], sc->bars);
    }
    p.setScene(target);
    bar = 0;
    message = name + " is in groove " + kSceneNames[target] + ".";
  }

  // ---- library ----
  static const char* kindName(int k) { return k == 0 ? "groove" : (k == 1 ? "instrument" : "drumkit"); }

  void fillStyles() {
    juce::StringArray styles;
    for (const auto& e : p.library()) if (e.kind == kindName(libKind) && e.style.isNotEmpty()) styles.addIfNotAlreadyThere(e.style);
    styles.sort(true);
    styleBox.clear(juce::dontSendNotification);
    styleBox.addItem("All styles", 1);
    for (int i = 0; i < styles.size(); ++i) styleBox.addItem(styles[i], i + 2);
    styleBox.setSelectedId(1, juce::dontSendNotification);
  }

  void filterLibrary() {
    shown.clear();
    const juce::String style = styleBox.getSelectedId() > 1 ? styleBox.getText() : juce::String();
    const auto& lib = p.library();
    for (size_t i = 0; i < lib.size(); ++i)
      if (lib[i].kind == kindName(libKind) && (style.isEmpty() || lib[i].style == style)) shown.push_back(int(i));
    list.deselectAllRows();
    list.updateContent();
    list.repaint();
  }

  void loadSelected() {
    const int row = list.getSelectedRow();
    if (row < 0 || row >= int(shown.size())) { message = "Pick an entry in the list first."; return; }
    const LibraryEntry& e = p.library()[size_t(shown[size_t(row)])];
    const juce::String err = p.importLibrary(e, scene(), synth, tempoBtn.getToggleState());
    message = err.isEmpty() ? "Loaded " + e.name + (libKind == 0 ? " into groove " + juce::String(kSceneNames[scene()])
                                                     : (libKind == 1 ? " onto " + juce::String(kTrackNames[synth + 1]) : juce::String()))
                            : e.name + ": " + err;
  }

  int getNumRows() override { return int(shown.size()); }
  void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override {
    if (row < 0 || row >= int(shown.size())) return;
    const LibraryEntry& e = p.library()[size_t(shown[size_t(row)])];
    if (selected) g.fillAll(col::orange.withAlpha(0.85f));
    g.setColour(selected ? col::ink : col::silk);
    g.setFont(juce::Font(juce::FontOptions(14.0f)));
    g.drawText(e.name, 8, 0, width - 170, height, juce::Justification::centredLeft, true);
    g.setColour(selected ? col::ink : col::muted);
    g.drawText(e.style, width - 160, 0, 152, height, juce::Justification::centredRight, true);
  }
  void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { loadSelected(); }

  // ---- layout ----
  void showTab() {
    const bool d = tab == 0, s = tab == 1, f = tab == 2, m = tab == 3, sg = tab == 4, l = tab == 5;
    const bool ed = d || s, br = ed && browse;
    kitBox.setVisible(d); accentBtn.setVisible(d || s);
    for (auto& b : padBtn) b.setVisible(d);
    for (auto* b : {hardness.get(), padVol.get(), padPitch.get(), padDecay.get(), padPan.get()}) b->setVisible(d);
    for (auto& b : synthBtn) b.setVisible(s);
    for (auto* b : {&addSynthBtn, &slideBtn, &chordBtn, &rollDown, &rollUp}) b->setVisible(s);
    instBox.setVisible(s); engineBox.setVisible(s);
    for (auto* b : synthBars) b->setVisible(s);
    for (auto* b : mixBars) b->setVisible(m);
    for (auto& b : muteBtn) b.setVisible(m);
    for (auto& b : fxTrackBtn) b.setVisible(f);
    for (int i = 0; i < 3; ++i) {
      pedalBox[i].setVisible(f); pedalOn[i].setVisible(f); pedalLeft[i].setVisible(f); pedalRight[i].setVisible(f);
      for (int k = 0; k < 4; ++k) { pedalKnob[i][k]->setVisible(f); voiceBtn[i][k].setVisible(f); }
    }
    for (auto& b : kindBtn) b.setVisible(l);
    loadBtn.setVisible(l); tempoBtn.setVisible(l); styleBox.setVisible(l); list.setVisible(l);
    for (auto* b : {&barAdd, &barDel, &barDouble, &loopBtn, &browseBtn}) b->setVisible(ed);
    patternList.setVisible(br); folderBtn.setVisible(br);
    for (auto& b : putBtn) b.setVisible(br);
    for (auto& b : songAdd) b.setVisible(sg);
    for (auto* b : {&songRemove, &songLeft, &songRight, &songEndBtn, &songPlayBtn}) b->setVisible(sg);
    for (auto& b : laneClear) b.setVisible(false);
    if (ed) patternsFor = -99;                                 // the list is made anew for this page
    resized();
    repaint();
  }

  void resized() override {
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(30);
    top.removeFromLeft(170);                                   // the name, painted
    for (auto* b : {&openBtn, &saveBtn, &undoBtn, &redoBtn}) { b->setBounds(top.removeFromLeft(62)); top.removeFromLeft(4); }
    midiBox.setBounds(top.removeFromRight(150));
    top.removeFromRight(6);
    syncBtn.setBounds(top.removeFromRight(118));
    r.removeFromTop(8);
    auto row2 = r.removeFromTop(34);
    playBtn.setBounds(row2.removeFromLeft(96)); row2.removeFromLeft(6);
    bpmDown.setBounds(row2.removeFromLeft(30));
    row2.removeFromLeft(84);                                   // the tempo, painted
    bpmUp.setBounds(row2.removeFromLeft(30)); row2.removeFromLeft(14);
    for (auto& b : sceneBtn) { b.setBounds(row2.removeFromLeft(50)); row2.removeFromLeft(3); }
    row2.removeFromLeft(8);
    songBtn.setBounds(row2.removeFromLeft(66));
    recBtn.setBounds(row2.removeFromRight(104));
    r.removeFromTop(8);
    auto tabs = r.removeFromTop(30);
    const int tw = (tabs.getWidth() - 20) / 6;
    for (auto& b : tabBtn) { b.setBounds(tabs.removeFromLeft(tw)); tabs.removeFromLeft(4); }
    r.removeFromTop(8);
    r.removeFromBottom(22);                                    // the message line, painted
    pageArea = r;

    if (tab == 0) {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      kitBox.setBounds(line1.removeFromLeft(220)); line1.removeFromLeft(8);
      hardness->setBounds(line1.removeFromLeft(200)); line1.removeFromLeft(8);
      accentBtn.setBounds(line1.removeFromLeft(90));
      a.removeFromTop(6);
      layoutBars(a.removeFromTop(26));
      a.removeFromTop(8);
      layoutBrowser(a);
      auto bottom = a.removeFromBottom(30);
      const int bw = (bottom.getWidth() - 18) / 4;
      for (auto* b : {padVol.get(), padPitch.get(), padDecay.get(), padPan.get()}) { b->setBounds(bottom.removeFromLeft(bw)); bottom.removeFromLeft(6); }
      a.removeFromBottom(8);
      auto names = a.removeFromLeft(74);
      const int rh = a.getHeight() / 8;
      for (int i = 0; i < 8; ++i) padBtn[i].setBounds(names.removeFromTop(rh).reduced(0, 2));
      a.removeFromLeft(6);
      gridArea = a.withHeight(rh * 8);
    } else if (tab == 1) {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      for (auto& b : synthBtn) { b.setBounds(line1.removeFromLeft(70)); line1.removeFromLeft(4); }
      addSynthBtn.setBounds(line1.removeFromLeft(78)); line1.removeFromLeft(10);
      instBox.setBounds(line1.removeFromLeft(150)); line1.removeFromLeft(6);
      engineBox.setBounds(line1.removeFromLeft(130));
      a.removeFromTop(6);
      const int cols = 7, rows = 3, bh = 26;
      auto bars = a.removeFromTop(rows * (bh + 4));
      const int cw = (bars.getWidth() - (cols - 1) * 4) / cols;
      for (int i = 0; i < synthBars.size(); ++i)
        synthBars[i]->setBounds(bars.getX() + (i % cols) * (cw + 4), bars.getY() + (i / cols) * (bh + 4), cw, bh);
      a.removeFromTop(4);
      auto line2 = a.removeFromTop(28);
      accentBtn.setBounds(line2.removeFromLeft(84)); line2.removeFromLeft(4);
      slideBtn.setBounds(line2.removeFromLeft(70)); line2.removeFromLeft(4);
      chordBtn.setBounds(line2.removeFromLeft(70)); line2.removeFromLeft(10);
      rollDown.setBounds(line2.removeFromLeft(70)); line2.removeFromLeft(4);
      rollUp.setBounds(line2.removeFromLeft(70));
      a.removeFromTop(6);
      layoutBars(a.removeFromTop(26));
      a.removeFromTop(6);
      layoutBrowser(a);
      rollArea = a;
    } else if (tab == 2) {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      for (auto& b : fxTrackBtn) { b.setBounds(line1.removeFromLeft(86)); line1.removeFromLeft(4); }
      a.removeFromTop(8);
      auto chips = a.removeFromTop(30);                         // the whole board: six slots in the order the sound takes
      chips.removeFromLeft(86); chips.removeFromRight(86);      // "IN" and "MIXER", painted
      const int chw = chips.getWidth() / ld::kFxSlots;
      for (int i = 0; i < ld::kFxSlots; ++i) chipArea[i] = chips.removeFromLeft(chw).reduced(3, 0);
      a.removeFromTop(10);
      const int gap = 26, pw = juce::jmin(300, (a.getWidth() - 2 * gap) / 3), ph = juce::jmin(a.getHeight(), 430);
      const int x0 = a.getX() + (a.getWidth() - (3 * pw + 2 * gap)) / 2;
      for (int i = 0; i < 3; ++i) {
        pedalArea[i] = juce::Rectangle<int>(x0 + i * (pw + gap), a.getY(), pw, ph);
        auto c = pedalArea[i].reduced(14, 12);
        c.removeFromTop(44);                                    // name and light, painted
        traceArea[i] = c.removeFromTop(juce::jmax(30, ph / 9)); c.removeFromTop(10);
        auto knobs = c.removeFromTop(juce::jmin(84, c.getWidth() / 4 + 16));
        const int kw = knobs.getWidth() / 4;
        for (int k = 0; k < 4; ++k) pedalKnob[i][k]->setBounds(knobs.removeFromLeft(kw).reduced(2, 0));
        c.removeFromTop(8);
        auto voices = c.removeFromTop(24);
        const int vw = voices.getWidth() / 4;
        for (int k = 0; k < 4; ++k) voiceBtn[i][k].setBounds(voices.removeFromLeft(vw).reduced(1, 0));
        aboutArea[i] = c.removeFromTop(32);
        auto foot = c.removeFromBottom(44);
        pedalLeft[i].setBounds(foot.removeFromLeft(36).reduced(0, 8));
        pedalRight[i].setBounds(foot.removeFromRight(36).reduced(0, 8));
        pedalOn[i].setBounds(foot.reduced(12, 0));
        c.removeFromBottom(8);
        pedalBox[i].setBounds(c.removeFromBottom(26));
      }
    } else if (tab == 3) {
      auto a = r;
      a.removeFromTop(26);                                      // the track names, painted
      const int cw = (a.getWidth() - 5 * 8) / 6;
      for (int t = 0; t < 5; ++t) {
        auto c = juce::Rectangle<int>(a.getX() + t * (cw + 8), a.getY(), cw, a.getHeight());
        for (int k = 0; k < 6; ++k) { mixBars[t * 6 + k]->setBounds(c.removeFromTop(30)); c.removeFromTop(6); }
        muteBtn[t].setBounds(c.removeFromTop(30));
      }
      auto c = juce::Rectangle<int>(a.getX() + 5 * (cw + 8), a.getY(), cw, a.getHeight());
      for (int k = 0; k < 3; ++k) { mixBars[30 + k]->setBounds(c.removeFromTop(30)); c.removeFromTop(6); }
    } else if (tab == 4) {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      line1.removeFromLeft(44);                                 // "ADD", painted
      for (auto& b : songAdd) { b.setBounds(line1.removeFromLeft(36)); line1.removeFromLeft(3); }
      line1.removeFromLeft(8);
      songLeft.setBounds(line1.removeFromLeft(32)); line1.removeFromLeft(3);
      songRight.setBounds(line1.removeFromLeft(32)); line1.removeFromLeft(6);
      songRemove.setBounds(line1.removeFromLeft(80)); line1.removeFromLeft(12);
      songEndBtn.setBounds(line1.removeFromRight(200)); line1.removeFromRight(6);
      songPlayBtn.setBounds(line1.removeFromRight(140));
      a.removeFromTop(8);
      auto lanes = a.removeFromBottom(22 + ld::kSceneLanes * 26);
      a.removeFromBottom(10);
      songArea = a;
      laneTitle = lanes.removeFromTop(22);
      for (int i = 0; i < ld::kSceneLanes; ++i) {
        auto row = lanes.removeFromTop(26).reduced(0, 2);
        laneClear[i].setBounds(row.removeFromRight(64));
        row.removeFromRight(6); row.removeFromLeft(190);        // the name, painted
        laneArea[i] = row;
      }
    } else {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      for (auto& b : kindBtn) { b.setBounds(line1.removeFromLeft(130)); line1.removeFromLeft(4); }
      line1.removeFromLeft(10);
      styleBox.setBounds(line1.removeFromLeft(220));
      a.removeFromTop(8);
      auto bottom = a.removeFromBottom(30);
      loadBtn.setBounds(bottom.removeFromLeft(220)); bottom.removeFromLeft(8);
      tempoBtn.setBounds(bottom.removeFromLeft(220));
      a.removeFromBottom(8);
      list.setBounds(a);
    }
  }

  // The bar line of the two editors: which bar is open, LOOP BAR, and the buttons that make the groove longer.
  void layoutBars(juce::Rectangle<int> area) {
    browseBtn.setBounds(area.removeFromRight(96)); area.removeFromRight(8);
    barDouble.setBounds(area.removeFromRight(36)); area.removeFromRight(3);
    barAdd.setBounds(area.removeFromRight(56)); area.removeFromRight(3);
    barDel.setBounds(area.removeFromRight(56)); area.removeFromRight(8);
    loopBtn.setBounds(area.removeFromLeft(86)); area.removeFromLeft(8);
    const int bw = juce::jlimit(22, 40, (area.getWidth() - 15 * 2) / 16);
    for (auto& b : barBtn) { b.setBounds(area.removeFromLeft(bw)); area.removeFromLeft(2); }
  }
  // Takes the browser's column off the right of an editor.
  void layoutBrowser(juce::Rectangle<int>& a) {
    browseArea = {};
    if (!browse) return;
    auto col = a.removeFromRight(juce::jlimit(200, 280, a.getWidth() / 4));
    a.removeFromRight(10);
    browseArea = col;
    col.removeFromTop(20);                                      // the title, painted
    auto put = col.removeFromBottom(26);
    const int pw = (put.getWidth() - 7 * 2) / 8;
    for (auto& b : putBtn) { b.setBounds(put.removeFromLeft(pw)); put.removeFromLeft(2); }
    col.removeFromBottom(18);                                   // "PUT IN", painted
    folderBtn.setBounds(col.removeFromBottom(24));
    col.removeFromBottom(6);
    patternList.setBounds(col);
  }

  // ---- drawing ----
  void paint(juce::Graphics& g) override {
    g.fillAll(col::chassis);
    g.setFont(juce::Font(juce::FontOptions(21.0f, juce::Font::bold)));
    g.setColour(col::silk);
    g.drawText("LOOP", 10, 10, 60, 30, juce::Justification::centredLeft);
    g.setColour(col::orange);
    g.drawText("MASTER", 66, 10, 100, 30, juce::Justification::centredLeft);

    // tempo
    const auto tempoBox = juce::Rectangle<int>(bpmDown.getRight() + 4, bpmDown.getY(), 76, bpmDown.getHeight());
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(tempoBox.toFloat(), 4.0f);
    g.setColour(col::red);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText(juce::String(p.shownBpm(), 1), tempoBox, juce::Justification::centred);

    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(message.isNotEmpty() ? col::yellow : col::muted);
    const juce::String hint = message.isNotEmpty() ? message
        : ((tab == 0 || tab == 1) && loopBar && !p.songMode() ? "LOOP BAR is lit: only the bar you have open plays. Unlight it to hear the whole groove."
        : (p.hostPlays() ? "The host leads: its play button, position and tempo."
           : (p.hostLeads() ? "The host stands still. PLAY here plays at the host's tempo; the host's own play button takes over."
                            : "Own transport. Start your host's playback, or press PLAY.")));
    g.drawText(hint, getLocalBounds().reduced(10).removeFromBottom(18), juce::Justification::centredLeft, true);

    const juce::ScopedLock sl(p.lock());
    if (tab == 0) paintGrid(g);
    else if (tab == 1) paintRoll(g);
    else if (tab == 2) paintPedals(g);
    else if (tab == 3) paintMixNames(g);
    else if (tab == 4) paintSong(g);
    if ((tab == 0 || tab == 1) && browse) paintBrowser(g);
  }

  int stepInScene() const {
    const ld::Engine& e = p.session().engine();
    return e.isPlaying() && e.currentScene() == scene() ? e.currentStep() : -1;
  }

  void paintGrid(juce::Graphics& g) {
    const ld::Scene& sc = p.session().project().scenes[scene()];
    const int playing = stepInScene();
    const float cw = float(gridArea.getWidth()) / 16.0f, ch = float(gridArea.getHeight()) / 8.0f;
    for (int row = 0; row < 8; ++row) {
      for (int c = 0; c < 16; ++c) {
        const int st = bar * 16 + c;
        const int v = sc.drums.steps[row][st];
        juce::Rectangle<float> cell(float(gridArea.getX()) + float(c) * cw, float(gridArea.getY()) + float(row) * ch, cw, ch);
        cell = cell.reduced(2.0f);
        juce::Colour fill = (c / 4) % 2 == 0 ? col::recess : col::recess.brighter(0.06f);
        if (v >= 3) fill = col::cyan;
        else if (v == 2) fill = col::pads[row];
        else if (v == 1) fill = col::pads[row].withAlpha(0.6f);
        g.setColour(fill);
        g.fillRoundedRectangle(cell, 3.0f);
        g.setColour(st == playing ? col::green : (v == 2 ? juce::Colours::white : col::line));
        g.drawRoundedRectangle(cell, 3.0f, st == playing ? 2.0f : 1.0f);
        if (v >= 3) {
          g.setColour(col::ink);
          g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
          g.drawText(juce::String(v - 1) + "x", cell.toNearestInt(), juce::Justification::centred);
        }
      }
    }
  }

  void paintRoll(juce::Graphics& g) {
    const ld::Project& pr = p.session().project();
    const ld::SynthPattern& pat = pr.scenes[scene()].synth[synth];
    const int playing = stepInScene();
    const int labelW = 46;
    const float cw = float(rollArea.getWidth() - labelW) / 16.0f, ch = float(rollArea.getHeight()) / float(kRollRows);
    const int oct = int(pr.synths[synth].octaveShift);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    for (int row = 0; row < kRollRows; ++row) {
      const int deg = rollBase + (kRollRows - 1 - row);
      const float y = float(rollArea.getY()) + float(row) * ch;
      const bool root = ((deg % 7) + 7) % 7 == 0;
      g.setColour(root ? col::silk : col::muted);
      g.drawText(noteName(ld::degreeToMidi(pr, oct, deg)), rollArea.getX(), int(y), labelW - 6, int(ch), juce::Justification::centredRight);
      for (int c = 0; c < 16; ++c) {
        juce::Rectangle<float> cell(float(rollArea.getX() + labelW) + float(c) * cw, y, cw, ch);
        cell = cell.reduced(1.5f);
        g.setColour(root ? col::recess.brighter(0.10f) : ((c / 4) % 2 == 0 ? col::recess : col::recess.brighter(0.05f)));
        g.fillRoundedRectangle(cell, 2.0f);
        if (bar * 16 + c == playing) { g.setColour(col::green.withAlpha(0.25f)); g.fillRoundedRectangle(cell, 2.0f); }
      }
    }
    const int count = juce::jmin(int(pat.count), ld::kMaxNotes);
    for (int k = 0; k < count; ++k) {
      const ld::Note& n = pat.notes[k];
      const int row = kRollRows - 1 - (int(n.degree) - rollBase);
      const int c0 = int(n.step) - bar * 16;
      if (row < 0 || row >= kRollRows || c0 + int(n.len) <= 0 || c0 >= 16) continue;
      const int a = juce::jmax(0, c0), b = juce::jmin(16, c0 + juce::jmax(1, int(n.len)));
      juce::Rectangle<float> cell(float(rollArea.getX() + labelW) + float(a) * cw, float(rollArea.getY()) + float(row) * ch, float(b - a) * cw, ch);
      cell = cell.reduced(1.5f);
      g.setColour(n.vel >= 2 ? col::synths[synth] : col::synths[synth].withAlpha(0.7f));
      g.fillRoundedRectangle(cell, 2.0f);
      if (n.vel >= 2) { g.setColour(juce::Colours::white); g.drawRoundedRectangle(cell, 2.0f, 1.0f); }
      if (n.flags & ld::kNoteSlide) { g.setColour(col::yellow); g.fillRect(cell.removeFromLeft(4.0f)); }
    }
  }

  // The order of the pedals is the order the sound runs through them.
  void swapPedals(int a, int b) {
    if (a < 0 || b < 0 || a >= ld::kFxSlots || b >= ld::kFxSlots) return;
    const juce::ScopedLock sl(p.lock());
    const ld::FxSlot sa = p.session().project().fx[fxTrack].slots[a], sb = p.session().project().fx[fxTrack].slots[b];
    p.session().setFxSlotFull(fxTrack, a, sb);                                                 // one undo step for both
    p.session().setFxSlotFull(fxTrack, b, sa, false);
    pedalPage = b / 3;                                         // follow the pedal
  }

  void paintPedals(juce::Graphics& g) {
    const ld::TrackFx& fx = p.session().project().fx[fxTrack];
    // the whole board, small: the path of the sound through all six slots
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(col::muted);
    g.drawText(juce::String(kTrackNames[fxTrack]).toUpperCase() + " IN", chipArea[0].getX() - 89, chipArea[0].getY(), 80, chipArea[0].getHeight(), juce::Justification::centredLeft);
    g.drawText("MIXER", chipArea[ld::kFxSlots - 1].getRight() + 9, chipArea[0].getY(), 80, chipArea[0].getHeight(), juce::Justification::centredRight);
    for (int i = 0; i < ld::kFxSlots; ++i) {
      const ld::FxSlot& sl = fx.slots[i];
      const PedalSpec* sp = pedalSpec(int(sl.type));
      const auto chip = chipArea[i].toFloat();
      g.setColour(col::line);
      g.fillRect(chip.getX() - 6.0f, chip.getCentreY() - 1.5f, 6.0f, 3.0f);
      if (i == ld::kFxSlots - 1) g.fillRect(chip.getRight(), chip.getCentreY() - 1.5f, 6.0f, 3.0f);
      g.setColour(sp ? juce::Colour(sp->top).withMultipliedSaturation(sl.bypass ? 0.35f : 1.0f).withMultipliedBrightness(sl.bypass ? 0.6f : 1.0f) : col::recess);
      g.fillRoundedRectangle(chip, 5.0f);
      const bool shown = i / 3 == pedalPage;
      g.setColour(shown ? col::silk : col::line);
      g.drawRoundedRectangle(chip.reduced(0.5f), 5.0f, shown ? 2.0f : 1.0f);
      g.setColour(sp ? (sp->lightInk ? juce::Colour(0xfff7f7f7) : juce::Colour(0xff1a1408)) : col::muted);
      g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
      g.drawText(juce::String(i + 1) + "  " + (sp ? juce::String(sp->name).toUpperCase() : juce::String("EMPTY")), chipArea[i].reduced(6, 0), juce::Justification::centredLeft, true);
    }
    for (int i = 0; i < 3; ++i) {
      const auto box = pedalArea[i].toFloat();
      const ld::FxSlot& sl = fx.slots[slotOf(i)];
      const PedalSpec* sp = pedalSpec(int(sl.type));
      if (i < 2) {                                           // the cable to the next one
        g.setColour(col::line);
        const float cy = box.getCentreY();
        g.fillRect(box.getRight(), cy - 2.0f, float(pedalArea[i + 1].getX()) - box.getRight(), 4.0f);
      }
      if (!sp) {
        g.setColour(col::recess);
        g.fillRoundedRectangle(box, 12.0f);
        g.setColour(col::line);
        g.drawRoundedRectangle(box.reduced(1.0f), 12.0f, 1.5f);
        g.setColour(col::muted);
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        g.drawText("SLOT " + juce::String(slotOf(i) + 1), pedalArea[i].withHeight(74), juce::Justification::centred);
        continue;
      }
      const juce::Colour top(sp->top), bottom(sp->bottom);
      g.setGradientFill(juce::ColourGradient(top, box.getX(), box.getY(), bottom, box.getX(), box.getBottom(), false));
      g.fillRoundedRectangle(box, 12.0f);
      g.setColour(juce::Colours::black.withAlpha(0.45f));
      g.drawRoundedRectangle(box.reduced(1.0f), 12.0f, 2.0f);
      g.setColour(juce::Colours::white.withAlpha(0.18f));       // the edge catches the light
      g.drawRoundedRectangle(box.reduced(4.0f), 9.0f, 1.0f);
      const juce::Colour ink = sp->lightInk ? juce::Colour(0xfff7f7f7) : juce::Colour(0xff1a1408);
      for (int corner = 0; corner < 4; ++corner) {              // four screws
        const float sx = corner % 2 ? box.getRight() - 11.0f : box.getX() + 11.0f, sy = corner / 2 ? box.getBottom() - 11.0f : box.getY() + 11.0f;
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(sx - 3.5f, sy - 3.5f, 7.0f, 7.0f);
        g.setColour(juce::Colour(0xffb9bcc2));
        g.fillEllipse(sx - 2.6f, sy - 2.6f, 5.2f, 5.2f);
        g.setColour(juce::Colour(0xff4a4d52));
        g.drawLine(sx - 1.8f, sy - 1.8f, sx + 1.8f, sy + 1.8f, 1.0f);
      }
      g.setColour(ink);
      g.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
      g.drawText(juce::String(sp->name).toUpperCase(), pedalArea[i].getX() + 22, pedalArea[i].getY() + 12, pedalArea[i].getWidth() - 100, 24, juce::Justification::centredLeft, true);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(ink.withAlpha(0.7f));
      g.drawText("LM-" + juce::String(sp->type).paddedLeft('0', 2) + "  " + sp->code, pedalArea[i].getX() + 22, pedalArea[i].getY() + 35, pedalArea[i].getWidth() - 100, 14, juce::Justification::centredLeft, true);
      // the light
      const juce::Rectangle<float> led(box.getRight() - 42.0f, box.getY() + 18.0f, 14.0f, 14.0f);
      g.setColour(juce::Colours::black.withAlpha(0.6f));
      g.fillEllipse(led.expanded(2.5f));
      if (!sl.bypass) { g.setColour(col::red.withAlpha(0.35f)); g.fillEllipse(led.expanded(6.0f)); }
      g.setColour(sl.bypass ? juce::Colour(0xff4a1a1a) : col::red.brighter(0.4f));
      g.fillEllipse(led);
      // the scope: what this pedal does to a wave
      const auto tr = traceArea[i].toFloat();
      g.setColour(juce::Colour(0xff0d0f10));
      g.fillRoundedRectangle(tr, 5.0f);
      g.setColour(juce::Colours::white.withAlpha(0.07f));
      g.drawHorizontalLine(int(tr.getCentreY()), tr.getX() + 4.0f, tr.getRight() - 4.0f);
      juce::Path wave;
      const int n = int(tr.getWidth()) - 12;
      for (int px = 0; px <= n; ++px) {
        const float v = juce::jlimit(-1.0f, 1.0f, pedalTrace(sp->type, float(px) / float(n)));
        const float wx = tr.getX() + 6.0f + float(px), wy = tr.getCentreY() - v * (tr.getHeight() * 0.5f - 5.0f);
        if (px == 0) wave.startNewSubPath(wx, wy); else wave.lineTo(wx, wy);
      }
      g.setColour(sl.bypass ? top.withMultipliedSaturation(0.3f).withAlpha(0.5f) : top.brighter(0.5f));
      g.strokePath(wave, juce::PathStrokeType(1.8f));
      // what the chosen character is
      g.setColour(ink.withAlpha(0.85f));
      g.setFont(juce::Font(juce::FontOptions(11.5f)));
      const int voice = juce::jlimit(0, 3, int(sl.voice));
      if (sp->about[voice]) g.drawFittedText(sp->about[voice], aboutArea[i].reduced(2, 2), juce::Justification::centred, 2);
    }
  }

  void paintBrowser(juce::Graphics& g) {
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(col::muted);
    const juce::String who = tab == 0 ? "DRUM" : juce::String(kTrackNames[synth + 1]).toUpperCase();
    g.drawText(who + " PATTERNS  -  click to listen", browseArea.withHeight(18), juce::Justification::centredLeft, true);
    g.drawText("PUT IN GROOVE", browseArea.getX(), putBtn[0].getY() - 18, browseArea.getWidth(), 16, juce::Justification::centredLeft, true);
  }

  // What a lane moves, in words.
  juce::String laneName(const ld::SceneLane& ln) const {
    const ld::Project& pr = p.session().project();
    int slot, knob;
    if (ld::fxSlotParam(ln.paramId, slot, knob)) {
      const int t = juce::jlimit(0, ld::kNumTracks - 1, int(ln.index));
      const ld::FxSlot& sl = pr.fx[t].slots[slot];
      const PedalSpec* sp = pedalSpec(int(sl.type));
      const juce::String what = sp ? juce::String(sp->name) + " " + knobName(*sp, juce::jlimit(0, 3, int(sl.voice)), knob) : "Slot " + juce::String(slot + 1);
      return juce::String(kTrackNames[t]) + ": " + what;
    }
    for (const auto& a : kAutoParams)
      if (a.pid == ln.paramId) return juce::String(kTrackNames[juce::jlimit(0, ld::kNumTracks - 1, ld::isSynthParam(ln.paramId) ? 1 + int(ln.index) : int(ln.index))]) + ": " + a.name;
    return "?";
  }

  void paintSong(juce::Graphics& g) {
    const ld::Project& pr = p.session().project();
    const ld::Engine& e = p.session().engine();
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(col::muted);
    g.drawText("ADD", songAdd[0].getX() - 44, songAdd[0].getY(), 40, songAdd[0].getHeight(), juce::Justification::centredLeft);
    // the song: every block is a groove, as wide as it is long
    int total = 0;
    for (int c = 0; c < pr.chainLen; ++c) total += juce::jlimit(1, ld::kMaxBars, int(pr.scenes[pr.chain[c]].bars));
    const int rowH = 46, gap = 6;
    const int maxRows = juce::jmax(1, songArea.getHeight() / (rowH + gap));
    int ppb = juce::jlimit(6, 44, songArea.getWidth() / juce::jmax(8, total));            // pixels per bar
    while (ppb > 6 && (total * ppb + songArea.getWidth() - 1) / songArea.getWidth() > maxRows) --ppb;
    songRects.clear();
    int x = songArea.getX(), y = songArea.getY(), barNo = 1;
    const bool live = e.isPlaying() && p.songMode();
    for (int c = 0; c < pr.chainLen; ++c) {
      const int sc = pr.chain[c], bars = juce::jlimit(1, ld::kMaxBars, int(pr.scenes[sc].bars)), w = bars * ppb;
      if (x + w > songArea.getRight() && x > songArea.getX()) { x = songArea.getX(); y += rowH + gap; }
      const juce::Rectangle<int> box(x, y, juce::jmin(w, songArea.getWidth()) - 2, rowH);
      songRects.push_back(box);
      const auto bf = box.toFloat();
      g.setColour(col::pads[sc % 8].withMultipliedBrightness(0.75f));
      g.fillRoundedRectangle(bf, 5.0f);
      g.setColour(juce::Colours::black.withAlpha(0.25f));
      for (int b = 1; b < bars; ++b) g.fillRect(float(x + b * ppb), bf.getY() + 4.0f, 1.0f, bf.getHeight() - 8.0f);
      g.setColour(c == songSel ? juce::Colours::white : juce::Colours::black.withAlpha(0.5f));
      g.drawRoundedRectangle(bf.reduced(0.5f), 5.0f, c == songSel ? 2.5f : 1.0f);
      g.setColour(col::ink);
      g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
      g.drawText(kSceneNames[sc], box.reduced(6, 2), juce::Justification::topLeft, false);
      g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
      g.drawText((box.getWidth() < 64 ? juce::String() : juce::String("bar ")) + juce::String(barNo), box.reduced(6, 3), juce::Justification::bottomLeft, false);
      if (live && e.chainPos() == c) {                         // where it is playing
        const int st = juce::jmax(0, e.currentStep());
        g.setColour(juce::Colours::white);
        g.fillRect(bf.getX() + bf.getWidth() * float(st) / float(bars * 16), bf.getY(), 2.5f, bf.getHeight());
      }
      x += w; barNo += bars;
    }
    // what moves inside the groove that is open
    const int sc = scene();
    const int pts = juce::jlimit(1, ld::kMaxBars, int(pr.scenes[sc].bars)) * 4;
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(col::muted);
    g.drawText(juce::String("KNOB MOVEMENTS IN GROOVE ") + kSceneNames[sc] + "  -  REC KNOBS writes them while it plays; draw here to change them",
               laneTitle, juce::Justification::centredLeft, true);
    for (int i = 0; i < ld::kSceneLanes; ++i) {
      const ld::SceneLane& ln = pr.scenes[sc].lanes[i];
      const auto area = laneArea[i].toFloat();
      g.setColour(col::recess);
      g.fillRoundedRectangle(area, 3.0f);
      if (ln.paramId < 0) continue;
      g.setColour(col::silk);
      g.setFont(juce::Font(juce::FontOptions(12.5f)));
      g.drawText(laneName(ln), laneArea[i].getX() - 190, laneArea[i].getY(), 184, laneArea[i].getHeight(), juce::Justification::centredLeft, true);
      g.setColour(col::line);
      for (int b = 4; b < pts; b += 4) g.fillRect(area.getX() + area.getWidth() * float(b) / float(pts), area.getY(), 1.0f, area.getHeight());
      juce::Path curve;
      for (int pt = 0; pt <= pts; ++pt) {
        const float px = area.getX() + area.getWidth() * float(pt) / float(pts);
        const float py = area.getBottom() - 2.0f - autoToUnit(ln.paramId, ln.points[pt]) * (area.getHeight() - 4.0f);
        if (pt == 0) curve.startNewSubPath(px, py); else curve.lineTo(px, py);
      }
      g.setColour(col::orange);
      g.strokePath(curve, juce::PathStrokeType(2.0f));
      if (e.isPlaying() && e.currentScene() == sc && e.currentStep() >= 0) {
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.fillRect(area.getX() + area.getWidth() * float(e.currentStep()) / float(pts * 4), area.getY(), 1.5f, area.getHeight());
      }
    }
  }

  void paintMixNames(juce::Graphics& g) {
    const int cw = (pageArea.getWidth() - 5 * 8) / 6;
    g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    for (int t = 0; t < 6; ++t) {
      g.setColour(t == 0 ? col::orange : (t == 5 ? col::silk : col::synths[t - 1]));
      g.drawText(t == 5 ? "Whole mix" : kTrackNames[t], pageArea.getX() + t * (cw + 8), pageArea.getY(), cw, 22, juce::Justification::centredLeft);
    }
  }

  // ---- mouse on the grids ----
  // Drawing in a lane: the point under the mouse takes the height of the mouse.
  void drawLane(const juce::MouseEvent& ev, bool first) {
    if (laneDrag < 0) return;
    const juce::ScopedLock sl(p.lock());
    const int sc = scene();
    const ld::SceneLane ln = p.session().project().scenes[sc].lanes[laneDrag];
    if (ln.paramId < 0) return;
    const int pts = juce::jlimit(1, ld::kMaxBars, int(p.session().project().scenes[sc].bars)) * 4;
    const auto area = laneArea[laneDrag];
    const int pt = juce::jlimit(0, pts, int(std::floor(float(ev.x - area.getX()) * float(pts) / float(juce::jmax(1, area.getWidth())) + 0.5f)));
    const float u = 1.0f - float(ev.y - area.getY() - 2) / float(juce::jmax(1, area.getHeight() - 4));
    p.session().writeLanePoint(sc, ln.paramId, ln.index, pt, autoFromUnit(ln.paramId, u), first);
  }
  void mouseDrag(const juce::MouseEvent& ev) override { if (tab == 4 && laneDrag >= 0) { drawLane(ev, false); repaint(); } }
  void mouseUp(const juce::MouseEvent&) override { laneDrag = -1; }
  void mouseDoubleClick(const juce::MouseEvent& ev) override {
    if (tab != 4) return;
    for (size_t c = 0; c < songRects.size(); ++c)
      if (songRects[c].contains(ev.getPosition())) {             // open that groove in the drum editor
        int sc = 0;
        { const juce::ScopedLock sl(p.lock()); sc = p.session().project().chain[c]; }
        p.setScene(sc); bar = 0; tab = 0; showTab();
        return;
      }
  }
  void mouseDown(const juce::MouseEvent& ev) override {
    if (tab == 2) {
      for (int i = 0; i < ld::kFxSlots; ++i) if (chipArea[i].contains(ev.getPosition())) { pedalPage = i / 3; repaint(); return; }
    }
    if (tab == 4) {
      for (size_t c = 0; c < songRects.size(); ++c) if (songRects[c].contains(ev.getPosition())) { songSel = int(c); repaint(); return; }
      for (int i = 0; i < ld::kSceneLanes; ++i) if (laneArea[i].contains(ev.getPosition())) { laneDrag = i; drawLane(ev, true); repaint(); return; }
      return;
    }
    const bool strong = accent || ev.mods.isRightButtonDown() || ev.mods.isShiftDown();
    if (tab == 0 && gridArea.contains(ev.getPosition())) {
      const int c = juce::jlimit(0, 15, (ev.x - gridArea.getX()) * 16 / juce::jmax(1, gridArea.getWidth()));
      const int row = juce::jlimit(0, 7, (ev.y - gridArea.getY()) * 8 / juce::jmax(1, gridArea.getHeight()));
      pad = row;
      const juce::ScopedLock sl(p.lock());
      p.session().toggleDrumStep(scene(), row, bar * 16 + c, strong);
    } else if (tab == 1 && rollArea.contains(ev.getPosition())) {
      const int labelW = 46;
      if (ev.x < rollArea.getX() + labelW) return;
      const int c = juce::jlimit(0, 15, (ev.x - rollArea.getX() - labelW) * 16 / juce::jmax(1, rollArea.getWidth() - labelW));
      const int row = juce::jlimit(0, kRollRows - 1, (ev.y - rollArea.getY()) * kRollRows / juce::jmax(1, rollArea.getHeight()));
      const int deg = rollBase + (kRollRows - 1 - row);
      const juce::ScopedLock sl(p.lock());
      const bool added = p.session().toggleNote(scene(), synth, bar * 16 + c, deg, strong, 1, slide);
      if (added && !slide && !p.session().engine().isPlaying()) {   // hear what was set
        p.session().engine().noteOn(synth, deg, strong ? 2 : 1);
        auditionSynth = synth; auditionDeg = deg; auditionTicks = 4;
      }
    }
    repaint();
  }
  int auditionSynth = -1, auditionDeg = 0, auditionTicks = 0;

  // ---- keeping the controls in step with the project ----
  void timerCallback() override {
    {
      const juce::ScopedLock sl(p.lock());
      ld::Session& s = p.session();
      if (auditionTicks > 0 && --auditionTicks == 0 && auditionSynth >= 0) s.engine().noteOff(auditionSynth, auditionDeg);
      const int sc = scene();
      const int bars = juce::jlimit(1, ld::kMaxBars, int(s.project().scenes[sc].bars));
      const int nSynths = juce::jlimit(2, ld::kNumSynths, int(s.getParam(ld::P_ACTIVE_SYNTHS, 0) + 0.5f));
      bar = juce::jlimit(0, bars - 1, bar);
      synth = juce::jlimit(0, nSynths - 1, synth);
      const bool onGrid = tab == 0 || tab == 1;
      for (int i = 0; i < 16; ++i) {
        barBtn[i].setVisible(onGrid && i < bars);
        barBtn[i].setToggleState(i == bar, juce::dontSendNotification);
      }
      for (int i = 0; i < 8; ++i) {
        padBtn[i].setToggleState(i == pad, juce::dontSendNotification);
        sceneBtn[i].setToggleState(i == sc, juce::dontSendNotification);
        sceneBtn[i].setButtonText(juce::String(kSceneNames[i]) + " " + juce::String(int(s.project().scenes[i].bars)));
      }
      for (int i = 0; i < 6; ++i) tabBtn[i].setToggleState(i == tab, juce::dontSendNotification);
      for (int i = 0; i < 4; ++i) {
        synthBtn[i].setVisible(tab == 1 && i < nSynths);
        synthBtn[i].setToggleState(i == synth, juce::dontSendNotification);
      }
      // the bar editor plays the bar that is open; everywhere else, and in the song, the whole groove plays
      s.engine().setLoopBar(onGrid && loopBar && !s.previewing() ? bar : -1);
      loopBtn.setToggleState(loopBar, juce::dontSendNotification);
      barAdd.setEnabled(bars < ld::kMaxBars); barDel.setEnabled(bars > 1); barDouble.setEnabled(bars < ld::kMaxBars);
      browseBtn.setToggleState(browse, juce::dontSendNotification);
      recBtn.setToggleState(recKnobs, juce::dontSendNotification);
      if (onGrid && browse && patternsFor != (tab == 0 ? 0 : 1 + synth)) rebuildPatterns();
      if (previewRow >= 0 && !s.previewing()) { previewRow = -1; previewStartedPlay = false; patternList.repaint(); }
      // REC KNOBS: the control that is held writes its value into the groove that is sounding, at the beat it is on
      if (recKnobs && p.touch.pid >= 0 && s.engine().isPlaying() && !s.previewing()) {
        float lo, hi;
        const int st = s.engine().currentStep(), now = s.engine().currentScene();
        if (st >= 0 && now >= 0 && now < ld::kNumScenes && autoRange(p.touch.pid, lo, hi)) {
          const int pts = juce::jlimit(1, ld::kMaxBars, int(s.project().scenes[now].bars)) * 4;
          const int pt = juce::jlimit(0, pts, (st + 2) / 4);
          if (s.writeLanePoint(now, p.touch.pid, p.touch.index, pt, juce::jlimit(lo, hi, p.touch.value), false) < 0)
            message = "This groove already has " + juce::String(ld::kSceneLanes) + " knob movements. CLEAR one on the SONG page.";
        }
      }
      addSynthBtn.setVisible(tab == 1 && nSynths < ld::kNumSynths);
      undoBtn.setEnabled(s.canUndo());
      redoBtn.setEnabled(s.canRedo());
      const bool leads = p.hostLeads(), hostPlays = p.hostPlays();
      playBtn.setEnabled(!hostPlays);
      playBtn.setButtonText(hostPlays ? "HOST" : (s.engine().isPlaying() ? "STOP" : "PLAY"));
      playBtn.setToggleState(s.engine().isPlaying(), juce::dontSendNotification);
      bpmDown.setEnabled(!leads); bpmUp.setEnabled(!leads);
      syncBtn.setToggleState(p.params.getRawParameterValue("sync")->load() > 0.5f, juce::dontSendNotification);
      songBtn.setToggleState(p.songMode(), juce::dontSendNotification);
      accentBtn.setToggleState(accent, juce::dontSendNotification);
      slideBtn.setToggleState(slide, juce::dontSendNotification);
      chordBtn.setToggleState(s.getParam(ld::P_SYN_CHORD, synth) > 0.5f, juce::dontSendNotification);
      midiBox.setSelectedId(1 + int(p.params.getRawParameterValue("miditrack")->load() + 0.5f), juce::dontSendNotification);
      for (int i = 0; i < 3; ++i) kindBtn[i].setToggleState(i == libKind, juce::dontSendNotification);

      if (tab == 0) {
        const int kit = juce::jlimit(0, 9, int(s.getParam(ld::P_DRUM_ENGINE, 0) + 0.5f));
        kitBox.setSelectedId(kit + 1, juce::dontSendNotification);
        for (int i = 0; i < 8; ++i) padBtn[i].setButtonText(kit == 4 ? "Slice " + juce::String(i + 1) : juce::String(kPadNames[i]));
        hardness->setVisible(kit != 4);
        hardness->refresh();
        const juce::String who = kit == 4 ? "Slice " + juce::String(pad + 1) : juce::String(kPadNames[pad]);
        padVol->setLabel(who + " volume"); padPitch->setLabel(who + " pitch"); padDecay->setLabel(who + " decay"); padPan->setLabel(who + " pan");
        for (auto* b : {padVol.get(), padPitch.get(), padDecay.get(), padPan.get()}) { b->setIndex(pad); b->refresh(); }
      } else if (tab == 1) {
        instBox.setSelectedId(1 + juce::jlimit(0, 7, int(s.getParam(ld::P_SYN_INSTRUMENT, synth) + 0.5f)), juce::dontSendNotification);
        engineBox.setSelectedId(1 + juce::jlimit(0, 3, int(s.getParam(ld::P_SYN_MODE, synth) + 0.5f)), juce::dontSendNotification);
        for (auto* b : synthBars) { b->setIndex(synth); b->refresh(); }
      } else if (tab == 2) {
        fxTrack = juce::jlimit(0, nSynths, fxTrack);
        for (int t = 0; t < 5; ++t) {
          fxTrackBtn[t].setVisible(t <= nSynths);
          fxTrackBtn[t].setToggleState(t == fxTrack, juce::dontSendNotification);
        }
        for (int i = 0; i < 3; ++i) {
          const int slot = slotOf(i);
          const ld::FxSlot& sl = s.project().fx[fxTrack].slots[slot];
          const PedalSpec* sp = pedalSpec(int(sl.type));
          pedalBox[i].setSelectedId(sp ? sp->type + 1 : 1, juce::dontSendNotification);
          const int voice = juce::jlimit(0, 3, int(sl.voice));
          const juce::Colour ink = sp && !sp->lightInk ? juce::Colour(0xff1a1408) : juce::Colour(0xfff7f7f7);
          for (int k = 0; k < 4; ++k) {
            const char* name = sp ? knobName(*sp, voice, k) : "";
            pedalKnob[i][k]->setVisible(name[0] != 0);
            pedalKnob[i][k]->set(ld::fxSlotParamId(slot, k), fxTrack, name, juce::Colour(sp ? sp->cap : 0xff1d1b18), ink);
            pedalKnob[i][k]->refresh();
            const bool has = sp != nullptr && k < int(ld::kFxVoices[sp->type]) && ld::kFxVoices[sp->type] > 1;
            voiceBtn[i][k].setVisible(has);
            if (has) { voiceBtn[i][k].setButtonText(sp->voices[k]); voiceBtn[i][k].setToggleState(k == voice, juce::dontSendNotification); }
          }
          pedalOn[i].setVisible(sp != nullptr);
          pedalOn[i].setButtonText(sl.bypass ? "OFF" : "ON");
          pedalOn[i].setToggleState(sp != nullptr && !sl.bypass, juce::dontSendNotification);
          pedalLeft[i].setEnabled(slot > 0); pedalRight[i].setEnabled(slot < ld::kFxSlots - 1);
        }
      } else if (tab == 3) {
        for (auto* b : mixBars) b->refresh();
        for (int t = 0; t < 5; ++t) {
          const bool used = t <= nSynths;
          for (int k = 0; k < 6; ++k) mixBars[t * 6 + k]->setEnabled(used);
          muteBtn[t].setEnabled(used);
          muteBtn[t].setToggleState(s.getParam(ld::P_FX_MUTE, t) > 0.5f, juce::dontSendNotification);
        }
      } else if (tab == 4) {
        songSel = juce::jlimit(0, juce::jmax(0, int(s.project().chainLen) - 1), songSel);
        songPlayBtn.setToggleState(p.songMode(), juce::dontSendNotification);
        songEndBtn.setButtonText(p.songLoop() ? "AFTER THE END: START OVER" : "AFTER THE END: SILENCE");
        songRemove.setEnabled(s.project().chainLen > 1);
        for (int i = 0; i < ld::kSceneLanes; ++i) laneClear[i].setVisible(s.project().scenes[sc].lanes[i].paramId >= 0);
      } else {
        tempoBtn.setVisible(libKind == 0);
      }
    }
    repaint();
  }
};

// ---------------------------------------------------------------------------------------------------------------

LoopMasterEditor::LoopMasterEditor(LoopMasterProcessor& proc) : juce::AudioProcessorEditor(&proc) {
  body_ = std::make_unique<Body>(proc);
  addAndMakeVisible(*body_);
  setResizable(true, true);
  setResizeLimits(940, 620, 1880, 1240);
  setSize(1060, 700);
}

LoopMasterEditor::~LoopMasterEditor() = default;

void LoopMasterEditor::resized() { if (body_) body_->setBounds(getLocalBounds()); }
void LoopMasterEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff17181c)); }
