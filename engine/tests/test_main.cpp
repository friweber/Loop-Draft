// Desktop tests for the Loop Draft engine. No external test framework: build and run ld_tests.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "loopdraft/engine.h"
#include "loopdraft/session.h"

// ---- count heap allocations, to prove the audio path makes none ----
static std::atomic<bool> g_count{false};
static std::atomic<long> g_allocs{0};
void* operator new(std::size_t n) {
  if (g_count.load(std::memory_order_relaxed)) g_allocs.fetch_add(1, std::memory_order_relaxed);
  if (void* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
  if (g_count.load(std::memory_order_relaxed)) g_allocs.fetch_add(1, std::memory_order_relaxed);
  return std::malloc(n ? n : 1);
}
void* operator new[](std::size_t n) { return operator new(n); }
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept { return operator new(n, t); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace ld;

static int g_fail = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_fail; std::printf("    FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { ++g_checks; const auto va = (a); const auto vb = (b); if (!(va == vb)) { ++g_fail; \
  std::printf("    FAIL %s:%d  %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, (long long)va, (long long)vb); } } while (0)

// A one-sample click: its position in the output shows exactly when a step fired.
static std::unique_ptr<Sample> click(double sr) {
  auto s = std::make_unique<Sample>();
  s->rate = sr; s->l = {1.0f, 0.0f, 0.0f, 0.0f};
  return s;
}
static Project emptyProject() {
  Project p = makeDemoProject();
  for (auto& sc : p.scenes) sc = Scene{};
  for (auto& fx : p.fx) { fx.drive = 0; fx.eqLow = 0; fx.eqHigh = 0; fx.delaySend = 0; fx.revSend = 0; }
  for (auto& pad : p.pads) { pad.vol = 0.9f; pad.choke = 0; }
  p.bpm = 120; p.swing = 0;
  return p;
}
static std::vector<long> clicks(const std::vector<float>& x, float thr = 0.2f) {
  std::vector<long> out;
  for (size_t i = 0; i < x.size(); ++i) if (std::fabs(x[i]) > thr) out.push_back(long(i));
  return out;
}
static void renderN(Engine& e, std::vector<float>& l, long frames, int block = 256) {
  const size_t bs = size_t(block);
  std::vector<float> a(bs), b(bs);
  while (frames > 0) {
    const int n = int(frames < block ? frames : block);
    e.render(a.data(), b.data(), n);
    l.insert(l.end(), a.begin(), a.begin() + n);
    frames -= n;
  }
}
static bool same(const std::vector<float>& a, const std::vector<float>& b) {
  return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}
static float peakOf(const std::vector<float>& x) { float p = 0; for (float v : x) p = std::fmax(p, std::fabs(v)); return p; }
static bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }

// ------------------------------------------------------------------ tests

static void testMusic() {
  Project p = makeDemoProject();   // D minor, octave 3
  const int want[8] = {50, 52, 53, 55, 57, 58, 60, 62};
  for (int d = 0; d < 8; ++d) CHECK_EQ(degreeToMidi(p, 0, d), want[d]);
  CHECK_EQ(degreeToMidi(p, 0, -1), 48);
  CHECK_EQ(degreeToMidi(p, -1, 0), 38);
  SynthParams sp; sp.chord = true;
  int m[3];
  CHECK_EQ(chordMidis(p, sp, 2, m), 3);
  CHECK(m[0] == 53 && m[1] == 57 && m[2] == 60);   // F major inside D minor
  const Scale scales[] = {Scale::Minor, Scale::Major, Scale::Dorian, Scale::Pentatonic};
  for (Scale sc : scales) for (int root = 0; root < 12; ++root) {
    p.scale = sc; p.root = root;
    bool allowed[12] = {};
    for (int d = 0; d < scaleSize(sc); ++d) allowed[degreeToMidi(p, 0, d) % 12] = true;
    for (int d = -7; d < 15; ++d) { chordMidis(p, sp, d, m); CHECK(allowed[m[0] % 12] && allowed[m[1] % 12] && allowed[m[2] % 12]); }
  }
}

static void testQueue() {
  SpscQueue<int> q(1024);
  const int N = 500000;
  std::thread prod([&] { for (int i = 0; i < N;) if (q.push(i)) ++i; else std::this_thread::yield(); });
  int expect = 0, v = 0; bool ordered = true;
  while (expect < N) { if (q.pop(v)) { if (v != expect) ordered = false; ++expect; } else std::this_thread::yield(); }
  prod.join();
  CHECK(ordered);
  CHECK(!q.pop(v));
}

static void testTimingAndSwing() {
  Project p = emptyProject();
  p.swing = 0.5f;
  p.scenes[0].drums.steps[0][0] = 2; p.scenes[0].drums.steps[0][1] = 2; p.scenes[0].drums.steps[0][4] = 2;
  auto c = click(48000);
  RenderOptions o; o.samples[0] = c.get(); o.tailSeconds = 0.1;
  const RenderResult r = renderOffline(p, o);
  const std::vector<long> got = clicks(r.l);
  CHECK_EQ(got.size(), size_t(3));
  if (got.size() == 3) { CHECK_EQ(got[0], 0); CHECK_EQ(got[1], 9000); CHECK_EQ(got[2], 24000); }   // 6000 per step, swung step +3000
  CHECK_EQ(r.l.size(), size_t(96000 + 4800));
}

static void testNoDriftOverEightBars() {
  Project p = emptyProject();
  p.bpm = 122;                                   // 5901.639... frames per step: not a whole number
  p.scenes[0].bars = 8;
  for (int s = 0; s < kMaxSteps; ++s) p.scenes[0].drums.steps[0][s] = 2;
  Engine e(48000);
  e.setPadSample(0, click(48000)); e.setProject(p); e.play(0);
  std::vector<float> l;
  const double sl = 48000.0 * 60.0 / 122.0 / 4.0;
  renderN(e, l, long(sl * 128 * 3) + 10, 333);   // three times round the 8-bar loop
  const std::vector<long> got = clicks(l);
  CHECK_EQ(got.size(), size_t(128 * 3 + 1));
  long worst = 0;
  for (size_t k = 0; k < got.size(); ++k) worst = std::max(worst, std::labs(got[k] - std::lround(double(k) * sl)));
  CHECK_EQ(worst, 0);                            // every one of 385 steps on its exact sample
}

static std::vector<float> renderSongBlocks(const std::function<int(int)>& blockAt) {
  Engine e(48000);
  e.setProject(makeDemoProject()); e.setSongMode(true); e.play(0);
  const long total = 48000 * 9;
  std::vector<float> out, a(1024), b(1024);
  long pos = 0; int k = 0;
  while (pos < total) {
    int n = blockAt(k++); if (n > 1024) n = 1024; if (n > total - pos) n = int(total - pos);
    e.render(a.data(), b.data(), n);
    for (int i = 0; i < n; ++i) { out.push_back(a[i]); out.push_back(b[i]); }
    pos += n;
  }
  return out;
}

static void testBlockSizeDoesNotChangeTheSound() {
  const std::vector<float> a = renderSongBlocks([](int) { return 64; });
  const std::vector<float> b = renderSongBlocks([](int) { return 480; });
  uint32_t seed = 7;
  const std::vector<float> c = renderSongBlocks([&](int) { seed = seed * 1664525u + 1013904223u; return 1 + int((seed >> 16) % 700); });
  CHECK(same(a, b)); CHECK(same(a, c));          // bit for bit
  CHECK(peakOf(a) > 0.2f); CHECK(finite(a));
}

static void testRendersAreRepeatable() {
  RenderOptions o; o.songMode = true;
  const RenderResult a = renderOffline(makeDemoProject(), o), b = renderOffline(makeDemoProject(), o);
  CHECK(same(a.l, b.l)); CHECK(same(a.r, b.r));
  bool stereo = false;
  for (size_t i = 0; i < a.l.size(); ++i) if (std::fabs(a.l[i] - a.r[i]) > 1e-3f) { stereo = true; break; }
  CHECK(stereo);
  o.sampleRate = 44100;
  const RenderResult c = renderOffline(makeDemoProject(), o);
  CHECK(finite(c.l) && peakOf(c.l) > 0.2f);
}

static void testLimiterCeilingHoldsUnderAbuse() {
  Project p = makeDemoProject();
  p.master = 1.0f; p.delayFeedback = 0.8f; p.bpm = 200;
  for (auto& fx : p.fx) { fx.level = 1; fx.drive = 1; fx.eqLow = 12; fx.eqHigh = 12; fx.delaySend = 0.8f; fx.revSend = 0.8f; }
  for (auto& s : p.synths) { s.res = 15; s.level = 1; s.chord = true; s.sustain = 1; s.lfoDepth = 1; }
  for (auto& sc : p.scenes) {
    for (auto& pad : sc.drums.steps) for (int s = 0; s < 16; ++s) pad[s] = 2;
    for (int y = 0; y < kNumSynths; ++y) for (int s = 0; s < 16; ++s) addNote(sc.synth[y], s, s % 8, 4, 2);
  }
  RenderOptions o; o.songMode = true;
  const RenderResult r = renderOffline(p, o);
  CHECK(finite(r.l) && finite(r.r));
  CHECK(peakOf(r.l) <= 0.9441f); CHECK(peakOf(r.r) <= 0.9441f);
  CHECK(peakOf(r.l) > 0.5f);
}

