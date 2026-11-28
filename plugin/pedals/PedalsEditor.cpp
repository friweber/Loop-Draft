// The window of Loop Pedals: the board from left to right, four pedals at a time, as large as on the floor.
#include "PedalsProcessor.h"

namespace {

#include "Look.inc"
#include "PedalSpecs.inc"

// What the line at the bottom says about the thing under the mouse.
std::map<const juce::Component*, juce::String>& tips() { static std::map<const juce::Component*, juce::String> m; return m; }
void tip(const juce::Component& c, const juce::String& text) { tips()[&c] = text; }

const char* const kNoteNames[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const kShapes[ld::kNumLfoShapes] = {"SINE", "TRIANGLE", "RAMP", "SQUARE", "RANDOM"};

// One knob of a pedal. It is a parameter of the plugin, so a DAW can record how it is turned.
class Dial : public juce::Component {
 public:
  Dial(LoopPedalsProcessor& proc, int knobIndex) : p(proc), k(knobIndex) {
    sl.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    sl.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    sl.setRange(0.0, 1.0, 0.0);
    sl.setDoubleClickReturnValue(true, 0.5);
    sl.onDragStart = [this] { drag = true; p.knobParam(slot, k)->beginChangeGesture(); p.lastSlot = slot; p.lastKnob = k; ++p.touchSerial; };
    sl.onDragEnd = [this] { drag = false; p.knobParam(slot, k)->endChangeGesture(); };
    sl.onValueChange = [this] { if (!setting) p.setKnob(slot, k, float(sl.getValue())); };
    addAndMakeVisible(sl);
    tip(sl, "Drag up or down to turn. A double-click puts it in the middle. Your DAW can record the movement; an LFO can move it too.");
  }
  void assign(int slotIndex, const juce::String& text, juce::Colour cap, juce::Colour inkColour, int era) {
    slot = slotIndex; ink = inkColour;
    if (label != text) { label = text; repaint(); }
    sl.setColour(juce::Slider::rotarySliderFillColourId, cap);
    sl.setColour(juce::Slider::rotarySliderOutlineColourId, inkColour);
    if (int(sl.getProperties().getWithDefault("era", 0)) != era) { sl.getProperties().set("era", era); sl.repaint(); }
    if (!drag) { setting = true; sl.setValue(double(p.knob(slot, k)), juce::dontSendNotification); setting = false; }
  }
  void resized() override { sl.setBounds(getLocalBounds().withTrimmedBottom(15)); }
  void paint(juce::Graphics& g) override {
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText(label.toUpperCase(), getLocalBounds().removeFromBottom(15), juce::Justification::centred, false);
  }

 private:
  LoopPedalsProcessor& p;
  int k, slot = 0;
  juce::Slider sl;
  juce::String label;
  juce::Colour ink;
  bool drag = false, setting = false;
};

// One pedal on the board.
class PedalView : public juce::Component {
 public:
  std::function<void(int from, int to)> onMove;

  explicit PedalView(LoopPedalsProcessor& proc) : p(proc) {
    for (int i = 0; i < 4; ++i) { dial[i] = std::make_unique<Dial>(p, i); addChildComponent(*dial[i]); }
    for (int i = 0; i < 4; ++i) {
      voice[i].onClick = [this, i] { p.setVoice(slot, i); };
      addChildComponent(voice[i]);
      tip(voice[i], "The character of the pedal: the same kind of effect, built another way. The display and the line under the buttons say what changes.");
    }
    mix.setSliderStyle(juce::Slider::LinearBar);
    mix.setRange(0.0, 1.0, 0.0);
    mix.setTextBoxIsEditable(false);
    mix.textFromValueFunction = [](double v) { return "Mix  " + juce::String(juce::roundToInt(v * 100.0)) + "%"; };
    mix.onDragStart = [this] { drag = true; p.knobParam(slot, 4)->beginChangeGesture(); p.lastSlot = slot; p.lastKnob = 4; ++p.touchSerial; };
    mix.onDragEnd = [this] { drag = false; p.knobParam(slot, 4)->endChangeGesture(); };
    mix.onValueChange = [this] { if (!setting) p.setKnob(slot, 4, float(mix.getValue())); };
    addChildComponent(mix);
    tip(mix, "How much of the pedal you hear next to the sound that goes in. 100%: only the pedal.");
    keep.onClick = [this] { p.setFlags(slot, flags ^ 1); };
    sync.onClick = [this] { p.setFlags(slot, flags ^ 2); };
    addChildComponent(keep); addChildComponent(sync);
    tip(keep, "KEEP SUB: everything below about 120 Hz goes round the pedal untouched. For bass: the effect on top, the low end stays solid.");
    tip(sync, "SYNC: the pedal's speed or delay time follows the tempo (the host's, or the TEMPO bar).");
    typeBox.addItem("Empty slot", 1);
    for (const auto& fam : kPedalFamilies) {
      typeBox.addSectionHeading(fam.name);
      for (int type : fam.types) if (const PedalSpec* sp = pedalSpec(type)) typeBox.addItem(sp->name, sp->type + 1);
    }
    typeBox.onChange = [this] { const int t = typeBox.getSelectedId() - 1; if (t >= 0 && t != type) p.setType(slot, t); };
    addAndMakeVisible(typeBox);
    tip(typeBox, "Which pedal sits in this slot. Choosing another one puts its knobs in the middle. \"Empty slot\" takes the pedal off the board.");
    onBtn.onClick = [this] { p.setOn(slot, !p.isOn(slot)); };
    left.onClick = [this] { if (onMove) onMove(slot, slot - 1); };
    right.onClick = [this] { if (onMove) onMove(slot, slot + 1); };
    addChildComponent(onBtn); addAndMakeVisible(left); addAndMakeVisible(right);
    tip(onBtn, "The foot switch: on or off. Off, the sound passes through untouched.");
    tip(left, "Moves this pedal one place earlier in the chain. The order matters: a drive before a delay distorts the note, after it the echoes.");
    tip(right, "Moves this pedal one place later in the chain.");
  }

  void setSlot(int s) { slot = s; }

  // 30 times a second.
  void refresh(const BoardLayout& lay) {
    type = lay.type[slot]; flags = lay.flags[slot];
    const int v = lay.voice[slot];
    const PedalSpec* sp = pedalSpec(type);
    const bool on = p.isOn(slot);
    typeBox.setSelectedId(type + 1, juce::dontSendNotification);
    left.setEnabled(slot > 0); right.setEnabled(slot < kSlots - 1);
    const int era = sp ? kFxEra[juce::jlimit(0, ld::kNumFxTypes - 1, type)] : 0;
    const juce::Colour ink = sp && !sp->lightInk ? juce::Colour(0xff1a1408) : juce::Colour(0xfff7f7f7);
    for (int i = 0; i < 4; ++i) {
      const juce::String name = sp ? juce::String(knobName(*sp, v, i)) : juce::String();
      dial[i]->setVisible(name.isNotEmpty());
      if (name.isNotEmpty()) dial[i]->assign(slot, name, juce::Colour(sp->cap), ink, era);
      const bool has = sp && i < int(ld::kFxVoices[type]) && ld::kFxVoices[type] > 1 && sp->voices[i] != nullptr;
      voice[i].setVisible(has);
      if (has) { voice[i].setButtonText(sp->voices[i]); voice[i].setToggleState(i == v, juce::dontSendNotification); }
    }
    mix.setVisible(sp != nullptr); keep.setVisible(sp != nullptr); onBtn.setVisible(sp != nullptr);
    sync.setVisible(sp && ld::fxCanSync(ld::FxType(type), v));
    if (sp) {
      if (!drag) { setting = true; mix.setValue(double(p.knob(slot, 4)), juce::dontSendNotification); setting = false; }
      keep.setToggleState((flags & 1) != 0, juce::dontSendNotification);
      sync.setToggleState((flags & 2) != 0, juce::dontSendNotification);
      onBtn.setButtonText(on ? "ON" : "OFF");
      onBtn.setToggleState(on, juce::dontSendNotification);
    }
    if (type != laidOutFor || v != laidOutVoice) { laidOutFor = type; laidOutVoice = v; resized(); }
    voiceNow = v;
    repaint();
  }

  void resized() override {
    auto r = getLocalBounds().reduced(16, 10);
    r.removeFromTop(42);                                        // name, code and year: painted
    scope = r.removeFromTop(46);
    r.removeFromTop(8);
    auto knobs = r.removeFromTop(82);
    int shown = 0;
    for (auto& d : dial) shown += d->isVisible() ? 1 : 0;
    const int kw = juce::jmin(74, knobs.getWidth() / juce::jmax(1, shown));
    knobs = knobs.withSizeKeepingCentre(kw * juce::jmax(1, shown), knobs.getHeight());
    for (auto& d : dial) if (d->isVisible()) d->setBounds(knobs.removeFromLeft(kw));
    r.removeFromTop(6);
    auto vr = r.removeFromTop(22);
    int nv = 0;
    for (auto& b : voice) nv += b.isVisible() ? 1 : 0;
    const int vw = (vr.getWidth() - (juce::jmax(1, nv) - 1) * 2) / juce::jmax(1, nv);
    for (auto& b : voice) if (b.isVisible()) { b.setBounds(vr.removeFromLeft(vw)); vr.removeFromLeft(2); }
    r.removeFromTop(4);
    about = r.removeFromTop(30);
    auto bottom = r.removeFromBottom(34);
    left.setBounds(bottom.removeFromLeft(34)); right.setBounds(bottom.removeFromRight(34));
    onBtn.setBounds(bottom.reduced(6, 0));
    r.removeFromBottom(8);
    typeBox.setBounds(r.removeFromBottom(24));
    r.removeFromTop(4);
    auto mr = r.removeFromTop(22);
    if (sync.isVisible()) { sync.setBounds(mr.removeFromRight(48)); mr.removeFromRight(3); }
    keep.setBounds(mr.removeFromRight(72)); mr.removeFromRight(3);
    mix.setBounds(mr);
  }

  void paint(juce::Graphics& g) override {
    const auto box = getLocalBounds().toFloat().reduced(4.0f);
    const PedalSpec* sp = pedalSpec(type);
    if (!sp) {                                                  // a free place on the board
      g.setColour(col::recess);
      g.fillRoundedRectangle(box, 10.0f);
      g.setColour(col::line);
      g.drawRoundedRectangle(box.reduced(1.0f), 10.0f, 1.5f);
      g.setColour(col::muted);
      g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
      g.drawText("SLOT " + juce::String(slot + 1) + " IS EMPTY", box.withTrimmedBottom(90.0f), juce::Justification::centred, false);
      g.setFont(juce::Font(juce::FontOptions(11.5f)));
      g.drawText("Choose a pedal in the box below", box.withTrimmedBottom(40.0f), juce::Justification::centred, false);
      return;
    }
    const bool on = p.isOn(slot);
    const bool digital = kFxEra[juce::jlimit(0, ld::kNumFxTypes - 1, type)] == 1;
    const juce::Colour top(sp->top), bottom(sp->bottom);
    const juce::Colour ink = sp->lightInk ? juce::Colour(0xfff7f7f7) : juce::Colour(0xff1a1408);
    const float rnd = digital ? 4.0f : 12.0f;
    g.setGradientFill(juce::ColourGradient(top, box.getX(), box.getY(), bottom, box.getX(), box.getBottom(), false));
    g.fillRoundedRectangle(box, rnd);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.drawRoundedRectangle(box.reduced(1.0f), rnd, 2.0f);
    if (digital) {                                              // a flat box with fine grooves
      g.setColour(juce::Colours::black.withAlpha(0.16f));
      for (float y = box.getY() + 6.0f; y < box.getY() + 46.0f; y += 3.0f) g.fillRect(box.getX() + 6.0f, y, box.getWidth() - 12.0f, 1.0f);
    } else {                                                    // painted metal: a lit edge, worn paint, four screws
      g.setColour(juce::Colours::white.withAlpha(0.18f));
      g.drawRoundedRectangle(box.reduced(4.0f), 9.0f, 1.0f);
      juce::Random wear(type * 7919);
      g.setColour(juce::Colour(0xffc9ccd1).withAlpha(0.35f));
      for (int n = 0; n < 16; ++n) {
        const bool side = wear.nextBool();
        const float u = wear.nextFloat(), len = 4.0f + wear.nextFloat() * 14.0f;
        if (side) g.fillRect(wear.nextBool() ? box.getX() + 1.5f : box.getRight() - 3.0f, box.getY() + 14.0f + u * (box.getHeight() - 40.0f), 1.5f, len);
        else g.fillRect(box.getX() + 14.0f + u * (box.getWidth() - 40.0f), wear.nextBool() ? box.getY() + 1.5f : box.getBottom() - 3.0f, len, 1.5f);
      }
      for (int corner = 0; corner < 4; ++corner) {
        const float sx = corner % 2 ? box.getRight() - 9.0f : box.getX() + 9.0f, sy = corner / 2 ? box.getBottom() - 9.0f : box.getY() + 9.0f;
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(sx - 3.5f, sy - 3.5f, 7.0f, 7.0f);
        g.setColour(juce::Colour(0xffb9bcc2));
        g.fillEllipse(sx - 2.6f, sy - 2.6f, 5.2f, 5.2f);
      }
    }
    g.setColour(ink);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText(juce::String(sp->name).toUpperCase(), 20, 12, getWidth() - 70, 22, juce::Justification::centredLeft, true);
    g.setColour(ink.withAlpha(0.65f));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("LM-" + juce::String(type).paddedLeft('0', 2) + "  " + sp->code + "    " + (digital ? "DIGITAL " : "ANALOG ") + kFxYear[juce::jlimit(0, ld::kNumFxTypes - 1, type)],
               20, 34, getWidth() - 40, 13, juce::Justification::centredLeft, false);
    const float lx = float(getWidth()) - 36.0f, ly = 16.0f;       // the light
    if (on) { g.setColour(juce::Colour(0xffff3b30).withAlpha(0.35f)); g.fillEllipse(lx - 4.0f, ly - 4.0f, 22.0f, 22.0f); }
    g.setColour(on ? juce::Colour(0xffff5a4d) : juce::Colour(0xff4a1d19));
    g.fillEllipse(lx, ly, 14.0f, 14.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(lx, ly, 14.0f, 14.0f, 1.5f);
    // The display: the wave as the pedal leaves it, following the knobs and the character.
    const auto tr = scope.toFloat();
    const float secs = float(std::fmod(juce::Time::getMillisecondCounterHiRes() * 0.001, 3600.0));
    const float amt = on ? p.knob(slot, 0) : 0.0f, mixNow = on ? p.knob(slot, 4) : 0.0f, rate = p.knob(slot, 1), p3 = p.knob(slot, 2);
    const int n = juce::jmax(8, int(tr.getWidth()) - 12);
    const auto at = [&](int px) { return juce::jlimit(-1.0f, 1.0f, pedalScope(type, voiceNow, amt, rate, p3, mixNow, float(px) / float(n), secs)); };
    if (digital) {
      g.setColour(top.withMultipliedSaturation(0.35f).withBrightness(on ? 0.78f : 0.5f));
      g.fillRect(tr);
      g.setColour(juce::Colours::black.withAlpha(0.55f));
      g.drawRect(tr, 1.5f);
      g.setColour(juce::Colour(0xff10160f).withAlpha(0.88f));
      const float half = tr.getHeight() * 0.5f - 4.0f;
      for (int px = 0; px <= n; px += 3) {
        const float q = std::round(at(px) * half / 3.0f) * 3.0f;
        g.fillRect(tr.getX() + 6.0f + float(px), tr.getCentreY() - juce::jmax(q, 0.0f) - 1.0f, 2.0f, std::fabs(q) + 2.0f);
      }
    } else {
      g.setColour(juce::Colour(0xff0d0f10));
      g.fillRoundedRectangle(tr, 9.0f);
      g.setColour(juce::Colour(0xff4a4640));
      g.drawRoundedRectangle(tr.reduced(0.5f), 9.0f, 2.0f);
      juce::Path wave;
      for (int px = 0; px <= n; ++px) {
        const float wx = tr.getX() + 6.0f + float(px), wy = tr.getCentreY() - at(px) * (tr.getHeight() * 0.5f - 5.0f);
        if (px == 0) wave.startNewSubPath(wx, wy); else wave.lineTo(wx, wy);
      }
      const juce::Colour beam = on ? top.brighter(0.5f) : top.withMultipliedSaturation(0.3f).withAlpha(0.5f);
      g.setColour(beam.withAlpha(0.25f));
      g.strokePath(wave, juce::PathStrokeType(5.0f));
      g.setColour(beam);
      g.strokePath(wave, juce::PathStrokeType(1.8f));
    }
    g.setColour(ink.withAlpha(0.85f));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    const char* line = sp->about[juce::jlimit(0, 3, voiceNow)];
    g.drawFittedText(line ? line : "", about, juce::Justification::centred, 2, 0.9f);
  }

 private:
  LoopPedalsProcessor& p;
  int slot = 0, type = 0, flags = 0, voiceNow = 0, laidOutFor = -1, laidOutVoice = -1;
  std::unique_ptr<Dial> dial[4];
  juce::TextButton voice[4], keep{"KEEP SUB"}, sync{"SYNC"}, onBtn{"ON"}, left{"<"}, right{">"};
  juce::Slider mix;
  juce::ComboBox typeBox;
  juce::Rectangle<int> scope, about;
  bool drag = false, setting = false;
};

// The three LFOs, side by side. Click one, then touch the knob it should move.
class LfoBar : public juce::Component {
 public:
  std::function<void(const juce::String&)> say;
  int armed = -1;

  explicit LfoBar(LoopPedalsProcessor& proc) : p(proc) {
    for (int i = 0; i < kLfos; ++i) {
      target[i].onClick = [this, i] {
        if (armed == i) { armed = -1; change(i, [](BoardLfo& l) { l.slot = -1; }); if (say) say("LFO " + juce::String(i + 1) + " is disconnected."); return; }
        armed = i; serial = p.touchSerial;
        if (say) say("LFO " + juce::String(i + 1) + " waits: now touch the knob (or the Mix bar) it should move. Click the button again to disconnect it instead.");
      };
      shape[i].onClick = [this, i] { change(i, [](BoardLfo& l) { l.shape = (l.shape + 1) % ld::kNumLfoShapes; }); };
      sync[i].setButtonText("SYNC");
      sync[i].onClick = [this, i] { change(i, [](BoardLfo& l) { l.sync = !l.sync; }); };
      for (auto* b : {&target[i], &shape[i], &sync[i]}) addAndMakeVisible(*b);
      tip(target[i], "Click, then touch the knob this LFO should keep moving. While it waits, a second click disconnects it.");
      tip(shape[i], "The shape of the movement: sine, triangle, ramp, square or random steps. A click steps to the next.");
      tip(sync[i], "SYNC: the speed is a length in bars and beats and follows the tempo. Off: free, in Hz.");
      for (juce::Slider* sl : {&rate[i], &depth[i]}) {
        const bool isRate = sl == &rate[i];
        sl->setSliderStyle(juce::Slider::LinearBar);
        sl->setRange(0.0, 1.0, 0.0);
        sl->setTextBoxIsEditable(false);
        sl->textFromValueFunction = [this, i, isRate](double v) { return isRate ? "Speed  " + rateText(i, float(v)) : "Depth  " + juce::String(juce::roundToInt(v * 100.0)) + "%"; };
        sl->onDragStart = [this, sl] { drag = sl; };
        sl->onDragEnd = [this] { drag = nullptr; };
        sl->onValueChange = [this, i, sl, isRate] { if (!setting) change(i, [sl, isRate](BoardLfo& l) { (isRate ? l.rate : l.depth) = float(sl->getValue()); }); };
        addAndMakeVisible(*sl);
        tip(*sl, isRate ? "How fast the LFO swings: from once in eight bars to once a sixteenth with SYNC, 0.05 to 20 Hz without."
                        : "How far the LFO moves the knob around where it stands. At 100% it swings over the knob's whole range.");
      }
    }
  }

  void tick(const BoardLayout& lay) {
    if (armed >= 0 && p.touchSerial != serial) {
      serial = p.touchSerial;
      const int i = armed, slot = p.lastSlot, knob = p.lastKnob;
      armed = -1;
      change(i, [slot, knob](BoardLfo& l) { l.slot = slot; l.knob = knob; if (l.depth < 0.05f) l.depth = 0.5f; });
      if (say) say("LFO " + juce::String(i + 1) + " now moves " + name(lay, slot, knob) + ". Speed and Depth are next to it.");
      return;
    }
    setting = true;
    for (int i = 0; i < kLfos; ++i) {
      const BoardLfo& l = lay.lfos[i];
      const bool on = l.slot >= 0 && lay.type[juce::jlimit(0, kSlots - 1, l.slot)] != 0;
      target[i].setButtonText(armed == i ? juce::String("TOUCH A KNOB...") : (on ? name(lay, l.slot, l.knob) : juce::String("CHOOSE A KNOB")));
      target[i].setToggleState(armed == i || on, juce::dontSendNotification);
      shape[i].setButtonText(kShapes[juce::jlimit(0, ld::kNumLfoShapes - 1, l.shape)]);
      sync[i].setToggleState(l.sync, juce::dontSendNotification);
      synced[i] = l.sync;
      if (drag != &rate[i]) { rate[i].setValue(double(l.rate), juce::dontSendNotification); rate[i].updateText(); }
      if (drag != &depth[i]) depth[i].setValue(double(l.depth), juce::dontSendNotification);
      now[i] = p.lfoNow[i].load(); shapeNow[i] = l.shape; depthNow[i] = on ? l.depth : 0.0f;
    }
    setting = false;
    repaint();
  }

  void resized() override {
    auto r = getLocalBounds().reduced(6, 4);
    const int w = (r.getWidth() - 2 * 10) / 3;
    for (int i = 0; i < kLfos; ++i) {
      auto u = r.removeFromLeft(w); r.removeFromLeft(10);
      unit[i] = u;
      u.removeFromLeft(52);
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
    for (int i = 0; i < kLfos; ++i) {
      const juce::Colour c(tint[i]);
      g.setColour(c);
      g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
      g.drawText("LFO " + juce::String(i + 1), unit[i].withWidth(50), juce::Justification::centredLeft, false);
      const auto b = scope[i].toFloat();
      g.setColour(juce::Colour(0xff0b0d0e));
      g.fillRoundedRectangle(b, 4.0f);
      juce::Path w;
      const float amp = (b.getHeight() * 0.5f - 4.0f) * (0.25f + 0.75f * depthNow[i]);
      for (int px = 0; px <= int(b.getWidth()) - 8; ++px) {
        const float ph = float(px) / (b.getWidth() - 8.0f);
        float v = 0.0f;
        switch (shapeNow[i]) {
          case 1: v = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
          case 2: v = 1.0f - 2.0f * ph; break;
          case 3: v = ph < 0.5f ? 1.0f : -1.0f; break;
          case 4: v = std::sin(float(int(ph * 6.0f)) * 12.9898f); break;
          default: v = std::sin(juce::MathConstants<float>::twoPi * ph); break;
        }
        const float x = b.getX() + 4.0f + float(px), y = b.getCentreY() - v * amp;
        if (px == 0) w.startNewSubPath(x, y); else w.lineTo(x, y);
      }
      g.setColour(c.withAlpha(depthNow[i] > 0.0f ? 0.9f : 0.3f));
      g.strokePath(w, juce::PathStrokeType(1.5f));
      const float y = b.getCentreY() - now[i] * amp;
      g.setColour(depthNow[i] > 0.0f ? juce::Colours::white : c.withAlpha(0.5f));
      g.fillEllipse(b.getRight() - 10.0f, y - 3.0f, 6.0f, 6.0f);
    }
  }

 private:
  static juce::String name(const BoardLayout& lay, int slot, int knob) {
    const int s = juce::jlimit(0, kSlots - 1, slot);
    const PedalSpec* sp = pedalSpec(lay.type[s]);
    if (!sp) return "Slot " + juce::String(s + 1);
    const juce::String k = knob >= 4 ? juce::String("Mix") : juce::String(knobName(*sp, lay.voice[s], juce::jlimit(0, 3, knob)));
    return juce::String(s + 1) + " " + sp->name + ": " + (k.isEmpty() ? juce::String("knob ") + juce::String(knob + 1) : k);
  }
  juce::String rateText(int i, float v) const {
    if (!synced[i]) { const float hz = ld::lfoHz(v); return juce::String(hz, hz < 1.0f ? 2 : 1) + " Hz"; }
    static const char* const names[8] = {"8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8", "1/16"};
    return names[juce::jlimit(0, 7, int(v * 8.0f + 0.5f))];
  }
  template <typename F> void change(int i, F f) { BoardLfo l = p.layout().lfos[i]; f(l); p.setLfo(i, l); }

  LoopPedalsProcessor& p;
  juce::TextButton target[kLfos], shape[kLfos], sync[kLfos];
  juce::Slider rate[kLfos], depth[kLfos];
  juce::Rectangle<int> unit[kLfos], scope[kLfos];
  juce::Slider* drag = nullptr;
  float now[kLfos] = {}, depthNow[kLfos] = {};
  int shapeNow[kLfos] = {};
  bool synced[kLfos] = {true, true, true};
  int serial = 0;
  bool setting = false;
};

class PedalsEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit PedalsEditor(LoopPedalsProcessor& proc) : AudioProcessorEditor(proc), p(proc), lfos(proc) {
    setLookAndFeel(&look);
    for (int i = 0; i < 4; ++i) {
      view[i] = std::make_unique<PedalView>(p);
      view[i]->onMove = [this](int from, int to) {
        if (to < 0 || to >= kSlots) return;
        p.swapSlots(from, to);
        show(to);
        message = "The pedal is now in place " + juce::String(to + 1) + " of the chain.";
      };
      addAndMakeVisible(*view[i]);
    }
    for (int i = 0; i < kSlots; ++i) {
      chip[i].onClick = [this, i] { show(i); };
      addAndMakeVisible(chip[i]);
      tip(chip[i], "The chain, from the instrument (left) to the output (right). A click brings that slot into view.");
    }
    fillBoards();
    boardBox.setTextWhenNothingSelected("Boards");
    boardBox.onChange = [this] {
      const int id = boardBox.getSelectedId();
      if (id >= 1000) { const juce::String name = userNames[id - 1000]; message = p.loadUser(name) ? name + " is on the floor." : "That board could not be read."; }
      else if (id >= 1) { p.loadFactory(id - 1); message = juce::String(kFactoryBoards[id - 1].name) + ": " + kFactoryBoards[id - 1].about; }
      first = 0; shownName = p.boardName;
    };
    addAndMakeVisible(boardBox);
    tip(boardBox, "Boards: ready-made ones for electric guitar, bass guitar and synth, and under them the ones you saved.");
    saveBtn.onClick = [this] { askName(); };
    deleteBtn.onClick = [this] {
      if (!userNames.contains(p.boardName)) { message = "Only boards you saved yourself can be deleted. \"" + p.boardName + "\" is not one of them."; return; }
      const juce::String name = p.boardName;
      message = p.deleteUser(name) ? "\"" + name + "\" is deleted from your boards. The pedals stay on the floor until you load another board." : "That board could not be deleted.";
      fillBoards();
    };
    newBtn.onClick = [this] { p.clearBoard(); first = 0; message = "An empty board. Choose a pedal in the box of each slot; SAVE AS keeps the board."; };
    tunerBtn.onClick = [this] { p.tunerOn = !p.tunerOn.load(); resized(); message = p.tunerOn.load() ? "Tuner on: the output is silent while you tune." : "Tuner off."; };
    for (auto* b : {&saveBtn, &deleteBtn, &newBtn, &tunerBtn}) addAndMakeVisible(*b);
    tip(saveBtn, "Saves the board as it stands (pedals, knobs, LFOs) under a name of your choice. It appears under \"My boards\" in the plugin and in the app.");
    tip(deleteBtn, "Deletes the board that is loaded from \"My boards\". The ready-made boards cannot be deleted.");
    tip(newBtn, "Takes every pedal off the board.");
    tip(tunerBtn, "A tuner for guitar and bass. While it is on, the output is silent.");
    inputBox.addItemList(p.inputMode->choices, 1);
    inputBox.onChange = [this] { p.inputMode->setValueNotifyingHost(p.inputMode->convertTo0to1(float(inputBox.getSelectedItemIndex()))); };
    addAndMakeVisible(inputBox);
    tip(inputBox, "Where the instrument is plugged in. A guitar in the first socket of the interface: Input 1. Stereo is for synths and whole tracks.");
    bar(inBar, p.inGain, "In  ", " dB", "How loud the instrument goes into the first pedal. Play hard: the IN light should turn yellow now and then, never red.");
    bar(outBar, p.outGain, "Out  ", " dB", "How loud the board goes out. Red on the OUT light means it hits the ceiling: turn this down.");
    bar(tempoBar, p.tempo, "Tempo  ", "", "The tempo for SYNC on pedals and LFOs when no host gives one (the app, or a host that is stopped still gives its own).");
    lfos.say = [this](const juce::String& t) { message = t; };
    addAndMakeVisible(lfos);
    if (const auto b = juce::SystemStats::getEnvironmentVariable("LOOPPEDALS_BOARD", ""); b.isNotEmpty()) {     // for screenshots
      p.loadFactory(juce::jlimit(0, kNumFactoryBoards - 1, b.getIntValue()));
      first = juce::jlimit(0, kSlots - 4, juce::SystemStats::getEnvironmentVariable("LOOPPEDALS_FIRST", "0").getIntValue());
      if (juce::SystemStats::getEnvironmentVariable("LOOPPEDALS_TUNER", "").isNotEmpty()) p.tunerOn = true;
      if (juce::SystemStats::getEnvironmentVariable("LOOPPEDALS_LFO", "").isNotEmpty()) { BoardLfo l; l.slot = 3; l.knob = 1; l.rate = 0.4f; l.depth = 0.6f; p.setLfo(0, l); }
    }
    setResizable(true, true);
    setResizeLimits(980, 620, 1800, 1000);
    setSize(1180, 700);
    startTimerHz(30);
  }
  ~PedalsEditor() override { stopTimer(); setLookAndFeel(nullptr); tips().clear(); }

  void resized() override {
    auto r = getLocalBounds().reduced(12);
    auto row = r.removeFromTop(32);
    row.removeFromLeft(168);                                    // the name, painted
    boardBox.setBounds(row.removeFromLeft(250)); row.removeFromLeft(6);
    saveBtn.setBounds(row.removeFromLeft(84)); row.removeFromLeft(4);
    deleteBtn.setBounds(row.removeFromLeft(72)); row.removeFromLeft(4);
    newBtn.setBounds(row.removeFromLeft(60));
    tunerBtn.setBounds(row.removeFromRight(90));
    r.removeFromTop(8);
    row = r.removeFromTop(26);
    inputBox.setBounds(row.removeFromLeft(160)); row.removeFromLeft(6);
    inBar.setBounds(row.removeFromLeft(150)); row.removeFromLeft(6);
    inMeter = row.removeFromLeft(110);
    outMeter = row.removeFromRight(110); row.removeFromRight(6);
    outBar.setBounds(row.removeFromRight(150)); row.removeFromRight(14);
    tempoBar.setBounds(row.removeFromRight(150));
    r.removeFromTop(8);
    auto chain = r.removeFromTop(28);
    chain.removeFromLeft(74); chain.removeFromRight(60);         // "GUITAR IN" and "OUT", painted
    chainArea = chain;
    const int cw = (chain.getWidth() - (kSlots - 1) * 6) / kSlots;
    for (int i = 0; i < kSlots; ++i) { chip[i].setBounds(chain.removeFromLeft(cw)); chain.removeFromLeft(6); }
    r.removeFromTop(8);
    r.removeFromBottom(2);
    helpArea = r.removeFromBottom(20);
    r.removeFromBottom(4);
    lfos.setBounds(r.removeFromBottom(60));
    r.removeFromBottom(8);
    tunerArea = p.tunerOn.load() ? r.removeFromTop(74) : juce::Rectangle<int>();
    if (p.tunerOn.load()) r.removeFromTop(8);
    const int vw = (r.getWidth() - 3 * 10) / 4;
    for (int i = 0; i < 4; ++i) { view[i]->setBounds(r.removeFromLeft(vw)); r.removeFromLeft(10); }
  }

  void paint(juce::Graphics& g) override {
    g.fillAll(col::chassis);
    g.setFont(juce::Font(juce::FontOptions(21.0f, juce::Font::bold)));
    g.setColour(col::silk);
    g.drawText("LOOP", 12, 12, 60, 32, juce::Justification::centredLeft, false);
    g.setColour(col::orange);
    g.drawText("PEDALS", 68, 12, 100, 32, juce::Justification::centredLeft, false);
    meter(g, inMeter, "IN", inShown);
    meter(g, outMeter, "OUT", outShown);
    g.setColour(col::muted);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText("INSTRUMENT", chainArea.getX() - 74, chainArea.getY(), 70, chainArea.getHeight(), juce::Justification::centredLeft, false);
    g.drawText("OUTPUT", chainArea.getRight() + 6, chainArea.getY(), 56, chainArea.getHeight(), juce::Justification::centredRight, false);
    g.setColour(col::line);
    g.fillRect(chainArea.getX(), chainArea.getCentreY() - 1, chainArea.getWidth(), 2);       // the cable
    if (!tunerArea.isEmpty()) paintTuner(g);
    g.setColour(col::silk.withAlpha(0.9f));
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawText(hover.isNotEmpty() ? hover : message, helpArea, juce::Justification::centredLeft, true);
  }

 private:
  void bar(juce::Slider& sl, juce::AudioParameterFloat* param, const char* before, const char* after, const char* about) {
    sl.setSliderStyle(juce::Slider::LinearBar);
    sl.setRange(double(param->range.start), double(param->range.end), 0.0);
    sl.setTextBoxIsEditable(false);
    sl.textFromValueFunction = [before, after](double v) { return juce::String(before) + (after[0] && v > 0.05 ? "+" : "") + juce::String(juce::roundToInt(v)) + after; };
    sl.setDoubleClickReturnValue(true, double(param->convertFrom0to1(static_cast<juce::AudioProcessorParameter*>(param)->getDefaultValue())));
    sl.onDragStart = [this, &sl, param] { held = &sl; param->beginChangeGesture(); };
    sl.onDragEnd = [this, param] { held = nullptr; param->endChangeGesture(); };
    sl.onValueChange = [this, &sl, param] { if (!setting) param->setValueNotifyingHost(param->convertTo0to1(float(sl.getValue()))); };
    addAndMakeVisible(sl);
    tip(sl, about);
  }
  void meter(juce::Graphics& g, juce::Rectangle<int> area, const char* name, float level) {
    g.setColour(col::muted);
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    g.drawText(name, area.removeFromLeft(28), juce::Justification::centredLeft, false);
    const auto b = area.reduced(0, 7).toFloat();
    g.setColour(col::recess);
    g.fillRoundedRectangle(b, 3.0f);
    const float db = juce::Decibels::gainToDecibels(level, -60.0f), f = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour(db > -1.0f ? col::red : (db > -12.0f ? col::yellow : col::green));
    g.fillRoundedRectangle(b.withWidth(b.getWidth() * f), 3.0f);
  }
  void paintTuner(juce::Graphics& g) {
    const auto b = tunerArea.toFloat();
    g.setColour(col::recess);
    g.fillRoundedRectangle(b, 8.0f);
    g.setColour(col::line);
    g.drawRoundedRectangle(b.reduced(0.5f), 8.0f, 1.0f);
    const float mid = b.getCentreX(), span = b.getWidth() * 0.32f;
    for (int c = -50; c <= 50; c += 10) {                        // the scale: 50 cents either way
      const float x = mid + float(c) / 50.0f * span;
      g.setColour(c == 0 ? col::green : col::line);
      g.fillRect(x - 1.0f, b.getY() + (c == 0 ? 8.0f : 16.0f), 2.0f, c == 0 ? b.getHeight() - 16.0f : b.getHeight() - 32.0f);
    }
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.setColour(col::muted);
    if (tunedHz <= 0.0f) { g.drawText("Play one open string. The output is silent while the tuner is on.", tunerArea.reduced(16, 0), juce::Justification::centredLeft, false); return; }
    const float midi = 69.0f + 12.0f * std::log2(tunedHz / 440.0f);
    const int note = juce::roundToInt(midi);
    const float cents = (midi - float(note)) * 100.0f;
    const bool good = std::fabs(cents) < 4.0f;
    g.setColour(good ? col::green : col::silk);
    g.setFont(juce::Font(juce::FontOptions(40.0f, juce::Font::bold)));
    g.drawText(juce::String(kNoteNames[((note % 12) + 12) % 12]) + juce::String(note / 12 - 1), tunerArea.reduced(18, 0).removeFromLeft(140), juce::Justification::centredLeft, false);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(col::muted);
    g.drawText(juce::String(tunedHz, 1) + " Hz    " + (good ? juce::String("in tune") : (cents < 0.0f ? juce::String(juce::roundToInt(-cents)) + " cents flat: tune up" : juce::String(juce::roundToInt(cents)) + " cents sharp: tune down")),
               tunerArea.reduced(18, 0).removeFromRight(260), juce::Justification::centredRight, false);
    const float x = mid + juce::jlimit(-1.0f, 1.0f, cents / 50.0f) * span;
    g.setColour(good ? col::green : col::orange);
    g.fillRoundedRectangle(x - 4.0f, b.getY() + 6.0f, 8.0f, b.getHeight() - 12.0f, 3.0f);
  }
  void show(int slot) {
    if (slot < first) first = slot; else if (slot > first + 3) first = slot - 3;
    first = juce::jlimit(0, kSlots - 4, first);
  }
  void fillBoards() {
    boardBox.clear(juce::dontSendNotification);
    const char* group = "";
    for (int i = 0; i < kNumFactoryBoards; ++i) {
      if (juce::String(group) != kFactoryBoards[i].group) { group = kFactoryBoards[i].group; boardBox.addSectionHeading(group); }
      boardBox.addItem(kFactoryBoards[i].name, i + 1);
    }
    userNames = p.userBoards();
    if (!userNames.isEmpty()) boardBox.addSectionHeading("My boards");
    for (int i = 0; i < userNames.size(); ++i) boardBox.addItem(userNames[i], 1000 + i);
    shownName.clear();
  }
  void askName() {
    auto* w = new juce::AlertWindow("Save this board", "A name for the board:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", userNames.contains(p.boardName) ? p.boardName : juce::String("My ") + p.boardName);
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PedalsEditor> self(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([self, w](int result) {
      if (result != 1 || self == nullptr) return;
      const juce::String name = w->getTextEditorContents("name").trim();
      if (name.isEmpty()) { self->message = "A board needs a name."; return; }
      self->message = self->p.saveUser(name) ? "Saved as \"" + self->p.boardName + "\". It is under \"My boards\", here and in the app." : "The board could not be saved (no access to the Documents folder?).";
      self->fillBoards();
    }), true);
  }
  void timerCallback() override {
    const BoardLayout lay = p.layout();
    for (int i = 0; i < 4; ++i) { view[i]->setSlot(first + i); view[i]->refresh(lay); }
    for (int i = 0; i < kSlots; ++i) {
      const PedalSpec* sp = pedalSpec(lay.type[i]);
      const bool seen = i >= first && i < first + 4;
      chip[i].setButtonText(juce::String(i + 1) + "  " + (sp ? juce::String(sp->name).toUpperCase() : juce::String("-")));
      chip[i].setColour(juce::TextButton::buttonColourId, sp ? juce::Colour(sp->top).withMultipliedBrightness(seen ? 1.0f : 0.45f) : col::faceplate);
      chip[i].setColour(juce::TextButton::textColourOffId, sp ? (sp->lightInk || !seen ? juce::Colour(0xfff7f7f7) : juce::Colour(0xff1a1408)) : col::muted);
      chip[i].setAlpha(sp && !p.isOn(i) ? 0.55f : 1.0f);
    }
    lfos.tick(lay);
    setting = true;
    if (held != &inBar) inBar.setValue(double(p.inGain->get()), juce::dontSendNotification);
    if (held != &outBar) outBar.setValue(double(p.outGain->get()), juce::dontSendNotification);
    if (held != &tempoBar) tempoBar.setValue(double(p.tempo->get()), juce::dontSendNotification);
    setting = false;
    inputBox.setSelectedItemIndex(p.inputMode->getIndex(), juce::dontSendNotification);
    tunerBtn.setToggleState(p.tunerOn.load(), juce::dontSendNotification);
    if (shownName != p.boardName) { shownName = p.boardName; boardBox.setText(shownName, juce::dontSendNotification); }
    inShown = juce::jmax(p.inPeak.load(), inShown * 0.85f);
    outShown = juce::jmax(p.outPeak.load(), outShown * 0.85f);
    if (p.tunerOn.load() && ++tunerTick % 3 == 0) {
      const float hz = p.tunerHz();
      if (hz > 0.0f) { tunedHz = tunedHz > 0.0f && std::fabs(hz / tunedHz - 1.0f) < 0.03f ? tunedHz + 0.4f * (hz - tunedHz) : hz; quiet = 0; }
      else if (++quiet > 12) tunedHz = 0.0f;
    }
    hover.clear();
    for (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse(); c != nullptr; c = c->getParentComponent()) {
      const auto it = tips().find(c);
      if (it != tips().end()) { hover = it->second; break; }
      if (c == this) break;
    }
    repaint();
  }

  LoopPedalsProcessor& p;
  Look look;
  std::unique_ptr<PedalView> view[4];
  juce::TextButton chip[kSlots], saveBtn{"SAVE AS..."}, deleteBtn{"DELETE"}, newBtn{"NEW"}, tunerBtn{"TUNER"};
  juce::ComboBox boardBox, inputBox;
  juce::Slider inBar, outBar, tempoBar;
  juce::Slider* held = nullptr;
  LfoBar lfos;
  juce::StringArray userNames;
  juce::Rectangle<int> inMeter, outMeter, chainArea, helpArea, tunerArea;
  juce::String message{"Plug in, pick a board at the top, play. Every knob can be moved by an LFO (bottom) or recorded by your DAW."}, hover, shownName;
  int first = 0, tunerTick = 0, quiet = 0;
  float inShown = 0.0f, outShown = 0.0f, tunedHz = 0.0f;
  bool setting = false;
};

}  // namespace

juce::AudioProcessorEditor* LoopPedalsProcessor::createEditor() { return new PedalsEditor(*this); }
