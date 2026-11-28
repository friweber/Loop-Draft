// Part of Editor.cpp and of the Loop Pedals editor (inside an unnamed namespace): the colours, and how buttons, bars and knobs are drawn.
namespace col {
const juce::Colour chassis(0xff17181c), faceplate(0xff202227), recess(0xff131418), line(0xff353942);
const juce::Colour silk(0xffebe5d5), muted(0xff969185), ink(0xff1b1c20);
const juce::Colour orange(0xffff6d1f), cyan(0xff29d3e8), yellow(0xffffca28), green(0xff3be083), red(0xffe53935);
const juce::Colour pads[8] = {juce::Colour(0xffe84738), juce::Colour(0xffff8a24), juce::Colour(0xffffc929), juce::Colour(0xff29d4e8),
                              juce::Colour(0xff5ea8ff), juce::Colour(0xff4ae08a), juce::Colour(0xffbf6bff), juce::Colour(0xffff5c94)};
const juce::Colour synths[4] = {juce::Colour(0xffff9426), juce::Colour(0xff29d4e8), juce::Colour(0xffff4da6), juce::Colour(0xff5ee870)};
}  // namespace col

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
    if (int(sl.getProperties().getWithDefault("era", 0)) == 1) {  // digital: a ring of lights up to the value, a flat dial with a notch
      const juce::Colour lit = cap.getBrightness() < 0.35f ? ink : cap;
      for (int i = 0; i <= 14; ++i) {
        const float f = float(i) / 14.0f, a = a0 + f * (a1 - a0);
        g.setColour(f <= pos + 0.001f ? lit.brighter(0.2f) : ink.withAlpha(0.22f));
        juce::Path seg;
        seg.addRectangle(-1.6f, -rad, 3.2f, rad * 0.16f);
        g.fillPath(seg, juce::AffineTransform::rotation(a).translated(cx, cy));
      }
      const float r = rad * 0.7f;
      g.setColour(juce::Colour(0xff15171a));
      g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
      g.setColour(juce::Colour(0xff3b4048));
      g.drawEllipse(cx - r + 0.5f, cy - r + 0.5f, r * 2.0f - 1.0f, r * 2.0f - 1.0f, 1.0f);
      g.setColour(lit.brighter(0.3f));
      g.drawLine(cx + std::sin(ang) * r * 0.45f, cy - std::cos(ang) * r * 0.45f, cx + std::sin(ang) * r * 0.92f, cy - std::cos(ang) * r * 0.92f, 2.0f);
      return;
    }
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