static void testAudioThreadNeverAllocates() {
  Engine e(48000);
  Project p = makeDemoProject();
  e.setSongMode(true); e.setRecording(true); e.play(0);
  std::vector<float> a(192), b(192);
  long during = 0;
  for (int k = 0; k < 1500; ++k) {               // six seconds of audio
    if (k % 40 == 0) { p.bpm += 1; p.fx[1].eqLow += 0.5f; e.setProject(p); }     // allocates, but on the control thread
    if (k % 97 == 0) e.setPadSample(k % 8, click(48000));
    g_allocs = 0; g_count = true;
    if (k % 13 == 0) e.triggerPad(k % 8, 1 + k % 2);
    if (k % 29 == 0) e.noteOn(k % 2, k % 9, 1);
    if (k % 31 == 0) e.noteOff(k % 2, k % 9);
    e.render(a.data(), b.data(), 192);
    g_count = false;
    during += g_allocs;
  }
  CHECK_EQ(during, 0);
  RecordedHit h; int rec = 0;
  while (e.pollRecorded(h)) ++rec;
  CHECK(rec > 0);
}

static void testChokeGroupCutsTheOtherPad() {
  Project p = emptyProject();
  p.pads[3].choke = 1; p.pads[4].choke = 1; p.pads[4].decay = 1.2f;
  auto tone = std::make_unique<Sample>(); tone->rate = 48000; tone->l.assign(48000, 0.5f);
  auto quiet = std::make_unique<Sample>(); quiet->rate = 48000; quiet->l.assign(64, 0.0f);
  Engine e(48000);
  e.setProject(p); e.setPadSample(4, std::move(tone)); e.setPadSample(3, std::move(quiet));
  std::vector<float> l;
  e.triggerPad(4); renderN(e, l, 4800);
  CHECK(std::fabs(l.back()) > 0.05f);            // open hat is ringing
  e.triggerPad(3); renderN(e, l, 1440);          // closed hat, then 30 ms
  CHECK(std::fabs(l.back()) < 1e-3f);            // open hat is gone
}

static void testScenesChangeOnTheBarLine() {
  Project p = emptyProject();                    // 120 BPM: one bar = 96000 frames
  Engine e(48000);
  e.setProject(p); e.play(0);
  std::vector<float> l;
  renderN(e, l, 1000);
  CHECK(e.isPlaying()); CHECK_EQ(e.currentScene(), 0);
  e.queueScene(2);
  renderN(e, l, 94999);                          // one frame before the bar line
  CHECK_EQ(e.currentScene(), 0); CHECK_EQ(e.queuedScene(), 2); CHECK_EQ(e.currentStep(), 15);
  renderN(e, l, 2);
  CHECK_EQ(e.currentScene(), 2); CHECK_EQ(e.queuedScene(), -1); CHECK_EQ(e.currentStep(), 0);
  e.stop(); renderN(e, l, 10);
  CHECK(!e.isPlaying()); CHECK_EQ(e.currentStep(), -1);
}

static void testSongModeAndLongPatterns() {
  Project p = emptyProject();
  p.scenes[0].bars = 2; p.scenes[1].bars = 8;
  p.scenes[0].drums.steps[0][31] = 2;            // last step of scene A
  p.scenes[1].drums.steps[0][127] = 2;           // last step of the 8-bar scene B
  p.chain[0] = 0; p.chain[1] = 1; p.chainLen = 2;
  auto c = click(48000);
  RenderOptions o; o.songMode = true; o.samples[0] = c.get(); o.tailSeconds = 0.1;
  const std::vector<long> got = clicks(renderOffline(p, o).l);
  CHECK_EQ(got.size(), size_t(2));
  if (got.size() == 2) { CHECK_EQ(got[0], 31L * 6000); CHECK_EQ(got[1], (32L + 127) * 6000); }
}

static void testLiveRecordingSnapsToTheGrid() {
  Project p = emptyProject();
  Engine e(48000);
  auto quiet = std::make_unique<Sample>(); quiet->rate = 48000; quiet->l.assign(64, 0.0f);
  e.setPadSample(5, std::move(quiet)); e.setPadSample(6, click(48000)); e.setProject(p); e.setRecording(true); e.play(0);
  std::vector<float> l;
  renderN(e, l, 13000, 100);                     // step 2 fired at 12000, step 3 is due at 18000
  e.triggerPad(5);
  renderN(e, l, 4000, 100);                      // now at 17000
  e.triggerPad(6);
  renderN(e, l, 100, 100);
  RecordedHit h;
  CHECK(e.pollRecorded(h)); CHECK(h.track == 0 && h.row == 5 && h.step == 2 && h.scene == 0);
  CHECK(e.pollRecorded(h)); CHECK(h.track == 0 && h.row == 6 && h.step == 3);
  CHECK(!e.pollRecorded(h));
  p.scenes[0].drums.steps[6][3] = 1;             // the UI writes the recorded note into the project
  e.setProject(p);
  renderN(e, l, 96000 + 18000 + 100 - long(l.size()), 100);
  const std::vector<long> got = clicks(l);
  CHECK_EQ(got.size(), size_t(2));               // the live hit, then the next time round. Not doubled at 18000.
  if (got.size() == 2) { CHECK_EQ(got[0], 17000); CHECK_EQ(got[1], 96000 + 18000); }
}

static void testHeldNotesAndStealing() {
  Engine e(48000);
  e.setProject(emptyProject());
  std::vector<float> l;
  e.noteOn(1, 0); renderN(e, l, 24000);
  for (int d = 0; d < 20; ++d) e.noteOn(0, d);   // more notes than voices: must steal, not crash
  renderN(e, l, 4800);
  CHECK(finite(l) && peakOf(l) > 0.01f);
  e.noteOff(1, 0); for (int d = 0; d < 20; ++d) e.noteOff(0, d);
  renderN(e, l, 48000 * 4);
  CHECK(std::fabs(l.back()) < 1e-4f);            // everything released and faded
}

struct MidiFile { int format = 0, ntrks = 0, tpq = 0; std::vector<std::vector<std::vector<int>>> tracks; bool ok = true; };
static MidiFile parseMidi(const std::vector<uint8_t>& b) {
  MidiFile m; size_t o = 0;
  const auto u32 = [&] { uint32_t v = (uint32_t(b[o]) << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]; o += 4; return v; };
  const auto u16 = [&] { int v = (b[o] << 8) | b[o + 1]; o += 2; return v; };
  if (std::memcmp(&b[o], "MThd", 4) != 0) { m.ok = false; return m; }
  o += 4; u32(); m.format = u16(); m.ntrks = u16(); m.tpq = u16();
  for (int t = 0; t < m.ntrks; ++t) {
    if (std::memcmp(&b[o], "MTrk", 4) != 0) { m.ok = false; return m; }
    o += 4; const uint32_t len = u32(); const size_t end = o + len; int tick = 0;
    std::vector<std::vector<int>> evs;
    while (o < end) {
      int d = 0; uint8_t c; do { c = b[o++]; d = (d << 7) | (c & 0x7f); } while (c & 0x80);
      tick += d;
      const int s = b[o++];
      if (s == 0xFF) { const int type = b[o++], n = b[o++]; evs.push_back({tick, 0xFF, type, n}); o += size_t(n); }
      else { evs.push_back({tick, s, b[o], b[o + 1]}); o += 2; }
    }
    if (o != end) m.ok = false;
    m.tracks.push_back(evs);
  }
  if (o != b.size()) m.ok = false;
  return m;
}

