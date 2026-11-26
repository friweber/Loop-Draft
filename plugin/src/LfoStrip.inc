// Part of Editor.cpp (inside its unnamed namespace): the three LFOs, side by side in one strip.
// An LFO is a slow wave that keeps moving one knob. Click an LFO's target button, then touch the knob it should
// move: that is all. Every LFO has a shape, a speed (in time with the tempo, or free) and a depth.
class LfoStrip : public juce::Component {
 public:
  std::function<juce::String(int pid, int index)> nameOf;      // "Bass: Cutoff"
  std::function<void(const juce::String&)> say;
  int armed = -1;                                              // the LFO that waits for a knob to be touched

  explicit LfoStrip(LoopMasterProcessor& proc) : p(proc) {
    static const char* const tips[3] = {
      "Click, then touch the knob or bar this LFO should move. While it waits, a second click disconnects it.",
      "The shape of the wave: sine, triangle, ramp, square or random steps. A click steps to the next.",
      "SYNC: the speed is a length in bars and beats and follows the tempo. Off: free, in Hz."};
    for (int i = 0; i < ld::kNumLfos; ++i) {
      target[i].onClick = [this, i] {
        if (armed == i) {                                       // second click: off
          armed = -1;
          change(i, [](ld::ModLfo& l) { l.paramId = -1; });
          if (say) say("LFO " + juce::String(i + 1) + " is disconnected.");
          return;
        }
        armed = i;
        serial = p.touchSerial;
        if (say) say("LFO " + juce::String(i + 1) + " waits: now touch the knob or bar it should move (on any page). Click the button again to disconnect it instead.");
      };
      shape[i].onClick = [this, i] { change(i, [](ld::ModLfo& l) { l.shape = uint8_t((l.shape + 1) % ld::kNumLfoShapes); }); };
      sync[i].setButtonText("SYNC");
      sync[i].onClick = [this, i] { change(i, [](ld::ModLfo& l) { l.sync = l.sync ? 0 : 1; }); };
      for (auto* b : {&target[i], &shape[i], &sync[i]}) addAndMakeVisible(*b);
      help(target[i], tips[0]); help(shape[i], tips[1]); help(sync[i], tips[2]);
      for (juce::Slider* sl : {&rate[i], &depth[i]}) {
        const bool isRate = sl == &rate[i];
        sl->setSliderStyle(juce::Slider::LinearBar);
        sl->setRange(0.0, 1.0, 0.0);
        sl->setTextBoxIsEditable(false);
        sl->textFromValueFunction = [this, i, isRate](double v) { return isRate ? "Speed  " + rateText(i, float(v)) : "Depth  " + juce::String(juce::roundToInt(v * 100.0)) + "%"; };
        sl->onDragStart = [this, sl] { drag = sl; const juce::ScopedLock lk(p.lock()); p.session().beginEdit(); };
        sl->onDragEnd = [this] { drag = nullptr; };
        sl->onValueChange = [this, i, sl, isRate] {
          if (setting) return;
          const juce::ScopedLock lk(p.lock());
          ld::ModLfo l = p.session().project().lfos[i];
          (isRate ? l.rate : l.depth) = float(sl->getValue());
          p.session().setLfo(i, l, false);
        };
        addAndMakeVisible(*sl);
        help(*sl, isRate ? "How fast the LFO swings: from once in eight bars to once a sixteenth with SYNC, 0.05 to 20 Hz without."
                         : "How far the LFO moves the knob around where it stands. At 100% it swings over the knob's whole range.");
      }
    }
  }

  // 30 times a second, from the window's timer.
  void tick() {
    const juce::ScopedLock lk(p.lock());
    ld::Session& s = p.session();
    if (armed >= 0 && p.touchSerial != serial) {                // a control was touched: that is the target
      serial = p.touchSerial;
      const int pid = p.lastPid, index = p.lastIndex, i = armed;
      armed = -1;
      if (ld::lfoCanMove(pid)) {
        ld::ModLfo l = s.project().lfos[i];
        l.paramId = int16_t(pid); l.index = int16_t(index);
        if (l.depth < 0.05f) l.depth = 0.5f;
        s.setLfo(i, l);
        if (say) say("LFO " + juce::String(i + 1) + " now moves " + (nameOf ? nameOf(pid, index) : juce::String("that knob")) + ". Speed and Depth are next to it.");
      } else if (say) say("An LFO cannot move that control. It can move levels, drive, the EQ, the sends, every pedal knob, and a synth's cutoff, resonance, decay, level and tone knobs.");
    }
    static const char* const shapes[ld::kNumLfoShapes] = {"SINE", "TRIANGLE", "RAMP", "SQUARE", "RANDOM"};
    setting = true;
    for (int i = 0; i < ld::kNumLfos; ++i) {
      const ld::ModLfo& l = s.project().lfos[i];
      const bool on = l.paramId >= 0;
      target[i].setButtonText(armed == i ? juce::String("TOUCH A KNOB...") : (on ? (nameOf ? nameOf(l.paramId, l.index) : juce::String("on")) : juce::String("CHOOSE A KNOB")));
      target[i].setToggleState(armed == i || on, juce::dontSendNotification);
      shape[i].setButtonText(shapes[juce::jmin(int(l.shape), ld::kNumLfoShapes - 1)]);
      sync[i].setToggleState(l.sync != 0, juce::dontSendNotification);
      if (drag != &rate[i]) { rate[i].setValue(double(l.rate), juce::dontSendNotification); rate[i].updateText(); }
      if (drag != &depth[i]) depth[i].setValue(double(l.depth), juce::dontSendNotification);
      now[i] = s.engine().lfoValue(i);
      shapeNow[i] = l.shape; depthNow[i] = on ? l.depth : 0.0f;
    }
    setting = false;
    repaint();
  }

