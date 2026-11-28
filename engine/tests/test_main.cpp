// Desktop tests for the Loop Draft engine. No external test framework: build and run ld_tests.
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

#include "loopdraft/engine.h"
#include "loopdraft/session.h"
#include "loopdraft/pedalboard.h"

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
  s.setSceneBars(2, 8); s.setSceneBars(3, 99); CHECK_EQ(s.project().scenes[2].bars, 8); CHECK_EQ(s.project().scenes[3].bars, kMaxBars);
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

static std::vector<float> renderSynthNotes(ld::SynthParams sp, bool slide, bool accent, int seconds = 1) {
  using namespace ld;
  auto p = std::make_unique<Project>(makeDemoProject());
  p->bpm = 120; p->swing = 0;
  for (auto& sc : p->scenes) { sc.drums = DrumPattern{}; for (auto& pat : sc.synth) pat.count = 0; sc.bars = 1; }
  for (auto& f : p->fx) { f.delaySend = 0; f.revSend = 0; f.drive = 0; f.eqLow = 0; f.eqHigh = 0; for (auto& sl : f.slots) sl.type = FxType::Off; }
  sp.chord = false; sp.lfoTarget = LfoTarget::Off; sp.feelMotion = 0.3f; sp.feelWarmth = 0.5f;
  p->synths[0] = sp;
  SynthPattern& pat = p->scenes[0].synth[0];
  addNote(pat, 0, 0, 4, accent ? 2 : 1);
  addNote(pat, 4, 7, 4, 1);                        // an octave up in a 7-note scale
  if (slide) pat.notes[1].flags = kNoteSlide;
  RenderOptions o; o.scene = 0; o.soloTrack = 1; o.tailSeconds = double(seconds); o.sampleRate = 48000;
  return renderOffline(*p, o).l;
}
static double zeroCrossHz(const std::vector<float>& x, size_t a, size_t b) {
  int n = 0; for (size_t i = a + 1; i < b && i < x.size(); ++i) if (x[i - 1] < 0 && x[i] >= 0) ++n;
  return double(n) * 48000.0 / double(b - a);
}
// Pitch of a tone with many overtones: the lag near the expected period where the signal matches itself best.
static double pitchNear(const std::vector<float>& x, size_t a, size_t n, double expectHz) {
  const int lo = int(48000.0 / expectHz * 0.9), hi = int(48000.0 / expectHz * 1.1) + 1;
  double best = -1e30; int bestLag = lo;
  for (int lag = lo; lag <= hi; ++lag) {
    double c = 0; for (size_t i = a; i < a + n; ++i) c += double(x[i]) * x[i + size_t(lag)];
    if (c > best) { best = c; bestLag = lag; }
  }
  return 48000.0 / double(bestLag);
}
static float rmsOf(const std::vector<float>& x, size_t a, size_t b) {
  double s = 0; for (size_t i = a; i < b && i < x.size(); ++i) s += double(x[i]) * x[i];
  return float(std::sqrt(s / double(b - a)));
}

static void testSlideAccentFold() {
  using namespace ld;
  SynthParams sp; sp.mode = SynthMode::Subtractive; sp.wave1 = Wave::Sine; sp.wave2 = Wave::Sine; sp.detune = 0;
  sp.cutoff = 9000; sp.res = 0.7f; sp.attack = 0.002f; sp.decay = 0.6f; sp.sustain = 0.8f; sp.release = 0.05f; sp.slideTime = 0.08f;
  // step 4 starts at 0.5 s (120 BPM). Without slide the second note is at its pitch at once and has a new attack.
  const auto plain = renderSynthNotes(sp, false, false), slid = renderSynthNotes(sp, true, false);
  const double f1 = zeroCrossHz(plain, 4800, 19200), f2 = zeroCrossHz(plain, 28800, 43200);
  CHECK(std::fabs(f2 / f1 - 2.0) < 0.05);
  const double early = zeroCrossHz(slid, 24000 + 240, 24000 + 1680), late = zeroCrossHz(slid, 36000, 43200);
  CHECK(early > f1 * 1.03 && early < f2 * 0.93);   // on its way
  CHECK(std::fabs(late / f2 - 1.0) < 0.03);        // arrived
  // no gap and no new attack at the slide: the level right at the note change stays up
  CHECK(rmsOf(slid, 23900, 24100) > 0.6f * rmsOf(slid, 20000, 22000));
  CHECK(rmsOf(plain, 23900, 24100) < 0.6f * rmsOf(plain, 20000, 22000));

  // accent: louder, and brighter through a low filter
  SynthParams saw = sp; saw.wave1 = Wave::Saw; saw.wave2 = Wave::Saw; saw.cutoff = 500; saw.res = 2.0f;
  const auto soft = renderSynthNotes(saw, false, false), acc = renderSynthNotes(saw, false, true);
  const auto hf = [](const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a + 1; i < b; ++i) { const double d = x[i] - x[i - 1]; s += d * d; } return std::sqrt(s); };
  CHECK(rmsOf(acc, 480, 4800) > 1.15f * rmsOf(soft, 480, 4800));
  CHECK(hf(acc, 480, 4800) / rmsOf(acc, 480, 4800) > 1.25 * hf(soft, 480, 4800) / rmsOf(soft, 480, 4800));

  // fold adds overtones to a sine, and 0 leaves the sound untouched
  SynthParams fo = sp; fo.fold = 0.8f;
  const auto folded = renderSynthNotes(fo, false, false);
  CHECK(hf(folded, 4800, 19200) / rmsOf(folded, 4800, 19200) > 2.0 * hf(plain, 4800, 19200) / rmsOf(plain, 4800, 19200));
  for (float x : folded) CHECK(x == x && std::fabs(x) <= 1.0f);

  // slide flag: set through the session, survives save/load, old files never show one
  Session a(48000.0);
  a.clearScene(0);
  CHECK(a.toggleNote(0, 0, 0, 0, false)); CHECK(a.toggleNote(0, 0, 4, 2, false, 1, true));
  CHECK_EQ(a.project().scenes[0].synth[0].count, 2);
  CHECK_EQ(int(a.project().scenes[0].synth[0].notes[1].flags), int(kNoteSlide));
  CHECK(a.toggleNote(0, 0, 0, 0, false, 1, true)); CHECK_EQ(int(a.project().scenes[0].synth[0].notes[0].flags), int(kNoteSlide));
  CHECK(a.toggleNote(0, 0, 0, 0, false, 1, true)); CHECK_EQ(int(a.project().scenes[0].synth[0].notes[0].flags), 0);
  CHECK(a.toggleNote(0, 0, 4, 2, true)); CHECK_EQ(int(a.project().scenes[0].synth[0].notes[1].flags), int(kNoteSlide));   // accent keeps the slide
  CHECK(a.save("/tmp/ld_slide.ldp"));
  Session b(48000.0); CHECK(b.load("/tmp/ld_slide.ldp"));
  CHECK_EQ(int(b.project().scenes[0].synth[0].notes[1].flags), int(kNoteSlide));
  auto old = std::make_unique<Project>();
  CHECK(readProjectFile((std::string(LD_FIXTURES) + "/project_v5raw_warehouse303.ldp").c_str(), *old, nullptr));
  for (auto& sc : old->scenes) for (auto& pat : sc.synth) for (auto& nt : pat.notes) CHECK_EQ(int(nt.flags), 0);
  const Project start = makeStartProject();
  for (auto& sc : start.scenes) for (auto& pat : sc.synth) for (auto& nt : pat.notes) CHECK_EQ(int(nt.flags), 0);
}

static void testPluckAndVector() {
  using namespace ld;
  SynthParams sp; sp.mode = SynthMode::Pluck; sp.cutoff = 12000; sp.res = 0.7f; sp.decay = 0.5f; sp.release = 0.3f; sp.pluckBright = 0.7f;
  const auto pl = renderSynthNotes(sp, false, false, 3);
  bool finite = true; float pk = 0; for (float x : pl) { finite = finite && x == x; pk = std::fmax(pk, std::fabs(x)); }
  CHECK(finite); CHECK(pk > 0.05f); CHECK(pk <= 1.0f);
  // in tune: the demo project is in a minor key, degree 0 and degree 7 are an octave apart
  SynthParams ref; ref.mode = SynthMode::Subtractive; ref.wave1 = Wave::Sine; ref.wave2 = Wave::Sine; ref.detune = 0; ref.cutoff = 9000; ref.sustain = 1;
  const auto rs = renderSynthNotes(ref, false, false);
  const double r1 = zeroCrossHz(rs, 4800, 19200);
  const double f1 = pitchNear(pl, 4800, 9600, r1), f2 = pitchNear(pl, 26400, 9600, 2.0 * r1);
  CHECK(std::fabs(f1 / r1 - 1.0) < 0.012); CHECK(std::fabs(f2 / (2.0 * r1) - 1.0) < 0.012);
  // a string rings out by itself and then the voice ends: silence at the end of the tail
  CHECK(rmsOf(pl, 1200, 6000) > 4.0f * rmsOf(pl, 16800, 21600) || rmsOf(pl, 1200, 6000) > 0.02f);
  CHECK(rmsOf(pl, pl.size() - 4800, pl.size()) < 1e-3f);
  // brightness changes the tone
  SynthParams dull = sp; dull.pluckBright = 0.0f;
  const auto dl = renderSynthNotes(dull, false, false, 3);
  const auto hf = [](const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a + 1; i < b; ++i) { const double d = x[i] - x[i - 1]; s += d * d; } return std::sqrt(s); };
  CHECK(hf(pl, 480, 9600) / rmsOf(pl, 480, 9600) > 1.3 * hf(dl, 480, 9600) / rmsOf(dl, 480, 9600));

  // Vector: each corner is its own sound; the middle is a mix; the same at any block size is covered by the engine tests
  SynthParams vp; vp.mode = SynthMode::Vector; vp.wave1 = Wave::Saw; vp.wave2 = Wave::Triangle; vp.detune = 0; vp.cutoff = 12000; vp.res = 0.7f;
  vp.sustain = 1.0f; vp.fmIndex = 3.0f; vp.fmModDecay = 2.0f; vp.fmRatio = 3.0f;
  std::vector<std::vector<float>> corner;
  for (int c = 0; c < 4; ++c) { vp.vecX = float(c & 1); vp.vecY = float(c >> 1); corner.push_back(renderSynthNotes(vp, false, false)); }
  for (int a = 0; a < 4; ++a) {
    CHECK(rmsOf(corner[a], 4800, 19200) > 0.02f);
    for (int b = a + 1; b < 4; ++b) {
      double d = 0; for (size_t i = 4800; i < 19200; ++i) d += std::fabs(double(corner[a][i]) - corner[b][i]);
      CHECK(d / 14400.0 > 0.01);
    }
  }
  // the sub corner sounds an octave below the note
  CHECK(std::fabs(zeroCrossHz(corner[3], 4800, 19200) / r1 - 0.5) < 0.03);
  // song automation can move the position
  auto p = std::make_unique<Project>(makeDemoProject());
  Session ses(48000.0);
  ses.setChainAuto(0, 1, 0, P_SYN_VEC_X, 0.0f, 1.0f);
  CHECK_EQ(ses.project().chainAuto[0].tracks[1][0].paramId, P_SYN_VEC_X);
  ses.setParam(P_SYN_MODE, 0, 3); CHECK(ses.project().synths[0].mode == SynthMode::Vector);
  ses.setParam(P_SYN_VEC_Y, 0, 2.0f); CHECK(ses.project().synths[0].vecY == 1.0f);

  // a named copy: saved next to the autosave, with its break, and loads back the same
  CHECK(ses.resampleSceneToJungle(0));
  CHECK(ses.saveCopy("/tmp/ld_named.ldp"));
  Session other(48000.0); CHECK(other.load("/tmp/ld_named.ldp"));
  CHECK(other.activeBreak() != nullptr); CHECK(other.project().synths[0].mode == SynthMode::Vector);
  CHECK(ses.dirty());                              // the copy does not count as the autosave
  // opening a named project is one undo step
  Session third(48000.0);
  const float bpmBefore = third.project().bpm;
  third.setParam(P_BPM, 0, 99.0f);
  CHECK(!third.openProject("/tmp/ld_missing.ldp")); CHECK(third.project().bpm == 99.0f);
  CHECK(third.openProject("/tmp/ld_named.ldp"));
  CHECK(third.activeBreak() != nullptr); CHECK(third.project().synths[0].mode == SynthMode::Vector); CHECK(padPeak(third, 0) > 0.02f);
  CHECK(third.undo()); CHECK(third.project().bpm == 99.0f); CHECK(third.activeBreak() == nullptr); CHECK(bpmBefore != 99.0f);
  // automation: moving a point while dragging is not an undo step of its own
  Session au(48000.0);
  au.setChainAuto(0, 1, 0, P_SYN_CUTOFF, 500.0f, 4000.0f);
  au.setChainAuto(0, 1, 0, P_SYN_CUTOFF, 600.0f, 4000.0f, false); au.setChainAuto(0, 1, 0, P_SYN_CUTOFF, 700.0f, 4000.0f, false);
  CHECK(au.project().chainAuto[0].tracks[1][0].startVal == 700.0f);
  CHECK(au.undo()); CHECK_EQ(au.project().chainAuto[0].tracks[1][0].paramId, -1);
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
  const float tempo = a.project().bpm;
  CHECK(a.resampleSceneToJungle(0));
  CHECK(a.project().bpm == tempo);                 // a bounced loop stays at its own tempo
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

