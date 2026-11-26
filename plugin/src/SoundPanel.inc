// Part of Editor.cpp (inside its unnamed namespace): the sound editors of the DRUMS and SYNTH pages.
//
// SYNTH: every engine is a synth of its own with its own front panel: only the controls that synth has, under its
// own name and colour. The row on top is the shelf; a click takes another synth out and puts it on the table.
// DRUMS: the pad that is selected (its sound, or a drum synthesized from scratch) and the break machine.

// A skin is everything about the look of the synths that is not the synth itself: the colours of each front panel,
// where its three displays and its groups of controls sit, the colours of the displays. There is one skin so far,
// "Original"; another one is another entry in kSkins, and MENU > Skin lists them all.
struct SynthFace {
  const char* code;
  const char* name;
  const char* about;
  juce::uint32 top, bottom, cap;        // the panel from top to bottom, and the knob caps
  bool lightInk;                        // light lettering on a dark panel, or the other way round
  juce::uint32 trace;                   // the colour of what the displays draw
  int order[3];                         // which column comes first: 0 the synth's own part, 1 the filter, 2 the envelope
  bool displaysOnTop;                   // the displays above their knobs, or below them like a console
  int era;                              // 0: analog (wooden cheeks, screws, round knobs, tube scopes). 1: digital (flat, grooves, rings of lights, LCDs)
  const char* year;                     // the year its kind of synth is from
};
struct Skin {
  const char* name;
  SynthFace faces[ld::kNumSynthModes];
  juce::uint32 displayBack, displayGrid;
  float corner;                         // how round the panels are
};
const Skin kSkins[] = {
  {"Original",
   {{"LM-A", "Analog", "Two oscillators into a filter. Basses, leads, pads: the all-rounder.", 0xff2d2a26, 0xff191715, 0xffe8792b, true, 0xffff9a4a, {0, 1, 2}, true, 0, "1972"},
    {"LM-F", "FM Two", "Two sine operators, one bending the other. Bells, electric pianos, metallic basses.", 0xff1f5a4c, 0xff11362d, 0xffd9f2e6, true, 0xff6dffcf, {0, 2, 1}, true, 1, "1983"},
    {"LM-S", "String", "A plucked string. Guitars, harps, kalimbas, slap basses.", 0xff7a5230, 0xff4d321b, 0xfff1ead8, true, 0xffffd9a0, {0, 1, 2}, false, 0, "1978"},
    {"LM-V", "Vector", "Four sound sources in the corners; X and Y blend between them. Moving, morphing sounds.", 0xff24406e, 0xff142645, 0xffffca28, true, 0xffffd84a, {1, 0, 2}, true, 1, "1986"},
    {"LM-U", "Unison", "Seven detuned saws and a sub. Supersaw leads, hoovers, reese basses, big pads.", 0xff6b2a5e, 0xff401638, 0xfff7d6ef, true, 0xffff8fe6, {2, 0, 1}, true, 1, "1996"},
    {"LM-D", "Drawbar", "An organ built from harmonics, with a percussive click. House organs, stabs, soft pads.", 0xffe6dcc3, 0xffbdb08f, 0xff5a2d1a, false, 0xffffb35c, {0, 2, 1}, false, 0, "1955"},
    {"LM-P", "Phase", "A sine wave bent out of shape, more at the start of each note. Digital basses, brass, resonant sweeps.", 0xffc3c9d1, 0xff8d95a0, 0xff1e4fa3, false, 0xff5cc8ff, {1, 2, 0}, true, 1, "1984"}},
   0xff0b0d0e, 0x24ffffff, 10.0f},
};
constexpr int kNumSkins = int(sizeof(kSkins) / sizeof(kSkins[0]));

class SoundPanel : public juce::Component {
 public:
  std::function<int()> sceneNow;                               // the groove that is open
  std::function<void(const juce::String&)> say;                // a line for the bottom of the window

