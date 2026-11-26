// Part of Editor.cpp (inside its unnamed namespace): the help sentences, the ready-made pedal boards, the guide
// and the Loop Master Live screen.

// ---- help ----
// Every control carries one sentence about itself; the line at the bottom shows it while the mouse is over it.
void help(juce::Component& c, const juce::String& text) { c.getProperties().set("help", text); }

// What a pedal knob does, by its name.
juce::String knobHelp(const juce::String& name) {
  static const char* const table[][2] = {
    {"Drive", "how hard the pedal is driven: more is dirtier"}, {"Gain", "how much distortion"}, {"Fuzz", "how much fuzz"},
    {"Tone", "darker to the left, brighter to the right"}, {"Level", "the loudness coming out of the pedal; the middle is as loud as going in"},
    {"Body", "how much low end stays"}, {"Mids", "the middle of the sound: scooped to the left, pushed to the right"},
    {"Bias", "to the right the fuzz starts to sputter and cut off"}, {"Boost", "how much louder"}, {"Treble", "the highs"}, {"Bass", "the lows"},
    {"Sustain", "how much the pedal squeezes: quiet parts come up, notes ring longer"}, {"Attack", "how much of the start of each note gets through"},
    {"Blend", "mixes the untouched sound back in"}, {"Depth", "how strong the effect is"}, {"Speed", "how fast it moves; with SYNC lit, a note length"},
    {"Mix", "how much of the pedal you hear against the untouched sound"}, {"Feedbk", "how much of the output goes round again: more is longer and wilder"},
    {"Manual", "where the sweep sits"}, {"Shape", "soft to the left, hard-edged to the right"}, {"Pan", "moves the pulses between left and right"},
    {"Balance", "between the highs' horn and the lows' drum"}, {"Cutoff", "where the filter closes"}, {"Reso", "a peak at the cutoff"},
    {"Env", "how much your playing opens the filter"}, {"Range", "how far, or where, the sweep goes"}, {"Sharp", "how narrow the peak is"},
    {"Sens", "how easily it reacts to your playing"}, {"Peak", "how narrow the peak is"}, {"Decay", "how long it rings"},
    {"Octave", "how much of the added octave"}, {"Dry", "how much of the untouched sound"}, {"Second", "a second added octave"},
    {"Pitch", "the pitch"}, {"Wobble", "how much the pitch drifts"}, {"W-Speed", "how fast it wobbles"},
    {"Time", "the time between repeats; with SYNC lit, a note length"},
    {"Repeats", "how many repeats"}, {"Age", "how worn the tape is: duller and less steady"}, {"Pre-dly", "a gap before the room answers"},
    {"Dwell", "how hard the springs are driven"}, {"Thresh", "how loud a sound must be to get through"}, {"Hold", "up: what is in the room now stays"},
    {"Crush", "fewer bits"}, {"Rate", "the note length of one slice, from quarter notes to 1/32"}, {"Width", "how long each slice stays open"},
    {"Pattern", "which of eight rhythms"}, {"Amount", "how much of every two beats is stutter"}, {"Gate", "how long the room stays open after a hit"},
    {"Size", "the size of the room"}, {"Clip", "to the right: a soft ceiling that makes it louder and denser"}, {"Glue", "squeezes the kit together"},
    {"Boom", "how loud the added sub is"}, {"Grain", "how long the little pieces are"}, {"Scatter", "how far apart in time and space"},
    {"Noise", "hiss or crackle"}, {"Filter", "left: every repeat darker; right: thinner"}, {"Shift", "how far every tone is moved"},
    {"Spread", "how different left and right are"}, {"Note", "which note of the project's scale it is tuned to"}, {"Bright", "how bright it rings"},
    {"Shimmer", "how much of the tail climbs"},
  };
  for (const auto& row : table)
    if (name == row[0]) return name.toUpperCase() + ": " + row[1] + ". Double-click puts it back.";
  return name.toUpperCase();
}

