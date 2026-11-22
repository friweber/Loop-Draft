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
  for (int t = 0; t < kNumTracks; ++t) {         // one stem per track, each with sound, each different
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

static void testPatternEditing() {
  SynthPattern pat;
  CHECK(addNote(pat, 4, 2, 3, 2)); CHECK(addNote(pat, 4, 2, 1, 1)); CHECK_EQ(pat.count, 1); CHECK_EQ(pat.notes[0].len, 1);
  CHECK(!addNote(pat, kMaxSteps, 0)); CHECK(!removeNote(pat, 5, 2)); CHECK(removeNote(pat, 4, 2)); CHECK_EQ(pat.count, 0);
  for (int i = 0; i < kMaxNotes; ++i) CHECK(addNote(pat, i % kMaxSteps, i / kMaxSteps));
  CHECK(!addNote(pat, 0, 99));
  Scene s; s.bars = 99; CHECK_EQ(sceneSteps(s), kMaxSteps); s.bars = 0; CHECK_EQ(sceneSteps(s), 16);
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
    {"pattern editing helpers", testPatternEditing},
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