  const Skin& skin() const { return kSkins[juce::jlimit(0, kNumSkins - 1, p.skin)]; }

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
    clock += 1.0f / 30.0f;
    if (!drums) { sp = s.project().synths[synth]; repaint(); }      // the displays move
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
        if (shelf[i].contains(at)) return juce::String(skin().faces[i].name) + ": " + skin().faces[i].about + " A click puts it on the table.";
    if (drums && waveArea.contains(at)) return "The sound of the selected pad, from its start to its end.";
    if (!drums) {
      if (disp[0].contains(at)) return face == 3 ? juce::String("The four sources in the corners. Drag the dot: where it is, is what you hear.")
                                                 : juce::String("What this synth's wave looks like right now. Turn its knobs and watch it change.");
      if (disp[1].contains(at)) return "The filter: everything to the right of the slope is taken away. The peak is the resonance.";
      if (disp[2].contains(at)) return "The shape of each note in time: how it comes in, falls, holds and fades.";
    }
    return {};
  }

  void dragVector(const juce::MouseEvent& ev, bool first) {
    const auto pad2 = vectorPad();
    const float x = juce::jlimit(0.0f, 1.0f, float(ev.x - pad2.getX()) / float(juce::jmax(1, pad2.getWidth())));
    const float y = juce::jlimit(0.0f, 1.0f, float(ev.y - pad2.getY()) / float(juce::jmax(1, pad2.getHeight())));
    const juce::ScopedLock sl(p.lock());
    if (first) p.session().beginEdit();
    p.session().setParam(ld::P_SYN_VEC_X, synth, x);
    p.session().setParam(ld::P_SYN_VEC_Y, synth, y);
  }
  void mouseDrag(const juce::MouseEvent& ev) override { if (dragXY) dragVector(ev, false); }
  void mouseUp(const juce::MouseEvent&) override { dragXY = false; }
  void mouseDown(const juce::MouseEvent& ev) override {
    if (drums) return;
    if (face == 3 && disp[0].contains(ev.getPosition())) { dragXY = true; dragVector(ev, true); return; }
    for (int i = 0; i < ld::kNumSynthModes; ++i)
      if (shelf[i].contains(ev.getPosition()) && i != face) {
        const juce::ScopedLock sl(p.lock());
        p.session().beginEdit();
        p.session().setParam(ld::P_SYN_MODE, synth, float(i));
        if (say) say(juce::String(skin().faces[i].name) + " is on the table. " + skin().faces[i].about);
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
    if (!drums) {                                               // three columns, each a display with its controls, and a row below
      const SynthFace& f = skin().faces[face];
      const int rowH = juce::jlimit(84, 112, r.getHeight() / 4), bottomH = rowH;
      auto bottom = r.removeFromBottom(bottomH);
      r.removeFromBottom(8);
      const int dispH = juce::jmax(60, r.getHeight() - rowH - 6);
      const int wide = r.getWidth() * 42 / 100, narrow = (r.getWidth() - wide - 16) / 2;
      int x = r.getX(), pool = 0;
      juce::Rectangle<int> colBox[3];
      for (int c = 0; c < 3; ++c) {
        const int which = f.order[c], w = which == 0 ? wide : narrow;
        juce::Rectangle<int> column(x, r.getY(), w, r.getHeight());
        disp[which] = f.displaysOnTop ? column.removeFromTop(dispH) : column.removeFromBottom(dispH);
        if (f.displaysOnTop) column.removeFromTop(6); else column.removeFromBottom(6);
        colBox[which] = column;
        x += w + 8;
      }
      int bx = bottom.getX();
      for (auto& sec : sections) {
        const int n = int(sec.knobs.size()), btnCols = (int(sec.buttons.size()) + 1) / 2;
        if (sec.slot < 3) sec.box = colBox[sec.slot];
        else {
          const int w = 16 + n * 62 + btnCols * 124;
          sec.box = juce::Rectangle<int>(bx, bottom.getY(), juce::jmin(w, bottom.getRight() - bx), bottom.getHeight());
          bx += w + 8;
        }
        auto in = sec.box.reduced(8, 4).withTrimmedTop(16);
        const int kws = n == 0 ? 60 : juce::jlimit(40, 66, (in.getWidth() - btnCols * 118) / n);
        for (int i = 0; i < n; ++i) {
          if (pool < kPool) knobs[pool]->setBounds(in.removeFromLeft(kws).withSizeKeepingCentre(kws - 4, juce::jmin(in.getHeight(), kws + 18)));
          ++pool;
        }
        for (size_t i = 0; i < sec.buttons.size(); i += 2) {
          auto colm = in.removeFromLeft(118).reduced(3, 0);
          const int bh = juce::jmin(26, (colm.getHeight() - 4) / 2);
          auto both = colm.withSizeKeepingCentre(colm.getWidth(), bh * 2 + 4);
          sec.buttons[i]->setBounds(both.removeFromTop(bh));
          both.removeFromTop(4);
          if (i + 1 < sec.buttons.size()) sec.buttons[i + 1]->setBounds(both.removeFromTop(bh));
        }
      }
      return;
    }
    // drums: the sections flow from left to right and wrap
    const int rows = 3;
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
    const SynthFace& f = skin().faces[face];
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
        const SynthFace& sf = skin().faces[i];
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
      g.fillRoundedRectangle(t, skin().corner);
      g.setColour(juce::Colours::black.withAlpha(0.5f));
      g.drawRoundedRectangle(t.reduced(1.0f), skin().corner, 2.0f);
      if (f.era == 0) {                                         // analog: wooden cheeks left and right, screws, a fine line round the panel
        for (int side = 0; side < 2; ++side) {
          const juce::Rectangle<float> cheek(side ? t.getRight() - 11.0f : t.getX(), t.getY() + 3.0f, 11.0f, t.getHeight() - 6.0f);
          g.setGradientFill(juce::ColourGradient(juce::Colour(0xff7b4a22), cheek.getX(), cheek.getY(), juce::Colour(0xff43260f), cheek.getRight(), cheek.getBottom(), false));
          g.fillRoundedRectangle(cheek, 4.0f);
          g.setColour(juce::Colours::black.withAlpha(0.22f));
          for (float y = cheek.getY() + 9.0f; y < cheek.getBottom() - 6.0f; y += 13.0f) g.fillRect(cheek.getX() + 2.0f + std::fmod(y, 5.0f), y, 1.0f, 8.0f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(t.reduced(15.0f, 4.0f), skin().corner - 3.0f, 1.0f);
        for (int corner = 0; corner < 4; ++corner) {
          const float sx = corner % 2 ? t.getRight() - 22.0f : t.getX() + 22.0f, sy = corner / 2 ? t.getBottom() - 11.0f : t.getY() + 11.0f;
          g.setColour(juce::Colours::black.withAlpha(0.4f));
          g.fillEllipse(sx - 3.5f, sy - 3.5f, 7.0f, 7.0f);
          g.setColour(juce::Colour(0xffb9bcc2));
          g.fillEllipse(sx - 2.6f, sy - 2.6f, 5.2f, 5.2f);
          g.setColour(juce::Colour(0xff4a4d52));
          g.drawLine(sx - 1.8f, sy - 1.8f, sx + 1.8f, sy + 1.8f, 1.0f);
        }
      } else {                                                  // digital: fine grooves across the top, a row of small lights
        g.setColour(juce::Colours::black.withAlpha(0.14f));
        for (float y = t.getY() + 5.0f; y < t.getY() + 44.0f; y += 3.0f) g.fillRect(t.getX() + 6.0f, y, t.getWidth() - 12.0f, 1.0f);
        for (int n = 0; n < 8; ++n) {
          g.setColour(n == face ? juce::Colour(f.trace) : juce::Colours::black.withAlpha(0.45f));
          g.fillRect(t.getRight() - 26.0f - float(n) * 11.0f, t.getBottom() - 9.0f, 7.0f, 3.0f);
        }
      }
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
      g.drawText(juce::String(f.code) + "  " + (f.era ? "DIGITAL " : "ANALOG ") + f.year + "   " + f.about, tableArea.getX() + 190, tableArea.getY() + 10, tableArea.getWidth() - 206, 26, juce::Justification::centredLeft, true);
    }
    if (!drums) paintDisplays(g);
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
  // ---- the displays: what the synth is doing, drawn from its controls as they stand ----
  static float frac(float t) { return t - std::floor(t); }
  static float shape(ld::Wave w, float t) {
    t = frac(t);
    switch (w) {
      case ld::Wave::Saw: return 2.0f * t - 1.0f;
      case ld::Wave::Square: return t < 0.5f ? 1.0f : -1.0f;
      case ld::Wave::Triangle: return 4.0f * std::abs(t - 0.5f) - 1.0f;
      default: return std::sin(juce::MathConstants<float>::twoPi * t);
    }
  }
  // How bright a note is at this moment of the display's own slow pulse: 1 when it starts, fading as the synth's
  // "fade" control says. This is what makes the FM and Phase pictures breathe.
  float pulse() const { return pulseFixed >= 0.0f ? pulseFixed : std::exp(-std::fmod(clock, 1.8f) / juce::jmax(0.02f, sp.fmModDecay)); }
  juce::Rectangle<int> vectorPad() const {
    const int side = juce::jmin(disp[0].getHeight() - 14, disp[0].getWidth() * 45 / 100);
    return juce::Rectangle<int>(disp[0].getX() + 10, disp[0].getCentreY() - side / 2, side, side);
  }
  // One sample of the synth's wave, t in cycles.
  float waveAt(float t) const {
    const float two = juce::MathConstants<float>::twoPi, det = sp.detune / 1200.0f;
    float x = 0.0f;
    switch (face) {
      case 1: x = std::sin(two * t + sp.fmIndex * pulse() * std::sin(two * t * sp.fmRatio)); break;
      case 3: {
        const float a = shape(sp.wave1, t), b = shape(sp.wave2, t * (1.0f + det) + clock * det * 30.0f);
        const float c = std::sin(two * t + sp.fmIndex * pulse() * std::sin(two * t * sp.fmRatio)), d = shape(ld::Wave::Square, t * 0.5f);
        x = 0.8f * ((1.0f - sp.vecX) * ((1.0f - sp.vecY) * a + sp.vecY * c) + sp.vecX * ((1.0f - sp.vecY) * b + sp.vecY * d));
        break;
      }
      case 4: {
        static const float at[6] = {-1.0f, -0.62f, -0.27f, 0.31f, 0.66f, 1.0f};
        const float spread = (std::abs(sp.detune) * 1.2f + 4.0f) / 1200.0f;
        const ld::Wave w = sp.wave1 == ld::Wave::Square ? ld::Wave::Square : ld::Wave::Saw;
        float side = 0.0f;
        for (int k = 0; k < 6; ++k) side += shape(w, t * (1.0f + spread * at[k]) + clock * spread * at[k] * 60.0f + 0.13f * float(k));
        x = shape(w, t) * (0.75f - 0.35f * sp.vecX) + side * sp.vecX * 0.26f + std::sin(two * t * 0.5f) * sp.vecY * 0.7f;
        break;
      }
      case 5: {
        static const float harm[6] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
        float sum = std::sin(two * t * 0.5f) * drawbar(0), norm = drawbar(0);
        for (int k = 0; k < 6; ++k) { sum += std::sin(two * t * harm[k]) * drawbar(k + 1); norm += drawbar(k + 1); }
        x = sum / juce::jmax(0.2f, norm) * 1.6f;
        break;
      }
      case 6: {
        const float d = juce::jlimit(0.0f, 1.0f, sp.fmIndex / 8.0f) * (sp.fmFeedback + (1.0f - sp.fmFeedback) * pulse());
        const float u = frac(t);
        if (sp.wave1 == ld::Wave::Saw) { const float knee = 0.5f - 0.49f * d; x = -std::cos(two * (u < knee ? 0.5f * u / knee : 0.5f + 0.5f * (u - knee) / (1.0f - knee))); }
        else if (sp.wave1 == ld::Wave::Square) { const float h = u < 0.5f ? 0.0f : 0.5f; x = -std::cos(two * (h + 0.5f * juce::jmin(1.0f, (u - h) * 2.0f / (1.0f - 0.97f * d)))); }
        else if (sp.wave1 == ld::Wave::Triangle) x = std::sin(two * u * (1.0f + d * (1.0f + sp.fmRatio * 2.0f) * 7.0f)) * (1.0f - u) * 1.3f;
        else x = -std::cos(two * juce::jmin(1.0f, u / (1.0f - 0.95f * d)));
        break;
      }
      default: x = 0.5f * (shape(sp.wave1, t) + shape(sp.wave2, t * (1.0f + det) + clock * det * 30.0f)); break;
    }
    if (sp.fold > 0.01f) x = std::sin(two * x * (1.0f + 5.0f * sp.fold) * 0.25f);
    return juce::jlimit(-1.0f, 1.0f, x);
  }
  // The level of an organ drawbar: 0 the sub, 1 .. 6 the harmonics. The same sums as the engine's.
  float drawbar(int k) const {
    static const float harm[6] = {1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
    if (k == 0) return 0.5f * (1.0f - 0.6f * sp.vecX);
    const int h = k - 1;
    return std::pow(harm[h], -(2.6f - 2.4f * sp.vecX)) * (h == 1 || h == 3 || h == 4 || h == 5 ? 1.0f - 0.95f * sp.vecY : 1.0f);
  }

  void paintDisplays(juce::Graphics& g) {
    const Skin& sk = skin();
    const SynthFace& f = sk.faces[face];
    const bool lcd = f.era == 1;                                // a digital synth shows dark lines on a lit pane, an analog one a glowing line on black
    const juce::Colour trace(lcd ? juce::Colour(0xff0e150d) : juce::Colour(f.trace)), back(lcd ? juce::Colour(f.trace).withMultipliedSaturation(0.4f).withBrightness(0.74f) : juce::Colour(sk.displayBack)),
        grid(lcd ? juce::Colour(0x30000000) : juce::Colour(sk.displayGrid));
    static const char* const ownTitle[ld::kNumSynthModes] = {"THE TWO OSCILLATORS, ADDED", "ONE OPERATOR BENDS THE OTHER  -  faint: the start of a note", "THE STRING", "DRAG THE DOT", "SEVEN VOICES DRIFTING", "THE DRAWBARS", "A SINE, BENT  -  faint: the start of a note"};
    const char* const titles[3] = {ownTitle[face], "FILTER", face == 2 ? "HOW LONG IT RINGS" : "ENVELOPE"};
    for (int d = 0; d < 3; ++d) {
      const auto b = disp[d].toFloat();
      g.setColour(back);
      g.fillRoundedRectangle(b, lcd ? 1.5f : 9.0f);
      if (lcd) {                                                // the rows of an LCD
        g.setColour(juce::Colours::black.withAlpha(0.06f));
        for (float y = b.getY() + 2.0f; y < b.getBottom() - 1.0f; y += 3.0f) g.fillRect(b.getX() + 1.0f, y, b.getWidth() - 2.0f, 1.0f);
      }
      g.setColour(lcd ? juce::Colours::black.withAlpha(0.6f) : juce::Colour(0xff4a4640));
      g.drawRoundedRectangle(b.reduced(0.5f), lcd ? 1.5f : 9.0f, lcd ? 1.5f : 2.5f);
      g.setColour(grid);
      g.fillRect(b.getX() + 6.0f, b.getCentreY(), b.getWidth() - 12.0f, 1.0f);
      g.setColour(trace.withAlpha(0.6f));
      g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
      g.drawText(titles[d], disp[d].reduced(8, 4), juce::Justification::topLeft, false);
    }
    const float two = juce::MathConstants<float>::twoPi;
    const auto glow = [&](const juce::Path& path, float width) {     // a bright line with a soft one behind it
      g.setColour(trace.withAlpha(0.22f));
      g.strokePath(path, juce::PathStrokeType(width + 4.0f));
      g.setColour(trace);
      g.strokePath(path, juce::PathStrokeType(width));
    };
    // ---- the synth's own display
    {
      auto area = disp[0].reduced(10, 16).toFloat();
      if (face == 3) {                                          // Vector: the pad with its four corners
        const auto pad2 = vectorPad().toFloat();
        g.setColour(grid);
        g.drawRect(pad2, 1.0f);
        g.drawLine(pad2.getX(), pad2.getCentreY(), pad2.getRight(), pad2.getCentreY(), 1.0f);
        g.drawLine(pad2.getCentreX(), pad2.getY(), pad2.getCentreX(), pad2.getBottom(), 1.0f);
        g.setColour(trace.withAlpha(0.75f));
        g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        g.drawText("OSC 1", pad2.reduced(4.0f), juce::Justification::topLeft, false);
        g.drawText("OSC 2", pad2.reduced(4.0f), juce::Justification::topRight, false);
        g.drawText("FM", pad2.reduced(4.0f), juce::Justification::bottomLeft, false);
        g.drawText("SUB", pad2.reduced(4.0f), juce::Justification::bottomRight, false);
        const float dx = pad2.getX() + sp.vecX * pad2.getWidth(), dy = pad2.getY() + sp.vecY * pad2.getHeight();
        g.setColour(trace.withAlpha(0.25f));
        g.fillEllipse(dx - 13.0f, dy - 13.0f, 26.0f, 26.0f);
        g.setColour(trace);
        g.fillEllipse(dx - 6.0f, dy - 6.0f, 12.0f, 12.0f);
        area = area.withTrimmedLeft(pad2.getWidth() + 14.0f);
      }
      if (face == 5) {                                          // Drawbar: seven bars pulled out as far as they sound
        const float bw = juce::jmin(18.0f, area.getWidth() * 0.3f / 7.0f);
        for (int k = 0; k < 7; ++k) {
          const float lv = juce::jlimit(0.0f, 1.0f, drawbar(k) * (k == 0 ? 1.6f : 1.0f));
          const juce::Rectangle<float> slot(area.getX() + float(k) * (bw + 5.0f), area.getY(), bw, area.getHeight());
          g.setColour(grid);
          g.fillRect(slot.withWidth(2.0f).withX(slot.getCentreX() - 1.0f));
          g.setColour(k == 0 || k == 1 ? juce::Colour(0xff8a4a2a) : (k == 2 || k == 4 || k == 5 || k == 6 ? juce::Colour(0xfff1ead8) : juce::Colour(0xff1d1b18)));
          g.fillRoundedRectangle(slot.getX(), slot.getY() + (slot.getHeight() - 14.0f) * lv, bw, 14.0f, 3.0f);
          g.setColour(trace.withAlpha(0.5f));
          g.fillRect(slot.getCentreX() - 1.0f, slot.getY(), 2.0f, (slot.getHeight() - 14.0f) * lv);
        }
        area = area.withTrimmedLeft(7.0f * (bw + 5.0f) + 10.0f);
      }
      juce::Path wave;
      const int n = juce::jmax(8, int(area.getWidth()));
      if (face == 2) {                                          // String: it swings between its two ends and dies away
        const float ring = juce::jmax(0.1f, sp.decay * 1.5f), age = std::fmod(clock, ring + 0.7f), fade = std::exp(-3.0f * age / ring);
        const int harmonics = 1 + int(sp.pluckBright * 7.0f);
        for (int i = 0; i <= n; ++i) {
          const float u = float(i) / float(n);
          float y = 0.0f;
          for (int hmn = 1; hmn <= harmonics; ++hmn) y += std::sin(juce::MathConstants<float>::pi * float(hmn) * u) * std::cos(two * float(hmn) * clock * 2.2f) / float(hmn) * std::exp(-float(hmn - 1) * age * 2.0f);
          const float px = area.getX() + area.getWidth() * u, py = area.getCentreY() - y * fade * area.getHeight() * 0.42f;
          if (i == 0) wave.startNewSubPath(px, py); else wave.lineTo(px, py);
        }
        g.setColour(trace);
        g.fillEllipse(area.getX() - 4.0f, area.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.fillEllipse(area.getRight() - 4.0f, area.getCentreY() - 4.0f, 8.0f, 8.0f);
      } else {
        const float cycles = face == 4 ? 4.0f : 2.0f;
        if (face == 6) {                                        // Phase: the plain sine it started as, faint, behind it
          juce::Path plain;
          for (int i = 0; i <= n; ++i) {
            const float px = area.getX() + float(i), py = area.getCentreY() + std::cos(two * cycles * float(i) / float(n)) * area.getHeight() * 0.42f;
            if (i == 0) plain.startNewSubPath(px, py); else plain.lineTo(px, py);
          }
          g.setColour(trace.withAlpha(0.25f));
          g.strokePath(plain, juce::PathStrokeType(1.2f));
        }
        if (face == 1 || face == 6 || face == 3) {                // faint: the wave at the very start of a note, before the bend fades
          juce::Path start;
          pulseFixed = 1.0f;
          for (int i = 0; i <= n; ++i) {
            const float px = area.getX() + float(i), py = area.getCentreY() - waveAt(cycles * float(i) / float(n)) * area.getHeight() * 0.42f;
            if (i == 0) start.startNewSubPath(px, py); else start.lineTo(px, py);
          }
          pulseFixed = -1.0f;
          g.setColour(trace.withAlpha(0.3f));
          g.strokePath(start, juce::PathStrokeType(1.2f));
        }
        for (int i = 0; i <= n; ++i) {
          const float px = area.getX() + float(i), py = area.getCentreY() - waveAt(cycles * float(i) / float(n)) * area.getHeight() * 0.42f;
          if (i == 0) wave.startNewSubPath(px, py); else wave.lineTo(px, py);
        }
      }
      glow(wave, 1.8f);
    }
    // ---- the filter: what is let through, from low notes on the left to high on the right
    {
      const auto area = disp[1].reduced(10, 16).toFloat();
      float fc = sp.cutoff * std::pow(2.0f, (0.5f - sp.feelWarmth) * 1.1f);
      if (sp.lfoTarget == ld::LfoTarget::Cutoff && sp.lfoDepth > 0.0f) fc *= std::pow(2.0f, std::sin(two * clock * sp.lfoRate) * sp.lfoDepth * 1.5f);   // the wobble, as it happens
      fc = juce::jlimit(40.0f, 16000.0f, fc);
      const float q = juce::jmax(0.5f, sp.res);
      juce::Path curve, fill;
      const int n = juce::jmax(8, int(area.getWidth()));
      for (int i = 0; i <= n; ++i) {
        const float hz = 30.0f * std::pow(16000.0f / 30.0f, float(i) / float(n)), r = hz / fc;
        const float mag = 1.0f / std::sqrt((1.0f - r * r) * (1.0f - r * r) + (r / q) * (r / q));
        const float db = juce::jlimit(-30.0f, 24.0f, 20.0f * std::log10(juce::jmax(1e-4f, mag)));
        const float px = area.getX() + float(i), py = area.getY() + area.getHeight() * (1.0f - (db + 30.0f) / 54.0f);
        if (i == 0) { curve.startNewSubPath(px, py); fill.startNewSubPath(px, area.getBottom()); fill.lineTo(px, py); } else { curve.lineTo(px, py); fill.lineTo(px, py); }
      }
      fill.lineTo(area.getRight(), area.getBottom());
      fill.closeSubPath();
      g.setColour(trace.withAlpha(0.16f));
      g.fillPath(fill);
      glow(curve, 1.8f);
    }
    // ---- the envelope: one note, from its start to after its end
    {
      const auto area = disp[2].reduced(10, 16).toFloat();
      juce::Path env;
      float dotX = 0.0f, dotY = 0.0f;
      const float cycle = std::fmod(clock, 2.4f) / 2.4f;         // a dot runs along it, again and again
      if (face == 2) {
        const float ring = juce::jmax(0.1f, sp.decay);
        for (int i = 0; i <= 60; ++i) {
          const float u = float(i) / 60.0f, y = std::exp(-u * 4.5f / juce::jlimit(0.15f, 1.0f, ring / 1.5f));
          const float px = area.getX() + area.getWidth() * u, py = area.getBottom() - y * area.getHeight();
          if (i == 0) env.startNewSubPath(px, py); else env.lineTo(px, py);
          if (std::abs(u - cycle) < 0.009f) { dotX = px; dotY = py; }
        }
      } else {
        const auto wd = [](float secs) { return std::sqrt(juce::jlimit(0.001f, 3.0f, secs)); };     // short times still show
        const float a = wd(sp.attack), d = wd(sp.decay), h = 0.6f, r = wd(sp.release), total = a + d + h + r;
        const float xs[5] = {0.0f, a / total, (a + d) / total, (a + d + h) / total, 1.0f}, ys[5] = {0.0f, 1.0f, sp.sustain, sp.sustain, 0.0f};
        for (int i = 0; i < 5; ++i) {
          const float px = area.getX() + area.getWidth() * xs[i], py = area.getBottom() - ys[i] * area.getHeight();
          if (i == 0) env.startNewSubPath(px, py); else env.lineTo(px, py);
        }
        for (int i = 0; i < 4; ++i)
          if (cycle >= xs[i] && cycle <= xs[i + 1]) {
            const float t = (cycle - xs[i]) / juce::jmax(1e-4f, xs[i + 1] - xs[i]);
            dotX = area.getX() + area.getWidth() * cycle; dotY = area.getBottom() - (ys[i] + (ys[i + 1] - ys[i]) * t) * area.getHeight();
          }
        g.setColour(trace.withAlpha(0.5f));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        static const char* const parts[4] = {"A", "D", "S", "R"};
        for (int i = 0; i < 4; ++i) g.drawText(parts[i], int(area.getX() + area.getWidth() * (xs[i] + xs[i + 1]) * 0.5f) - 8, int(area.getBottom()) - 1, 16, 12, juce::Justification::centred, false);
      }
      glow(env, 1.8f);
      if (dotX > 0.0f) { g.setColour(lcd ? juce::Colours::black : juce::Colours::white); g.fillEllipse(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f); }
    }
  }

  struct Spec { const char* label; int pid; float lo, hi, centre; juce::String (*fmt)(float); const char* about; };
  struct Section { juce::String title; std::vector<Spec> knobs; std::vector<juce::TextButton*> buttons; juce::Rectangle<int> box; bool newRow = false, wide = false; int slot = 3; };

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
    if (!drums && sections.size() >= 3) { sections[0].slot = 0; sections[1].slot = 1; sections[2].slot = 2; }
    // hand the knobs out
    const SynthFace& f = skin().faces[face];
    const juce::Colour cap = drums ? col::pads[pad % 8] : juce::Colour(f.cap);
    const juce::Colour ink = drums ? col::silk : (f.lightInk ? juce::Colour(0xfff4f1e8) : juce::Colour(0xff1a1408));
    int pool = 0;
    for (auto& sec : sections) {
      for (const auto& sp : sec.knobs) {
        if (pool >= kPool) break;
        knobs[pool]->configure(sp.pid, drums ? pad : synth, sp.label, sp.lo, sp.hi, sp.centre, sp.fmt, cap, ink);
        knobs[pool]->setEra(drums ? 0 : f.era);
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
  bool drums = true, built = false, breakOn = false, wasCustom = false, dragXY = false;
  juce::Rectangle<int> disp[3];                                // the synth's own display, the filter's, the envelope's
  ld::SynthParams sp;                                          // what the displays draw, copied every tick
  float clock = 0.0f, pulseFixed = -1.0f;
  int pad = 0, synth = 0, face = 0;
};
