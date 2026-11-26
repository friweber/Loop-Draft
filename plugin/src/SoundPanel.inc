// Part of Editor.cpp (inside its unnamed namespace): the sound editors of the DRUMS and SYNTH pages.
//
// SYNTH: every engine is a synth of its own with its own front panel: only the controls that synth has, under its
// own name and colour. The row on top is the shelf; a click takes another synth out and puts it on the table.
// DRUMS: the pad that is selected (its sound, or a drum synthesized from scratch) and the break machine.

struct SynthFace {
  const char* code;
  const char* name;
  const char* about;
  juce::uint32 top, bottom, cap;
  bool lightInk;
};
const SynthFace kSynthFaces[ld::kNumSynthModes] = {
  {"LM-A", "Analog", "Two oscillators into a filter. Basses, leads, pads: the all-rounder.", 0xff2d2a26, 0xff191715, 0xffe8792b, true},
  {"LM-F", "FM Two", "Two sine operators, one bending the other. Bells, electric pianos, metallic basses.", 0xff1f5a4c, 0xff11362d, 0xffd9f2e6, true},
  {"LM-S", "String", "A plucked string. Guitars, harps, kalimbas, slap basses.", 0xff7a5230, 0xff4d321b, 0xfff1ead8, true},
  {"LM-V", "Vector", "Four sound sources in the corners; X and Y blend between them. Moving, morphing sounds.", 0xff24406e, 0xff142645, 0xffffca28, true},
  {"LM-U", "Unison", "Seven detuned saws and a sub. Supersaw leads, hoovers, reese basses, big pads.", 0xff6b2a5e, 0xff401638, 0xfff7d6ef, true},
  {"LM-D", "Drawbar", "An organ built from harmonics, with a percussive click. House organs, stabs, soft pads.", 0xffe6dcc3, 0xffbdb08f, 0xff5a2d1a, false},
  {"LM-P", "Phase", "A sine wave bent out of shape, more at the start of each note. Digital basses, brass, resonant sweeps.", 0xffc3c9d1, 0xff8d95a0, 0xff1e4fa3, false},
};

class SoundPanel : public juce::Component {
 public:
  std::function<int()> sceneNow;                               // the groove that is open
  std::function<void(const juce::String&)> say;                // a line for the bottom of the window