  void resized() override {
    auto r = getLocalBounds().reduced(6, 4);
    const int w = (r.getWidth() - 2 * 10) / 3;
    for (int i = 0; i < ld::kNumLfos; ++i) {
      auto u = r.removeFromLeft(w); r.removeFromLeft(10);
      unit[i] = u;
      u.removeFromLeft(52);                                     // the name, painted
      scope[i] = u.removeFromRight(70).reduced(0, 2); u.removeFromRight(6);
      auto line1 = u.removeFromTop(u.getHeight() / 2 - 1); u.removeFromTop(2);
      sync[i].setBounds(line1.removeFromRight(50)); line1.removeFromRight(3);
      shape[i].setBounds(line1.removeFromRight(78)); line1.removeFromRight(3);
      target[i].setBounds(line1);
      rate[i].setBounds(u.removeFromLeft(u.getWidth() / 2 - 2)); u.removeFromLeft(4);
      depth[i].setBounds(u);
    }
  }

  void paint(juce::Graphics& g) override {
    g.setColour(col::faceplate);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setColour(col::line);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f, 1.0f);
    static const juce::uint32 tint[3] = {0xffe8792b, 0xff5cc8ff, 0xff9be05a};
    for (int i = 0; i < ld::kNumLfos; ++i) {
      const juce::Colour c(tint[i]);
      g.setColour(c);
      g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
      g.drawText("LFO " + juce::String(i + 1), unit[i].withWidth(50), juce::Justification::centredLeft, false);
      const auto b = scope[i].toFloat();
      g.setColour(juce::Colour(0xff0b0d0e));
      g.fillRoundedRectangle(b, 4.0f);
      juce::Path w;                                             // one cycle of the shape, as deep as the LFO is set
      const float amp = (b.getHeight() * 0.5f - 4.0f) * (0.25f + 0.75f * depthNow[i]);
      for (int px = 0; px <= int(b.getWidth()) - 8; ++px) {
        const float ph = float(px) / (b.getWidth() - 8.0f);
        float v = 0.0f;
        switch (shapeNow[i]) {
          case 1: v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
          case 2: v = 1.0f - 2.0f * ph; break;
          case 3: v = ph < 0.5f ? 1.0f : -1.0f; break;
          case 4: v = std::sin(float(int(ph * 6.0f)) * 12.9898f) ; break;
          default: v = std::sin(juce::MathConstants<float>::twoPi * ph); break;
        }
        const float x = b.getX() + 4.0f + float(px), y = b.getCentreY() - v * amp;
        if (px == 0) w.startNewSubPath(x, y); else w.lineTo(x, y);
      }
      g.setColour(c.withAlpha(depthNow[i] > 0.0f ? 0.9f : 0.3f));
      g.strokePath(w, juce::PathStrokeType(1.5f));
      // where the wave stands right now: a light that goes up and down
      const float y = b.getCentreY() - now[i] * amp;
      g.setColour(c.withAlpha(0.3f));
      g.fillEllipse(b.getRight() - 12.0f, y - 5.0f, 10.0f, 10.0f);
      g.setColour(depthNow[i] > 0.0f ? juce::Colours::white : c.withAlpha(0.5f));
      g.fillEllipse(b.getRight() - 10.0f, y - 3.0f, 6.0f, 6.0f);
    }
  }

 private:
  juce::String rateText(int i, float v) const {
    bool synced = true;
    { const juce::ScopedLock lk(p.lock()); synced = p.session().project().lfos[i].sync != 0; }
    if (!synced) { const float hz = ld::lfoHz(v); return juce::String(hz, hz < 1.0f ? 2 : 1) + " Hz"; }
    static const char* const names[8] = {"8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16"};
    return names[juce::jlimit(0, 7, int(v * 8.0f + 0.5f))];
  }
  template <typename F> void change(int i, F f) {
    const juce::ScopedLock lk(p.lock());
    ld::ModLfo l = p.session().project().lfos[i];
    f(l);
    p.session().setLfo(i, l);
  }

  LoopMasterProcessor& p;
  juce::TextButton target[ld::kNumLfos], shape[ld::kNumLfos], sync[ld::kNumLfos];
  juce::Slider rate[ld::kNumLfos], depth[ld::kNumLfos];
  juce::Rectangle<int> unit[ld::kNumLfos], scope[ld::kNumLfos];
  juce::Slider* drag = nullptr;
  float now[ld::kNumLfos] = {}, depthNow[ld::kNumLfos] = {};
  int shapeNow[ld::kNumLfos] = {};
  int serial = 0;
  bool setting = false;
};
