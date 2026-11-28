// Part of Editor.cpp and of the Loop Pedals editor (inside an unnamed namespace): every pedal, as the screen shows it.
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
   {"Smooth", "Hard"}, {"Fades the noise out", "Slams shut"}},  // made for drums and synths
  {22, "PNCH", "Punch", 0xffd8402a, 0xff9c2516, true, 0xfff1ead8, {"Attack", "Sustain", "Level", "Clip"},
   {"Punch", "Snap"},
   {"Makes the hit of every drum harder or softer, its ring longer or shorter", "Shapes only the top of the hit, the body stays"}},
  {23, "CRSH", "Crusher", 0xff4a4f58, 0xff2a2d33, true, 0xffff6d1f, {"Drive", "Tone", "Glue", "Body"},
   {"Tape", "Diode", "Clip"},
   {"Glue and round saturation for the whole kit", "Crunchy", "Flat tops: the gabber kick"}},
  {24, "SLCE", "Slicer", 0xff12b5a5, 0xff0a756b, false, 0xff1d1b18, {"Depth", "Rate", "Width", "Pattern"},
   {"Gate", "Stutter"},
   {"Sixteen steps in time with the tempo let the sound through or not", "Every two beats the last slice is repeated"}},
  {25, "GVRB", "Gated Verb", 0xffb04a9a, 0xff742c64, true, 0xfff1ead8, {"Level", "Gate", "Size", "Tone"},
   {"Gated", "Reverse"},
   {"A huge room that is cut off: the snare of the eighties", "Swells up to nothing"}},
  {26, "BOOM", "Boom", 0xff2a2f6b, 0xff171a40, true, 0xffffca28, {"Boom", "Pitch", "Decay", "Sens"},
   {"Sub", "Thump"},
   {"Every low hit sets off a clean sub tone", "With a pitch drop, like a drum machine's kick"}},
  {27, "CLD", "Cloud", 0xffa8c8e8, 0xff7396be, false, 0xff1d1b18, {"Mix", "Grain", "Scatter", "Feedbk"},
   {"Cloud", "Reverse", "Octave", "Fifth"},
   {"Small grains of what you played, scattered in time and space", "The grains run backwards", "Half of them an octave up", "Half of them a fifth up"}},
  {28, "LOFI", "Lo-Fi", 0xffc9a36a, 0xff94733f, false, 0xff1d1b18, {"Wobble", "Age", "Drive", "Noise"},
   {"Cassette", "Reel", "Vinyl"},
   {"Wow, flutter, hiss and a narrow band", "A studio machine: gentler, with a bump in the lows", "Slow wow and crackle"}},
  {29, "DUB", "Dub Delay", 0xff1f6e3a, 0xff124421, true, 0xffffca28, {"Level", "Time", "Feedbk", "Filter"},
   {"Dub", "Ping-pong", "Wide"},
   {"Always in time. FILTER left: every repeat darker; right: thinner", "Repeats jump from side to side", "A different time on each side"}},
  {30, "FSH", "Freq Shift", 0xff8c96a3, 0xff59626e, true, 0xff1d1b18, {"Shift", "Mix", "Feedbk", "Spread"},
   {"Up", "Down", "Split"},
   {"Every tone moved up by the same number of hertz: metal and bells", "Moved down", "Down on the left, up on the right"}},
  {31, "RES", "Resonator", 0xffb8860b, 0xff7d5a05, true, 0xfff1ead8, {"Mix", "Note", "Decay", "Bright"},
   {"String", "Tube", "Chord"},
   {"A string tuned to the project's scale rings along with every hit", "A pipe: hollow, only the odd overtones", "Three strings: the note, its third and fifth"}},
  {32, "SHIM", "Shimmer", 0xffe8d9f5, 0xffb59fd0, false, 0xff1d1b18, {"Level", "Decay", "Shimmer", "Tone"},
   {"Octave", "Fifth", "Freeze"},
   {"A reverb whose tail climbs an octave", "Climbs a fifth", "HOLD up: the room stands still while you play over it"}},
  {33, "AMP-V", "Amp: Vintage", 0xffc9a86a, 0xff8e6f3a, false, 0xff2a1d10, {"Gain", "Tone", "Level", "Presence"},
   {"Tweed '57", "Blackface '65", "Chime '63", "Plexi '68"},
   {"Small and loose: thick mids, gives way under every chord, rasps when pushed. Blues, early rock'n'roll",
    "Loud and clean: scooped mids, a glassy top, tight lows. Funk, country, surf, the pedal platform",
    "Class A jangle: bright, breaks up early and sings in the power amp. British invasion, indie",
    "The stack: bark in the upper mids, crunch that cleans up when you play softly. Classic rock"}},
  {34, "CAB", "Cab", 0xff6b4a2c, 0xff3d2915, true, 0xff1d1b18, {"Size", "Mic", "Level", "Thump"},
   {"4x12", "2x12 Open", "1x10", "Bass 8x10"},
   {"A closed box with four speakers: big and tight. Put it after an amp or a drive", "An open-backed combo: airy, less low end", "One small speaker: boxy, for old records",
    "A bass stack: deep, with a soft top"}},
  {35, "EQ", "Tone Control", 0xffd8d8d0, 0xffa9a99f, false, 0xff1d1b18, {"Low", "Mid", "High", "Mid Freq"},
   {"Guitar", "Bass", "Wide"},
   {"Three bands, 15 dB up or down; in the middle it does nothing", "The bands sit where a bass needs them", "Low and high sit far apart: for synths and whole mixes"}},
  {36, "AMP-S", "Amp: Stage", 0xff2a2623, 0xff141210, true, 0xffd9c9a0, {"Gain", "Tone", "Level", "Presence"},
   {"British 800", "Modern", "Jazz Clean", "Bass Tube"},
   {"Hotter and tighter than the stack: punching mids, a rasp on top. Eighties rock and metal",
    "A wall of gain: scooped mids, huge lows, a fizzing top. Modern metal",
    "Transistors: flat, wide and spotless at any volume. Jazz, clean chorus sounds, synths",
    "300 watts of tubes for bass: the lows go through whole, the mids growl when pushed"}},
};
// What knob `i` of a pedal is called on a given character. "" = this knob does nothing there.
const char* knobName(const PedalSpec& sp, int voice, int i) {
  if (i == 4) return "Mix";
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
    if (sp.type == 24 && voice == 1) return "Amount";
  }
  if (i == 2 && sp.type == 32 && voice == 2) return "Hold";
  if (i == 2 && sp.type == 13 && voice == 1) return "";
  return sp.knobs[i];
}
// What the pedal does to a wave, as a little scope trace: x 0 .. 1, result -1 .. 1.
// How old each pedal's kind is: the look follows. 0: analog (a painted metal box, screws, round knobs, a glowing
// tube scope). 1: digital (a flat box with grooves, a ring of lights round each dial, an LCD).
const int kFxEra[ld::kNumFxTypes] = {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0};
const char* const kFxYear[ld::kNumFxTypes] = {"", "1977", "1998", "1976", "1974", "1972", "1975", "1978", "1966", "1965", "1963", "1968", "1977", "1967",
                                              "1967", "1972", "1982", "1971", "1983", "1974", "1986", "1985", "1996", "1999", "2001", "1984", "1980",
                                              "2005", "1979", "1973", "1972", "1994", "2008", "1957", "1962", "1975", "1981"};