static void testMidiExport() {
  Project p = makeDemoProject(); p.bpm = 120;
  MidiFile m = parseMidi(buildMidi(p, false, 1));
  CHECK(m.ok); CHECK_EQ(m.format, 1); CHECK_EQ(m.ntrks, 4); CHECK_EQ(m.tpq, 480);
  int kicks = 0, others = 0; std::vector<int> snares;
  for (auto& e : m.tracks[1]) if ((e[1] & 0xF0) == 0x90) {
    CHECK_EQ(e[1] & 0x0F, 9);
    if (e[2] == 36) ++kicks; else if (e[2] == 38) snares.push_back(e[0]); else if (e[2] != 42) ++others;
  }
  CHECK_EQ(kicks, 5); CHECK_EQ(others, 0); CHECK(snares.size() == 2 && snares[0] == 480 && snares[1] == 1440);
  int bassOn = 0, bassOff = 0, firstOn = -1, firstOff = -1;
  for (auto& e : m.tracks[2]) {
    if ((e[1] & 0xF0) == 0x90) { ++bassOn; if (firstOn < 0) firstOn = e[0]; }
    if ((e[1] & 0xF0) == 0x80) { ++bassOff; if (firstOff < 0) firstOff = e[0]; }
  }
  CHECK_EQ(bassOn, 4); CHECK_EQ(bassOff, 4); CHECK_EQ(firstOff - firstOn, 2 * 120 - 12);   // a note two steps long
  int lead = 0; for (auto& e : m.tracks[3]) if ((e[1] & 0xF0) == 0x90) { ++lead; CHECK_EQ(e[1] & 0x0F, 1); }
  CHECK_EQ(lead, 6);

  p.synths[1].chord = true; p.swing = 0.5f;
  m = parseMidi(buildMidi(p, false, 1));
  lead = 0; int swung = -1;
  for (auto& e : m.tracks[3]) if ((e[1] & 0xF0) == 0x90) { ++lead; if (swung < 0 && e[0] > 0) swung = e[0]; }
  CHECK_EQ(lead, 18); CHECK_EQ(swung, 3 * 120 + 60);   // step 4 of the bar is an off-beat sixteenth: pushed by half a step

  p.scenes[3].bars = 8;
  m = parseMidi(buildMidi(p, true, 0));
  int lastTick = 0; for (auto& e : m.tracks[1]) lastTick = std::max(lastTick, e[0]);
  CHECK(m.ok); CHECK(lastTick > 3 * 1920);       // the song export runs through all chained scenes
}

static void testWavRoundTripAndStems() {
  RenderOptions o; o.songMode = true;
  const Project p = makeDemoProject();
  const RenderResult master = renderOffline(p, o);
  const std::string path = "/tmp/ld_test_roundtrip.wav";
  CHECK(writeWav16(path.c_str(), master.l.data(), master.r.data(), master.l.size(), master.sampleRate));
  const std::unique_ptr<Sample> back = loadWav(path.c_str());
  CHECK(back != nullptr);
  if (back) {
    CHECK_EQ(back->frames(), master.l.size()); CHECK_EQ(back->r.size(), master.r.size()); CHECK(back->rate == 48000.0);
    float worst = 0;
    for (size_t i = 0; i < back->frames() && i < master.l.size(); ++i) worst = std::fmax(worst, std::fabs(back->l[i] - master.l[i]));
    CHECK(worst < 2.0f / 32768.0f);              // 16-bit rounding
  }
  CHECK(loadWav("/tmp/ld_no_such_file.wav") == nullptr);
  std::remove(path.c_str());
  std::vector<float> prev;
  for (int t = 0; t < 1 + p.activeSynths; ++t) { // one stem per active track, each with sound, each different
    o.soloTrack = t;
    const RenderResult stem = renderOffline(p, o);
    CHECK(peakOf(stem.l) > 0.02f); CHECK(!same(stem.l, master.l)); CHECK(!same(stem.l, prev));
    prev = stem.l;
  }
}

// The real situation on the phone: the UI thread edits and plays while the audio thread renders.
static void testTwoThreads() {
  Engine e(48000);
  std::atomic<bool> done{false};
  std::thread audio([&] {
    std::vector<float> a(192), b(192);
    bool ok = true;
    for (int k = 0; k < 4000; ++k) { e.render(a.data(), b.data(), 192); for (float v : a) if (!std::isfinite(v)) ok = false; }
    done = ok;
  });
  Project p = makeDemoProject();
  e.setSongMode(true); e.setRecording(true); e.play(0);
  long sent = 0; int seen = 0; RecordedHit h;
  while (!audio.joinable() || sent < 3000) {
    p.bpm = 100.0f + float(sent % 60); p.swing = float(sent % 5) * 0.1f; p.fx[sent % 3].level = 0.5f + 0.1f * float(sent % 4);
    p.scenes[sent % 4].drums.steps[sent % 8][sent % 16] ^= 1;
    e.setProject(p);
    e.triggerPad(int(sent % 8)); e.noteOn(int(sent % 2), int(sent % 7)); e.noteOff(int(sent % 2), int((sent + 3) % 7));
    if (sent % 50 == 0) e.setPadSample(int(sent % 8), sent % 100 == 0 ? click(48000) : nullptr);
    if (sent % 300 == 0) e.queueScene(int(sent / 300) % 4);
    while (e.pollRecorded(h)) ++seen;
    (void)e.currentStep(); (void)e.currentScene(); (void)e.peak(0); (void)e.isPlaying();
    ++sent;
    if (sent % 8 == 0) std::this_thread::yield();
  }
  audio.join();
  e.collectGarbage();
  CHECK(done.load()); CHECK(seen > 0);
}

static void testSessionUndoSaveLoad() {
  Session s(48000);
  const float bpm0 = s.getParam(P_BPM, 0);
  CHECK(!s.canUndo());
  CHECK_EQ(s.toggleDrumStep(0, 5, 3, false), 1); CHECK_EQ(s.toggleDrumStep(0, 5, 3, true), 2);
  CHECK_EQ(s.toggleDrumStep(0, 5, 3, true), 1);  CHECK_EQ(s.toggleDrumStep(0, 5, 3, false), 0);
  CHECK(s.toggleNote(1, 0, 7, 3, false)); CHECK(!s.toggleNote(1, 0, 7, 3, false));
  s.beginEdit(); s.setParam(P_BPM, 0, 140); s.setParam(P_BPM, 0, 999);       // one gesture, clamped
  CHECK(s.getParam(P_BPM, 0) == 240.0f);
  s.beginEdit(); s.setParam(P_SYN_CHORD, 1, 1); s.setParam(P_SCALE, 0, 1); s.setParam(P_SYN_OCTAVE, 0, -2); s.setParam(P_FX_MUTE, 2, 1);
  CHECK(s.project().synths[1].chord && s.project().scale == Scale::Major && s.project().synths[0].octaveShift == -2 && s.project().fx[2].mute);
  CHECK(s.getParam(P_SYN_CHORD, 1) == 1.0f && s.getParam(P_SYN_OCTAVE, 0) == -2.0f);
  s.setSceneBars(2, 8); s.setSceneBars(3, 99); CHECK_EQ(s.project().scenes[2].bars, 8); CHECK_EQ(s.project().scenes[3].bars, 8);
  s.copyScene(1, 3); CHECK_EQ(s.project().scenes[3].drums.steps[1][4], 1);
  s.chainAdd(2); CHECK_EQ(s.project().chainLen, 5); s.chainRemove(0); CHECK_EQ(s.project().chain[0], 1);
  s.clearScene(1); CHECK_EQ(s.project().scenes[1].synth[1].count, 0);

  const std::string path = "/tmp/ld_test_project.ldp";
  CHECK(s.dirty()); CHECK(s.save(path.c_str())); CHECK(!s.dirty());
  Session t(44100);
  CHECK(t.load(path.c_str()));
  CHECK(t.getParam(P_BPM, 0) == 240.0f); CHECK_EQ(t.project().scenes[2].bars, 8); CHECK_EQ(t.project().chainLen, 4);
  CHECK(!t.load("/tmp/ld_no_such_project.ldp"));
  { FILE* f = std::fopen(path.c_str(), "wb"); std::fputs("not a project file", f); std::fclose(f); }
  CHECK(!t.load(path.c_str())); CHECK(t.getParam(P_BPM, 0) == 240.0f);       // a broken file leaves the project alone
  std::remove(path.c_str());

  int undos = 0; while (s.undo()) ++undos;
  CHECK(undos >= 12); CHECK(s.getParam(P_BPM, 0) == bpm0); CHECK(!s.project().synths[1].chord);
  CHECK_EQ(s.project().scenes[0].drums.steps[5][3], 0); CHECK_EQ(s.project().chainLen, 4);
  CHECK(s.redo()); CHECK_EQ(s.project().scenes[0].drums.steps[5][3], 1);
  s.resetToDemo(); CHECK(!s.canRedo());
}

static void testSessionRecordsLiveHits() {
  Session s(48000);
  s.beginEdit(); s.setParam(P_BPM, 0, 120); s.clearScene(0);
  Engine& e = s.engine();
  e.setRecording(true); e.play(0);
  std::vector<float> l;
  renderN(e, l, 13000, 100);
  e.triggerPad(2); e.noteOn(1, 4);
  renderN(e, l, 200, 100);
  CHECK(s.poll());
  CHECK_EQ(s.project().scenes[0].drums.steps[2][2], 1);
  CHECK_EQ(s.project().scenes[0].synth[1].count, 1); CHECK_EQ(s.project().scenes[0].synth[1].notes[0].step, 2);
  CHECK(!s.poll());
  CHECK(s.undo()); CHECK_EQ(s.project().scenes[0].drums.steps[2][2], 0);     // one undo removes the recorded take
}

static std::vector<float> padHit2(Session& s, int pad) {
  std::vector<float> l;
  s.engine().triggerPad(pad, 2);
  renderN(s.engine(), l, 9600);
  return l;
}