// ---- ready-made pedal boards ----
// flags: 1 = KEEP SUB, 2 = SYNC
struct BoardSlot { int type, voice; float a, r, p3, p4, mix; int flags; };
struct Board { const char* group; const char* name; BoardSlot s[4]; };
const Board kBoards[] = {
  {"Drums", "Punchy kit", {{22, 0, 0.7f, 0.45f, 0.5f, 0.5f, 1.0f, 0}, {23, 0, 0.3f, 0.6f, 0.5f, 0.5f, 0.5f, 0}}},
  {"Drums", "Crushed break", {{23, 2, 0.6f, 0.55f, 0.6f, 0.5f, 0.6f, 1}, {2, 0, 0.35f, 0.3f, 0.5f, 0.5f, 0.5f, 0}, {25, 0, 0.35f, 0.4f, 0.5f, 0.5f, 1.0f, 0}}},
  {"Drums", "Eighties room", {{22, 0, 0.65f, 0.5f, 0.5f, 0.5f, 1.0f, 0}, {25, 0, 0.6f, 0.45f, 0.6f, 0.6f, 1.0f, 0}}},
  {"Drums", "Sliced and echoed", {{24, 0, 0.9f, 0.6f, 0.5f, 0.45f, 1.0f, 0}, {29, 1, 0.4f, 0.6f, 0.55f, 0.35f, 1.0f, 1}}},
  {"Drums", "Lo-fi kit", {{28, 0, 0.5f, 0.6f, 0.6f, 0.5f, 1.0f, 0}, {6, 1, 0.6f, 0.4f, 0.5f, 0.6f, 1.0f, 0}}},
  {"Drums", "Big kick", {{26, 1, 0.6f, 0.4f, 0.5f, 0.5f, 1.0f, 0}, {22, 0, 0.7f, 0.4f, 0.5f, 0.7f, 1.0f, 0}}},
  {"Bass", "Acid", {{1, 1, 0.6f, 0.6f, 0.5f, 0.5f, 1.0f, 0}, {5, 1, 0.35f, 0.75f, 0.85f, 0.4f, 1.0f, 0}}},
  {"Bass", "Dub", {{6, 2, 0.6f, 0.5f, 0.5f, 0.7f, 1.0f, 0}, {3, 1, 0.5f, 0.3f, 0.5f, 0.4f, 1.0f, 1}, {29, 0, 0.35f, 0.6f, 0.6f, 0.25f, 1.0f, 1}}},
  {"Bass", "Fuzz with the lows kept", {{8, 1, 0.7f, 0.5f, 0.5f, 0.5f, 0.7f, 1}, {21, 0, 0.3f, 0.4f, 0.5f, 0.3f, 1.0f, 0}}},
  {"Bass", "Wide and moving", {{3, 2, 0.7f, 0.3f, 0.6f, 0.6f, 1.0f, 1}, {4, 2, 0.5f, 0.3f, 0.5f, 0.5f, 1.0f, 3}, {7, 2, 0.4f, 0.5f, 0.5f, 0.5f, 0.35f, 1}}},
  {"Synths", "Ambient cloud", {{27, 2, 0.6f, 0.6f, 0.7f, 0.4f, 1.0f, 0}, {32, 0, 0.5f, 0.7f, 0.5f, 0.6f, 1.0f, 0}}},
  {"Synths", "Tape keys", {{28, 1, 0.5f, 0.4f, 0.5f, 0.3f, 1.0f, 0}, {19, 2, 0.3f, 0.5f, 0.45f, 0.6f, 1.0f, 2}, {20, 1, 0.3f, 0.5f, 0.5f, 0.3f, 1.0f, 0}}},
  {"Synths", "Trance gate", {{24, 0, 1.0f, 0.6f, 0.6f, 0.3f, 1.0f, 0}, {18, 0, 0.4f, 0.6f, 0.5f, 0.5f, 1.0f, 2}, {20, 2, 0.35f, 0.6f, 0.5f, 0.3f, 1.0f, 0}}},
  {"Synths", "Metal bells", {{30, 2, 0.6f, 0.6f, 0.3f, 0.6f, 1.0f, 0}, {31, 2, 0.6f, 0.4f, 0.6f, 0.6f, 1.0f, 0}, {20, 1, 0.3f, 0.6f, 0.6f, 0.3f, 1.0f, 0}}},
  {"Synths", "Swirl", {{10, 2, 0.8f, 0.55f, 0.3f, 0.7f, 1.0f, 2}, {4, 3, 0.7f, 0.3f, 0.5f, 0.5f, 1.0f, 0}, {18, 2, 0.35f, 0.6f, 0.5f, 0.5f, 1.0f, 2}}},
};
constexpr int kNumBoards = int(sizeof(kBoards) / sizeof(kBoards[0]));