float pedalTrace(int type, float x);
// What the pedal's display shows right now: its picture, bent by where the knobs stand and by the character that is
// chosen, and moving if the pedal moves. x: 0 .. 1 across the display; secs: a clock.
float pedalScope(int type, int voice, float amount, float rate, float p3, float mix, float x, float secs) {
  const float t = juce::MathConstants<float>::twoPi * x;
  const float dry = std::sin(t * 3.0f) * 0.6f;
  float xx = x;
  switch (type) {
    case 3: case 4: case 10: case 11: case 12: case 13: case 14: case 24: case 28: case 30:      // these move by themselves
      xx = x + secs * (0.05f + 0.5f * rate); xx -= std::floor(xx); break;
    case 18: case 19: case 20: case 25: case 27: case 29: case 32: case 31:                      // these ring: longer or shorter
      xx = juce::jlimit(0.0f, 1.0f, x / (0.45f + 1.1f * rate)); break;
    default: break;
  }
  float wet = pedalTrace(type, xx);
  switch (voice) {                                             // every character bends the picture its own way
    case 1: wet = wet + 0.35f * wet * std::fabs(wet) + 0.12f; break;
    case 2: wet = wet * 0.8f + 0.2f * std::sin(t * 11.0f) * (0.4f + 0.6f * std::fabs(wet)); break;
    case 3: wet = std::round(wet * 4.0f) / 4.0f; break;
    default: break;
  }
  wet *= 0.75f + 0.5f * p3;
  const float k = (0.2f + 0.8f * amount) * mix;
  return dry * (1.0f - k) + wet * k;
}
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
    case 22: { const float u = std::fmod(x * 3.0f, 1.0f); return std::sin(u * 40.0f) * (u < 0.08f ? 1.0f : 0.45f * std::exp(-5.0f * u)); }
    case 23: return juce::jlimit(-0.75f, 0.75f, std::sin(t * 2.0f) * 1.6f + 0.25f * std::sin(t * 9.0f));
    case 24: return std::fmod(x * 8.0f, 1.0f) < 0.55f && (int(x * 8.0f) % 4 != 3) ? std::sin(t * 24.0f) * 0.8f : 0.0f;
    case 25: return x < 0.06f ? 0.9f : (x < 0.6f ? std::sin(t * 31.0f + 2.0f * std::sin(t * 9.0f)) * 0.55f : 0.0f);
    case 26: return std::sin(juce::MathConstants<float>::twoPi * (2.0f * x + 1.2f * (1.0f - std::exp(-8.0f * x)))) * std::exp(-2.2f * x) * 0.9f;
    case 27: { float y = 0.0f; for (int n = 0; n < 6; ++n) { const float c = 0.1f + 0.16f * float(n), d = (x - c) / 0.05f; y += std::exp(-d * d) * std::sin(t * (14.0f + 9.0f * float(n % 3))) * (n % 2 ? 0.5f : 0.8f); } return y; }
    case 28: return std::sin(t * 4.0f + 1.5f * std::sin(t * 0.7f)) * 0.6f + 0.12f * std::sin(t * 61.0f) * std::sin(t * 3.3f);
    case 29: { float y = 0.0f; for (int n = 0; n < 5; ++n) { const float c = 0.08f + 0.2f * float(n), d = (x - c) / (0.02f + 0.012f * float(n)); y += std::exp(-d * d) * std::pow(0.7f, float(n)); } return y * 1.7f - 0.85f; }
    case 30: return std::sin(t * (2.0f + 9.0f * x)) * 0.8f;
    case 31: return std::sin(t * 12.0f) * std::exp(-3.0f * std::fmod(x * 2.0f, 1.0f)) * 0.9f;
    case 32: return (std::sin(t * 5.0f) * (1.0f - x) + std::sin(t * 10.0f) * x) * (0.4f + 0.5f * std::sin(juce::MathConstants<float>::pi * x));
    case 33: return std::tanh(std::sin(t * 3.0f) * 4.0f + 0.3f) * 0.8f;                        // an amp: the wave pressed flat
    case 36: return std::tanh(std::sin(t * 3.0f) * 9.0f) * 0.75f;                                 // a hot amp: nearly square
    case 34: return 0.75f * std::sin(t * 2.0f) + 0.1f * std::sin(t * 6.0f);                    // a speaker: round
    case 35: return 0.5f * std::sin(t * 2.0f) + 0.3f * std::sin(t * 7.0f);                      // a tone control: lows and highs
    default: return 0.0f;
  }
}
const PedalSpec* pedalSpec(int type) {
  for (const auto& sp : kPedals) if (sp.type == type) return &sp;
  return nullptr;
}
// how the menu is ordered: families, each with its pedals (by type number)
struct PedalFamily { const char* name; int types[8]; };   // a 0 ends the list
const PedalFamily kPedalFamilies[] = {
  {"Amp and tone", {33, 36, 34, 35, 0, 0, 0, 0}},
  {"Drive", {1, 7, 8, 9, 2, 0, 0, 0}},
  {"Dynamics", {6, 21, 0, 0, 0, 0, 0, 0}},
  {"Modulation", {3, 12, 4, 10, 11, 13, 17, 0}},
  {"Filter and pitch", {5, 14, 15, 16, 0, 0, 0, 0}},
  {"Echo and space", {18, 19, 29, 20, 25, 32, 0, 0}},
  {"For drums", {22, 23, 24, 26, 0, 0, 0, 0}},
  {"Textures", {27, 28, 30, 31, 0, 0, 0, 0}},
};