static void testHardDrumEnginesAndCustomDrumSynth() {
  ld::Session ses(48000.0);
  // 1. Verify all 5 new hard drum engines (5..9) render clean, distinct, bounded audio
  for (int eng = int(ld::DrumEngine::HardcoreGabber); eng <= int(ld::DrumEngine::GlitchBreakcore); ++eng) {
    ses.setDrumEngine(eng);
    CHECK_EQ(int(ses.project().drumEngine), eng);
    ses.engine().triggerPad(0, 2);
    ses.engine().triggerPad(1, 2);
    float l[512] = {}, r[512] = {};
    ses.engine().render(l, r, 512);
    float pk = 0.0f;
    for (int i = 0; i < 512; ++i) { CHECK(std::isfinite(l[i])); pk = std::fmax(pk, std::fabs(l[i])); }
    CHECK(pk > 0.05f && pk <= 1.0f);
  }
  // 2. Kit Hardness macro
  ses.setKitHardness(0.80f);
  CHECK(std::fabs(ses.getParam(ld::P_KIT_HARDNESS, 0) - 0.80f) < 0.01f);
  CHECK(std::fabs(ses.getParam(ld::P_FX_DRIVE, 0) - 0.80f) < 0.01f);

  // 3. Expert Menu Custom Drum Synthesizer: template + custom tweak + save/load v6
  ses.applyDrumSynthTemplate(0, 3); // Gabber Kick template on Pad 0
  CHECK(ses.project().customDrums[0].enabled);
  ses.setParam(ld::P_DSYN_FREQ, 0, 58.0f);
  ses.setParam(ld::P_DSYN_DRIVE, 0, 0.90f);
  ses.synthesizeCustomPad(0, true);
  float l[512] = {}, r[512] = {};
  ses.engine().render(l, r, 512);
  float pk = 0.0f;
  for (int i = 0; i < 512; ++i) pk = std::fmax(pk, std::fabs(l[i]));
  CHECK(pk > 0.05f && pk <= 1.0f);

  const char* pth = "/tmp/ld_v6_custom_drum.ldp";
  CHECK(ses.save(pth));
  ld::Session ses2(48000.0);
  CHECK(ses2.load(pth));
  CHECK(ses2.project().customDrums[0].enabled);
  CHECK(std::fabs(ses2.getParam(ld::P_DSYN_FREQ, 0) - 58.0f) < 0.1f);
  std::remove(pth);
}

// The drum synth of a pad is project state: it follows undo, survives kit changes and the slicer, and is in the export.
static void testDrumSynthIsProjectState() {
  using namespace ld;
  Session a(48000.0);
  float w0[32], w1[32];
  CHECK_EQ(a.padWaveform(2, w0, 32), 32);
  a.applyDrumSynthTemplate(2, 5);                  // pad 2 becomes a long metallic clang
  CHECK(a.project().customDrums[2].enabled);
  CHECK_EQ(a.padWaveform(2, w1, 32), 32);
  float diff = 0; for (int i = 0; i < 32; ++i) diff += std::fabs(w0[i] - w1[i]);
  CHECK(diff > 0.5f);
  const float synthPeak = padPeak(a, 2);
  CHECK(synthPeak > 0.05f);
  // turning a drum-synth control switches a pad over and rebuilds its sound; no extra call needed
  a.setParam(P_DSYN_LEN, 4, 1.2f); CHECK(a.project().customDrums[4].enabled);
  // changing the kit keeps the synthesized pads; so does a trip through the break slicer
  a.setDrumEngine(int(DrumEngine::HardcoreGabber)); CHECK(a.project().customDrums[2].enabled);
  a.setDrumEngine(int(DrumEngine::JungleSlicer)); a.setDrumEngine(int(DrumEngine::Punch909));
  CHECK(a.project().customDrums[2].enabled);
  float w2[32]; a.padWaveform(2, w2, 32);
  diff = 0; for (int i = 0; i < 32; ++i) diff += std::fabs(w2[i] - w1[i]);
  CHECK(diff < 1e-4f);
  // offline render uses the synthesized pad: a long clang still sounds where the 909 hat has died away
  auto p = std::make_unique<Project>(a.project());
  for (auto& sc : p->scenes) { sc.drums = DrumPattern{}; for (auto& pat : sc.synth) pat.count = 0; sc.bars = 1; }
  for (auto& f : p->fx) { f.delaySend = 0; f.revSend = 0; }
  p->scenes[0].drums.steps[2][0] = 2; p->bpm = 120; p->pads[2].decay = 2.0f;
  RenderOptions o; o.scene = 0; o.soloTrack = 0; o.tailSeconds = 0; o.sampleRate = 48000;
  const auto withSynth = renderOffline(*p, o).l;
  p->customDrums[2].enabled = false;
  const auto kitOnly = renderOffline(*p, o).l;
  CHECK(rmsOf(withSynth, 14400, 19200) > 4.0f * rmsOf(kitOnly, 14400, 19200) + 1e-4f);
  // undo steps back through every change; save and load bring the pads back
  CHECK(a.save("/tmp/ld_dsyn.ldp"));
  Session b(48000.0); CHECK(b.load("/tmp/ld_dsyn.ldp"));
  CHECK(b.project().customDrums[2].enabled); CHECK(b.project().customDrums[4].enabled); CHECK(!b.project().customDrums[0].enabled);
  CHECK(std::fabs(b.project().customDrums[4].length - 1.2f) < 1e-6f); CHECK(int(b.project().drumEngine) == int(DrumEngine::Punch909));
  b.clearCustomPad(2); CHECK(!b.project().customDrums[2].enabled);
  CHECK(b.undo()); CHECK(b.project().customDrums[2].enabled);
  // hardness through setParam also drives the drum track, and the ids of the synth controls did not move
  b.setParam(P_KIT_HARDNESS, 0, 0.6f); CHECK(std::fabs(b.project().fx[0].drive - 0.6f) < 1e-6f);
  // the default of a control does not depend on what the session was changed to
  { Session q(48000.0); const float d0 = Session::defaultParam(P_SYN_CUTOFF, 0); q.setParam(P_SYN_CUTOFF, 0, 4321.0f);
    CHECK(Session::defaultParam(P_SYN_CUTOFF, 0) == d0); CHECK(d0 == makeDemoProject().synths[0].cutoff);
    CHECK(Session::defaultParam(P_FX_EQ_LOW, 2) == makeDemoProject().fx[2].eqLow); CHECK(Session::defaultParam(P_KIT_HARDNESS, 0) == 0.0f); }
  CHECK_EQ(int(P_SYN_FOLD), 57); CHECK_EQ(int(P_SYN_VEC_X), 60); CHECK_EQ(int(P_KIT_HARDNESS), 62); CHECK_EQ(int(P_DSYN_FREQ), 70);
}