// ---- the guide: a few numbered steps for the page that is open ----
struct GuideText { const char* title; const char* steps[6]; };
const GuideText kGuides[7] = {
  {"Drums: build a groove",
   {"The letters A to H at the top are your grooves: the parts of your set. Pick one.",
    "On the right is a list of patterns. A click plays one; the letters under the list put it into a groove.",
    "In the grid, a click sets or removes a hit. A right-click (or ACCENT lit) makes it a loud one.",
    "The numbers open a bar. + BAR and x2 make the groove longer; COPY and PASTE move a bar's content.",
    "While LOOP BAR is lit only the open bar plays, so you can work on it. Unlit: the whole groove.",
    "Point at anything: the line at the bottom of the window says what it does."}},
  {"Synth: bass lines and melodies",
   {"Pick the track: Bass, Lead, Syn 3 or Syn 4. Each has its own sound and its own notes.",
    "The patterns on the right work as on the drums page: click to hear, A to H to keep.",
    "In the note grid, a click sets a note and a click on a note removes it. ACCENT and SLIDE apply to the next notes you set.",
    "The bars under the track buttons shape the sound. Move one while it plays.",
    "The notes always fit the project's key, so nothing sounds wrong.", nullptr}},
  {"Pedals: shape the sound of a track",
   {"Pick the track. Its sound runs through six slots from left to right: the strip on top shows all six.",
    "BOARDS puts a ready-made chain on the track. A good place to start.",
    "Each box is a pedal: the list at its bottom chooses which one, the switch under the knobs its character.",
    "Under that: Mix against the untouched sound, KEEP SUB (the lows go round the pedal), SYNC (in time with the tempo).",
    "< and > move a pedal along the chain. ON switches it off without losing its settings.",
    "Too much at once? MENU > Fewer pedal controls."}},
  {"Mix: how loud everything is",
   {"One column per track: its level, drive, tone and how much goes to the shared echo and reverb.",
    "The small bar beside each name shows how loud the track is right now.",
    "MUTE silences a track. SOLO silences all the others; a second click brings them back.", nullptr, nullptr, nullptr}},
  {"Song: put the grooves in a row",
   {"Each block is a groove, as wide as it is long. ADD A to H puts one after the selected block.",
    "Drag a block to move it. DUP repeats it, REMOVE takes it out, a double-click opens it for editing.",
    "With PLAY THE SONG lit and FOLLOW HOST on, the song is tied to your DAW's timeline, from the bar set at STARTS AT BAR.",
    "Knob movements: light REC KNOBS at the top, let it play, and hold and move any knob. Its curve appears below.",
    "You can redraw a curve with the mouse, and CLEAR it.", nullptr}},
  {"Library: ready-made material",
   {"Choose what you are after: whole grooves, synth sounds or drum kits.", "Narrow the list by style, pick an entry and press LOAD.",
    "A groove goes into the groove that is selected at the top (A to H).", nullptr, nullptr, nullptr}},
  {"Loop Master Live: play your set",
   {"The big pads are your grooves. A click starts one at the next bar line, so it is always in time.",
    "Hold an effect pad for as long as you want it: stutter, gate, crush, echo, wash, lo-fi.",
    "The filter strip: drag left and the highs go, right and the lows go. A double-click puts it back.",
    "Each track has MUTE, SOLO and a level.",
    "MIDI LEARN: light it, click a pad or control, then press a key or turn a knob on your controller.",
    "Keys: 1 to 8 start grooves, the space bar starts and stops, Q W E R T mute the tracks."}},
};