// A save file written by build 5 (file version 1) must still open, with every setting intact.
static void testOldSaveFilesAreConverted() {
  Session s(48000);
  CHECK(s.load(LD_FIXTURES "/project_v1.ldp"));
  const Project& p = s.project();
  CHECK(p.bpm == 97.0f); CHECK(p.swing == 0.25f); CHECK_EQ(p.root, 9); CHECK(p.scale == Scale::Dorian);
  CHECK(p.pads[6].pitch == -5.0f); CHECK_EQ(p.pads[2].choke, 2); CHECK_EQ(p.pads[4].gmNote, 46);
  CHECK(p.synths[0].cutoff == 432.0f); CHECK(p.synths[1].chord); CHECK_EQ(p.synths[1].octaveShift, 1);
  CHECK(p.synths[0].lfoTarget == LfoTarget::Pitch); CHECK(p.synths[0].wave2 == Wave::Square);
  CHECK(std::fabs(p.fx[2].revSend - 0.61f) < 1e-6f); CHECK(p.fx[1].mute); CHECK(!p.fx[0].mute);
  CHECK(std::fabs(p.master - 0.66f) < 1e-6f); CHECK(p.delayFeedback == 0.5f);
  CHECK_EQ(p.scenes[2].bars, 5); CHECK_EQ(p.scenes[2].drums.steps[7][70], 2); CHECK_EQ(p.scenes[1].drums.steps[1][4], 1);
  bool found = false;
  for (int i = 0; i < p.scenes[3].synth[0].count; ++i) {
    const Note& n = p.scenes[3].synth[0].notes[i];
    if (n.step == 13 && n.degree == -3 && n.len == 4 && n.vel == 2) found = true;
  }
  CHECK(found);
  CHECK_EQ(p.chainLen, 6); CHECK_EQ(p.chain[4], 2); CHECK_EQ(p.chain[5], 0);
  // things that did not exist yet start at their defaults
  CHECK(p.synths[0].mode == SynthMode::Subtractive); CHECK(p.pads[3].sliceEnd == 1.0f); CHECK(!p.pads[3].reverse);
  CHECK_EQ(int(p.drumEngine), 0); CHECK_EQ(p.chainAuto[0].tracks[2][0].paramId, -1); CHECK_EQ(p.activeSynths, 2);
  CHECK(s.dirty());                                // so it is written back in the current format
  const std::string path = "/tmp/ld_test_converted.ldp";
  CHECK(s.save(path.c_str()));
  Session t(48000);
  CHECK(t.load(path.c_str())); CHECK(!t.dirty()); CHECK(t.project().bpm == 97.0f); CHECK_EQ(t.project().chainLen, 6);
  std::remove(path.c_str());
}

// A project saved by v0.3 (build 7, file version 4) must open with its jungle kit, FM sound and automation.
static void testV03SaveFilesAreConverted() {
  Session s(48000);
  CHECK(s.load(LD_FIXTURES "/project_v4.ldp"));
  const Project& p = s.project();
  CHECK(p.bpm == 151.0f); CHECK(p.swing == 0.125f); CHECK_EQ(p.root, 4); CHECK(p.scale == Scale::Pentatonic);
  CHECK_EQ(int(p.drumEngine), 4); CHECK_EQ(p.pads[2].choke, 1);
  CHECK(std::fabs(p.pads[5].sliceStart - 0.3f) < 1e-6f); CHECK(std::fabs(p.pads[5].sliceEnd - 0.7f) < 1e-6f); CHECK(p.pads[5].reverse);
  CHECK(std::fabs(p.pads[3].sliceStart - 0.375f) < 1e-6f); CHECK(!p.pads[3].reverse);
  CHECK(p.synths[1].mode == SynthMode::Fm); CHECK(p.synths[1].fmRatio == 3.5f); CHECK(p.synths[1].fmIndex == 6.0f);
  CHECK(p.synths[0].cutoff == 777.0f); CHECK(std::fabs(p.fx[1].delaySend - 0.44f) < 1e-6f); CHECK(p.fx[2].mute);
  CHECK_EQ(p.scenes[1].bars, 3); CHECK_EQ(p.scenes[1].drums.steps[2][40], 5);
  bool lead = false, bass = false;
  for (int i = 0; i < p.scenes[2].synth[1].count; ++i) { const Note& n = p.scenes[2].synth[1].notes[i]; if (n.step == 9 && n.degree == 12 && n.len == 3 && n.vel == 2) lead = true; }
  for (int i = 0; i < p.scenes[0].synth[0].count; ++i) { const Note& n = p.scenes[0].synth[0].notes[i]; if (n.step == 3 && n.degree == -7 && n.len == 2) bass = true; }
  CHECK(lead); CHECK(bass);
  CHECK_EQ(p.chainLen, 5); CHECK_EQ(p.chain[4], 3);
  // the four fixed automation targets of v0.3 arrive as free-choice lanes on the right tracks
  CHECK_EQ(p.chainAuto[0].tracks[1][0].paramId, P_SYN_CUTOFF); CHECK(p.chainAuto[0].tracks[1][0].startVal == 200.0f && p.chainAuto[0].tracks[1][0].endVal == 5000.0f);
  CHECK_EQ(p.chainAuto[0].tracks[0][0].paramId, P_FX_DRIVE);   CHECK(std::fabs(p.chainAuto[0].tracks[0][0].endVal - 0.9f) < 1e-6f);
  CHECK_EQ(p.chainAuto[2].tracks[2][0].paramId, P_SYN_CUTOFF); CHECK(p.chainAuto[2].tracks[2][0].startVal == 4000.0f);
  CHECK_EQ(p.chainAuto[4].tracks[2][0].paramId, P_FX_DELAY);   CHECK(std::fabs(p.chainAuto[4].tracks[2][0].endVal - 0.6f) < 1e-6f);
  CHECK_EQ(p.chainAuto[1].tracks[2][0].paramId, -1);
  // new things start at their neutral defaults
  CHECK_EQ(p.activeSynths, 2); CHECK(p.synths[0].feelWarmth == 0.5f); CHECK(p.fx[0].slots[0].type == FxType::Off);
  CHECK(s.dirty());
  CHECK(peakOf(padHit2(s, 1)) > 0.02f);            // and the jungle kit is really loaded
}

static std::vector<uint8_t> fileBytes(const std::string& path) {
  std::vector<uint8_t> out;
  if (FILE* f = std::fopen(path.c_str(), "rb")) { int c; while ((c = std::fgetc(f)) != EOF) out.push_back(uint8_t(c)); std::fclose(f); }
  return out;
}

// The current file format: everything survives, and files from a future version with extra or changed
// fields still open.
static void testProjectFileFormat() {
  Session a(48000);
  a.setActiveSynths(4); a.loadJungleKit(); a.applyInstrument(2, int(InstrumentPreset::SuperPad));
  a.setFxSlot(3, 1, int(FxType::Phaser), 0.33f, 0.77f, true); a.setFxSlot(0, 0, int(FxType::Bitcrush), 0.9f, 0.1f, false);
  a.setChainAuto(3, 4, 2, P_SYN_FM_INDEX, 0.5f, 7.0f); a.setChainAuto(0, 0, 1, P_FX_REVERB, 0.0f, 0.5f);
  a.toggleNote(3, 3, 127, 35, true, 8); a.toggleNote(1, 2, 0, -21, false, 1);
  a.setSceneBars(3, 8); a.setDrumStep(3, 7, 127, 4); a.chainAdd(2);
  a.beginEdit(); a.setParam(P_SYN_WARMTH, 3, 0.91f); a.setParam(P_PAD_REVERSE, 6, 1); a.setParam(P_FX_EQ_LOW, 4, -7); a.setParam(P_SWING, 0, 0.33f);
  const std::string f1 = "/tmp/ld_test_fmt1.ldp", f2 = "/tmp/ld_test_fmt2.ldp";
  CHECK(a.save(f1.c_str()));
  Session b(44100);
  CHECK(b.load(f1.c_str())); CHECK(!b.dirty());
  const Project& p = b.project();
  CHECK_EQ(p.activeSynths, 4); CHECK_EQ(int(p.drumEngine), 4); CHECK(p.synths[2].instrument == InstrumentPreset::SuperPad);
  CHECK(p.fx[3].slots[1].type == FxType::Phaser && p.fx[3].slots[1].bypass && std::fabs(p.fx[3].slots[1].rate - 0.77f) < 1e-6f);
  CHECK_EQ(p.chainAuto[3].tracks[4][2].paramId, P_SYN_FM_INDEX); CHECK(p.chainAuto[3].tracks[4][2].endVal == 7.0f);
  CHECK_EQ(p.scenes[3].synth[3].count, 1); CHECK_EQ(p.scenes[3].synth[3].notes[0].degree, 35); CHECK_EQ(p.scenes[3].synth[3].notes[0].len, 8);
  CHECK_EQ(p.scenes[1].synth[2].notes[0].degree, -21); CHECK_EQ(p.scenes[3].drums.steps[7][127], 4);
  CHECK(std::fabs(p.synths[3].feelWarmth - 0.91f) < 1e-6f); CHECK(p.pads[6].reverse); CHECK(p.fx[4].eqLow == -7.0f);
  CHECK(b.save(f2.c_str()));
  const std::vector<uint8_t> x = fileBytes(f1), y = fileBytes(f2);
  CHECK(x.size() > 4000 && x == y);                // saving what was loaded gives the same file, byte for byte

  // a "newer" file: an unknown record, and a known field stored with a different size
  std::vector<uint8_t> z(x.begin(), x.end() - 9);  // without the closing "end" record, which goes back on below
  const auto rec = [&](const std::string& key, const std::vector<uint8_t>& data) {
    z.push_back(uint8_t(key.size())); z.push_back(0); z.insert(z.end(), key.begin(), key.end());
    const uint32_t n = uint32_t(data.size()); for (int i = 0; i < 4; ++i) z.push_back(uint8_t(n >> (8 * i)));
    z.insert(z.end(), data.begin(), data.end());
  };
  rec("future.thing", {1, 2, 3, 4, 5, 6, 7}); rec("bpm", {0, 0, 0, 0, 0, 0, 0, 0}); rec("pad0.vol", {0, 0, 0, 0}); rec("end", {});
  { FILE* f = std::fopen(f2.c_str(), "wb"); std::fwrite(z.data(), 1, z.size(), f); std::fclose(f); }
  Session c(48000);
  CHECK(c.load(f2.c_str())); CHECK(c.project().bpm == 168.0f); CHECK(c.project().pads[0].vol == 0.0f); CHECK_EQ(c.project().activeSynths, 4);
  // a file cut off is refused and changes nothing, also when the cut falls between two records
  { FILE* f = std::fopen(f2.c_str(), "wb"); std::fwrite(x.data(), 1, x.size() - 9, f); std::fclose(f); }
  CHECK(!c.load(f2.c_str()));
  { FILE* f = std::fopen(f2.c_str(), "wb"); std::fwrite(x.data(), 1, x.size() / 2, f); std::fclose(f); }
  CHECK(!c.load(f2.c_str())); CHECK_EQ(c.project().activeSynths, 4);
  std::remove(f1.c_str()); std::remove(f2.c_str());
}

