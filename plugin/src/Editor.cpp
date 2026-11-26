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
const char* const kSceneNames[4] = {"A", "B", "C", "D"};
const char* const kKits[10] = {"808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break", "Hardcore Gabber",
                               "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore"};
const char* const kInstruments[8] = {"Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead"};
const char* const kEngines[4] = {"Subtractive", "FM 2-Op", "Pluck", "Vector"};
const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

// Every pedal: the six standard ones of Loop Draft and the stompbox collection.
struct PedalSpec {
  int type;
  const char* code;
  const char* name;
  juce::uint32 top, bottom;
  bool lightInk;
  const char* knob1;
  const char* knob2;                    // "" = this pedal has one control
};
const PedalSpec kPedals[] = {
  {1, "OD", "Overdrive", 0xffe0971b, 0xffb56e0c, false, "Drive", ""},
  {2, "BIT", "Bitcrush", 0xff218c9c, 0xff135b68, true, "Crush", "Rate"},
  {3, "CHO", "Chorus", 0xff2b6ec2, 0xff1a4682, true, "Depth", "Speed"},
  {4, "PHA", "Phaser", 0xffd95621, 0xffa1350d, true, "Depth", "Speed"},
  {5, "LPF", "Low-Pass", 0xff1f7a52, 0xff124d33, true, "Cutoff", "Reso"},
  {6, "COMP", "Compressor", 0xffc22d32, 0xff871b1f, true, "Squash", ""},
  {7, "DST", "Distortion", 0xffe0661b, 0xffa8430c, true, "Gain", "Tone"},
  {8, "FUZZ", "Fuzz", 0xff7a3fb5, 0xff4f2480, true, "Fuzz", "Tone"},
  {9, "BST", "Booster", 0xffe8e2d0, 0xffbdb59c, false, "Boost", "Treble"},
  {10, "TREM", "Tremolo", 0xff3e9a45, 0xff25692b, true, "Depth", "Speed"},
  {11, "VIB", "Vibrato", 0xff2f8fc9, 0xff1b5f8c, true, "Depth", "Speed"},
  {12, "FLG", "Flanger", 0xff8a5ad6, 0xff5b3699, true, "Feedbk", "Speed"},
  {13, "ROTO", "Rotary", 0xff8c5a32, 0xff5e3a1d, true, "Depth", "Speed"},
  {14, "WAH", "Wah", 0xff33363d, 0xff1c1e22, true, "Depth", "Speed"},
  {15, "AWAH", "Auto-Wah", 0xffe3c22b, 0xffb09210, false, "Sens", "Peak"},
  {16, "OCT", "Octaver", 0xff6b4a2e, 0xff45301d, true, "Sub", "Tone"},
  {17, "RING", "Ring Mod", 0xff7d8794, 0xff525b66, true, "Mix", "Pitch"},
  {18, "DLY", "Delay", 0xffdce6f2, 0xffa9b8cc, false, "Level", "Time"},
  {19, "TAPE", "Tape Echo", 0xff2e5e46, 0xff1b3b2b, true, "Level", "Time"},
  {20, "VERB", "Reverb", 0xff5e7fa3, 0xff3b5575, true, "Level", "Size"},
  {21, "GATE", "Gate", 0xff26282d, 0xff121316, true, "Thresh", "Decay"},
};
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
      if (auto* hp = p_.hostParam(pid_, index_)) hp->beginChangeGesture();
      else { const juce::ScopedLock sl(p_.lock()); p_.session().beginEdit(); }
    };
    slider_.onDragEnd = [this] {
      dragging_ = false;
      if (auto* hp = p_.hostParam(pid_, index_)) hp->endChangeGesture();
    };
    slider_.onValueChange = [this] {
      if (setting_) return;
      const float v = float(slider_.getValue());
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
  bool accent = false, slide = false;
  juce::String message;

  // header
  juce::TextButton openBtn{"OPEN"}, saveBtn{"SAVE"}, undoBtn{"UNDO"}, redoBtn{"REDO"}, playBtn{"PLAY"}, syncBtn{"FOLLOW HOST"}, songBtn{"SONG"};
  juce::TextButton bpmDown{"-"}, bpmUp{"+"};
  juce::TextButton sceneBtn[4], tabBtn[5], barBtn[8];
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
  int fxTrack = 0;
  juce::TextButton fxTrackBtn[5], pedalOn[3], pedalLeft[3], pedalRight[3];
  juce::ComboBox pedalBox[3];
  std::unique_ptr<ParamBar> pedalA[3], pedalR[3];
  juce::Rectangle<int> pedalArea[3];

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
    tab = juce::jlimit(0, 4, juce::SystemStats::getEnvironmentVariable("LOOPMASTER_TAB", "0").getIntValue());   // for screenshots
    const auto button = [this](juce::TextButton& b, std::function<void()> fn) {
      b.onClick = std::move(fn);
      addAndMakeVisible(b);
    };

    button(openBtn, [this] { openProject(); });
    button(saveBtn, [this] { saveProject(); });
    button(undoBtn, [this] { const juce::ScopedLock sl(p.lock()); p.session().undo(); });
    button(redoBtn, [this] { const juce::ScopedLock sl(p.lock()); p.session().redo(); });
    button(playBtn, [this] {
      if (p.hostLeads()) return;
      const juce::ScopedLock sl(p.lock());
      if (p.session().engine().isPlaying()) p.session().engine().stop(); else p.session().engine().play(scene());
    });
    button(syncBtn, [this] {
      if (auto* prm = p.params.getParameter("sync")) prm->setValueNotifyingHost(prm->getValue() > 0.5f ? 0.0f : 1.0f);
    });
    button(songBtn, [this] { p.setSongMode(!p.songMode()); });
    button(bpmDown, [this] { nudgeBpm(-1); });
    button(bpmUp, [this] { nudgeBpm(1); });
    for (int i = 0; i < 4; ++i) {
      sceneBtn[i].setButtonText(kSceneNames[i]);
      button(sceneBtn[i], [this, i] { p.setScene(i); bar = 0; });
    }
    for (int i = 0; i < 5; ++i) {
      static const char* const tabs[5] = {"DRUMS", "SYNTH", "PEDALS", "MIX", "LIBRARY"};
      tabBtn[i].setButtonText(tabs[i]);
      button(tabBtn[i], [this, i] { tab = i; message.clear(); showTab(); });
    }
    for (int i = 0; i < 8; ++i) {
      barBtn[i].setButtonText("BAR " + juce::String(i + 1));
      button(barBtn[i], [this, i] { bar = i; });
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
        const ld::FxSlot cur = p.session().project().fx[fxTrack].slots[i];
        if (type == int(cur.type)) return;
        // a new pedal starts in the middle of its range, and switched on
        p.session().setFxSlot(fxTrack, i, type, 0.5f, 0.4f, false);
      };
      pedalOn[i].onClick = [this, i] {
        const juce::ScopedLock sl(p.lock());
        const ld::FxSlot cur = p.session().project().fx[fxTrack].slots[i];
        p.session().setFxSlot(fxTrack, i, int(cur.type), cur.amount, cur.rate, !cur.bypass);
      };
      pedalLeft[i].setButtonText("<");
      pedalRight[i].setButtonText(">");
      pedalLeft[i].onClick = [this, i] { swapPedals(i, i - 1); };
      pedalRight[i].onClick = [this, i] { swapPedals(i, i + 1); };
      pedalA[i] = std::make_unique<ParamBar>(p, "-", ld::P_FX_SLOT1_AMOUNT + 2 * i, 0, 0.0f, 1.0f, fmtPct);
      pedalR[i] = std::make_unique<ParamBar>(p, "-", ld::P_FX_SLOT1_RATE + 2 * i, 0, 0.0f, 1.0f, fmtPct);
      for (juce::Component* c : {static_cast<juce::Component*>(&pedalBox[i]), static_cast<juce::Component*>(&pedalOn[i]),
                                 static_cast<juce::Component*>(&pedalLeft[i]), static_cast<juce::Component*>(&pedalRight[i]),
                                 static_cast<juce::Component*>(pedalA[i].get()), static_cast<juce::Component*>(pedalR[i].get())})
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
    message = err.isEmpty() ? "Loaded " + e.name + (libKind == 0 ? " into scene " + juce::String(kSceneNames[scene()])
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
    const bool d = tab == 0, s = tab == 1, f = tab == 2, m = tab == 3, l = tab == 4;
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
      pedalA[i]->setVisible(f); pedalR[i]->setVisible(f);
    }
    for (auto& b : kindBtn) b.setVisible(l);
    loadBtn.setVisible(l); tempoBtn.setVisible(l); styleBox.setVisible(l); list.setVisible(l);
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
    for (auto& b : sceneBtn) { b.setBounds(row2.removeFromLeft(74)); row2.removeFromLeft(4); }
    row2.removeFromLeft(8);
    songBtn.setBounds(row2.removeFromLeft(80));
    r.removeFromTop(8);
    auto tabs = r.removeFromTop(30);
    const int tw = (tabs.getWidth() - 16) / 5;
    for (auto& b : tabBtn) { b.setBounds(tabs.removeFromLeft(tw)); tabs.removeFromLeft(4); }
    r.removeFromTop(8);
    r.removeFromBottom(22);                                    // the message line, painted
    pageArea = r;

    if (tab == 0) {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      kitBox.setBounds(line1.removeFromLeft(220)); line1.removeFromLeft(8);
      hardness->setBounds(line1.removeFromLeft(200)); line1.removeFromLeft(8);
      accentBtn.setBounds(line1.removeFromLeft(90)); line1.removeFromLeft(12);
      layoutBars(line1);
      a.removeFromTop(8);
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
      rollUp.setBounds(line2.removeFromLeft(70)); line2.removeFromLeft(12);
      layoutBars(line2);
      a.removeFromTop(6);
      rollArea = a;
    } else if (tab == 2) {
      auto a = r;
      auto line1 = a.removeFromTop(28);
      for (auto& b : fxTrackBtn) { b.setBounds(line1.removeFromLeft(86)); line1.removeFromLeft(4); }
      a.removeFromTop(34);                                      // the signal path, painted
      const int gap = 26, pw = juce::jmin(280, (a.getWidth() - 2 * gap) / 3), ph = juce::jmin(a.getHeight(), 330);
      const int x0 = a.getX() + (a.getWidth() - (3 * pw + 2 * gap)) / 2;
      for (int i = 0; i < 3; ++i) {
        pedalArea[i] = juce::Rectangle<int>(x0 + i * (pw + gap), a.getY(), pw, ph);
        auto c = pedalArea[i].reduced(16, 14);
        c.removeFromTop(64);                                    // name and light, painted
        pedalBox[i].setBounds(c.removeFromTop(28)); c.removeFromTop(12);
        pedalA[i]->setBounds(c.removeFromTop(32)); c.removeFromTop(8);
        pedalR[i]->setBounds(c.removeFromTop(32)); c.removeFromTop(14);
        auto foot = c.removeFromBottom(46);
        pedalLeft[i].setBounds(foot.removeFromLeft(40).reduced(0, 8));
        pedalRight[i].setBounds(foot.removeFromRight(40).reduced(0, 8));
        pedalOn[i].setBounds(foot.reduced(10, 0));
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

  void layoutBars(juce::Rectangle<int> area) {
    for (auto& b : barBtn) { b.setBounds(area.removeFromLeft(56)); area.removeFromLeft(3); }
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
        : (p.hostLeads() ? "The host leads: its play button, position and tempo."
                         : "Own transport. Start your host's playback, or press PLAY.");
    g.drawText(hint, getLocalBounds().reduced(10).removeFromBottom(18), juce::Justification::centredLeft, true);

    const juce::ScopedLock sl(p.lock());
    if (tab == 0) paintGrid(g);
    else if (tab == 1) paintRoll(g);
    else if (tab == 2) paintPedals(g);
    else if (tab == 3) paintMixNames(g);
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
    if (a < 0 || b < 0 || a > 2 || b > 2) return;
    const juce::ScopedLock sl(p.lock());
    const ld::FxSlot sa = p.session().project().fx[fxTrack].slots[a], sb = p.session().project().fx[fxTrack].slots[b];
    p.session().setFxSlot(fxTrack, a, int(sb.type), sb.amount, sb.rate, sb.bypass);            // one undo step for both
    p.session().setFxSlot(fxTrack, b, int(sa.type), sa.amount, sa.rate, sa.bypass, false);
  }

  void paintPedals(juce::Graphics& g) {
    const ld::TrackFx& fx = p.session().project().fx[fxTrack];
    // the path of the sound
    const int y = pedalArea[0].getY() - 20;
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    g.setColour(col::muted);
    g.drawText(juce::String(kTrackNames[fxTrack]).toUpperCase() + "  IN", pedalArea[0].getX(), y - 8, 160, 16, juce::Justification::centredLeft);
    g.drawText("TO THE MIXER", pedalArea[2].getRight() - 160, y - 8, 160, 16, juce::Justification::centredRight);
    for (int i = 0; i < 3; ++i) {
      const auto box = pedalArea[i].toFloat();
      const ld::FxSlot& sl = fx.slots[i];
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
        g.drawText("SLOT " + juce::String(i + 1), pedalArea[i].withHeight(74), juce::Justification::centred);
        continue;
      }
      const juce::Colour top(sp->top), bottom(sp->bottom);
      g.setGradientFill(juce::ColourGradient(top, box.getX(), box.getY(), bottom, box.getX(), box.getBottom(), false));
      g.fillRoundedRectangle(box, 12.0f);
      g.setColour(juce::Colours::black.withAlpha(0.45f));
      g.drawRoundedRectangle(box.reduced(1.0f), 12.0f, 2.0f);
      const juce::Colour ink = sp->lightInk ? juce::Colour(0xfff7f7f7) : juce::Colour(0xff1a1408);
      g.setColour(ink);
      g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
      g.drawText(juce::String(sp->name).toUpperCase(), pedalArea[i].getX() + 16, pedalArea[i].getY() + 14, pedalArea[i].getWidth() - 60, 26, juce::Justification::centredLeft, true);
      g.setFont(juce::Font(juce::FontOptions(12.0f)));
      g.setColour(ink.withAlpha(0.75f));
      g.drawText(juce::String(sp->code) + "  -  slot " + juce::String(i + 1), pedalArea[i].getX() + 16, pedalArea[i].getY() + 42, pedalArea[i].getWidth() - 60, 16, juce::Justification::centredLeft, true);
      // the light
      const juce::Rectangle<float> led(box.getRight() - 38.0f, box.getY() + 18.0f, 16.0f, 16.0f);
      g.setColour(juce::Colours::black.withAlpha(0.6f));
      g.fillEllipse(led.expanded(2.0f));
      g.setColour(sl.bypass ? juce::Colour(0xff4a1a1a) : col::red.brighter(0.3f));
      g.fillEllipse(led);
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
  void mouseDown(const juce::MouseEvent& ev) override {
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
      for (int i = 0; i < 8; ++i) {
        barBtn[i].setVisible(onGrid && bars > 1 && i < bars);
        barBtn[i].setToggleState(i == bar, juce::dontSendNotification);
        padBtn[i].setToggleState(i == pad, juce::dontSendNotification);
      }
      for (int i = 0; i < 4; ++i) {
        sceneBtn[i].setToggleState(i == sc, juce::dontSendNotification);
        sceneBtn[i].setButtonText(juce::String(kSceneNames[i]) + "  " + juce::String(int(s.project().scenes[i].bars)) + (s.project().scenes[i].bars == 1 ? " bar" : " bars"));
        tabBtn[i].setToggleState(i == tab, juce::dontSendNotification);
        synthBtn[i].setVisible(tab == 1 && i < nSynths);
        synthBtn[i].setToggleState(i == synth, juce::dontSendNotification);
      }
      addSynthBtn.setVisible(tab == 1 && nSynths < ld::kNumSynths);
      undoBtn.setEnabled(s.canUndo());
      redoBtn.setEnabled(s.canRedo());
      const bool leads = p.hostLeads();
      playBtn.setEnabled(!leads);
      playBtn.setButtonText(leads ? "HOST" : (s.engine().isPlaying() ? "STOP" : "PLAY"));
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
          const ld::FxSlot& sl = s.project().fx[fxTrack].slots[i];
          const PedalSpec* sp = pedalSpec(int(sl.type));
          pedalBox[i].setSelectedId(sp ? sp->type + 1 : 1, juce::dontSendNotification);
          pedalA[i]->setIndex(fxTrack); pedalR[i]->setIndex(fxTrack);
          pedalA[i]->setLabel(sp ? sp->knob1 : "-"); pedalR[i]->setLabel(sp && sp->knob2[0] ? sp->knob2 : "-");
          pedalA[i]->setVisible(sp != nullptr); pedalR[i]->setVisible(sp != nullptr && sp->knob2[0] != 0);
          pedalA[i]->refresh(); pedalR[i]->refresh();
          pedalOn[i].setVisible(sp != nullptr);
          pedalOn[i].setButtonText(sl.bypass ? "OFF" : "ON");
          pedalOn[i].setToggleState(sp != nullptr && !sl.bypass, juce::dontSendNotification);
          pedalLeft[i].setEnabled(i > 0); pedalRight[i].setEnabled(i < 2);
        }
      } else if (tab == 3) {
        for (auto* b : mixBars) b->refresh();
        for (int t = 0; t < 5; ++t) {
          const bool used = t <= nSynths;
          for (int k = 0; k < 6; ++k) mixBars[t * 6 + k]->setEnabled(used);
          muteBtn[t].setEnabled(used);
          muteBtn[t].setToggleState(s.getParam(ld::P_FX_MUTE, t) > 0.5f, juce::dontSendNotification);
        }
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
  setResizeLimits(860, 560, 1720, 1120);
  setSize(980, 640);
}

LoopMasterEditor::~LoopMasterEditor() = default;

void LoopMasterEditor::resized() { if (body_) body_->setBounds(getLocalBounds()); }
void LoopMasterEditor::paint(juce::Graphics& g) { g.fillAll(juce::Colour(0xff17181c)); }