class GuideView : public juce::Component {
 public:
  std::function<void()> onClose;
  void show(int which) { which_ = juce::jlimit(0, 6, which); setVisible(true); toFront(false); repaint(); }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colours::black.withAlpha(0.72f));
    const GuideText& gt = kGuides[which_];
    int count = 0;
    while (count < 6 && gt.steps[count]) ++count;
    const int w = juce::jmin(700, getWidth() - 40), h = 96 + count * 52;
    const juce::Rectangle<int> box((getWidth() - w) / 2, juce::jmax(10, (getHeight() - h) / 2), w, h);
    g.setColour(col::faceplate);
    g.fillRoundedRectangle(box.toFloat(), 10.0f);
    g.setColour(col::orange);
    g.drawRoundedRectangle(box.toFloat().reduced(1.0f), 10.0f, 2.0f);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText(gt.title, box.reduced(22, 14).removeFromTop(30), juce::Justification::centredLeft, true);
    auto body = box.reduced(22, 14).withTrimmedTop(40);
    for (int i = 0; i < count; ++i) {
      auto row = body.removeFromTop(52);
      g.setColour(col::orange);
      g.fillEllipse(float(row.getX()), float(row.getY() + 8), 26.0f, 26.0f);
      g.setColour(col::ink);
      g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
      g.drawText(juce::String(i + 1), row.getX(), row.getY() + 8, 26, 26, juce::Justification::centred);
      g.setColour(col::silk);
      g.setFont(juce::Font(juce::FontOptions(14.0f)));
      g.drawFittedText(gt.steps[i], row.withTrimmedLeft(38).reduced(0, 2), juce::Justification::centredLeft, 2);
    }
    g.setColour(col::muted);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText("Click anywhere to close. MENU > Guide brings this back for the page you are on.", box.reduced(22, 10).removeFromBottom(18),
               juce::Justification::centredLeft, true);
  }
  void mouseDown(const juce::MouseEvent&) override {
    setVisible(false);
    if (onClose) onClose();
  }

 private:
  int which_ = 0;
};

// ---- Loop Master Live ----
// One screen to play a set from: big groove pads, effects to hold, a filter, mutes and levels. Everything on it
// can be tied to a key or knob of a MIDI controller.
class LiveView : public juce::Component {
 public:
  using P = LoopMasterProcessor;
  std::function<void()> onExit, onGuide;
  juce::String hover;                                           // what the mouse is over; set by the editor

  explicit LiveView(LoopMasterProcessor& proc) : p(proc) {
    setOpaque(true);
    setWantsKeyboardFocus(true);
    for (int t = 0; t < ld::kNumTracks; ++t) {
      level[t] = std::make_unique<ParamBar>(p, "Level", ld::P_FX_LEVEL, t, 0.0f, 1.0f, fmtPct);
      help(*level[t], juce::String(kTrackNames[t]) + " level. With MIDI LEARN lit, click the track's name to tie a controller knob to it.");
      addAndMakeVisible(*level[t]);
    }
    exitBtn.onClick = [this] { if (onExit) onExit(); };
    guideBtn.onClick = [this] { if (onGuide) onGuide(); };
    learnBtn.onClick = [this] { learning = !learning; if (!learning) p.liveLearn(-1); repaint(); };
    songBtn.onClick = [this] { p.setSongMode(!p.songMode()); };
    bpmDown.onClick = [this] { nudge(-1.0f); };
    bpmUp.onClick = [this] { nudge(1.0f); };
    for (auto* b : {&exitBtn, &guideBtn, &learnBtn, &songBtn, &bpmDown, &bpmUp}) { b->setWantsKeyboardFocus(false); addAndMakeVisible(b); }
    help(exitBtn, "Back to the editor. Everything keeps playing.");
    help(learnBtn, "MIDI LEARN: light it, click a pad or control here, then press a key or turn a knob on your controller. A right-click forgets.");
    help(songBtn, "Lit: the song from the SONG page plays, block after block. Unlit: you start the grooves yourself.");
    help(guideBtn, "How this screen works, in six steps.");
  }