static std::vector<float> padHit(Session& s, int pad) {
  std::vector<float> l;
  s.engine().triggerPad(pad, 2);
  renderN(s.engine(), l, 9600);
  return l;
}

// The jungle kit belongs to the project: it has to come back after a restart and go away on undo.
static void testKitFollowsTheProject() {
  Session std0(48000);
  const std::vector<float> stdSound = padHit(std0, 1);
  Session a(48000);
  a.loadJungleKit();
  CHECK_EQ(int(a.project().drumEngine), 4); CHECK(a.getParam(P_DRUM_ENGINE, 0) == 4.0f);
  const std::vector<float> jungle = padHit(a, 1);
  CHECK(peakOf(jungle) > 0.02f); CHECK(!same(jungle, stdSound));
  const std::string path = "/tmp/ld_test_kit.ldp";
  CHECK(a.save(path.c_str()));
  Session b(48000);                                // "restart the app"
  CHECK(b.load(path.c_str())); CHECK_EQ(int(b.project().drumEngine), 4);
  CHECK(same(padHit(b, 1), jungle));               // same slices, not the standard drums cut into pieces
  std::remove(path.c_str());
  a.setParam(P_DRUM_ENGINE, 0, 0); CHECK_EQ(int(a.project().drumEngine), 4);          // read-only through setParam
  CHECK(a.undo()); CHECK_EQ(int(a.project().drumEngine), 0); CHECK(a.project().bpm == 122.0f);
  std::vector<float> l; renderN(a.engine(), l, 48000 * 12);       // let delay and reverb of the old hit die away
  const std::vector<float> afterUndo = padHit(a, 1);
  float worst = 0; for (size_t i = 0; i < afterUndo.size(); ++i) worst = std::fmax(worst, std::fabs(afterUndo[i] - stdSound[i]));
  CHECK(worst < 1e-3f);                            // the standard snare again
  CHECK(a.redo()); CHECK_EQ(int(a.project().drumEngine), 4);
  a.resetToDemo(); CHECK_EQ(int(a.project().drumEngine), 0);

  float w0[64], w1[64];
  Session d(48000);
  CHECK_EQ(d.padWaveform(0, w0, 64), 64); CHECK(w0[0] > 0.3f && w0[63] < 0.1f);   // a kick: loud start, quiet end
  d.loadJungleKit();
  CHECK_EQ(d.padWaveform(0, w1, 64), 64); CHECK(w1[16] > 0.3f);                    // the break has a snare a quarter in
  CHECK_EQ(d.padWaveform(9, w1, 64), 0);

  Session c(48000);
  c.loadJungleKit(); c.setSceneBars(1, 2); c.writeBreakPattern(1);
  int on = 0; for (int pad = 0; pad < kNumPads; ++pad) for (int st = 0; st < 32; ++st) if (c.project().scenes[1].drums.steps[pad][st]) ++on;
  CHECK_EQ(on, 16); CHECK_EQ(c.project().scenes[1].drums.steps[3][6], 1); CHECK_EQ(c.project().scenes[1].drums.steps[4][16 + 8], 2);
  RenderOptions o; o.scene = 1;
  CHECK(peakOf(renderOffline(c.project(), o).l) > 0.1f);          // the export plays the jungle kit too
}

static std::vector<float> renderAutomated(const Project& p, const std::function<int(int)>& blockAt, long total) {
  Engine e(48000);
  e.setProject(p); e.setSongMode(true); e.play(0);
  std::vector<float> out, a(1024), b(1024);
  long pos = 0; int k = 0;
  while (pos < total) {
    int n = blockAt(k++); if (n > 1024) n = 1024; if (n > total - pos) n = int(total - pos);
    e.render(a.data(), b.data(), n);
    out.insert(out.end(), a.begin(), a.begin() + n);
    pos += n;
  }
  return out;
}

static void testAutomationTiming() {
  Project p = makeDemoProject();
  p.bpm = 120;                                     // one bar = 96000 frames
  p.chain[0] = 1; p.chain[1] = 1; p.chainLen = 2;
  p.chainAuto[0].tracks[2][0] = AutoLane{int16_t(P_SYN_CUTOFF), 300.0f, 6000.0f};
  p.chainAuto[0].tracks[2][1] = AutoLane{int16_t(P_SYN_RES), 1.0f, 9.0f};
  p.chainAuto[0].tracks[0][0] = AutoLane{int16_t(P_FX_DRIVE), 0.0f, 0.9f};
  p.chainAuto[1].tracks[2][0] = AutoLane{int16_t(P_FX_DELAY), 0.6f, 0.0f};
  p.chainAuto[1].tracks[1][2] = AutoLane{int16_t(P_SYN_WARMTH), 0.1f, 0.9f};
  p.fx[2].slots[0] = FxSlot{FxType::Chorus, 0.7f, 0.5f, false}; p.fx[2].slots[1] = FxSlot{FxType::Phaser, 0.6f, 0.4f, false};
  p.fx[0].slots[0] = FxSlot{FxType::Bitcrush, 0.5f, 0.5f, false}; p.fx[1].slots[2] = FxSlot{FxType::Compressor, 0.6f, 0.0f, false};
  const long total = 96000 * 2 + 5000;
  const std::vector<float> a = renderAutomated(p, [](int) { return 64; }, total);
  const std::vector<float> b = renderAutomated(p, [](int) { return 441; }, total);
  uint32_t seed = 3;
  const std::vector<float> c = renderAutomated(p, [&](int) { seed = seed * 1664525u + 1013904223u; return 1 + int((seed >> 16) % 900); }, total);
  CHECK(same(a, b)); CHECK(same(a, c));            // sweeps do not depend on the audio block size
  Project plain = p; plain.chainAuto[0] = ChainAuto{}; plain.chainAuto[1] = ChainAuto{};
  CHECK(!same(a, renderAutomated(plain, [](int) { return 64; }, total)));

  // A slot's automation must hold until the slot really ends. Slot 1 holds the lead filter at 200 Hz, slot 2 has none.
  Project q = makeDemoProject(); q.bpm = 120; q.chain[0] = 1; q.chain[1] = 1; q.chainLen = 2;
  q.chainAuto[0].tracks[2][0] = AutoLane{int16_t(P_SYN_CUTOFF), 200.0f, 200.0f};
  Project fixed = q; fixed.chainAuto[0] = ChainAuto{}; fixed.synths[1].cutoff = 200.0f; fixed.synths[1].lfoDepth = q.synths[1].lfoDepth;
  const std::vector<float> withAuto = renderAutomated(q, [](int) { return 256; }, 96000 + 20000);
  const std::vector<float> staticCut = renderAutomated(fixed, [](int) { return 256; }, 96000 + 20000);
  bool sameInSlot = true; for (long i = 0; i < 96000; ++i) if (withAuto[size_t(i)] != staticCut[size_t(i)]) { sameInSlot = false; break; }
  CHECK(sameInSlot);                               // identical right up to the last sample of slot 1
  bool differsAfter = false; for (long i = 96000; i < 96000 + 20000; ++i) if (withAuto[size_t(i)] != staticCut[size_t(i)]) { differsAfter = true; break; }
  CHECK(differsAfter);                             // then slot 2 goes back to the project's own cutoff
}