// Presets and grooves as JSON text: what goes in comes out, mistakes are named with their place, and nothing changes on an error.
static void testContentJson() {
  using namespace ld;
  std::string err;
  ContentInfo info;
  // --- a groove written by hand, the way an author would
  const std::string groove = R"({
    "kind": "groove", "name": "Rolling acid", "style": "acid techno",
    "bpm": 136, "swing": 0.1, "root": "F#", "scale": "dorian", "bars": 2,
    "drums": { "kit": "909 Punch",
               "steps": { "kick": "x...x...x...x...", "ch": "..x. ..x. ..x. ..3.|..x...x...x...X.", "clap": "....X.......X..." } },
    "synths": [
      { "track": "bass", "preset": "Acid 303",
        "notes": [ { "step": 0, "degree": 0, "len": 2, "accent": true }, { "step": 3, "degree": 7, "slide": true }, { "step": 20, "midi": 54 } ] },
      { "track": "syn3", "instrument": { "engine": "pluck", "pluckBright": 0.8, "decay": 0.4, "track": { "reverb": 0.3, "fx": [ { "type": "chorus", "amount": 0.6 } ] } },
        "notes": [ { "step": 8, "degree": 4 } ] }
    ] })";
  auto p = std::make_unique<Project>(makeDemoProject());
  ContentTarget tg; tg.scene = 1;
  const float oldBass = p->synths[0].cutoff;
  CHECK(applyContent(groove, *p, tg, err, &info));
  CHECK(err.empty()); CHECK(info.kind == "groove"); CHECK(info.name == "Rolling acid"); CHECK(info.style == "acid techno"); CHECK(info.warnings.empty());
  CHECK(p->bpm == 136.0f); CHECK(std::fabs(p->swing - 0.1f) < 1e-6f); CHECK_EQ(p->root, 6); CHECK(p->scale == Scale::Dorian);
  CHECK(p->drumEngine == DrumEngine::Punch909);
  const Scene& sc = p->scenes[1];
  CHECK_EQ(sc.bars, 2);
  CHECK_EQ(int(sc.drums.steps[0][0]), 1); CHECK_EQ(int(sc.drums.steps[0][20]), 1);   // a one-bar row repeats over both bars
  CHECK_EQ(int(sc.drums.steps[0][1]), 0); CHECK_EQ(int(sc.drums.steps[3][14]), 4);   // '3' = roll of three hits
  CHECK_EQ(int(sc.drums.steps[3][30]), 2); CHECK_EQ(int(sc.drums.steps[2][4]), 2);
  CHECK_EQ(int(sc.drums.steps[1][0]), 0);                                            // snare not listed: silent
  CHECK_EQ(sc.synth[0].count, 3);
  CHECK_EQ(int(sc.synth[0].notes[0].vel), 2); CHECK_EQ(int(sc.synth[0].notes[0].len), 2);
  CHECK_EQ(int(sc.synth[0].notes[1].degree), 7); CHECK_EQ(int(sc.synth[0].notes[1].flags), int(kNoteSlide));
  CHECK_EQ(degreeToMidi(*p, p->synths[0].octaveShift, sc.synth[0].notes[2].degree), 54);   // F#3 is the root
  CHECK(p->synths[0].instrument == InstrumentPreset::Acid303); CHECK(p->synths[0].cutoff != oldBass);
  CHECK_EQ(p->activeSynths, 3); CHECK(p->synths[2].mode == SynthMode::Pluck); CHECK(std::fabs(p->synths[2].pluckBright - 0.8f) < 1e-6f);
  CHECK(std::fabs(p->fx[3].revSend - 0.3f) < 1e-6f); CHECK(p->fx[3].slots[0].type == FxType::Chorus); CHECK(p->fx[3].slots[1].type == FxType::Off);
  CHECK_EQ(p->scenes[0].bars, makeDemoProject().scenes[0].bars);                      // other scenes untouched
  // without tempo and key
  auto q = std::make_unique<Project>(makeDemoProject());
  tg.tempoAndKey = false;
  CHECK(applyContent(groove, *q, tg, err, nullptr));
  CHECK(q->bpm == makeDemoProject().bpm); CHECK_EQ(q->root, makeDemoProject().root); CHECK_EQ(q->scenes[1].synth[0].count, 3);
  tg.tempoAndKey = true;

  // --- errors name the place, and leave the project alone
  const auto bad = [&](const std::string& text, const char* expect) {
    auto x = std::make_unique<Project>(makeDemoProject());
    const auto before = std::make_unique<Project>(*x);
    std::string e;
    const bool ok = applyContent(text, *x, ContentTarget{}, e, nullptr);
    CHECK(!ok);
    if (e.find(expect) == std::string::npos) { std::printf("    got: %s\n", e.c_str()); CHECK(false); }
    CHECK(std::memcmp(static_cast<const void*>(x.get()), static_cast<const void*>(before.get()), sizeof(Project)) == 0);
  };
  bad("{ \"kind\": \"groove\", ", "not valid JSON, line 1");
  bad("[1,2]", "must be one JSON object");
  bad(R"({"name":"x"})", "kind: is missing");
  bad(R"({"kind":"song","name":"x"})", "is not one of: instrument, drumkit, groove");
  bad(R"({"kind":"groove"})", "name: is missing");
  bad(R"({"kind":"groove","name":"x","bpm":300})", "bpm: 300 is outside 40 .. 240");
  bad(R"({"kind":"groove","name":"x","scale":"phrygian"})", "scale: \"phrygian\" is not one of: minor, major, dorian, pentatonic");
  bad(R"({"kind":"groove","name":"x","drums":{"steps":{"cowbell":"x..."}}})", "drums.steps.cowbell: is not a pad");
  bad(R"({"kind":"groove","name":"x","drums":{"steps":{"kick":"x..o"}}})", "'o' is not a step");
  bad(R"({"kind":"groove","name":"x","bars":1,"drums":{"steps":{"kick":"x...x...x...x...x...x...x...x..."}}})", "32 steps, but 1 bar(s) have only 16");
  bad(R"({"kind":"groove","name":"x","drums":{"steps":{"kick":"x...x."}}})", "6 steps is not a whole number of bars");
  bad(R"({"kind":"groove","name":"x","bars":1,"synths":[{"track":"bass","notes":[{"step":16,"degree":0}]}]})", "synths[0].notes[0].step: 16 is outside 0 .. 15");
  bad(R"({"kind":"groove","name":"x","synths":[{"track":"bass","notes":[{"step":0}]}]})", "needs either \"degree\"");
  bad(R"({"kind":"groove","name":"x","synths":[{"track":"bass","notes":[{"step":0,"degree":0},{"step":0,"degree":0}]}]})", "listed twice");
  bad(R"({"kind":"groove","name":"x","synths":[{"notes":[]}]})", "synths[0]: needs a \"track\"");
  bad(R"({"kind":"groove","name":"x","synths":[{"track":"bass"},{"track":"bass"}]})", "synths[1].track: this track is listed twice");
  bad(R"({"kind":"instrument","name":"x","cutoff":"bright"})", "cutoff: must be a number");
  bad(R"({"kind":"instrument","name":"x","lfo":{"target":"filter"}})", "lfo.target: \"filter\" is not one of: off, cutoff, pitch, volume");
  bad(R"({"kind":"instrument","name":"x","track":{"fx":[{"amount":0.5}]}})", "track.fx[0]: needs a \"type\"");
  bad(R"({"kind":"instrument","name":"x","track":{"fx":[{"type":"chorus"},{"type":"phaser"},{"type":"filter"},{"type":"bitcrush"},{"type":"wah"},{"type":"gate"},{"type":"fuzz"}]}})", "6 effect slots");
  bad(R"({"kind":"drumkit","name":"x","kit":"jungle","pads":{"kick":{"synth":{"pitch":50}}}})", "the break slicer has no synthesized pads");
  bad(R"({"kind":"drumkit","name":"x","pads":{"kick":{"synth":{"pitch":5}}}})", "pads.kick.synth.pitch: 5 is outside 25 .. 1200");
  // unknown keys are not errors, they are reported
  CHECK(checkContent(R"({"kind":"instrument","name":"x","filterEnv":0.5,"lfo":{"shape":"sine"}})", err, &info));
  CHECK_EQ(int(info.warnings.size()), 2); CHECK(info.warnings[0].find("filterEnv") != std::string::npos);
  // a note outside the scale is moved and reported
  CHECK(checkContent(R"({"kind":"groove","name":"x","root":"C","scale":"major","synths":[{"track":"lead","notes":[{"step":0,"midi":61}]}]})", err, &info));
  CHECK_EQ(int(info.warnings.size()), 1); CHECK(info.warnings[0].find("not in the scale") != std::string::npos);

  // --- an instrument: a file replaces the whole sound, so the same file always gives the same sound
  auto a = std::make_unique<Project>(makeDemoProject());
  a->synths[1].fold = 0.9f; a->synths[1].chord = true;
  ContentTarget ti; ti.synth = 1;
  CHECK(applyContent(R"({"kind":"instrument","name":"Glass","engine":"fm","fm":{"ratio":3.5,"index":4},"attack":0.01})", *a, ti, err, &info));
  CHECK(a->synths[1].mode == SynthMode::Fm); CHECK(a->synths[1].fmRatio == 3.5f); CHECK(a->synths[1].fold == 0.0f); CHECK(!a->synths[1].chord);
  CHECK(a->synths[0].mode == makeDemoProject().synths[0].mode);

  // --- export and import again: instrument, drum kit and groove come back the same
  Session ses(48000.0);
  ses.loadStartPreset();
  ses.setParam(P_SYN_MODE, 2, 3); ses.setParam(P_SYN_VEC_X, 2, 0.8f); ses.setParam(P_SYN_FOLD, 2, 0.35f);
  ses.setFxSlot(3, 1, int(FxType::Phaser), 0.7f, 0.2f, true);
  ses.setDrumEngine(int(DrumEngine::EbmElectro)); ses.applyDrumSynthTemplate(0, 3); ses.setParam(P_PAD_PAN, 4, -0.5f); ses.setKitHardness(0.4f);
  ses.setParam(P_FX_DRIVE, 0, 0.15f);
  ses.toggleNote(0, 0, 4, 2, false, 1, true);
  const Project src = ses.project();
  const std::string ij = instrumentToJson(src, 2, "My \"vector\" pad", "ambient"), dj = drumkitToJson(src, "EBM plus"), gj = grooveToJson(src, 0, "Scene A");
  CHECK(checkContent(ij, err, &info)); CHECK(info.name == "My \"vector\" pad"); CHECK(info.warnings.empty());
  CHECK(checkContent(dj, err, &info)); CHECK(info.kind == "drumkit"); CHECK(info.warnings.empty());
  CHECK(checkContent(gj, err, &info)); CHECK(info.warnings.empty());
  Session dst(48000.0);
  ContentTarget t2; t2.synth = 1; t2.scene = 2;
  CHECK(dst.importContent(ij, t2, err));
  CHECK(std::memcmp(&dst.project().synths[1], &src.synths[2], sizeof(SynthParams)) != 0);        // "instrument" tag differs at most...
  { SynthParams x = dst.project().synths[1], y = src.synths[2]; x.instrument = y.instrument = InstrumentPreset::Custom;
    CHECK(x.mode == y.mode); CHECK(std::fabs(x.vecX - y.vecX) < 1e-5f); CHECK(std::fabs(x.fold - y.fold) < 1e-5f);
    CHECK(std::fabs(x.cutoff - y.cutoff) < 0.01f); CHECK(x.wave1 == y.wave1); CHECK(x.lfoTarget == y.lfoTarget); CHECK(x.octaveShift == y.octaveShift); }
  CHECK(dst.project().fx[2].slots[1].type == FxType::Phaser); CHECK(dst.project().fx[2].slots[1].bypass);
  CHECK(dst.undo()); CHECK(dst.project().synths[1].mode != SynthMode::Vector);                   // one undo step
  CHECK(dst.importContent(dj, t2, err));
  CHECK(dst.project().drumEngine == DrumEngine::EbmElectro); CHECK(dst.project().customDrums[0].enabled);
  CHECK(std::fabs(dst.project().customDrums[0].baseFreq - src.customDrums[0].baseFreq) < 1e-3f);
  CHECK(std::fabs(dst.project().pads[4].pan + 0.5f) < 1e-5f); CHECK(std::fabs(dst.project().kitHardness - 0.4f) < 1e-5f);
  CHECK(std::fabs(dst.project().fx[0].drive - 0.15f) < 1e-5f);                                   // the drive set by hand survives
  CHECK(padPeak(dst, 0) > 0.05f);                                                                // and the sounds are really loaded
  CHECK(dst.importContent(gj, t2, err));
  const Scene& g = dst.project().scenes[2];
  CHECK_EQ(g.bars, src.scenes[0].bars); CHECK(std::memcmp(&g.drums, &src.scenes[0].drums, sizeof(DrumPattern)) == 0);
  for (int y = 0; y < src.activeSynths; ++y) {
    CHECK_EQ(g.synth[y].count, src.scenes[0].synth[y].count);
    CHECK(std::memcmp(g.synth[y].notes, src.scenes[0].synth[y].notes, sizeof(Note) * size_t(g.synth[y].count)) == 0);
  }
  CHECK(dst.project().bpm == src.bpm); CHECK_EQ(dst.project().root, src.root); CHECK_EQ(dst.project().activeSynths, src.activeSynths);
  // a failed import through the session leaves no undo step behind
  Session clean(48000.0);
  CHECK(!clean.importContent("{}", ContentTarget{}, err)); CHECK(!clean.canUndo()); CHECK(!err.empty());
  // Vector Y automation survives an edit (its id is the highest one that can be automated)
  clean.setChainAuto(0, 1, 0, P_SYN_VEC_Y, 0.1f, 0.9f); clean.setParam(P_BPM, 0, 100.0f);
  CHECK_EQ(clean.project().chainAuto[0].tracks[1][0].paramId, P_SYN_VEC_Y);
  // the slicer: naming the kit that is already on keeps an own break
  Session js(48000.0);
  CHECK(js.resampleSceneToJungle(0)); const uint32_t id = js.project().breakId;
  CHECK(js.importContent(R"({"kind":"groove","name":"amen","bars":1,"drums":{"kit":"Jungle Break","steps":{"kick":"x.x.x.x.x.x.x.x."}}})", ContentTarget{}, err));
  CHECK_EQ(js.project().breakId, id); CHECK(js.activeBreak() != nullptr);
}