  // from the editor's timer, with the lock held
  void tick() {
    ++frame;
    for (auto& l : level) l->refresh();
    learnBtn.setToggleState(learning, juce::dontSendNotification);
    songBtn.setToggleState(p.songMode(), juce::dontSendNotification);
    bpmDown.setEnabled(!p.hostLeads()); bpmUp.setEnabled(!p.hostLeads());
    if (learning && wasLearning >= 0 && p.liveLearning() < 0 && p.liveMapName(wasLearning).isNotEmpty()) note = "Tied to " + p.liveMapName(wasLearning) + ".";
    wasLearning = p.liveLearning();
    repaint();
  }

  void resized() override {
    auto r = getLocalBounds().reduced(14);
    auto top = r.removeFromTop(58);
    playArea = top.removeFromLeft(150); top.removeFromLeft(10);
    bpmDown.setBounds(top.removeFromLeft(34).reduced(0, 10));
    tempoArea = top.removeFromLeft(104).reduced(4, 6);
    bpmUp.setBounds(top.removeFromLeft(34).reduced(0, 10)); top.removeFromLeft(14);
    exitBtn.setBounds(top.removeFromRight(96).reduced(0, 12)); top.removeFromRight(6);
    guideBtn.setBounds(top.removeFromRight(70).reduced(0, 12)); top.removeFromRight(6);
    learnBtn.setBounds(top.removeFromRight(112).reduced(0, 12)); top.removeFromRight(6);
    songBtn.setBounds(top.removeFromRight(70).reduced(0, 12)); top.removeFromRight(10);
    counterArea = top;
    r.removeFromTop(12);
    r.removeFromBottom(22);                                     // the line that explains
    auto grooves = r.removeFromTop(juce::jmax(110, r.getHeight() * 30 / 100));
    const int gw = (grooves.getWidth() - 7 * 8) / 8;
    for (int i = 0; i < 8; ++i) { grooveArea[i] = grooves.removeFromLeft(gw); grooves.removeFromLeft(8); }
    r.removeFromTop(14);
    auto fx = r.removeFromTop(juce::jmax(84, r.getHeight() * 32 / 100));
    filterArea = fx.removeFromRight(fx.getWidth() * 34 / 100);
    fx.removeFromRight(12);
    const int fw = (fx.getWidth() - 5 * 8) / 6;
    for (int i = 0; i < 6; ++i) { fxArea[i] = fx.removeFromLeft(fw); fx.removeFromLeft(8); }
    r.removeFromTop(14);
    const int tw = (r.getWidth() - 4 * 8) / 5;
    for (int t = 0; t < ld::kNumTracks; ++t) {
      auto c = juce::Rectangle<int>(r.getX() + t * (tw + 8), r.getY(), tw, r.getHeight());
      nameArea[t] = c.removeFromTop(26);
      level[t]->setBounds(c.removeFromBottom(30)); c.removeFromBottom(6);
      soloArea[t] = c.removeFromBottom(juce::jmin(34, c.getHeight() / 3)); c.removeFromBottom(6);
      muteArea[t] = c;
    }
  }

  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour(0xff0c0d0f));
    const juce::ScopedLock sl(p.lock());
    ld::Session& s = p.session();
    const ld::Project& pr = s.project();
    const ld::Engine& e = s.engine();
    const bool playing = e.isPlaying(), blink = (frame / 6) % 2 == 0;
    const int now = e.currentScene(), next = e.queuedScene(), step = juce::jmax(0, e.currentStep());
    const auto pad = [&](juce::Rectangle<int> area, juce::Colour c, bool lit, const juce::String& big, const juce::String& small, int target) {
      const auto f = area.toFloat();
      g.setColour(lit ? c : c.withMultipliedBrightness(0.28f));
      g.fillRoundedRectangle(f, 8.0f);
      const bool wait = learning && target >= 0 && p.liveLearning() == target;
      g.setColour(wait ? juce::Colours::white : (learning && target >= 0 ? col::orange.withAlpha(0.8f) : juce::Colours::black.withAlpha(0.5f)));
      g.drawRoundedRectangle(f.reduced(1.0f), 8.0f, wait ? 3.0f : (learning && target >= 0 ? 2.0f : 1.0f));
      g.setColour(lit ? col::ink : col::silk);
      const float fit = float(area.getWidth() - 16) / (0.66f * float(juce::jmax(1, big.length())));      // as big as the word allows
      g.setFont(juce::Font(juce::FontOptions(juce::jlimit(12.0f, 34.0f, juce::jmin(float(area.getHeight()) / 3.0f, fit)), juce::Font::bold)));
      g.drawText(big, area.reduced(6, 6), juce::Justification::centred, false);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      if (small.isNotEmpty()) g.drawText(small, area.reduced(8, 5), juce::Justification::bottomLeft, true);
      const juce::String tied = wait ? juce::String("press a key / turn a knob") : (target >= 0 ? p.liveMapName(target) : juce::String());
      if (tied.isNotEmpty()) g.drawText(tied, area.reduced(8, 5), juce::Justification::topRight, true);
    };
    // transport
    pad(playArea, playing ? col::green : col::orange, true, p.hostPlays() ? "HOST" : (playing ? "STOP" : "PLAY"), {}, P::kLivePlay);
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(tempoArea.toFloat(), 4.0f);
    g.setColour(col::red);
    g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    g.drawText(juce::String(p.shownBpm(), 1), tempoArea, juce::Justification::centred);
    g.setColour(col::silk);
    g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    juce::String where = juce::String(kSceneNames[juce::jlimit(0, 7, now)]);
    if (playing) where << "   bar " << (step / 16 + 1) << " . " << ((step / 4) % 4 + 1);
    if (next >= 0 && next != now) where << "   next: " << kSceneNames[juce::jlimit(0, 7, next)];
    g.drawText(where, counterArea, juce::Justification::centredLeft, true);
    // grooves
    for (int i = 0; i < 8; ++i) {
      const ld::Scene& sc = pr.scenes[i];
      bool any = false;
      for (const auto& row : sc.drums.steps)
        for (int st = 0; st < sc.bars * 16 && !any; ++st) any = row[st] != 0;
      for (const auto& pat : sc.synth) any = any || pat.count > 0;
      const bool on = playing && now == i, queued = next == i && next != now;
      pad(grooveArea[i], col::pads[i], on || (queued && blink), kSceneNames[i],
          any ? juce::String(int(sc.bars)) + (sc.bars == 1 ? " bar" : " bars") : juce::String("empty"), P::kLiveGroove + i);
      if (on) {                                                 // how far through the groove it is
        const auto f = grooveArea[i].toFloat().reduced(8.0f, 0.0f);
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(f.getX(), f.getBottom() - 26.0f, f.getWidth(), 5.0f);
        g.setColour(col::ink);
        g.fillRect(f.getX(), f.getBottom() - 26.0f, f.getWidth() * float(step + 1) / float(juce::jmax(1, int(sc.bars) * 16)), 5.0f);
      }
    }
    // effects to hold
    for (int i = 0; i < 6; ++i) pad(fxArea[i], juce::Colour(0xffd0d4da), p.liveFxHeld() == i, P::liveFxName(i), "hold", P::kLiveFx + i);
    // the filter
    {
      const auto f = filterArea.toFloat();
      g.setColour(col::recess);
      g.fillRoundedRectangle(f, 8.0f);
      const bool wait = learning && p.liveLearning() == P::kLiveFilter;
      g.setColour(wait ? juce::Colours::white : (learning ? col::orange.withAlpha(0.8f) : col::line));
      g.drawRoundedRectangle(f.reduced(1.0f), 8.0f, wait ? 3.0f : (learning ? 2.0f : 1.0f));
      const float v = p.liveFilter(), mid = f.getCentreX(), x = mid + v * (f.getWidth() * 0.5f - 14.0f);
      g.setColour(col::line);
      g.fillRect(mid - 1.0f, f.getY() + 26.0f, 2.0f, f.getHeight() - 52.0f);
      g.setColour(col::orange.withAlpha(0.45f));
      g.fillRect(juce::jmin(mid, x), f.getY() + 30.0f, std::abs(x - mid), f.getHeight() - 60.0f);
      g.setColour(std::abs(v) > 0.03f ? col::orange : col::silk);
      g.fillRoundedRectangle(x - 7.0f, f.getY() + 22.0f, 14.0f, f.getHeight() - 44.0f, 4.0f);
      g.setColour(col::muted);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.drawText("HIGHS OFF", filterArea.reduced(10, 5), juce::Justification::topLeft, false);
      g.drawText("LOWS OFF", filterArea.reduced(10, 5), juce::Justification::topRight, false);
      g.drawText("FILTER", filterArea.reduced(10, 5), juce::Justification::centredTop, false);
      const juce::String tied = wait ? juce::String("turn a knob") : p.liveMapName(P::kLiveFilter);
      if (tied.isNotEmpty()) g.drawText(tied, filterArea.reduced(10, 5), juce::Justification::centredBottom, false);
    }
    // tracks
    const int used = 1 + juce::jlimit(1, ld::kNumSynths, int(pr.activeSynths));
    for (int t = 0; t < ld::kNumTracks; ++t) {
      const bool has = t < used, muted = pr.fx[t].mute;
      const juce::Colour tc = t == 0 ? col::orange : col::synths[t - 1];
      g.setColour(has ? tc : col::line);
      g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
      g.drawText(kTrackNames[t], nameArea[t], juce::Justification::centredLeft, true);
      const bool waitL = learning && p.liveLearning() == P::kLiveLevel + t;
      const juce::String tied = waitL ? juce::String("turn a knob") : p.liveMapName(P::kLiveLevel + t);
      g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
      g.setColour(waitL ? juce::Colours::white : col::muted);
      if (tied.isNotEmpty()) g.drawText(tied, nameArea[t].withTrimmedRight(74), juce::Justification::centredRight, true);
      const auto meter = nameArea[t].withTrimmedLeft(nameArea[t].getWidth() - 68).reduced(0, 9).toFloat();
      g.setColour(col::recess);
      g.fillRect(meter);
      const float lv = juce::jlimit(0.0f, 1.0f, std::sqrt(e.trackPeak(t)));
      g.setColour(lv > 0.95f ? col::red : col::green);
      g.fillRect(meter.withWidth(meter.getWidth() * lv));
      if (!has) continue;
      pad(muteArea[t], muted ? col::red : tc, true, muted ? "MUTED" : "ON", {}, P::kLiveMute + t);
      pad(soloArea[t], col::yellow, p.liveSoloed() == t, "SOLO", {}, -1);
    }
    g.setColour(col::muted);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    const juce::String line = hover.isNotEmpty() ? hover : (note.isNotEmpty() ? note
        : (learning ? juce::String("MIDI LEARN: click a pad or control (a track's name for its level), then press a key or turn a knob. Right-click forgets.")
                    : juce::String("A groove starts at the next bar line. Hold an effect. Keys: 1-8 grooves, space play/stop, Q W E R T mutes.")));
    g.drawText(line, getLocalBounds().reduced(14, 6).removeFromBottom(18), juce::Justification::centredLeft, true);
  }

  // What is under a point: a live target, 100 + track for SOLO, or -1.
  int hit(juce::Point<int> at) const {
    if (playArea.contains(at)) return P::kLivePlay;
    for (int i = 0; i < 8; ++i)
      if (grooveArea[i].contains(at)) return P::kLiveGroove + i;
    for (int i = 0; i < 6; ++i)
      if (fxArea[i].contains(at)) return P::kLiveFx + i;
    if (filterArea.contains(at)) return P::kLiveFilter;
    for (int t = 0; t < ld::kNumTracks; ++t) {
      if (muteArea[t].contains(at)) return P::kLiveMute + t;
      if (soloArea[t].contains(at)) return 100 + t;
      if (nameArea[t].contains(at)) return P::kLiveLevel + t;
    }
    return -1;
  }
  // The sentence for what is under the mouse.
  juce::String helpAt(juce::Point<int> at) const {
    const int h = hit(at);
    if (h == P::kLivePlay) return "Starts and stops. Inside a DAW that is playing, the DAW's own play button is in charge.";
    if (h >= P::kLiveGroove && h < P::kLiveGroove + 8) return "Groove pad: a click starts this groove at the next bar line. It blinks while it waits.";
    if (h >= P::kLiveFx && h < P::kLiveFx + 6) return "Hold it: the effect is on everything for as long as you hold. Echo and wash ring out after you let go.";
    if (h == P::kLiveFilter) return "Filter over everything: drag left and the highs go, right and the lows go. A double-click puts it back in the middle.";
    if (h >= P::kLiveMute && h < P::kLiveMute + ld::kNumTracks) return "Silences this track, or brings it back.";
    if (h >= 100) return "SOLO: only this track is heard. A second click brings the others back as they were.";
    return {};
  }

  void mouseDown(const juce::MouseEvent& ev) override {
    grabKeyboardFocus();
    note.clear();
    const int h = hit(ev.getPosition());
    if (h < 0) return;
    if (learning && h < 100) {
      if (ev.mods.isPopupMenu()) { p.liveForget(h); note = "Forgotten."; }
      else p.liveLearn(p.liveLearning() == h ? -1 : h);
      return;
    }
    if (h == P::kLivePlay) p.livePlayStop();
    else if (h >= P::kLiveGroove && h < P::kLiveGroove + 8) p.setScene(h - P::kLiveGroove);
    else if (h >= P::kLiveFx && h < P::kLiveFx + 6) { held = h - P::kLiveFx; p.liveFx(held, true); }
    else if (h == P::kLiveFilter) { filtering = true; dragFilter(ev); }
    else if (h >= P::kLiveMute && h < P::kLiveMute + ld::kNumTracks) p.liveToggleMute(h - P::kLiveMute);
    else if (h >= 100) p.liveSolo(h - 100);
  }
  void mouseDrag(const juce::MouseEvent& ev) override { if (filtering) dragFilter(ev); }
  void mouseUp(const juce::MouseEvent&) override {
    if (held >= 0) { p.liveFx(held, false); held = -1; }
    filtering = false;
  }
  void mouseDoubleClick(const juce::MouseEvent& ev) override { if (filterArea.contains(ev.getPosition())) p.setLiveFilter(0.0f); }
  bool keyPressed(const juce::KeyPress& key) override {
    const int c = int(key.getTextCharacter());
    if (c >= '1' && c <= '8') { p.setScene(c - '1'); return true; }
    if (key == juce::KeyPress::spaceKey) { p.livePlayStop(); return true; }
    static const char mutes[] = "qwert";
    for (int t = 0; t < ld::kNumTracks; ++t)
      if (c == mutes[t] || c == mutes[t] - 32) { p.liveToggleMute(t); return true; }
    return false;
  }

 private:
  void dragFilter(const juce::MouseEvent& ev) { p.setLiveFilter(float(ev.x - filterArea.getCentreX()) / (float(filterArea.getWidth()) * 0.5f - 14.0f)); }
  void nudge(float by) {
    if (p.hostLeads()) return;
    const juce::ScopedLock sl(p.lock());
    p.session().beginEdit();
    p.session().setParam(ld::P_BPM, 0, p.session().getParam(ld::P_BPM, 0) + by);
  }
  LoopMasterProcessor& p;
  std::unique_ptr<ParamBar> level[ld::kNumTracks];
  juce::TextButton exitBtn{"EDITOR"}, guideBtn{"GUIDE"}, learnBtn{"MIDI LEARN"}, songBtn{"SONG"}, bpmDown{"-"}, bpmUp{"+"};
  juce::Rectangle<int> playArea, tempoArea, counterArea, grooveArea[8], fxArea[6], filterArea, nameArea[ld::kNumTracks], muteArea[ld::kNumTracks],
      soloArea[ld::kNumTracks];
  juce::String note;
  int frame = 0, held = -1, wasLearning = -1;
  bool learning = false, filtering = false;
};