static void testRollsInMidiAndOverLongRuns() {
  Project p = emptyProject();
  p.scenes[0].drums.steps[0][0] = 5; p.scenes[0].drums.steps[0][8] = 4; p.scenes[0].drums.steps[1][4] = 3;
  const MidiFile m = parseMidi(buildMidi(p, false, 0));
  std::vector<int> kick, snare;
  for (auto& e : m.tracks[1]) if ((e[1] & 0xF0) == 0x90) { (e[2] == 36 ? kick : snare).push_back(e[0]); CHECK_EQ(e[3], 127); }
  CHECK(m.ok);
  CHECK(kick == std::vector<int>({0, 30, 60, 90, 960, 1000, 1040})); CHECK(snare == std::vector<int>({480, 540}));
  // 3x rolls at 122 BPM (no whole number of frames per hit): every hit stays within one frame of its ideal position
  p.bpm = 122; p.scenes[0] = Scene{};
  for (int st = 0; st < 16; ++st) p.scenes[0].drums.steps[0][st] = 4;
  Engine e(48000);
  e.setPadSample(0, click(48000)); e.setProject(p); e.play(0);
  std::vector<float> l;
  const double sl = 48000.0 * 60.0 / 122.0 / 4.0;
  renderN(e, l, long(sl * 16 * 4) + 5, 500);
  const std::vector<long> got = clicks(l);
  CHECK_EQ(got.size(), size_t(16 * 4 * 3 + 1));
  long worst = 0;
  for (size_t k = 0; k < got.size(); ++k) worst = std::max(worst, std::labs(got[k] - std::lround(double(k) * sl / 3.0)));
  CHECK(worst <= 1);
}

static void testPatternEditing() {
  SynthPattern pat;
  CHECK(addNote(pat, 4, 2, 3, 2)); CHECK(addNote(pat, 4, 2, 1, 1)); CHECK_EQ(pat.count, 1); CHECK_EQ(pat.notes[0].len, 1);
  CHECK(!addNote(pat, kMaxSteps, 0)); CHECK(!removeNote(pat, 5, 2)); CHECK(removeNote(pat, 4, 2)); CHECK_EQ(pat.count, 0);
  for (int i = 0; i < kMaxNotes; ++i) CHECK(addNote(pat, i % kMaxSteps, i / kMaxSteps));
  CHECK(!addNote(pat, 0, 99));
  Scene s; s.bars = 99; CHECK_EQ(sceneSteps(s), kMaxSteps); s.bars = 0; CHECK_EQ(sceneSteps(s), 16);
}


static void testFmAndOctaveIsolation() {
  ld::Session ses(48000.0);
  ses.setParam(ld::P_SYN_MODE, 1, 1.0f);
  ses.setParam(ld::P_SYN_FM_RATIO, 1, 3.0f);
  ses.setParam(ld::P_SYN_FM_INDEX, 1, 4.0f);
  ses.setParam(ld::P_SYN_FM_DECAY, 1, 0.2f);
  ses.setParam(ld::P_SYN_FM_FB, 1, 0.3f);
  CHECK(ses.getParam(ld::P_SYN_MODE, 1) > 0.5f);
  // Per-sequence octave isolation: adding a +1 octave note (degree +7) in scene 1 leaves scene 0 untouched
  const int origCount0 = ses.project().scenes[0].synth[1].count;
  CHECK(ses.toggleNote(1, 1, 0, 7, false));
  CHECK(ses.project().scenes[0].synth[1].count == origCount0);
  // Verify FM synth renders non-silent, bounded stereo audio
  ses.engine().noteOn(1, 7, 2);
  float l[512] = {}, r[512] = {};
  ses.engine().render(l, r, 512);
  float peak = 0.0f;
  for (int i = 0; i < 512; ++i) peak = std::fmax(peak, std::fabs(l[i]));
  CHECK(peak > 0.01f && peak <= 1.0f);
  // Save & load roundtrip preserves FM settings
  const char* pth = "/tmp/ld_fm_test.ldp";
  CHECK(ses.save(pth));
  ld::Session ses2(48000.0);
  CHECK(ses2.load(pth));
  CHECK(ses2.getParam(ld::P_SYN_MODE, 1) > 0.5f);
  CHECK(std::fabs(ses2.getParam(ld::P_SYN_FM_RATIO, 1) - 3.0f) < 0.01f);
}


static void testSongAutomation() {
  ld::Session ses(48000.0);
  // Free-choice automation: up to 3 lanes per track!
  ses.setChainAuto(0, 2, 0, ld::P_SYN_CUTOFF, 180.0f, 6500.0f);
  ses.setChainAuto(0, 2, 1, ld::P_SYN_RES, 1.0f, 10.0f);
  ses.setChainAuto(0, 2, 2, ld::P_FX_DELAY, 0.0f, 0.7f);
  ses.setChainAuto(0, 0, 0, ld::P_FX_DRIVE, 0.0f, 0.9f);
  CHECK_EQ(ses.project().chainAuto[0].tracks[2][0].paramId, ld::P_SYN_CUTOFF);
  CHECK_EQ(ses.project().chainAuto[0].tracks[2][2].paramId, ld::P_FX_DELAY);
  ld::RenderOptions o; o.songMode = true; o.tailSeconds = 0.2;
  const ld::RenderResult withAuto = ld::renderOffline(ses.project(), o);
  ld::Project plain = ses.project();
  plain.chainAuto[0] = ld::ChainAuto{};
  const ld::RenderResult noAuto = ld::renderOffline(plain, o);
  CHECK(finite(withAuto.l) && peakOf(withAuto.l) > 0.1f && peakOf(withAuto.l) <= 0.9441f);
  CHECK(!same(withAuto.l, noAuto.l));
  // Save & load v4 preserves free-choice automation
  const char* pth = "/tmp/ld_auto_test.ldp";
  CHECK(ses.save(pth));
  ld::Session ses2(48000.0);
  CHECK(ses2.load(pth));
  CHECK_EQ(ses2.project().chainAuto[0].tracks[2][0].paramId, ld::P_SYN_CUTOFF);
  CHECK(std::fabs(ses2.project().chainAuto[0].tracks[2][0].endVal - 6500.0f) < 0.1f);
  std::remove(pth);
}

static void testFourTracksInstrumentsAndFxChain() {
  ld::Session ses(48000.0);
  ses.setActiveSynths(4);
  CHECK_EQ(ses.project().activeSynths, 4);
  // Apply Instrument presets + Feel shaping on tracks 3 & 4
  ses.applyInstrument(2, int(ld::InstrumentPreset::ReeseDnB));
  ses.applyInstrument(3, int(ld::InstrumentPreset::FmBell));
  ses.setParam(ld::P_SYN_WARMTH, 2, 0.85f);
  ses.setParam(ld::P_SYN_MOTION, 2, 0.80f);
  CHECK(ses.project().synths[2].instrument == ld::InstrumentPreset::ReeseDnB);
  CHECK(ses.project().synths[3].mode == ld::SynthMode::Fm);
  // Build a 3-slot Vital-style FX chain on Lead track (track 2)
  ses.setFxSlot(2, 0, int(ld::FxType::Chorus), 0.7f, 0.5f, false);
  ses.setFxSlot(2, 1, int(ld::FxType::Phaser), 0.6f, 0.4f, false);
  ses.setFxSlot(2, 2, int(ld::FxType::Compressor), 0.5f, 0.0f, false);
  // Add notes on Synth 3 and Synth 4 and render
  CHECK(ses.toggleNote(0, 2, 0, 0, false));
  CHECK(ses.toggleNote(0, 3, 4, 4, false));
  ld::RenderOptions o; o.songMode = false; o.scene = 0; o.tailSeconds = 0.2;
  const ld::RenderResult r = ld::renderOffline(ses.project(), o);
  CHECK(finite(r.l) && finite(r.r) && peakOf(r.l) > 0.1f && peakOf(r.l) <= 0.9441f);
}