// Song automation reaches the effect slots and the drum hardness, and those ids work as ordinary parameters too.
static void testAutomationOfFxSlotsAndHardness() {
  using namespace ld;
  Session ses(48000.0);
  ses.setFxSlot(1, 1, int(FxType::Filter), 0.9f, 0.2f, false);
  ses.setParam(P_FX_SLOT2_AMOUNT, 1, 0.25f);
  CHECK(std::fabs(ses.project().fx[1].slots[1].amount - 0.25f) < 1e-6f); CHECK(std::fabs(ses.getParam(P_FX_SLOT2_RATE, 1) - 0.2f) < 1e-6f);
  ses.setParam(P_FX_SLOT3_RATE, 0, 5.0f); CHECK(ses.project().fx[0].slots[2].rate == 1.0f);       // clamped
  ses.setChainAuto(0, 1, 0, P_FX_SLOT2_AMOUNT, 0.05f, 1.0f); ses.setChainAuto(0, 0, 1, P_KIT_HARDNESS, 0.0f, 1.0f);
  ses.setParam(P_BPM, 0, 120.0f);                                                                  // any later edit keeps the lanes
  CHECK_EQ(ses.project().chainAuto[0].tracks[1][0].paramId, P_FX_SLOT2_AMOUNT);
  CHECK_EQ(ses.project().chainAuto[0].tracks[0][1].paramId, P_KIT_HARDNESS);
  // a filter slot on the bass, swept by the song: closed -> quiet and dull at the start, open at the end of the slot
  auto p = std::make_unique<Project>(makeDemoProject());
  p->bpm = 120; p->chainLen = 1; p->chain[0] = 0;
  for (auto& sc : p->scenes) { sc.drums = DrumPattern{}; for (auto& pat : sc.synth) pat.count = 0; sc.bars = 1; }
  for (auto& f : p->fx) { f.delaySend = 0; f.revSend = 0; }
  SynthParams sp; sp.wave1 = Wave::Saw; sp.wave2 = Wave::Saw; sp.cutoff = 9000; sp.sustain = 1.0f; sp.lfoTarget = LfoTarget::Off;
  p->synths[0] = sp;
  for (int st = 0; st < 16; st += 2) addNote(p->scenes[0].synth[0], st, 0, 2, 1);
  p->fx[1].slots[0].type = FxType::Filter; p->fx[1].slots[0].amount = 1.0f; p->fx[1].slots[0].rate = 0.1f;
  RenderOptions o; o.songMode = true; o.soloTrack = 1; o.tailSeconds = 0; o.sampleRate = 48000;
  const auto hf = [](const std::vector<float>& x, size_t a, size_t b) { double s2 = 0; for (size_t i = a + 1; i < b; ++i) { const double d = x[i] - x[i - 1]; s2 += d * d; } return std::sqrt(s2); };
  const auto flat = renderOffline(*p, o).l;
  p->chainAuto[0].tracks[1][0] = AutoLane{int16_t(P_FX_SLOT1_AMOUNT), 0.02f, 1.0f};
  const auto swept = renderOffline(*p, o).l;
  CHECK(hf(swept, 2400, 21600) < 0.5 * hf(flat, 2400, 21600));          // first quarter: filter nearly closed
  CHECK(hf(swept, 76800, 93600) > 0.7 * hf(flat, 76800, 93600));        // last part: nearly as bright as without the sweep
  // hardness on the drum track changes the drums while the slot plays
  auto d = std::make_unique<Project>(makeDemoProject());
  d->chainLen = 1; d->chain[0] = 0; d->fx[0].drive = 0.0f;
  for (auto& f : d->fx) { f.delaySend = 0; f.revSend = 0; }
  RenderOptions od; od.songMode = true; od.soloTrack = 0; od.tailSeconds = 0;
  const auto clean = renderOffline(*d, od).l;
  d->chainAuto[0].tracks[0][0] = AutoLane{int16_t(P_KIT_HARDNESS), 1.0f, 1.0f};
  const auto hard = renderOffline(*d, od).l;
  double diff = 0; for (size_t i = 0; i < clean.size(); ++i) diff += std::fabs(double(clean[i]) - hard[i]);
  CHECK(diff / double(clean.size()) > 0.005);
}

// The plugin's side of the engine: the host's clock, MIDI notes, automation and separate outputs.
static void testHostClockMidiAndOutputs() {
  const double sr = 48000.0;
  const int block = 256;
  std::vector<float> l(static_cast<size_t>(block), 0.0f), r(static_cast<size_t>(block), 0.0f);
  // renders `blocks` blocks at `bpm`, starting at quarter note `qn`; returns the position after them
  const auto run = [&](Engine& e, double qn, double bpm, int blocks, bool playing = true) {
    for (int b = 0; b < blocks; ++b) {
      e.hostTransport(playing, qn, bpm);
      e.render(l.data(), r.data(), block);
      if (playing) qn += double(block) * bpm / (60.0 * sr);
    }
    return qn;
  };

  Project p = makeDemoProject();
  p.swing = 0.0f;
  {  // follows the host's position and tempo, whatever the project's own tempo is
    Engine e(sr);
    e.setProject(p);
    double qn = run(e, 0.0, 120.0, 40);                      // 40 * 256 frames = 0.2133 s = 0.4267 quarter notes
    CHECK(e.isPlaying());
    CHECK_EQ(e.currentStep(), int(std::floor(qn * 4.0 - 1e-9)) % 16);
    qn = run(e, qn, 120.0, 400);
    CHECK_EQ(e.currentStep(), int(std::floor(qn * 4.0 - 1e-9)) % 16);
    qn = run(e, qn, 77.0, 300);                               // the host changes tempo: still in step
    CHECK_EQ(e.currentStep(), int(std::floor(qn * 4.0 - 1e-9)) % 16);
    qn = run(e, 2.5, 120.0, 3);                               // the host jumps into a step: step 10 is where it is
    CHECK_EQ(e.currentStep(), 10);
    qn = run(e, qn, 120.0, 30);
    CHECK_EQ(e.currentStep(), int(std::floor(qn * 4.0 - 1e-9)) % 16);
    run(e, qn, 120.0, 2, false);                              // the host stops
    CHECK(!e.isPlaying());
    qn = run(e, qn, 120.0, 4);                                // it plays again
    e.stop(); e.play(2);                                      // our own buttons do nothing while the host plays
    qn = run(e, qn, 120.0, 4);
    CHECK(e.isPlaying()); CHECK_EQ(e.currentScene(), 0);
    run(e, qn, 120.0, 2, false);
    CHECK(!e.isPlaying());
    e.play(0);                                                // the host stands still: PLAY starts our own transport, at the host's tempo
    run(e, qn, 120.0, 30, false);
    CHECK(e.isPlaying()); CHECK(e.currentStep() >= 0);
    qn = run(e, 1.0, 120.0, 3);                               // the host starts: it leads again, from where it is
    CHECK(e.isPlaying()); CHECK_EQ(e.currentStep(), 4);
    run(e, qn, 120.0, 2, false);
    CHECK(!e.isPlaying());
    e.play(0);
    run(e, qn, 120.0, 2, false);
    CHECK(e.isPlaying());
    e.stop();
    run(e, qn, 120.0, 2, false);
    CHECK(!e.isPlaying());
    e.hostTransportOff();
    e.play(0);
    e.render(l.data(), r.data(), block);
    CHECK(e.isPlaying());
  }
  {  // every step fires exactly once per pass, also with swing and a tempo that is not a round number
    Project q = p;
    q.swing = 0.3f;
    for (auto& sc : q.scenes) { sc = Scene(); sc.bars = 1; }
    for (int st = 0; st < 16; ++st) q.scenes[0].drums.steps[0][st] = 1;      // a kick on every sixteenth
    Engine a(sr), b(sr);
    a.setProject(q); b.setProject(q);
    std::vector<float> la, lb;
    a.play(0);                                                 // own transport at the project's tempo
    double qn = 0.0;
    for (int i = 0; i < 500; ++i) {
      a.render(l.data(), r.data(), block); la.insert(la.end(), l.begin(), l.end());
      b.hostTransport(true, qn, double(q.bpm)); b.render(l.data(), r.data(), block); lb.insert(lb.end(), l.begin(), l.end());
      qn += double(block) * double(q.bpm) / (60.0 * sr);
    }
    double diff = 0.0, sig = 0.0;
    for (size_t i = 0; i < la.size(); ++i) { diff += double(la[i] - lb[i]) * double(la[i] - lb[i]); sig += double(la[i]) * double(la[i]); }
    CHECK(sig > 1.0);
    CHECK(diff < sig * 1e-3);                                  // led by a host at the same tempo it sounds the same
  }
  {  // song mode: the host's position picks the slot of the chain
    Project q = p;
    q.chainLen = 3; q.chain[0] = 0; q.chain[1] = 1; q.chain[2] = 2;
    for (auto& sc : q.scenes) sc.bars = 1;
    Engine e(sr);
    e.setProject(q);
    e.setSongMode(true);
    run(e, 4.0 + 1.0, 120.0, 3);                              // bar 2, beat 2
    CHECK_EQ(e.chainPos(), 1); CHECK_EQ(e.currentScene(), 1); CHECK_EQ(e.currentStep(), 4);
    run(e, 12.0 + 8.0 + 0.5, 120.0, 3);                       // past the end: the chain repeats
    CHECK_EQ(e.chainPos(), 2); CHECK_EQ(e.currentStep(), 2);
  }
  {  // notes from a keyboard, separate outputs, automation
    Project q = p;
    for (auto& sc : q.scenes) sc = Scene();
    Engine e(sr);
    e.setProject(q);
    std::vector<float> tl(static_cast<size_t>(kNumTracks * block), 0.0f), tr(static_cast<size_t>(kNumTracks * block), 0.0f);
    float* pl[kNumTracks]; float* pr[kNumTracks];
    for (int t = 0; t < kNumTracks; ++t) { pl[t] = tl.data() + size_t(t) * block; pr[t] = tr.data() + size_t(t) * block; }
    double trackSum[kNumTracks] = {}, mainSum = 0.0;
    const auto go = [&](int blocks) {
      for (int t = 0; t < kNumTracks; ++t) trackSum[t] = 0.0;
      mainSum = 0.0;
      for (int b = 0; b < blocks; ++b) {
        e.setTrackOutputs(pl, pr);
        e.render(l.data(), r.data(), block);
        for (int i = 0; i < block; ++i) mainSum += std::fabs(l[i]);
        for (int t = 0; t < kNumTracks; ++t) for (int i = 0; i < block; ++i) trackSum[t] += std::fabs(pl[t][i]);
      }
    };
    e.render(l.data(), r.data(), block);
    e.rtNoteOn(1, 61, 2);                                      // C#4 on the lead: not in every scale, still plays
    go(40);
    CHECK(trackSum[2] > 1.0); CHECK(trackSum[0] == 0.0); CHECK(trackSum[1] == 0.0); CHECK(mainSum > 1.0);
    const double held = trackSum[2];
    e.rtNoteOff(1, 61);
    go(200); go(40);
    CHECK(trackSum[2] < held * 0.05);                          // released
    CHECK_EQ(e.rtPadForMidiNote(int(q.pads[0].gmNote)), 0);
    CHECK_EQ(e.rtPadForMidiNote(63), 3); CHECK_EQ(e.rtPadForMidiNote(1), -1);
    e.rtTriggerPad(0, 2);
    go(20);
    CHECK(trackSum[0] > 0.5);
    const double drumsLoud = trackSum[0];
    go(400);
    e.rtSetOverride(P_FX_LEVEL, 0, 0.0f);                      // the host pulls the drum fader down
    go(40);                                                    // the fader glides there
    e.rtTriggerPad(0, 2);
    go(20);
    CHECK(trackSum[0] < drumsLoud * 0.01);
    e.rtClearOverrides();
    go(40);
    e.rtTriggerPad(0, 2);
    go(20);
    CHECK(trackSum[0] > drumsLoud * 0.5);
    e.render(l.data(), r.data(), block);                       // without setTrackOutputs the pointers are not used again
  }
  {  // the audio thread still does not allocate
    Engine e(sr);
    e.setProject(p);
    e.render(l.data(), r.data(), block);
    g_allocs = 0; g_count = true;
    double qn = 0.0;
    for (int b = 0; b < 200; ++b) {
      e.hostTransport(true, qn, 133.0);
      if (b == 20) e.rtNoteOn(0, 40, 1);
      if (b == 60) e.rtNoteOff(0, 40);
      if (b == 80) e.rtSetOverride(P_SYN_CUTOFF, 0, 900.0f);
      e.render(l.data(), r.data(), block);
      qn += double(block) * 133.0 / (60.0 * sr);
    }
    g_count = false;
    CHECK_EQ(long(g_allocs), 0L);
  }
}