  explicit SoundPanel(LoopMasterProcessor& proc) : p(proc) {
    for (int i = 0; i < kPool; ++i) addChildComponent(knobs.add(new Knob(p, ld::P_SYN_CUTOFF)));
    const auto add = [this](juce::TextButton& b, std::function<void()> f, const char* tip) {
      b.onClick = std::move(f);
      help(b, tip);
      addChildComponent(b);
    };
    // synth
    add(wave1Btn, [this] { cycle(ld::P_SYN_WAVE1, 4); }, "The wave of oscillator 1 (on Phase: the shape the sine is bent towards). A click steps to the next.");
    add(wave2Btn, [this] { cycle(ld::P_SYN_WAVE2, 4); }, "The wave of oscillator 2. A click steps to the next.");
    add(lfoBtn, [this] { cycle(ld::P_SYN_LFO_TARGET, 4); }, "What the slow wobble (LFO) moves: nothing, the filter, the pitch or the loudness.");
    add(octBtn, [this] {
      const juce::ScopedLock sl(p.lock());
      const int o = int(std::floor(p.session().getParam(ld::P_SYN_OCTAVE, synth) + 0.5f));
      p.session().beginEdit();
      p.session().setParam(ld::P_SYN_OCTAVE, synth, float(o >= 2 ? -2 : o + 1));
    }, "The octave this synth plays in. A click goes up; after +2 it starts again at -2.");
    // drums
    add(reverseBtn, [this] { toggle(ld::P_PAD_REVERSE); }, "Plays this pad's sound backwards.");
    add(chokeBtn, [this] {
      const juce::ScopedLock sl(p.lock());
      const int c = int(p.session().getParam(ld::P_PAD_CHOKE, pad) + 0.5f);
      p.session().beginEdit();
      p.session().setParam(ld::P_PAD_CHOKE, pad, float((c + 1) % 3));
    }, "Choke group: pads in the same group cut each other off, as a closed hat cuts an open one.");
    static const char* const tmplNames[6] = {"KICK", "SNARE", "HAT", "GABBER", "ZAP", "CLANG"};
    for (int i = 0; i < 6; ++i) {
      tmplBtn[i].setButtonText(tmplNames[i]);
      add(tmplBtn[i], [this, i] { const juce::ScopedLock sl(p.lock()); p.session().applyDrumSynthTemplate(pad, i); },
          "Builds this pad's sound from scratch, starting as this kind of drum. Then shape it with the knobs.");
    }
    add(kitSoundBtn, [this] { const juce::ScopedLock sl(p.lock()); p.session().clearCustomPad(pad); }, "Back to the sound this pad has in the drum kit.");
    add(useBreakBtn, [this] {
      const juce::ScopedLock sl(p.lock());
      p.session().setDrumEngine(int(ld::DrumEngine::JungleSlicer));
      if (say) say("The break machine is on: the eight pads are now eight slices of a drum break.");
    }, "Turns the drums into the break machine: the eight pads become eight slices of a breakbeat.");
    add(loadBreakBtn, [this] {
      chooser = std::make_unique<juce::FileChooser>("A drum break as a WAV file (at most 24 seconds)", juce::File(), "*.wav");
      chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& fc) {
        const juce::File f = fc.getResult();
        if (f == juce::File()) return;
        const juce::ScopedLock sl(p.lock());
        const bool ok = p.session().loadJungleBreakWav(f.getFullPathName().toRawUTF8());
        if (say) say(ok ? f.getFileName() + " is cut into eight slices, one per pad." : "That file could not be read as a WAV of at most 24 seconds.");
      });
    }, "Loads a breakbeat of your own (WAV, ideally one or two bars) and cuts it into eight slices.");
    add(writeBreakBtn, [this] {
      const juce::ScopedLock sl(p.lock());
      p.session().writeBreakPattern(sceneNow ? sceneNow() : 0);
      if (say) say("The eight slices are in the grid in their original order. Now move them about.");
    }, "Writes the eight slices into this groove in their original order: the break as it was. A starting point for chopping.");
    add(resliceBtn, [this] {
      const juce::ScopedLock sl(p.lock());
      const bool ok = p.session().resampleSceneToJungle(sceneNow ? sceneNow() : 0);
      if (say) say(ok ? "This groove's drums were recorded and are now the break, cut into eight slices." : "There are no drums in this groove to record.");
    }, "Records the drums of this groove as they sound now and makes that the break: your own pattern, chopped.");
  }

  // call with the lock held
  void setTarget(bool drumsNow, int padNow, int synthNow) {
    const int mode = juce::jlimit(0, ld::kNumSynthModes - 1, int(p.session().getParam(ld::P_SYN_MODE, synthNow) + 0.5f));
    const bool jungle = int(p.session().getParam(ld::P_DRUM_ENGINE, 0) + 0.5f) == int(ld::DrumEngine::JungleSlicer);
    if (drumsNow == drums && padNow == pad && synthNow == synth && mode == face && jungle == breakOn && built) return;
    drums = drumsNow; pad = padNow; synth = synthNow; face = mode; breakOn = jungle; built = true;
    build();
    resized();
    repaint();
  }

  // call with the lock held
  void tick() {
    if (!isVisible()) return;
    ld::Session& s = p.session();
    for (auto* k : knobs) if (k->isVisible()) k->refresh();
    if (drums) {
      reverseBtn.setToggleState(s.getParam(ld::P_PAD_REVERSE, pad) > 0.5f, juce::dontSendNotification);
      const int c = int(s.getParam(ld::P_PAD_CHOKE, pad) + 0.5f);
      chokeBtn.setButtonText(c == 0 ? "CHOKE: OFF" : "CHOKE: GROUP " + juce::String(c));
      const bool custom = s.project().customDrums[pad].enabled;
      kitSoundBtn.setEnabled(custom);
      if (custom != wasCustom) { wasCustom = custom; repaint(); }
      useBreakBtn.setToggleState(breakOn, juce::dontSendNotification);
      writeBreakBtn.setEnabled(breakOn);
      if ((++frame % 8) == 1) {                                 // the picture of the pad's sound
        float now[kWave];
        const int n = s.padWaveform(pad, now, kWave);
        bool changed = n != waveN;
        for (int i = 0; i < n && !changed; ++i) changed = now[i] != wave[i];
        if (changed) { waveN = n; for (int i = 0; i < n; ++i) wave[i] = now[i]; repaint(); }
      }
    } else {
      static const char* const waves[4] = {"SAW", "SQUARE", "TRIANGLE", "SINE"};
      static const char* const shapes[4] = {"SHAPE: SAW", "SHAPE: SQUARE", "SHAPE: RESONANT", "SHAPE: PULSE"};
      static const char* const targets[4] = {"LFO: OFF", "LFO > FILTER", "LFO > PITCH", "LFO > LOUDNESS"};
      const int w1 = juce::jlimit(0, 3, int(s.getParam(ld::P_SYN_WAVE1, synth) + 0.5f)), w2 = juce::jlimit(0, 3, int(s.getParam(ld::P_SYN_WAVE2, synth) + 0.5f));
      wave1Btn.setButtonText(face == int(ld::SynthMode::Phase) ? juce::String(shapes[w1]) : (face == int(ld::SynthMode::Unison) ? juce::String(w1 == 1 ? "SQUARES" : "SAWS") : "1: " + juce::String(waves[w1])));
      wave2Btn.setButtonText("2: " + juce::String(waves[w2]));
      lfoBtn.setButtonText(targets[juce::jlimit(0, 3, int(s.getParam(ld::P_SYN_LFO_TARGET, synth) + 0.5f))]);
      const int o = int(std::floor(s.getParam(ld::P_SYN_OCTAVE, synth) + 0.5f));
      octBtn.setButtonText("OCTAVE " + juce::String(o > 0 ? "+" : "") + juce::String(o));
    }
  }

  // The sentence for a painted part under the mouse.
  juce::String helpAt(juce::Point<int> at) const {
    if (!drums)
      for (int i = 0; i < ld::kNumSynthModes; ++i)
        if (shelf[i].contains(at)) return juce::String(kSynthFaces[i].name) + ": " + kSynthFaces[i].about + " A click puts it on the table.";
    if (drums && waveArea.contains(at)) return "The sound of the selected pad, from its start to its end.";
    return {};
  }

  void mouseDown(const juce::MouseEvent& ev) override {
    if (drums) return;
    for (int i = 0; i < ld::kNumSynthModes; ++i)
      if (shelf[i].contains(ev.getPosition()) && i != face) {
        const juce::ScopedLock sl(p.lock());
        p.session().beginEdit();
        p.session().setParam(ld::P_SYN_MODE, synth, float(i));
        if (say) say(juce::String(kSynthFaces[i].name) + " is on the table. " + kSynthFaces[i].about);
        return;
      }
  }

  void resized() override {
    auto r = getLocalBounds();
    if (!built) return;
    if (drums) {
      waveArea = r.removeFromTop(juce::jlimit(36, 70, r.getHeight() / 6));
      r.removeFromTop(6);
    } else {
      auto top = r.removeFromTop(40);
      const int sw = (top.getWidth() - 6 * (ld::kNumSynthModes - 1)) / ld::kNumSynthModes;
      for (int i = 0; i < ld::kNumSynthModes; ++i) { shelf[i] = top.removeFromLeft(sw); top.removeFromLeft(6); }
      r.removeFromTop(8);
      tableArea = r;
      r = r.reduced(12, 8);
      r.removeFromTop(34);                                      // the synth's name, painted
    }
    // the sections flow from left to right and wrap
    const int rows = drums ? 3 : 2;
    const int rowH = juce::jlimit(84, 128, (r.getHeight() - (rows - 1) * 8) / rows);
    const int kw = juce::jlimit(52, 70, (r.getWidth() - 40) / 13);
    int x = r.getX(), y = r.getY(), pool = 0;
    for (auto& sec : sections) {
      const int btnCols = (int(sec.buttons.size()) + 1) / 2, bw = sec.wide ? 80 : 112;
      // a section with many controls gets narrower knobs, so all of it fits in one row
      const int kws = sec.knobs.empty() ? kw : juce::jmin(kw, (r.getWidth() - 16 - btnCols * bw) / int(sec.knobs.size()));
      const int w = 16 + int(sec.knobs.size()) * kws + btnCols * bw;
      if ((x + w > r.getRight() && x > r.getX()) || sec.newRow) { x = r.getX(); y += rowH + 8; }
      sec.box = juce::Rectangle<int>(x, y, juce::jmin(w, r.getWidth()), rowH);
      auto in = sec.box.reduced(8, 4).withTrimmedTop(16);
      for (size_t i = 0; i < sec.knobs.size(); ++i) {
        if (pool < kPool) knobs[pool]->setBounds(in.removeFromLeft(kws).withSizeKeepingCentre(kws - 4, juce::jmin(in.getHeight(), kws + 18)));
        ++pool;
      }
      for (size_t i = 0; i < sec.buttons.size(); i += 2) {
        auto colm = in.removeFromLeft(bw).reduced(3, 0);
        const int bh = juce::jmin(26, (colm.getHeight() - 4) / 2);
        auto both = colm.withSizeKeepingCentre(colm.getWidth(), bh * 2 + 4);
        sec.buttons[i]->setBounds(both.removeFromTop(bh));
        both.removeFromTop(4);
        if (i + 1 < sec.buttons.size()) sec.buttons[i + 1]->setBounds(both.removeFromTop(bh));
      }
      x += w + 8;
    }
  }

  void paint(juce::Graphics& g) override {
    if (!built) return;
    const SynthFace& f = kSynthFaces[face];
    juce::Colour ink = col::silk, dim = col::muted, boxFill = col::recess, boxLine = col::line;
    if (drums) {
      g.setColour(juce::Colour(0xff0d0f10));
      g.fillRoundedRectangle(waveArea.toFloat(), 5.0f);
      g.setColour(col::pads[pad % 8]);
      const float mid = float(waveArea.getCentreY()), half = float(waveArea.getHeight()) * 0.5f - 4.0f;
      for (int i = 0; i < waveN; ++i) {
        const float x = float(waveArea.getX()) + 6.0f + float(i) * float(waveArea.getWidth() - 12) / float(juce::jmax(1, waveN));
        const float h = juce::jmax(0.5f, wave[i] * half);
        g.fillRect(x, mid - h, juce::jmax(1.0f, float(waveArea.getWidth() - 12) / float(juce::jmax(1, waveN)) - 0.5f), h * 2.0f);
      }
      g.setColour(col::muted);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.drawText(juce::String(kPadNames[pad]).toUpperCase() + (wasCustom ? "  -  SYNTHESIZED" : (breakOn ? "  -  A SLICE OF THE BREAK" : "  -  FROM THE KIT")), waveArea.reduced(10, 4),
                 juce::Justification::topLeft, false);
    } else {
      // the shelf
      for (int i = 0; i < ld::kNumSynthModes; ++i) {
        const SynthFace& sf = kSynthFaces[i];
        const auto b = shelf[i].toFloat();
        g.setGradientFill(juce::ColourGradient(juce::Colour(sf.top), b.getX(), b.getY(), juce::Colour(sf.bottom), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle(b, 5.0f);
        g.setColour(i == face ? juce::Colours::white : juce::Colours::black.withAlpha(0.5f));
        g.drawRoundedRectangle(b.reduced(1.0f), 5.0f, i == face ? 2.5f : 1.0f);
        g.setColour(sf.lightInk ? juce::Colour(0xfff4f1e8) : juce::Colour(0xff1a1408));
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText(juce::String(sf.name).toUpperCase(), shelf[i].reduced(8, 3), juce::Justification::centredLeft, true);
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.setColour((sf.lightInk ? juce::Colour(0xfff4f1e8) : juce::Colour(0xff1a1408)).withAlpha(0.65f));
        g.drawText(sf.code, shelf[i].reduced(8, 4), juce::Justification::bottomRight, false);
      }
      // the synth on the table
      const auto t = tableArea.toFloat();
      g.setGradientFill(juce::ColourGradient(juce::Colour(f.top), t.getX(), t.getY(), juce::Colour(f.bottom), t.getX(), t.getBottom(), false));
      g.fillRoundedRectangle(t, 10.0f);
      g.setColour(juce::Colours::black.withAlpha(0.5f));
      g.drawRoundedRectangle(t.reduced(1.0f), 10.0f, 2.0f);
      g.setColour(juce::Colours::white.withAlpha(0.15f));
      g.drawRoundedRectangle(t.reduced(4.0f), 7.0f, 1.0f);
      ink = f.lightInk ? juce::Colour(0xfff4f1e8) : juce::Colour(0xff1a1408);
      dim = ink.withAlpha(0.7f);
      boxFill = juce::Colours::black.withAlpha(f.lightInk ? 0.22f : 0.08f);
      boxLine = ink.withAlpha(0.25f);
      g.setColour(ink);
      g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
      g.drawText(juce::String(f.name).toUpperCase(), tableArea.getX() + 16, tableArea.getY() + 8, 220, 28, juce::Justification::centredLeft, false);
      g.setColour(juce::Colour(f.cap));
      g.fillRect(tableArea.getX() + 16, tableArea.getY() + 37, 60, 3);
      g.setColour(dim);
      g.setFont(juce::Font(juce::FontOptions(12.0f)));
      g.drawText(juce::String(f.code) + "   " + f.about, tableArea.getX() + 190, tableArea.getY() + 10, tableArea.getWidth() - 206, 26, juce::Justification::centredLeft, true);
    }
    for (const auto& sec : sections) {
      g.setColour(boxFill);
      g.fillRoundedRectangle(sec.box.toFloat(), 6.0f);
      g.setColour(boxLine);
      g.drawRoundedRectangle(sec.box.toFloat().reduced(0.5f), 6.0f, 1.0f);
      g.setColour(dim);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.drawText(sec.title, sec.box.reduced(9, 4), juce::Justification::topLeft, true);
    }
  }

 private:
  struct Spec { const char* label; int pid; float lo, hi, centre; juce::String (*fmt)(float); const char* about; };
  struct Section { juce::String title; std::vector<Spec> knobs; std::vector<juce::TextButton*> buttons; juce::Rectangle<int> box; bool newRow = false, wide = false; };

  void cycle(int pid, int count) {
    const juce::ScopedLock sl(p.lock());
    const int v = int(p.session().getParam(pid, synth) + 0.5f);
    p.session().beginEdit();
    p.session().setParam(pid, synth, float((v + 1) % count));
  }
  void toggle(int pid) {
    const juce::ScopedLock sl(p.lock());
    p.session().beginEdit();
    p.session().setParam(pid, pad, p.session().getParam(pid, pad) > 0.5f ? 0.0f : 1.0f);
  }

  // Decides which sections, knobs and buttons there are for what is selected.
  void build() {
    sections.clear();
    for (auto* b : {&wave1Btn, &wave2Btn, &lfoBtn, &octBtn, &reverseBtn, &chokeBtn, &kitSoundBtn, &useBreakBtn, &loadBreakBtn, &writeBreakBtn, &resliceBtn}) b->setVisible(false);
    for (auto& b : tmplBtn) b.setVisible(false);
    using S = Spec;
    if (drums) {
      sections.push_back({juce::String("THIS PAD: ") + juce::String(kPadNames[pad]).toUpperCase(),
        {S{"Volume", ld::P_PAD_VOL, 0.0f, 1.0f, 0.0f, fmtPct, "how loud this pad is"}, S{"Pan", ld::P_PAD_PAN, -1.0f, 1.0f, 0.0f, fmtPan, "left or right"},
         S{"Pitch", ld::P_PAD_PITCH, -12.0f, 12.0f, 0.0f, fmtSemis, "tunes the pad up or down"}, S{"Decay", ld::P_PAD_DECAY, 0.03f, 1.5f, 0.3f, fmtSecs, "how long it rings"}},
        {&reverseBtn, &chokeBtn}, {}, false, false});
      Section brk{breakOn ? "BREAK MACHINE  -  on: the pads are slices of a break" : "BREAK MACHINE  -  a breakbeat cut into eight slices, for jungle and breakbeat",
        {S{"Brk pitch", ld::P_JUNGLE_PITCH, -12.0f, 12.0f, 0.0f, fmtSemis, "tunes the whole break; lower is slower and heavier"},
         S{"Tight", ld::P_JUNGLE_TIGHT, 0.08f, 1.5f, 0.5f, fmtSecs, "how long each slice may ring: short is tight and choppy"},
         S{"Crunch", ld::P_JUNGLE_CRUNCH, 0.0f, 1.0f, 0.0f, fmtPct, "the grit of an old sampler"},
         S{"Slice from", ld::P_PAD_SLICE_START, 0.0f, 1.0f, 0.0f, fmtPct, "where in the break this pad's slice starts"},
         S{"Slice to", ld::P_PAD_SLICE_END, 0.0f, 1.0f, 0.0f, fmtPct, "where this pad's slice ends"}},
        {&useBreakBtn, &loadBreakBtn, &writeBreakBtn, &resliceBtn}, {}, false, false};
      sections.push_back(brk);
      sections.push_back({"BUILD THIS PAD FROM SCRATCH  -  pick a starting point, then shape it",
        {S{"Pitch", ld::P_DSYN_FREQ, 25.0f, 1200.0f, 140.0f, fmtInt, "the note the drum settles on, in hertz"},
         S{"Drop", ld::P_DSYN_DROP, 0.0f, 600.0f, 0.0f, fmtInt, "how far the pitch falls at the start: the punch of a kick"},
         S{"Drop time", ld::P_DSYN_PDEC, 0.005f, 0.3f, 0.05f, fmtMs, "how fast it falls"},
         S{"Tone", ld::P_DSYN_TONE, 0.0f, 1.0f, 0.0f, fmtPct, "how much pitched body"}, S{"Noise", ld::P_DSYN_NOISE, 0.0f, 1.0f, 0.0f, fmtPct, "how much noise: snares and hats"},
         S{"Colour", ld::P_DSYN_NFREQ, 300.0f, 10000.0f, 2500.0f, fmtInt, "dark or bright noise"},
         S{"FM", ld::P_DSYN_FM, 0.0f, 8.0f, 0.0f, fmtOne, "metallic overtones"}, S{"FM ratio", ld::P_DSYN_FMRATIO, 0.5f, 8.0f, 0.0f, fmtOne, "which overtones"},
         S{"Drive", ld::P_DSYN_DRIVE, 0.0f, 1.0f, 0.0f, fmtPct, "distortion"}, S{"Click", ld::P_DSYN_CLICK, 0.0f, 1.0f, 0.0f, fmtPct, "a click at the very start"},
         S{"Length", ld::P_DSYN_LEN, 0.05f, 1.5f, 0.4f, fmtSecs, "how long the drum is"}},
        {&tmplBtn[0], &tmplBtn[1], &tmplBtn[2], &tmplBtn[3], &tmplBtn[4], &tmplBtn[5], &kitSoundBtn}, {}, true, true});
    } else {
      const S cutoff{"Cutoff", ld::P_SYN_CUTOFF, 60.0f, 12000.0f, 900.0f, fmtInt, "where the filter closes: left is dark"}, reso{"Reso", ld::P_SYN_RES, 0.5f, 15.0f, 3.0f, fmtOne, "a peak at the cutoff: the squelch"};
      const S fmRatio{"Ratio", ld::P_SYN_FM_RATIO, 0.5f, 8.0f, 0.0f, fmtOne, "which overtones the second operator adds; whole numbers are harmonic"};
      const S fmIndex{"Amount", ld::P_SYN_FM_INDEX, 0.0f, 8.0f, 0.0f, fmtOne, "how strongly the tone is bent: more is brighter"};
      const S fmDecay{"Fade", ld::P_SYN_FM_DECAY, 0.01f, 2.0f, 0.3f, fmtSecs, "how fast that brightness dies away in each note"};
      const S fmFb{"Feedback", ld::P_SYN_FM_FB, 0.0f, 1.0f, 0.0f, fmtPct, "feeds the operator into itself: towards noise"};
      const S detune{"Detune", ld::P_SYN_DETUNE, -50.0f, 50.0f, 0.0f, fmtCents, "how far oscillator 2 is off pitch: thickness"};
      switch (face) {
        case 1: sections.push_back({"OPERATORS", {fmRatio, fmIndex, fmDecay, fmFb}, {}, {}, false, false}); break;
        case 2:
          sections.push_back({"STRING", {S{"Bright", ld::P_SYN_PLUCK_BRIGHT, 0.0f, 1.0f, 0.0f, fmtPct, "a soft finger or a hard pick"},
                                         S{"Ring", ld::P_SYN_DECAY, 0.03f, 3.0f, 0.5f, fmtSecs, "how long the string rings"}}, {}, {}, false, false});
          break;
        case 3:
          sections.push_back({"SOURCES  -  1 and 2 are oscillators, the other corners an FM tone and a sub",
            {S{"X", ld::P_SYN_VEC_X, 0.0f, 1.0f, 0.0f, fmtPct, "blends from oscillator 1 and the FM tone to oscillator 2 and the sub"},
             S{"Y", ld::P_SYN_VEC_Y, 0.0f, 1.0f, 0.0f, fmtPct, "blends from the oscillators to the FM tone and the sub"}, detune, fmRatio, fmIndex},
            {&wave1Btn, &wave2Btn}, {}, false, false});
          break;
        case 4:
          sections.push_back({"STACK", {S{"Spread", ld::P_SYN_DETUNE, -50.0f, 50.0f, 0.0f, fmtCents, "how far apart the seven are tuned: from tight to huge"},
                                        S{"Stack", ld::P_SYN_VEC_X, 0.0f, 1.0f, 0.0f, fmtPct, "how much of the six outer voices against the middle one"},
                                        S{"Sub", ld::P_SYN_VEC_Y, 0.0f, 1.0f, 0.0f, fmtPct, "a sine an octave below"}}, {&wave1Btn}, {}, false, false});
          break;
        case 5:
          sections.push_back({"DRAWBARS", {S{"Tone", ld::P_SYN_VEC_X, 0.0f, 1.0f, 0.0f, fmtPct, "from the low drawbars only to all of them: dark to bright"},
                                           S{"Hollow", ld::P_SYN_VEC_Y, 0.0f, 1.0f, 0.0f, fmtPct, "takes the even harmonics out: towards a clarinet"},
                                           S{"Perc", ld::P_SYN_FM_INDEX, 0.0f, 8.0f, 0.0f, fmtOne, "a short ping at the start of each note"},
                                           S{"Perc len", ld::P_SYN_FM_DECAY, 0.01f, 2.0f, 0.3f, fmtSecs, "how long the ping lasts"}}, {}, {}, false, false});
          break;
        case 6:
          sections.push_back({"PHASE", {S{"Bend", ld::P_SYN_FM_INDEX, 0.0f, 8.0f, 0.0f, fmtOne, "how far the sine is bent towards the shape: more is brighter"},
                                        S{"Fade", ld::P_SYN_FM_DECAY, 0.01f, 2.0f, 0.3f, fmtSecs, "how fast the bend relaxes in each note"},
                                        S{"Stay", ld::P_SYN_FM_FB, 0.0f, 1.0f, 0.0f, fmtPct, "how much of the bend stays while the note is held"},
                                        S{"Sweep", ld::P_SYN_FM_RATIO, 0.5f, 8.0f, 0.0f, fmtOne, "on the resonant shape: how high the sweep reaches"}, detune},
                              {&wave1Btn}, {}, false, false});
          break;
        default: sections.push_back({"OSCILLATORS", {detune}, {&wave1Btn, &wave2Btn}, {}, false, false}); break;
      }
      sections.push_back({"FILTER", {cutoff, reso}, {}, {}, false, false});
      sections.push_back({face == 2 ? "LOUDNESS" : "ENVELOPE  -  the shape of each note",
        face == 2 ? std::vector<Spec>{S{"Release", ld::P_SYN_RELEASE, 0.01f, 3.0f, 0.3f, fmtSecs, "how long a note fades after it ends"}}
                  : std::vector<Spec>{S{"Attack", ld::P_SYN_ATTACK, 0.001f, 1.0f, 0.05f, fmtMs, "how fast a note comes in"}, S{"Decay", ld::P_SYN_DECAY, 0.02f, 3.0f, 0.4f, fmtSecs, "how fast it falls to the sustain level"},
                                      S{"Sustain", ld::P_SYN_SUSTAIN, 0.0f, 1.0f, 0.0f, fmtPct, "the level while the note is held"}, S{"Release", ld::P_SYN_RELEASE, 0.01f, 3.0f, 0.3f, fmtSecs, "how long it fades after the note ends"}},
        {}, {}, false, false});
      sections.push_back({"LFO  -  a slow wobble", {S{"Rate", ld::P_SYN_LFO_RATE, 0.05f, 20.0f, 3.0f, fmtOne, "how fast it wobbles"}, S{"Depth", ld::P_SYN_LFO_DEPTH, 0.0f, 1.0f, 0.0f, fmtPct, "how much"}},
                          {&lfoBtn}, {}, false, false});
      sections.push_back({"CHARACTER", {S{"Warmth", ld::P_SYN_WARMTH, 0.0f, 1.0f, 0.0f, fmtPct, "rounder highs and a little saturation"}, S{"Motion", ld::P_SYN_MOTION, 0.0f, 1.0f, 0.0f, fmtPct, "more drift and wobble: alive, less exact"},
                                        S{"Fold", ld::P_SYN_FOLD, 0.0f, 1.0f, 0.0f, fmtPct, "folds the wave over itself: buzzing overtones"}, S{"Slide", ld::P_SYN_SLIDE_TIME, 0.01f, 0.4f, 0.08f, fmtMs, "how long a slide note takes to arrive"}},
                          {}, {}, false, false});
      sections.push_back({"OUT", {S{"Level", ld::P_SYN_LEVEL, 0.0f, 1.0f, 0.0f, fmtPct, "how loud this synth is"}}, {&octBtn}, {}, false, false});
    }
    // hand the knobs out
    const SynthFace& f = kSynthFaces[face];
    const juce::Colour cap = drums ? col::pads[pad % 8] : juce::Colour(f.cap);
    const juce::Colour ink = drums ? col::silk : (f.lightInk ? juce::Colour(0xfff4f1e8) : juce::Colour(0xff1a1408));
    int pool = 0;
    for (auto& sec : sections) {
      for (const auto& sp : sec.knobs) {
        if (pool >= kPool) break;
        knobs[pool]->configure(sp.pid, drums ? pad : synth, sp.label, sp.lo, sp.hi, sp.centre, sp.fmt, cap, ink);
        help(*knobs[pool], juce::String(sp.label).toUpperCase() + ": " + sp.about + ". Double-click puts it back.");
        knobs[pool]->setVisible(true);
        ++pool;
      }
      for (auto* b : sec.buttons) b->setVisible(true);
    }
    for (; pool < kPool; ++pool) knobs[pool]->setVisible(false);
  }

  static constexpr int kPool = 24, kWave = 200;
  LoopMasterProcessor& p;
  juce::OwnedArray<Knob> knobs;
  std::vector<Section> sections;
  juce::TextButton wave1Btn, wave2Btn, lfoBtn, octBtn, reverseBtn{"REVERSE"}, chokeBtn, tmplBtn[6], kitSoundBtn{"KIT SOUND"}, useBreakBtn{"SWITCH IT ON"},
      loadBreakBtn{"YOUR OWN WAV..."}, writeBreakBtn{"WRITE BREAK"}, resliceBtn{"CHOP THIS GROOVE"};
  std::unique_ptr<juce::FileChooser> chooser;
  juce::Rectangle<int> shelf[ld::kNumSynthModes], tableArea, waveArea;
  float wave[kWave] = {};
  int waveN = 0, frame = 0;
  bool drums = true, built = false, breakOn = false, wasCustom = false;
  int pad = 0, synth = 0, face = 0;
};