static void testJungleSlicerAndRatchets() {
  // 1. Verify slice boundaries and reverse playback on Jungle kit
  ld::Session ses(48000.0);
  ses.loadJungleKit();
  CHECK(std::fabs(ses.getParam(ld::P_BPM, 0) - 168.0f) < 0.01f);
  CHECK(std::fabs(ses.getParam(ld::P_PAD_SLICE_START, 2) - 0.25f) < 0.01f);
  CHECK(std::fabs(ses.getParam(ld::P_PAD_SLICE_END, 2) - 0.375f) < 0.01f);
  ses.setParam(ld::P_PAD_REVERSE, 2, 1.0f);
  CHECK(ses.getParam(ld::P_PAD_REVERSE, 2) > 0.5f);
  ses.engine().triggerPad(2, 2);
  float l[1024] = {}, r[1024] = {};
  ses.engine().render(l, r, 1024);
  float pk = 0.0f;
  for (int i = 0; i < 1024; ++i) pk = std::fmax(pk, std::fabs(l[i]));
  CHECK(pk > 0.01f && pk <= 1.0f);

  // 2. Verify sample-accurate 4x ratchet roll (step value 5) lands 4 clicks inside one 16th step (6000 frames @ 120 BPM)
  ld::Project p = emptyProject();
  p.scenes[0].drums.steps[0][0] = 5; // 4x roll -> hits at 0, 1500, 3000, 4500
  auto c = click(48000);
  ld::RenderOptions o; o.samples[0] = c.get(); o.tailSeconds = 0.05;
  const ld::RenderResult rr = ld::renderOffline(p, o);
  const std::vector<long> got = clicks(rr.l);
  CHECK_EQ(got.size(), size_t(4));
  if (got.size() == 4) {
    CHECK_EQ(got[0], 0); CHECK_EQ(got[1], 1500); CHECK_EQ(got[2], 3000); CHECK_EQ(got[3], 4500);
  }
}


static void testDrumEnginesAndJungleResample() {
  ld::Session ses(48000.0);
  // 1. Test all 4 synthesized drum engines produce distinct, bounded audio
  for (int eng = 0; eng <= 3; ++eng) {
    ses.setDrumEngine(eng);
    CHECK_EQ(int(ses.project().drumEngine), eng);
    ses.engine().triggerPad(0, 2);
    float l[512] = {}, r[512] = {};
    ses.engine().render(l, r, 512);
    float pk = 0.0f;
    for (int i = 0; i < 512; ++i) pk = std::fmax(pk, std::fabs(l[i]));
    CHECK(pk > 0.05f && pk <= 1.0f);
  }
  // 2. Resample user's own processed drum loop from scene 1 into the Jungle Slicer
  ses.setDrumEngine(int(ld::DrumEngine::Punch909));
  ses.setFxSlot(0, 0, int(ld::FxType::Bitcrush), 0.4f, 0.3f, false);
  ses.resampleSceneToJungle(1);
  CHECK(ses.project().drumEngine == ld::DrumEngine::JungleSlicer);
  CHECK(std::fabs(ses.getParam(ld::P_PAD_SLICE_START, 4) - 0.5f) < 0.01f);
  // 3. Apply Jungle global tuning (Repitch, Tightness, Crunch)
  ses.applyJungleTuning(3.0f, 0.35f, 0.55f);
  CHECK(std::fabs(ses.getParam(ld::P_PAD_PITCH, 0) - 3.0f) < 0.01f);
  CHECK(std::fabs(ses.getParam(ld::P_PAD_DECAY, 7) - 0.35f) < 0.01f);
  CHECK(std::fabs(ses.getParam(ld::P_FX_DRIVE, 0) - 0.55f) < 0.01f);
  ses.engine().triggerPad(4, 2);
  float l[512] = {}, r[512] = {};
  ses.engine().render(l, r, 512);
  float pk = 0.0f;
  for (int i = 0; i < 512; ++i) pk = std::fmax(pk, std::fabs(l[i]));
  CHECK(pk > 0.01f && pk <= 1.0f);
}


static float padPeak(ld::Session& ses, int pad) {
  float l[2048] = {}, r[2048] = {}, pk = 0.0f;
  for (int i = 0; i < 96; ++i) ses.engine().render(l, r, 2048);   // let earlier hits and effect tails ring out
  ses.engine().triggerPad(pad, 2);
  ses.engine().render(l, r, 2048);
  for (int i = 0; i < 2048; ++i) pk = std::fmax(pk, std::fabs(l[i]));
  return pk;
}

// A user's own break is audio outside the project struct: it has to survive save/load, follow undo and reach the export.
static void testOwnBreakPersistsAndUndoes() {
  using namespace ld;
  const std::string dir = "/tmp/ld_break_test";
  if (std::system(("mkdir -p " + dir).c_str()) != 0) return;
  const std::string wav = dir + "/in.wav", proj = dir + "/p.ldp";
  // a "break" that is silent in its first half and loud in its second: slices 0-3 silent, 4-7 loud
  std::vector<float> l(48000, 0.0f);
  for (size_t i = 24000; i < l.size(); ++i) l[i] = 0.8f * float(std::sin(double(i) * 0.05));
  CHECK(writeWav16(wav.c_str(), l.data(), l.data(), l.size(), 48000.0));

  Session a(48000.0);
  CHECK(!a.loadJungleBreakWav((dir + "/missing.wav").c_str()));
  CHECK_EQ(int(a.project().drumEngine), 0);
  CHECK(a.loadJungleBreakWav(wav.c_str()));
  CHECK_EQ(int(a.project().drumEngine), 4); CHECK(a.project().breakId != 0); CHECK(a.activeBreak() != nullptr);
  CHECK(padPeak(a, 1) < 0.01f); CHECK(padPeak(a, 5) > 0.2f);
  float w[16];
  CHECK_EQ(a.padWaveform(0, w, 16), 16); CHECK(w[2] < 0.01f); CHECK(w[12] > 0.5f);

  // offline render (export) plays the same audio
  a.clearScene(0); a.setDrumStep(0, 5, 0, 2);
  RenderOptions o; o.scene = 0; o.soloTrack = 0; o.tailSeconds = 0;
  const auto brk = a.activeBreak();
  for (int i = 0; i < kNumPads; ++i) o.samples[i] = brk.get();
  const RenderResult rr = renderOffline(a.project(), o);
  float pk = 0; for (float x : rr.l) pk = std::fmax(pk, std::fabs(x));
  CHECK(pk > 0.1f);

  // save and load in a fresh session
  CHECK(a.save(proj.c_str()));
  Session b(48000.0);
  CHECK(b.load(proj.c_str()));
  CHECK(b.activeBreak() != nullptr); CHECK_EQ(b.project().breakId, a.project().breakId);
  CHECK(padPeak(b, 1) < 0.01f); CHECK(padPeak(b, 5) > 0.2f);

  // undo goes back to the 808, redo brings the break back; the built-in break is a separate step
  CHECK(a.undo()); CHECK(a.undo()); CHECK(a.undo());
  CHECK_EQ(int(a.project().drumEngine), 0); CHECK(a.activeBreak() == nullptr); CHECK(padPeak(a, 1) > 0.05f);
  CHECK(a.redo()); CHECK(a.activeBreak() != nullptr); CHECK(padPeak(a, 1) < 0.01f);
  a.setDrumEngine(4); CHECK(a.activeBreak() == nullptr); CHECK_EQ(a.project().breakId, 0u); CHECK(padPeak(a, 1) > 0.02f);
  CHECK(a.undo()); CHECK(padPeak(a, 1) < 0.01f);

  // if the audio file is lost the project still loads, with the built-in break
  std::remove((proj + ".break.wav").c_str());
  Session c(48000.0);
  CHECK(c.load(proj.c_str())); CHECK_EQ(c.project().breakId, 0u); CHECK_EQ(int(c.project().drumEngine), 4);
  CHECK(padPeak(c, 1) > 0.02f);

  // bouncing: an empty scene is refused, a real one becomes the break and its effects are not applied twice
  Session d(48000.0);
  d.clearScene(2); CHECK(!d.resampleSceneToJungle(2)); CHECK_EQ(int(d.project().drumEngine), 0);
  d.setFxSlot(0, 0, int(FxType::Bitcrush), 0.4f, 0.3f, false);
  CHECK(d.resampleSceneToJungle(0)); CHECK(d.activeBreak() != nullptr);
  CHECK(d.project().fx[0].slots[0].type == FxType::Off); CHECK(d.project().fx[0].drive == 0.0f);
  // tuning through setParam reaches all pads; switching between hit kits keeps pad edits
  d.setParam(P_JUNGLE_PITCH, 0, -5.0f); CHECK(d.project().pads[6].pitch == -5.0f);
  Session e(48000.0);
  e.setParam(P_PAD_VOL, 3, 0.33f); e.setDrumEngine(1); CHECK(std::fabs(e.project().pads[3].vol - 0.33f) < 1e-6f);
  e.setParam(P_JUNGLE_PITCH, 0, 4.0f); CHECK(e.project().pads[0].pitch != 4.0f);
}