// The stompbox collection: every pedal does something, stays within bounds, dies away, and never allocates.
// Eight grooves of up to sixteen bars, six pedals, one bar going round, knob movements inside a groove,
// the song with a host, listening before choosing.
static void testPedalChain() {
  const double sr = 48000.0;
  const int n = 4800;
  std::vector<float> l(static_cast<size_t>(n)), r(static_cast<size_t>(n));
  const auto fill = [&](double hz, float amp) { for (int i = 0; i < n; ++i) l[size_t(i)] = r[size_t(i)] = amp * float(std::sin(6.283185307 * hz * double(i) / sr)); };
  const auto rms = [&]() { double s = 0.0; for (int i = n / 2; i < n; ++i) s += double(l[size_t(i)]) * double(l[size_t(i)]); return std::sqrt(s / double(n / 2)); };
  auto chain = std::make_unique<PedalChain>();
  chain->prepare(sr);
  FxSlot slots[kBoardSlots];
  // every pedal and every character, on a quiet and on a hot note: nothing blows up
  for (int type = 1; type < kNumFxTypes; ++type)
    for (int voice = 0; voice < kFxVoices[type]; ++voice)
      for (float amp : {0.05f, 0.9f}) {
        for (auto& sl : slots) sl = FxSlot();
        slots[0].type = FxType(type); slots[0].voice = uint8_t(voice); slots[0].amount = 0.8f; slots[0].rate = 0.6f; slots[0].p3 = 0.5f; slots[0].p4 = 0.7f;
        fill(200.0, amp);
        chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
        bool ok = true;
        for (int i = 0; i < n; ++i) ok = ok && std::isfinite(l[size_t(i)]) && std::fabs(l[size_t(i)]) < 8.0f && std::isfinite(r[size_t(i)]);
        CHECK(ok);
      }
  for (auto& sl : slots) sl = FxSlot();
  // an empty board changes nothing
  fill(200.0, 0.3f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  CHECK(std::fabs(rms() - 0.3 / std::sqrt(2.0)) < 1e-3);
  // the tone control in the middle is flat; LOW up lifts a low note, HIGH down takes the top away
  slots[0].type = FxType::Eq; slots[0].amount = 0.5f; slots[0].rate = 0.5f; slots[0].p3 = 0.5f; slots[0].p4 = 0.5f;
  fill(200.0, 0.3f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  CHECK(std::fabs(rms() - 0.3 / std::sqrt(2.0)) < 2e-3);
  slots[0].amount = 1.0f;
  fill(50.0, 0.1f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  CHECK(rms() > 0.1 / std::sqrt(2.0) * 2.5);
  slots[0].amount = 0.5f; slots[0].p3 = 0.0f;
  fill(9000.0, 0.1f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  CHECK(rms() < 0.1 / std::sqrt(2.0) * 0.8);
  // the speaker lets 300 Hz through and very little of 9 kHz
  slots[0] = FxSlot(); slots[0].type = FxType::Cab; slots[0].p4 = 0.3f;
  fill(300.0, 0.1f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  const double mid = rms();
  fill(9000.0, 0.1f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  CHECK(mid > 0.04 && rms() < mid * 0.35);
  // the lead channel squashes: a note ten times louder comes out less than twice as loud; the clean channel stays loud and clear
  slots[0] = FxSlot(); slots[0].type = FxType::Amp; slots[0].voice = 2; slots[0].amount = 0.7f;
  fill(200.0, 0.05f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  const double soft = rms();
  fill(200.0, 0.5f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  CHECK(soft > 0.05 && rms() < soft * 2.0 && rms() < 1.2);
  slots[0].voice = 0; slots[0].amount = 0.3f;
  fill(200.0, 0.2f); chain->process(l.data(), r.data(), n, slots, kBoardSlots, 120.0f, false, 0.0);
  std::printf("      amp clean %.3f lead soft %.3f cab mid %.3f\n", rms(), soft, mid);
  CHECK(rms() > 0.08 && rms() < 0.6);
}

static void testLfos() {
  const double sr = 48000.0;
  const int block = 256;
  std::vector<float> l(static_cast<size_t>(block), 0.0f), r(static_cast<size_t>(block), 0.0f);
  Session a(sr);
  ModLfo lf; lf.paramId = P_FX_LEVEL; lf.index = 0; lf.shape = 3; lf.sync = 0; lf.rate = 0.62f; lf.depth = 1.0f;   // a square, about 2 Hz
  a.beginEdit(); a.setParam(P_FX_LEVEL, 0, 0.5f);
  { Scene only; for (int st = 0; st < 16; ++st) only.drums.steps[3][st] = 2; a.putScene(0, only); }   // nothing but hats on every step
  a.engine().play(0);
  const auto run = [&](int blocks, double& lo, double& hi) {
    lo = 1e9; hi = 0.0;
    for (int w = 0; w < blocks / 24; ++w) {
      double sum = 0.0;
      for (int b = 0; b < 24; ++b) { a.engine().render(l.data(), r.data(), block); for (float x : l) sum += double(x) * double(x); }
      const double e = std::sqrt(sum / double(24 * block));
      if (e < lo) lo = e; if (e > hi) hi = e;
    }
  };
  double lo0, hi0, lo1, hi1;
  run(480, lo0, hi0);
  a.setLfo(0, lf);
  run(480, lo1, hi1);
  CHECK(lo0 > 0.0 && lo1 / hi1 < 0.5 * lo0 / hi0);   // with the LFO the track opens and shuts far more than without                        // with it the track opens and shuts
  CHECK(std::fabs(a.engine().lfoValue(0)) == 1.0f);
  a.undo(); CHECK_EQ(int(a.project().lfos[0].paramId), -1); a.redo();
  ModLfo two; two.paramId = P_SYN_CUTOFF; two.index = 1; two.shape = 4; two.rate = 0.5f; two.depth = 0.3f;
  a.setLfo(2, two);
  for (int id : {int(P_SYN_CUTOFF), int(P_FX_SLOT3_AMOUNT), int(P_FX_EQ_HIGH), int(P_SYN_FOLD)}) {   // nothing blows up, whatever the target
    ModLfo x; x.paramId = int16_t(id); x.index = 1; x.depth = 1.0f; x.sync = 0; x.rate = 1.0f; a.setLfo(1, x);
    for (int b = 0; b < 40; ++b) { a.engine().render(l.data(), r.data(), block); for (float v : l) CHECK(std::isfinite(v) && std::fabs(v) < 4.0f); }
  }
  const char* path = "/tmp/ld_test_lfo.ldp";
  CHECK(a.save(path));
  Session b(sr);
  CHECK(b.load(path));
  CHECK(b.project().lfos[0].paramId == P_FX_LEVEL && b.project().lfos[0].shape == 3 && b.project().lfos[0].sync == 0 && b.project().lfos[0].depth == 1.0f);
  CHECK(b.project().lfos[2].paramId == P_SYN_CUTOFF && b.project().lfos[2].index == 1 && b.project().lfos[2].rate == 0.5f);
  CHECK(lfoBeats(0.0f) == 32.0f && lfoBeats(1.0f) == 0.25f && lfoHz(0.0f) < 0.06f && lfoHz(1.0f) > 19.0f && lfoHz(1.0f) < 21.0f);
  Scene sc; sc.bars = 99; sc.drums.steps[0][0] = 2;
  b.putScene(5, sc); CHECK_EQ(b.project().scenes[5].bars, kMaxBars); CHECK_EQ(b.project().scenes[5].drums.steps[0][0], 2);
}

static void testGroovesLanesLoopAndPreview() {
  const double sr = 48000.0;
  const int block = 256;
  std::vector<float> l(static_cast<size_t>(block), 0.0f), r(static_cast<size_t>(block), 0.0f);
  const auto rmsOf = [&](Engine& e, int blocks) {
    double sum = 0.0;
    for (int b = 0; b < blocks; ++b) { e.render(l.data(), r.data(), block); for (float x : l) sum += double(x) * double(x); }
    return std::sqrt(sum / double(blocks * block));
  };
  {  // the file keeps all of it, and the six knobs of slot six are parameters like the others
    Session a(sr);
    a.setSceneBars(7, 16); a.setDrumStep(7, 3, 250, 2); a.toggleNote(7, 1, 255, 4, true, 1);
    a.setFxSlot(4, 5, int(FxType::Reverb), 0.3f, 0.6f, false); a.setFxVoice(4, 5, 2);
    a.beginEdit(); a.setParam(P_FX_SLOT6_P4, 4, 0.9f); a.setParam(P_FX_SLOT4_RATE, 0, 0.11f);
    CHECK(a.getParam(fxSlotParamId(5, 3), 4) == 0.9f && a.project().fx[4].slots[5].p4 == 0.9f && a.project().fx[0].slots[3].rate == 0.11f);
    for (int slot = 0; slot < kFxSlots; ++slot) for (int knob = 0; knob < 4; ++knob) {
      int sl = -1, kn = -1;
      CHECK(fxSlotParam(fxSlotParamId(slot, knob), sl, kn) && sl == slot && kn == knob);
    }
    CHECK_EQ(a.writeLanePoint(7, P_SYN_CUTOFF, 0, 64, 5000.0f), 0);
    CHECK_EQ(a.writeLanePoint(7, P_FX_SLOT6_P4, 4, 3, 0.25f), 1);
    CHECK_EQ(a.findLane(7, P_SYN_CUTOFF, 0), 0);
    CHECK(a.project().scenes[7].lanes[0].points[0] == a.getParam(P_SYN_CUTOFF, 0));      // a new lane starts as the knob stands
    a.chainAdd(7); a.chainInsert(0, 5); a.chainMove(0, 2);
    CHECK_EQ(a.project().chain[2], 5); CHECK_EQ(a.project().chain[0], 0);
    const char* path = "/tmp/ld_test_groove.ldp";
    CHECK(a.save(path));
    Session b(sr);
    CHECK(b.load(path));
    const Project& p = b.project();
    CHECK_EQ(p.scenes[7].bars, 16); CHECK_EQ(p.scenes[7].drums.steps[3][250], 2); CHECK_EQ(p.scenes[7].synth[1].notes[0].step, 255);
    CHECK(p.fx[4].slots[5].type == FxType::Reverb && p.fx[4].slots[5].voice == 2 && p.fx[4].slots[5].p4 == 0.9f);
    CHECK(p.scenes[7].lanes[0].paramId == P_SYN_CUTOFF && p.scenes[7].lanes[0].points[64] == 5000.0f);
    CHECK(p.scenes[7].lanes[1].paramId == P_FX_SLOT6_P4 && p.scenes[7].lanes[1].index == 4 && p.scenes[7].lanes[1].points[3] == 0.25f);
    CHECK_EQ(p.chain[2], 5); CHECK_EQ(p.chain[p.chainLen - 1], 7);
    b.clearLane(7, 0);
    CHECK_EQ(b.findLane(7, P_SYN_CUTOFF, 0), -1);
  }
  {  // a file from before: its drums were one record of 8 x 128 steps
    const char* path = "/tmp/ld_test_olddrums.ldp";
    FILE* f = std::fopen(path, "wb");
    CHECK(f != nullptr);
    if (f) {
      const char magic[8] = {'L', 'D', 'P', 'R', 'O', 'J', 0, 1};
      const uint32_t head[2] = {7, 0};
      std::fwrite(magic, 1, 8, f); std::fwrite(head, 4, 2, f);
      std::vector<uint8_t> drums(8 * 128, 0);
      drums[size_t(2 * 128 + 127)] = 2; drums[size_t(7 * 128 + 5)] = 1;
      const char* key = "sc1.drums";
      const uint16_t kl = 9; const uint32_t n = 1024;
      std::fwrite(&kl, 2, 1, f); std::fwrite(key, 1, kl, f); std::fwrite(&n, 4, 1, f); std::fwrite(drums.data(), 1, n, f);
      const unsigned char end[9] = {3, 0, 'e', 'n', 'd', 0, 0, 0, 0};
      std::fwrite(end, 1, 9, f); std::fclose(f);
      auto p = std::make_unique<Project>();
      CHECK(readProjectFile(path, *p, nullptr));
      CHECK_EQ(p->scenes[1].drums.steps[2][127], 2); CHECK_EQ(p->scenes[1].drums.steps[7][5], 1); CHECK_EQ(p->scenes[1].drums.steps[2][128], 0);
    }
  }
  {  // longer grooves repeat what is there; a pattern from the browser fits itself to the groove
    Session s(sr);
    s.clearScene(2); s.setSceneBars(2, 1);
    s.setDrumStep(2, 0, 4, 1); s.toggleNote(2, 0, 2, 3, false, 2);
    s.writeLanePoint(2, P_FX_LEVEL, 0, 2, 0.2f);
    s.growScene(2, 4);
    const Scene& sc = s.project().scenes[2];
    CHECK_EQ(sc.bars, 4); CHECK_EQ(sc.drums.steps[0][4 + 48], 1); CHECK_EQ(sc.drums.steps[0][5 + 48], 0);
    CHECK_EQ(sc.synth[0].count, 4); CHECK_EQ(sc.synth[0].notes[3].step, 2 + 48);
    CHECK(sc.lanes[0].points[2 + 12] == 0.2f && sc.lanes[0].points[3 + 12] != 0.2f);
    DrumPattern two;
    two.steps[1][0] = 2; two.steps[1][20] = 1;
    s.putDrums(2, two, 2);                                  // two bars into four: twice
    CHECK_EQ(s.project().scenes[2].bars, 4); CHECK_EQ(s.project().scenes[2].drums.steps[1][32], 2); CHECK_EQ(s.project().scenes[2].drums.steps[1][52], 1);
    CHECK_EQ(s.project().scenes[2].drums.steps[0][4], 0);   // the old drums are gone
    CHECK_EQ(s.project().scenes[2].synth[0].count, 4);      // the bass is not
    SynthPattern eight;
    Note n; n.step = 100; n.degree = 2;
    eight.notes[0] = n; eight.count = 1;
    s.putSynth(2, 1, eight, 8);                             // eight bars into four: the groove grows, the rest repeats
    CHECK_EQ(s.project().scenes[2].bars, 8); CHECK_EQ(s.project().scenes[2].synth[1].notes[0].step, 100);
    CHECK_EQ(s.project().scenes[2].drums.steps[1][64 + 52], 1); CHECK_EQ(s.project().scenes[2].synth[0].count, 8);
    CHECK(s.undo()); CHECK_EQ(s.project().scenes[2].bars, 4);
  }
  Project base = makeDemoProject();
  for (auto& sc : base.scenes) sc = Scene();
  base.swing = 0.0f; base.bpm = 120;
  for (auto& f : base.fx) { f.delaySend = 0.0f; f.revSend = 0.0f; }
  {  // one bar goes round: own transport, and with a host
    Project p = base;
    p.scenes[0].bars = 4;
    p.scenes[0].drums.steps[0][0] = 2;                      // a kick in bar one only
    Engine e(sr);
    e.setProject(p);
    e.setLoopBar(2);
    e.play(0);
    for (int b = 0; b < 800; ++b) { e.render(l.data(), r.data(), block); const int st = e.currentStep(); CHECK(st < 0 || (st >= 32 && st < 48)); }
    e.stop(); e.render(l.data(), r.data(), block);
    for (int b = 0; b < 200; ++b) e.render(l.data(), r.data(), block);
    e.play(0);
    CHECK(rmsOf(e, 400) < 1e-4);                            // bar three is empty
    e.setLoopBar(-1);
    CHECK(rmsOf(e, 800) > 1e-3);                            // the whole groove again: the kick is back
    e.stop(); e.render(l.data(), r.data(), block);
    e.setLoopBar(1);
    double qn = 9.0;                                        // the host stands in bar three, beat two
    for (int b = 0; b < 300; ++b) {
      e.hostTransport(true, qn, 120.0); e.render(l.data(), r.data(), block);
      qn += double(block) * 120.0 / (60.0 * sr);
      const int st = e.currentStep();
      CHECK(st >= 16 && st < 32);
      if (b == 0) CHECK_EQ(st, 16 + 4);
    }
    e.setLoopBar(9);                                        // a bar the groove does not have: the whole groove plays
    for (int b = 0; b < 3; ++b) { e.hostTransport(true, qn, 120.0); e.render(l.data(), r.data(), block); qn += double(block) * 120.0 / (60.0 * sr); }
    CHECK_EQ(e.currentStep(), int(std::floor(qn * 4.0 - 1e-9)) % 64);
  }
  {  // a knob that moves inside the groove: the drums fade over two bars, the same at every block size
    Project p = base;
    p.scenes[0].bars = 2;
    for (int st = 0; st < 32; st += 2) p.scenes[0].drums.steps[3][st] = 1;
    SceneLane& ln = p.scenes[0].lanes[0];
    ln.paramId = P_FX_LEVEL; ln.index = 0;
    for (int pt = 0; pt < kLanePoints; ++pt) ln.points[pt] = pt >= 8 ? 0.0f : 1.0f - float(pt) / 8.0f;
    const auto take = [&](int blk, std::vector<float>& out) {
      Engine e(sr);
      e.setProject(p);
      e.play(0);
      out.assign(size_t(4.0 * sr), 0.0f);                   // two bars at 120 are four seconds
      std::vector<float> rr(out.size());
      for (size_t at = 0; at < out.size(); at += size_t(blk)) e.render(out.data() + at, rr.data() + at, int(std::min(size_t(blk), out.size() - at)));
    };
    std::vector<float> a, b;
    take(64, a); take(480, b);
    CHECK(a == b);
    const auto part = [&](double from, double to) { double sum = 0.0; for (size_t i = size_t(from * sr); i < size_t(to * sr); ++i) sum += double(a[i]) * double(a[i]); return std::sqrt(sum / ((to - from) * sr)); };
    CHECK(part(0.0, 0.5) > 3.0 * part(3.0, 3.5));
    CHECK(part(1.0, 1.5) > 1.15 * part(2.0, 2.5) && part(2.0, 2.5) > 1.3 * part(3.0, 3.5));
    p.scenes[0].lanes[0].paramId = -1;
    take(64, b);
    CHECK(part(3.0, 3.5) < 0.4 * std::sqrt([&] { double sum = 0.0; for (size_t i = size_t(3.0 * sr); i < size_t(3.5 * sr); ++i) sum += double(b[i]) * double(b[i]); return sum / (0.5 * sr); }()));
  }
  {  // the song with a host: after the last groove it is silent, and it comes back when the host goes back
    Project p = base;
    p.scenes[0].bars = 1; p.scenes[1].bars = 1;
    for (int st = 0; st < 16; st += 4) { p.scenes[0].drums.steps[0][st] = 2; p.scenes[1].drums.steps[1][st] = 2; }
    p.chain[0] = 0; p.chain[1] = 1; p.chainLen = 2;         // two bars = eight quarter notes
    Engine e(sr);
    e.setProject(p);
    e.setSongMode(true); e.setSongLoop(false);
    double qn = 0.0;
    const auto run = [&](int blocks) {
      double sum = 0.0;
      for (int b = 0; b < blocks; ++b) {
        e.hostTransport(true, qn, 120.0); e.render(l.data(), r.data(), block);
        for (float x : l) sum += double(x) * double(x);
        qn += double(block) * 120.0 / (60.0 * sr);
      }
      return std::sqrt(sum / double(blocks * block));
    };
    CHECK(run(700) > 1e-3);                                 // 3.7 s: inside the song
    CHECK(e.isPlaying()); CHECK_EQ(e.chainPos(), 1);
    run(200);                                               // past 4 s: over
    CHECK(!e.isPlaying());
    run(600);                                               // the last hit rings out
    CHECK(run(300) < 1e-5);
    qn = 2.0;
    CHECK(run(100) > 1e-3);
    CHECK(e.isPlaying()); CHECK_EQ(e.chainPos(), 0);
    e.setSongLoop(true);                                    // as on the phone: round and round
    qn = 9.0; run(4);
    CHECK(e.isPlaying()); CHECK_EQ(e.chainPos(), 0); CHECK_EQ(e.currentStep(), 4);
  }
  {  // a bar copied and put somewhere else; meters; what is held on the whole mix while playing live
    Session s(sr);
    s.clearScene(1); s.setSceneBars(1, 3);
    s.setDrumStep(1, 2, 16 + 5, 2); s.toggleNote(1, 0, 16 + 3, 4, true, 2); s.toggleNote(1, 1, 40, 1, false, 1);
    Scene clip;
    s.copyBar(1, 1, clip);
    CHECK_EQ(clip.drums.steps[2][5], 2); CHECK_EQ(clip.synth[0].count, 1); CHECK_EQ(clip.synth[0].notes[0].step, 3); CHECK_EQ(clip.synth[1].count, 0);
    s.pasteBar(1, 2, clip, 0);                              // only the drums
    CHECK_EQ(s.project().scenes[1].drums.steps[2][32 + 5], 2); CHECK_EQ(s.project().scenes[1].synth[0].count, 1); CHECK_EQ(s.project().scenes[1].synth[1].count, 1);
    s.pasteBar(1, 2, clip, -1);                             // everything: the lead's note in bar three goes, the bass comes
    CHECK_EQ(s.project().scenes[1].synth[0].count, 2); CHECK_EQ(s.project().scenes[1].synth[0].notes[1].step, 32 + 3); CHECK_EQ(s.project().scenes[1].synth[1].count, 0);
    s.pasteBar(1, 5, clip, -1);                             // a bar the groove does not have: nothing happens
    CHECK_EQ(s.project().scenes[1].synth[0].count, 2);

    Project p = base;
    p.scenes[0].bars = 1;
    for (int st = 0; st < 16; st += 4) p.scenes[0].drums.steps[0][st] = 2;
    Engine e(sr);
    e.setProject(p);
    e.play(0);
    const double plain = rmsOf(e, 375);
    CHECK(e.trackPeak(0) > 0.05f); CHECK(e.trackPeak(2) < 1e-4f);
    e.setDjFilter(1.0f);                                    // all the way to the right: the lows are gone, and a kick is mostly lows
    rmsOf(e, 375);
    CHECK(rmsOf(e, 375) < 0.4 * plain);
    e.setDjFilter(0.0f);
    rmsOf(e, 375);
    CHECK(std::fabs(rmsOf(e, 375) - plain) < 0.05 * plain);
    e.setPerformFx(int(FxType::DubDelay), 1, 0.9f, 0.6f, 0.7f, 0.5f);
    CHECK(std::fabs(rmsOf(e, 375) - plain) < 0.05 * plain); // chosen, not held: nothing changes
    e.holdPerformFx(true);
    CHECK(rmsOf(e, 375) > 1.1 * plain);
    e.holdPerformFx(false);
    e.stop();
    rmsOf(e, 60);
    CHECK(rmsOf(e, 200) > 1e-4);                            // the echoes ring on after it is let go ...
    rmsOf(e, 2600);
    CHECK(rmsOf(e, 100) < 1e-5);                            // ... and are gone in the end
    e.setPerformFx(int(FxType::Slicer), 0, 1.0f, 0.6f, 0.4f, 0.55f);
    e.play(0);
    e.holdPerformFx(true);
    CHECK(rmsOf(e, 375) < 0.9 * plain);                     // a gate over everything
    e.holdPerformFx(false);
    rmsOf(e, 375);
    CHECK(std::fabs(rmsOf(e, 375) - plain) < 0.05 * plain);
  }
  {  // listening before choosing leaves the project alone
    Session s(sr);
    const auto before = std::make_unique<Project>(s.project());
    Scene cand;
    cand.bars = 1;
    for (int st = 0; st < 16; ++st) cand.drums.steps[4][st] = 2;
    Note n; n.step = 0; n.degree = 0; n.len = 8;
    cand.synth[0].notes[0] = n; cand.synth[0].count = 1;
    s.preview(&cand, 0);
    CHECK(s.previewing());
    CHECK(std::memcmp(static_cast<const void*>(&s.project()), static_cast<const void*>(before.get()), sizeof(Project)) == 0);
    s.engine().play(3);
    double heard = 0.0;
    for (int b = 0; b < 100; ++b) { s.engine().render(l.data(), r.data(), block); for (float x : l) heard += double(x) * double(x); }
    CHECK(heard > 1e-3);
    s.preview(nullptr);
    CHECK(!s.previewing());
    s.preview(&cand, 1);
    s.setDrumStep(0, 0, 0, 1);                              // an edit ends it too
    CHECK(!s.previewing());
  }
}

// The synths of the collection: each one sounds, each one differently, and a preset can name each of them.
static void testSynthCollection() {
  const double sr = 48000.0;
  std::vector<std::vector<float>> takes;
  for (int mode = 0; mode < kNumSynthModes; ++mode) {
    for (int shape = 0; shape < (mode == int(SynthMode::Phase) ? 4 : 1); ++shape) {
      auto p = std::make_unique<Project>(makeDemoProject());
      for (auto& sc : p->scenes) sc = Scene();
      for (auto& f : p->fx) { f = TrackFx(); f.delaySend = 0.0f; f.revSend = 0.0f; }
      SynthParams& sp = p->synths[0];
      sp = SynthParams();
      sp.mode = SynthMode(mode); sp.wave1 = Wave(shape); sp.cutoff = 6000.0f; sp.sustain = 0.8f; sp.fmIndex = 5.0f; sp.vecX = 0.7f; sp.vecY = 0.4f;
      Note n; n.step = 0; n.degree = 0; n.len = 8;
      Note m = n; m.step = 8; m.degree = 4; m.vel = 2;
      p->scenes[0].synth[0].notes[0] = n; p->scenes[0].synth[0].notes[1] = m; p->scenes[0].synth[0].count = 2;
      Engine e(sr);
      e.setProject(*p);
      e.play(0);
      std::vector<float> l(size_t(sr * 1.5)), r(l.size());
      for (size_t at = 0; at < l.size(); at += 256) e.render(l.data() + at, r.data() + at, int(std::min<size_t>(256, l.size() - at)));
      double rms = 0.0; float peak = 0.0f; bool finite = true;
      for (float x : l) { finite = finite && std::isfinite(x); rms += double(x) * double(x); peak = std::max(peak, std::fabs(x)); }
      rms = std::sqrt(rms / double(l.size()));
      CHECK(finite); CHECK(rms > 0.01); CHECK(rms < 0.6); CHECK(peak < 1.01f);
      for (const auto& other : takes) {                          // no two of them are the same sound
        double d = 0.0;
        for (size_t i = 0; i < l.size(); ++i) d += double(l[i] - other[i]) * double(l[i] - other[i]);
        CHECK(std::sqrt(d / double(l.size())) > 0.1 * rms);
      }
      takes.push_back(l);
    }
  }
  static const char* const names[kNumSynthModes] = {"subtractive", "fm", "pluck", "vector", "unison", "organ", "phase"};
  for (int mode = 0; mode < kNumSynthModes; ++mode) {
    auto p = std::make_unique<Project>(makeDemoProject());
    std::string err;
    const std::string json = std::string(R"({"kind":"instrument","name":"x","engine":")") + names[mode] + R"("})";
    CHECK(applyContent(json, *p, ContentTarget{}, err, nullptr));
    CHECK_EQ(int(p->synths[0].mode), mode);
  }
  Session s(sr);
  s.beginEdit(); s.setParam(P_SYN_MODE, 1, 6.0f);
  CHECK(s.project().synths[1].mode == SynthMode::Phase);
  CHECK(s.save("/tmp/ld_test_mode.ldp"));
  Session back(sr);
  CHECK(back.load("/tmp/ld_test_mode.ldp"));
  CHECK(back.project().synths[1].mode == SynthMode::Phase);
}

static void testPedalCollection() {
  const double sr = 48000.0;
  const int block = 256;
  Project base = makeDemoProject();
  for (auto& sc : base.scenes) sc = Scene();
  base.swing = 0.0f; base.bpm = 120;
  for (auto& f : base.fx) { f.delaySend = 0.0f; f.revSend = 0.0f; f.drive = 0.0f; for (auto& sl : f.slots) sl.type = FxType::Off; }
  SynthPattern& pat = base.scenes[0].synth[0];
  for (int st = 0; st < 16; st += 4) { Note n; n.step = uint16_t(st); n.degree = int8_t(st / 4); n.len = 2; pat.notes[pat.count++] = n; }
  for (int st = 0; st < 16; st += 4) base.scenes[0].drums.steps[0][st] = 2;

  const auto render = [&](const Project& p, double playSeconds, double tailSeconds, std::vector<float>& out, double& tailPeak) {
    Engine e(sr);
    e.setProject(p);
    e.play(0);
    std::vector<float> l(static_cast<size_t>(block), 0.0f), r(static_cast<size_t>(block), 0.0f);
    out.clear();
    for (int b = 0; b < int(playSeconds * sr / block); ++b) { e.render(l.data(), r.data(), block); out.insert(out.end(), l.begin(), l.end()); }
    e.stop();
    tailPeak = 0.0;
    const int tailBlocks = int(tailSeconds * sr / block);
    for (int b = 0; b < tailBlocks; ++b) {
      e.render(l.data(), r.data(), block);
      if (b >= tailBlocks - 20) for (float x : l) tailPeak = std::max(tailPeak, double(std::fabs(x)));
    }
  };
  std::vector<float> dry, wet;
  double tail = 0.0;
  render(base, 2.0, 1.0, dry, tail);
  double dryRms = 0.0;
  for (float x : dry) dryRms += double(x) * double(x);
  dryRms = std::sqrt(dryRms / double(dry.size()));
  CHECK(dryRms > 0.01);

  for (int type = int(FxType::Distortion); type < kNumFxTypes; ++type) {
    for (int track = 0; track < 2; ++track) {                  // on the drums and on the bass
      Project p = base;
      p.fx[track].slots[0].type = FxType(type);
      p.fx[track].slots[0].amount = 0.7f;
      p.fx[track].slots[0].rate = 0.5f;
      render(p, 2.0, 14.0, wet, tail);
      double rms = 0.0, diff = 0.0, peak = 0.0;
      bool finite = true;
      for (size_t i = 0; i < wet.size(); ++i) {
        finite = finite && std::isfinite(wet[i]);
        rms += double(wet[i]) * double(wet[i]);
        diff += double(wet[i] - dry[i]) * double(wet[i] - dry[i]);
        peak = std::max(peak, double(std::fabs(wet[i])));
      }
      rms = std::sqrt(rms / double(wet.size())); diff = std::sqrt(diff / double(wet.size()));
      if (!finite || !(diff > dryRms * 0.01) || !(rms > dryRms * 0.2) || !(peak < 1.5) || !(tail < 2e-3))
        std::printf("    pedal %d on track %d: rms %.4f (dry %.4f) diff %.4f peak %.3f tail %.5f\n", type, track, rms, dryRms, diff, peak, tail);
      CHECK(finite);
      CHECK(diff > dryRms * 0.01);                            // it changes the sound
      CHECK(rms > dryRms * 0.2);                              // without swallowing it
      CHECK(peak < 1.5);
      CHECK(tail < 2e-3);                                     // and falls silent: no feedback that runs away
    }
  }
  // every character of every pedal, with the knobs in the middle and with all of them at either end
  for (int type = 1; type < kNumFxTypes; ++type) {
    std::vector<float> first;
    for (int voice = 0; voice < int(kFxVoices[type]); ++voice) {
      for (int set = 0; set < 3; ++set) {
        Project p = base;
        FxSlot& sl = p.fx[1].slots[0];
        sl.type = FxType(type); sl.voice = uint8_t(voice);
        sl.amount = set == 0 ? 0.7f : (set == 1 ? 1.0f : 0.0f); sl.rate = set == 0 ? 0.5f : (set == 1 ? 1.0f : 0.0f);
        sl.p3 = set == 0 ? 0.5f : (set == 1 ? 1.0f : 0.0f); sl.p4 = sl.p3;
        render(p, 2.0, set == 0 ? (Pedal::rings(FxType(type)) ? 19.0 : 2.0) : 0.5, wet, tail);
        double rms = 0.0, peak = 0.0, diff = 0.0;
        bool finite = true;
        for (size_t i = 0; i < wet.size(); ++i) {
          finite = finite && std::isfinite(wet[i]);
          rms += double(wet[i]) * double(wet[i]);
          peak = std::max(peak, double(std::fabs(wet[i])));
          if (set == 0 && voice > 0) diff += double(wet[i] - first[i]) * double(wet[i] - first[i]);
        }
        rms = std::sqrt(rms / double(wet.size())); diff = std::sqrt(diff / double(wet.size()));
        const bool legacy = voice == 0 && type <= int(FxType::Compressor);   // unchanged on purpose
        const bool ok = finite && peak < 2.0 && (set != 0 || (rms > dryRms * 0.25 && (legacy || rms < dryRms * 3.0) && tail < 2e-3 && (voice == 0 || type == int(FxType::Gate) || diff > dryRms * 0.02)));
        if (std::getenv("LD_PEDAL_TABLE") && set == 0) std::printf("    %2d/%d  x%.2f  peak %.2f\n", type, voice, rms / dryRms, peak);
        if (!ok) std::printf("    pedal %d character %d knobs %d: rms %.4f (dry %.4f) peak %.3f tail %.5f differs %.4f\n", type, voice, set, rms, dryRms, peak, tail, diff);
        CHECK(ok);                       // finite, not too loud; in the middle also: about as loud as dry, silent in the end, its own sound
        if (set == 0 && voice == 0) first = wet;
      }
    }
  }
  {  // a standard pedal on its first character with the new knobs untouched is the pedal old projects have
    Session ses(sr);
    ses.setFxSlot(1, 0, int(FxType::Phaser), 0.6f, 0.4f, false);
    ses.setParam(P_FX_SLOT1_P3, 1, 0.8f); ses.setFxVoice(1, 0, 3);
    CHECK(ses.project().fx[1].slots[0].voice == 3 && ses.project().fx[1].slots[0].p3 == 0.8f);
    ses.setFxSlot(1, 0, int(FxType::Phaser), 0.2f, 0.4f, false);           // what the phone apps do when a knob moves
    CHECK(ses.project().fx[1].slots[0].voice == 3 && ses.project().fx[1].slots[0].p3 == 0.8f);
    ses.setFxVoice(1, 0, 9);
    CHECK(ses.project().fx[1].slots[0].voice == 3);
    const std::string path = "/tmp/ld_pedalvoice.ldp";
    CHECK(ses.save(path.c_str()));
    Session back(sr);
    CHECK(back.load(path.c_str()));
    CHECK(back.project().fx[1].slots[0].voice == 3 && back.project().fx[1].slots[0].p3 == 0.8f && back.project().fx[1].slots[0].p4 == 0.5f);
    ses.setFxSlot(1, 0, int(FxType::Wah), 0.2f, 0.4f, false);              // another pedal starts neutral
    CHECK(ses.project().fx[1].slots[0].voice == 0 && ses.project().fx[1].slots[0].p3 == 0.5f);
    ses.setFxVoice(1, 0, 3);
    CHECK(ses.project().fx[1].slots[0].voice == 2);                       // the wah has three
  }
  {  // every pedal has a dry/wet knob, can leave the lows alone, and some follow the tempo
    const auto diffTo = [&](const std::vector<float>& x) { double d = 0.0; for (size_t i = 0; i < x.size(); ++i) d += double(x[i] - dry[i]) * double(x[i] - dry[i]); return std::sqrt(d / double(x.size())); };
    Project p = base;
    FxSlot& sl = p.fx[1].slots[0];
    sl.type = FxType::Fuzz; sl.amount = 0.8f; sl.mix = 0.0f;
    render(p, 2.0, 0.5, wet, tail);
    CHECK(diffTo(wet) < 1e-6);                                // all the way dry: as if it were not there
    sl.mix = 1.0f;
    render(p, 2.0, 0.5, wet, tail);
    const double full = diffTo(wet);
    sl.mix = 0.5f;
    render(p, 2.0, 0.5, wet, tail);
    CHECK(diffTo(wet) > 0.2 * full && diffTo(wet) < 0.8 * full);
    sl.mix = 1.0f; sl.type = FxType::Tremolo; sl.voice = 3; sl.amount = 1.0f; sl.flags = kFxKeepSub;   // a chopper that may not touch the lows
    const auto lows = [&](const std::vector<float>& x, bool second) {   // the level below 100 Hz in one half of every beat
      double z1 = 0.0, z2 = 0.0, sum = 0.0; int count = 0;
      const double kk = 1.0 - std::exp(-6.2831853 * 60.0 / sr);
      for (size_t i = 0; i < x.size(); ++i) {
        z1 += kk * (double(x[i]) - z1); z2 += kk * (z1 - z2);
        const double beatPos = double(i) / (sr * 0.5);                  // 120 bpm
        if (i > size_t(sr * 0.5) && ((beatPos - std::floor(beatPos)) >= 0.5) == second) { sum += z2 * z2; ++count; }
      }
      return std::sqrt(sum / double(count));
    };
    sl.rate = 0.55f; sl.flags = kFxSync;                      // one chop per quarter note, locked to the beat
    Project q = p;
    q.scenes[0].synth[0] = SynthPattern();
    Note held; held.step = 0; held.degree = 0; held.len = 64;
    q.scenes[0].synth[0].notes[0] = held; q.scenes[0].synth[0].count = 1; q.scenes[0].bars = 4;
    for (auto& row : q.scenes[0].drums.steps) for (auto& v : row) v = 0;
    render(q, 4.0, 0.5, wet, tail);
    const auto half = [&](bool second) { double sum = 0.0; int count = 0; for (size_t i = size_t(sr * 0.5); i < wet.size(); ++i) { const double b = double(i) / (sr * 0.5); if (((b - std::floor(b)) >= 0.5) == second) { sum += double(wet[i]) * double(wet[i]); ++count; } } return std::sqrt(sum / double(count)); };
    CHECK(half(false) > 4.0 * half(true));                    // open in the first half of every beat, shut in the second
    const double shutLows = lows(wet, true);
    q.fx[1].slots[0].flags = uint8_t(kFxSync | kFxKeepSub);
    render(q, 4.0, 0.5, wet, tail);
    CHECK(lows(wet, true) > 3.0 * shutLows);                  // the same, but the lows now pass while it is shut
    CHECK(fxCanSync(FxType::Tremolo, 0) && !fxCanSync(FxType::Fuzz, 0) && !fxCanSync(FxType::Chorus, 0) && fxCanSync(FxType::Chorus, 1));

    Session ses(sr);                                          // the three of them in a project file and in a preset
    ses.setFxSlot(2, 4, int(FxType::DubDelay), 0.4f, 0.6f, false);
    ses.setFxFlags(2, 4, kFxKeepSub | kFxSync);
    ses.beginEdit(); ses.setParam(P_FX_SLOT5_MIX, 2, 0.3f);
    CHECK(ses.project().fx[2].slots[4].mix == 0.3f && ses.getParam(fxSlotParamId(4, 4), 2) == 0.3f);
    CHECK(ses.save("/tmp/ld_test_mix.ldp"));
    Session back(sr);
    CHECK(back.load("/tmp/ld_test_mix.ldp"));
    CHECK(back.project().fx[2].slots[4].type == FxType::DubDelay && back.project().fx[2].slots[4].mix == 0.3f && back.project().fx[2].slots[4].flags == (kFxKeepSub | kFxSync));
    ses.setFxSlot(2, 4, int(FxType::Cloud), 0.4f, 0.6f, false);          // another pedal starts neutral
    CHECK(ses.project().fx[2].slots[4].mix == 1.0f && ses.project().fx[2].slots[4].flags == 0);
    auto pr = std::make_unique<Project>(makeDemoProject());
    std::string err;
    CHECK(applyContent(R"({"kind":"instrument","name":"x","track":{"fx":[{"type":"shimmer","voice":2,"mix":0.25,"keepSub":true,"sync":true},{"type":"slicer"}]}})", *pr, ContentTarget{}, err, nullptr));
    CHECK(pr->fx[1].slots[0].type == FxType::Shimmer && pr->fx[1].slots[0].voice == 2 && pr->fx[1].slots[0].mix == 0.25f && pr->fx[1].slots[0].flags == (kFxKeepSub | kFxSync));
    CHECK(pr->fx[1].slots[1].type == FxType::Slicer && pr->fx[1].slots[1].mix == 1.0f);
  }
  {  // all three slots of every track busy, pedals swapped while playing: no allocation on the audio thread
    Project p = base;
    Engine e(sr);
    e.setProject(p);
    e.play(0);
    std::vector<float> l(static_cast<size_t>(block), 0.0f), r(static_cast<size_t>(block), 0.0f);
    e.render(l.data(), r.data(), block);
    std::vector<Project> variants;
    for (int v = 0; v < 6; ++v) {
      Project q = base;
      for (int t = 0; t < kNumTracks; ++t)
        for (int sl = 0; sl < kFxSlots; ++sl) { q.fx[t].slots[sl].type = FxType(1 + (v * 5 + t * 3 + sl) % (kNumFxTypes - 1)); q.fx[t].slots[sl].voice = uint8_t((v + sl) % kFxVoices[int(q.fx[t].slots[sl].type)]); }
      variants.push_back(q);
    }
    long during = 0;
    for (int v = 0; v < 6; ++v) {
      e.setProject(variants[size_t(v)]);                      // the control thread may allocate
      g_allocs = 0; g_count = true;
      for (int b = 0; b < 60; ++b) e.render(l.data(), r.data(), block);
      g_count = false;
      during += long(g_allocs);
      e.collectGarbage();
    }
    CHECK_EQ(during, 0L);
  }
  {  // a preset file can name the new pedals, and unknown numbers from a newer build are switched off
    Project p = base;
    p.fx[1].slots[1].type = FxType::TapeEcho;
    const std::string json = instrumentToJson(p, 0, "Echo Bass");
    CHECK(json.find("tapeecho") != std::string::npos);
    Project q = base;
    std::string err;
    CHECK(applyContent(json, q, ContentTarget(), err));
    CHECK(q.fx[1].slots[1].type == FxType::TapeEcho);
    q.fx[2].slots[0].type = FxType(200);
    sanitize(q);
    CHECK(q.fx[2].slots[0].type == FxType::Off);
  }
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
    {"slide, accent, fold", testSlideAccentFold},
    {"pluck and vector engines, named project copy", testPluckAndVector},
    {"hard drum kits and the drum synth", testHardDrumEnginesAndCustomDrumSynth},
    {"drum synth pads are project state", testDrumSynthIsProjectState},
    {"presets and grooves as JSON", testContentJson},
    {"automation of effect slots and drum hardness", testAutomationOfFxSlotsAndHardness},
    {"host clock, MIDI notes, automation and separate outputs", testHostClockMidiAndOutputs},
    {"the synth collection", testSynthCollection},
    {"the stompbox collection", testPedalCollection},
    {"grooves, bar loop, knob movements, song with a host, preview", testGroovesLanesLoopAndPreview},
    {"three LFOs on knobs of your choice", testLfos},
    {"a pedal board for sound from outside; amp, cab and tone control", testPedalChain},
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