// The built-in start preset: the delivered file converts, the embedded copy is the same project, and it plays.
static void testStartPreset() {
  using namespace ld;
  auto raw = std::make_unique<Project>();
  bool conv = false;
  CHECK(readProjectFile((std::string(LD_FIXTURES) + "/project_v5raw_warehouse303.ldp").c_str(), *raw, &conv));
  CHECK(conv);
  CHECK(raw->bpm == 136.0f); CHECK_EQ(raw->root, 2); CHECK(raw->scale == Scale::Minor); CHECK_EQ(raw->activeSynths, 4);
  CHECK(raw->drumEngine == DrumEngine::Punch909); CHECK_EQ(raw->chainLen, 8);
  const int want[8] = {0, 0, 1, 1, 2, 2, 3, 2};
  for (int i = 0; i < 8; ++i) CHECK_EQ(raw->chain[i], want[i]);
  CHECK(raw->fx[4].mute); CHECK(raw->synths[1].mode == SynthMode::Fm); CHECK(raw->synths[2].chord);
  CHECK(raw->fx[1].slots[0].type == FxType::Overdrive);

  auto emb = std::make_unique<Project>(makeStartProject());
  CHECK(emb->bpm == raw->bpm); CHECK_EQ(emb->chainLen, raw->chainLen); CHECK_EQ(emb->activeSynths, raw->activeSynths);
  CHECK(std::memcmp(emb->scenes, raw->scenes, sizeof(raw->scenes)) == 0);
  CHECK(std::memcmp(emb->pads, raw->pads, sizeof(raw->pads)) == 0);
  CHECK(emb->synths[0].cutoff == raw->synths[0].cutoff); CHECK(emb->fx[3].revSend == raw->fx[3].revSend);

  RenderOptions o; o.songMode = true; o.tailSeconds = 1.0;
  const RenderResult rr = renderOffline(*emb, o);
  float pk = 0; double sum = 0; bool finite = true;
  for (size_t i = 0; i < rr.l.size(); ++i) {
    finite = finite && rr.l[i] == rr.l[i] && rr.r[i] == rr.r[i];
    pk = std::fmax(pk, std::fmax(std::fabs(rr.l[i]), std::fabs(rr.r[i]))); sum += double(rr.l[i]) * rr.l[i];
  }
  CHECK(finite); CHECK(pk > 0.3f); CHECK(pk <= 1.0f); CHECK(std::sqrt(sum / double(rr.l.size())) > 0.03);

  Session ses(48000.0);
  ses.loadStartPreset();
  CHECK(ses.project().bpm == 136.0f); CHECK(ses.project().drumEngine == DrumEngine::Punch909);
  CHECK(ses.undo()); CHECK(ses.project().bpm == 122.0f); CHECK(ses.project().drumEngine == DrumEngine::Classic808);
}

// Leaving the break slicer and coming back, in the same session or after a restart, loses nothing.
static void testSlicerSurvivesLeaving() {
  using namespace ld;
  const std::string dir = "/tmp/ld_slicer_test";
  if (std::system(("mkdir -p " + dir).c_str()) != 0) return;
  const std::string proj = dir + "/p.ldp";
  Session a(48000.0);
  const DrumPattern hits = a.project().scenes[0].drums;
  const float kickVol = 0.41f;
  a.setParam(P_PAD_VOL, 0, kickVol);
  CHECK(a.resampleSceneToJungle(0));
  const uint32_t id = a.project().breakId;
  CHECK(id != 0);
  // the bounced scene plays the slices in order
  for (int i = 0; i < kNumPads; ++i) CHECK_EQ(int(a.project().scenes[0].drums.steps[i][i * 2]), 2);
  a.setDrumStep(0, 3, 9, 1); a.setParam(P_PAD_REVERSE, 5, 1.0f); a.setParam(P_PAD_SLICE_START, 2, 0.3f);
  const DrumPattern loop = a.project().scenes[0].drums;

  a.setDrumEngine(1);                              // out to the 909: the hit pattern and pad settings are back
  CHECK(std::memcmp(&a.project().scenes[0].drums, &hits, sizeof(hits)) == 0);
  CHECK(std::fabs(a.project().pads[0].vol - kickVol) < 1e-6f); CHECK(!a.project().pads[5].reverse);
  CHECK(a.activeBreak() == nullptr); CHECK_EQ(a.project().breakId, id);
  a.setDrumStep(0, 0, 1, 2);
  const DrumPattern hits2 = a.project().scenes[0].drums;

  a.setDrumEngine(4);                              // and back: own break, own loop, own slice edits
  CHECK(a.activeBreak() != nullptr); CHECK_EQ(a.project().breakId, id);
  CHECK(std::memcmp(&a.project().scenes[0].drums, &loop, sizeof(loop)) == 0);
  CHECK(a.project().pads[5].reverse); CHECK(std::fabs(a.project().pads[2].sliceStart - 0.3f) < 1e-6f);

  // restart while a hit kit is selected: the break and the loop must still be there afterwards
  a.setDrumEngine(0);
  CHECK(a.save(proj.c_str()));
  Session b(48000.0);
  CHECK(b.load(proj.c_str()));
  CHECK_EQ(int(b.project().drumEngine), 0); CHECK(std::memcmp(&b.project().scenes[0].drums, &hits2, sizeof(hits2)) == 0);
  b.setDrumEngine(4);
  CHECK(b.activeBreak() != nullptr);
  CHECK(std::memcmp(&b.project().scenes[0].drums, &loop, sizeof(loop)) == 0); CHECK(b.project().pads[5].reverse);
  CHECK(padPeak(b, 0) > 0.05f);

  // restart inside the slicer
  CHECK(b.save(proj.c_str()));
  Session c(48000.0);
  CHECK(c.load(proj.c_str()));
  CHECK_EQ(int(c.project().drumEngine), 4); CHECK(c.activeBreak() != nullptr);
  CHECK(std::memcmp(&c.project().scenes[0].drums, &loop, sizeof(loop)) == 0);
  c.setDrumEngine(2); CHECK(std::memcmp(&c.project().scenes[0].drums, &hits2, sizeof(hits2)) == 0);
  CHECK(c.undo()); CHECK(c.activeBreak() != nullptr);
}

int main() {
  const struct { const char* name; void (*fn)(); } tests[] = {
    {"scales and chords stay in key", testMusic},
    {"lock-free queue keeps order across threads", testQueue},
    {"steps and swing land on the exact sample", testTimingAndSwing},
    {"no drift over 8-bar loops at 122 BPM", testNoDriftOverEightBars},
    {"block size does not change the sound", testBlockSizeDoesNotChangeTheSound},
    {"renders are repeatable and stereo", testRendersAreRepeatable},
    {"limiter ceiling holds under abuse", testLimiterCeilingHoldsUnderAbuse},
    {"audio thread never allocates", testAudioThreadNeverAllocates},
    {"choke group cuts the other pad", testChokeGroupCutsTheOtherPad},
    {"scenes change on the bar line", testScenesChangeOnTheBarLine},
    {"song mode and 8-bar patterns", testSongModeAndLongPatterns},
    {"live recording snaps to the grid", testLiveRecordingSnapsToTheGrid},
    {"held notes release, voices get stolen safely", testHeldNotesAndStealing},
    {"MIDI export", testMidiExport},
    {"WAV round trip and stems", testWavRoundTripAndStems},
    {"UI thread and audio thread run together", testTwoThreads},
    {"session: undo, parameters, save and load", testSessionUndoSaveLoad},
    {"session: live hits are written into the project", testSessionRecordsLiveHits},
    {"save files from build 5 are converted", testOldSaveFilesAreConverted},
    {"save files from v0.3 (build 7) are converted", testV03SaveFilesAreConverted},
    {"project file format: complete and tolerant of newer files", testProjectFileFormat},
    {"drum kit follows the project (restart, undo, export)", testKitFollowsTheProject},
    {"song automation: block-size independent, exact slot ends", testAutomationTiming},
    {"rolls: in MIDI export, no drift", testRollsInMidiAndOverLongRuns},
    {"pattern editing helpers", testPatternEditing},
    {"FM synth engine and per-sequence octave isolation", testFmAndOctaveIsolation},
    {"song-menu sequence automation", testSongAutomation},
    {"jungle breakbeat slicer and ratchet rolls", testJungleSlicerAndRatchets},
    {"4 synth tracks, instrument engine and Vital FX chain", testFourTracksInstrumentsAndFxChain},
    {"drum engines, jungle tuning and resample-to-break", testDrumEnginesAndJungleResample},
    {"own break: save, load, undo, export", testOwnBreakPersistsAndUndoes},
    {"start preset Warehouse 303", testStartPreset},
    {"break slicer survives leaving and restarting", testSlicerSurvivesLeaving},
  };
  int failedTests = 0;
  for (const auto& t : tests) {
    const int before = g_fail;
    t.fn();
    const bool ok = g_fail == before;
    if (!ok) ++failedTests;
    std::printf("%s  %s\n", ok ? "ok  " : "FAIL", t.name);
  }
  std::printf("\n%zu tests, %d failed (%d checks)\n", sizeof(tests) / sizeof(tests[0]), failedTests, g_checks);
  return g_fail ? 1 : 0;
}
