// End-to-end check of prototype/index.html in headless Chrome.
// Run: NODE_PATH=$(npm root -g) node tests/browser.test.js   (needs playwright and a Chrome binary)
const { chromium } = require('playwright');
const assert = require('node:assert');
const path = require('path');
const fs = require('fs');

const url = 'file://' + path.join(__dirname, '..', 'prototype', 'index.html');
const shots = process.env.SHOTS_DIR;
const sleep = ms => new Promise(r => setTimeout(r, ms));
const results = [];
async function step(name, fn) { await fn(); results.push(name); console.log('ok  ' + name); }

(async () => {
  const browser = await chromium.launch({
    executablePath: process.env.CHROME_BIN || '/usr/bin/google-chrome',
    args: ['--autoplay-policy=no-user-gesture-required']
  });
  const ctx = await browser.newContext({ viewport: { width: 390, height: 844 }, hasTouch: true, acceptDownloads: true });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', e => errors.push(String(e)));
  page.on('console', m => { if (m.type() === 'error') errors.push(m.text()); });
  await page.goto(url);
  const S = () => page.evaluate(() => JSON.parse(JSON.stringify(window.__pm.state())));
  const count = (st, sc, kind) => st.scenes[sc][kind].flat().filter(Boolean).length;

  await step('loads with the demo groove', async () => {
    const st = await S();
    assert.equal(st.v, 2); assert.equal(count(st, 0, 'drums'), 8);
    if (shots) await page.screenshot({ path: path.join(shots, 'drums.png') });
  });

  await step('plays, and the playhead follows the audio clock', async () => {
    await page.click('#playBtn');
    const seen = new Set();
    for (let i = 0; i < 30; i++) {
      const s = await page.evaluate(() => { const e = document.querySelector('.cell.playhead'); return e ? e.dataset.step : null; });
      if (s !== null) seen.add(s);
      await sleep(60);
    }
    assert.ok(seen.size >= 6, 'playhead visited ' + seen.size + ' steps');
    assert.equal(await page.evaluate(() => window.__pm.playing()), true);
  });

  await step('no step tap is lost while playing', async () => {
    await page.click('#laneChips [data-lane="5"]');           // silent select: Rim (empty lane in scene A)
    const before = await S();
    assert.equal(before.scenes[0].drums[5].filter(Boolean).length, 0);
    for (let s = 0; s < 16; s++) await page.click('#drumGrid .cell[data-step="' + s + '"]');
    const st = await S();
    assert.equal(st.scenes[0].drums[5].filter(Boolean).length, 16);
  });

  await step('undo and redo', async () => {
    for (let i = 0; i < 16; i++) await page.click('#undoBtn');
    assert.equal((await S()).scenes[0].drums[5].filter(Boolean).length, 0);
    await page.click('#redoBtn');
    assert.equal((await S()).scenes[0].drums[5].filter(Boolean).length, 1);
    await page.click('#undoBtn');
  });

  await step('live recording writes quantized notes', async () => {
    await page.click('#recBtn');
    for (let i = 0; i < 4; i++) { await page.dispatchEvent('.drum-pad[data-idx="6"]', 'pointerdown'); await sleep(170); }
    const tom = (await S()).scenes[0].drums[6].filter(Boolean).length;
    assert.ok(tom >= 3 && tom <= 4, 'tom steps recorded: ' + tom);
    await page.click('#recBtn');
    await page.dispatchEvent('.drum-pad[data-idx="7"]', 'pointerdown');
    assert.equal((await S()).scenes[0].drums[7].filter(Boolean).length, 0, 'nothing is recorded with REC off');
  });

  await step('scene change waits for the bar line', async () => {
    await page.click('#sceneStrip [data-scene="1"]');
    assert.equal(await page.locator('.scene-pill.queued').count(), 1);
    assert.equal(await page.evaluate(() => window.__pm.scene()), 0, 'not switched immediately');
    await page.waitForFunction(() => window.__pm.scene() === 1, null, { timeout: 4000 });
    assert.equal(await page.locator('.scene-pill.queued').count(), 0);
  });

  await step('slider change, chord mode and scale are stored', async () => {
    await page.click('.nav-btn[data-tab="synth"]');
    await page.selectOption('#scaleSel', 'major');
    await page.click('#chordBtn');
    await page.dispatchEvent('.scale-pad[data-deg="2"]', 'pointerdown');
    const cut = page.locator('#synthCtrls input').first();
    await cut.dispatchEvent('pointerdown');
    await cut.evaluate(el => { el.value = 900; el.dispatchEvent(new Event('input', { bubbles: true })); });
    await page.click('#assignBtn');
    await page.click('#synthCtrls [data-mod="amp"] .ctrl-label');
    const st = await S();
    assert.equal(st.scale, 'major'); assert.equal(st.chord, true); assert.equal(st.synth.cutoff, 900); assert.equal(st.synth.lfoTarget, 'amp');
    if (shots) { await page.screenshot({ path: path.join(shots, 'synth.png') }); await page.click('#rollBtn'); await page.screenshot({ path: path.join(shots, 'roll.png') }); }
  });

  await step('stops, and survives a reload', async () => {
    await page.click('#playBtn');
    assert.equal(await page.evaluate(() => window.__pm.playing()), false);
    await sleep(400);
    const before = await S();
    await page.reload();
    assert.deepEqual(await S(), before);
    assert.equal(await page.evaluate(() => window.__pm.scene()), 1);
  });

  await step('WAV render uses the real engine: timing, swing, stereo, repeatable to 16 bit', async () => {
    const r = await page.evaluate(async () => {
      const st = defaultState(); st.bpm = 120; st.swing = 0.5;
      st.scenes[0] = emptyScene(); st.scenes[0].drums[0][0] = 2; st.scenes[0].drums[0][1] = 2; st.scenes[0].drums[0][4] = 2;
      st.fx.tracks[0].delaySend = 0; st.fx.tracks[0].revSend = 0; st.pads[0].decay = 0.05;   // short kick, so hits do not overlap
      const buf = await renderOffline(st, [0], []);
      const x = buf.getChannelData(0), on = []; let quietUntil = 0;
      for (let i = 0; i < x.length; i++) if (i > quietUntil && Math.abs(x[i]) > 0.02) { on.push(i / buf.sampleRate); quietUntil = i + 0.12 * buf.sampleRate; }
      const full = defaultState();
      const a = await renderOffline(full, [2, 2], []), b = await renderOffline(full, [2, 2], []);
      let maxDiff = 0, peak = 0, diffLR = 0;
      const aL = a.getChannelData(0), aR = a.getChannelData(1), bL = b.getChannelData(0);
      for (let i = 0; i < aL.length; i++) { maxDiff = Math.max(maxDiff, Math.abs(aL[i] - bL[i])); peak = Math.max(peak, Math.abs(aL[i])); diffLR = Math.max(diffLR, Math.abs(aL[i] - aR[i])); }
      const mute = defaultState(); mute.fx.tracks[1].level = 0;
      const c = await renderOffline(mute, [2, 2], []); let differs = false; const cL = c.getChannelData(0);
      for (let i = 0; i < aL.length; i++) if (Math.abs(aL[i] - cL[i]) > 1e-3) { differs = true; break; }
      return { on, sr: buf.sampleRate, ch: buf.numberOfChannels, maxDiff, peak, diffLR, differs, dur: a.duration };
    });
    assert.equal(r.sr, 48000); assert.equal(r.ch, 2);
    assert.ok(r.on.length >= 3, 'onsets found: ' + r.on.length);
    // A sine kick needs a few ms to rise above the detection threshold, so compare spacing between hits.
    const near = (a, b) => Math.abs(a - b) < 0.002;
    assert.ok(r.on[0] >= 0.02 && r.on[0] < 0.03, 'first kick at ' + r.on[0]);
    assert.ok(near(r.on[1] - r.on[0], 0.125 * 1.5), 'swung kick after ' + (r.on[1] - r.on[0]));
    assert.ok(near(r.on[2] - r.on[0], 0.5), 'beat 2 kick after ' + (r.on[2] - r.on[0]));
    // Chrome's offline renders differ by a few 1e-6 between runs; that is below one 16-bit step (1.5e-5).
    assert.ok(r.maxDiff < 1.5e-5, 'two renders match to within one 16-bit step: ' + r.maxDiff);
    assert.ok(r.peak > 0.2 && r.peak <= 1.0, 'peak ' + r.peak);
    assert.ok(r.diffLR > 0.001, 'reverb makes it true stereo');
    assert.ok(r.differs, 'muting the synth changes the render');
  });

  await step('exports download as valid files', async () => {
    await page.click('.nav-btn[data-tab="song"]');
    if (shots) await page.screenshot({ path: path.join(shots, 'song.png') });
    let [d] = await Promise.all([page.waitForEvent('download'), page.click('#exportMidiBtn')]);
    assert.equal(d.suggestedFilename(), 'loopdraft_scene_B.mid');
    assert.equal(fs.readFileSync(await d.path()).subarray(0, 4).toString(), 'MThd');
    await page.click('#songModeBtn');
    [d] = await Promise.all([page.waitForEvent('download', { timeout: 30000 }), page.click('#exportWavBtn')]);
    assert.equal(d.suggestedFilename(), 'loopdraft_song.wav');
    const wav = fs.readFileSync(await d.path());
    assert.equal(wav.subarray(0, 4).toString(), 'RIFF');
    const secs = (wav.length - 44) / (48000 * 4);
    assert.ok(Math.abs(secs - (4 * 16 * 60 / 122 / 4 + 2.5)) < 0.01, 'song length ' + secs);
    await page.click('#songModeBtn');
  });

  await step('copy, clear and reset can all be undone', async () => {
    const before = await S();
    await page.click('#copyRow [data-copy="3"]');
    assert.deepEqual((await S()).scenes[3], before.scenes[1]);
    await page.click('#clearSceneBtn');
    assert.equal(count(await S(), 1, 'drums'), 0);
    await page.click('#resetBtn');
    assert.equal((await S()).scale, 'minor');
    for (let i = 0; i < 3; i++) await page.click('#undoBtn');
    assert.deepEqual(await S(), before);
  });

  await step('touch targets are large enough', async () => {
    await page.click('.nav-btn[data-tab="drums"]');
    await page.click('#laneModeBtn');
    if (shots) await page.screenshot({ path: path.join(shots, 'lanes.png') });
    const sizes = await page.evaluate(() => ['.lanes .cell', '.drum-pad', '.scene-pill', '#playBtn', '.nav-btn'].map(s => {
      const r = document.querySelector(s).getBoundingClientRect(); return [s, Math.round(r.width), Math.round(r.height)];
    }));
    sizes.forEach(([s, w, h]) => assert.ok(w >= 30 && h >= 30, s + ' is ' + w + 'x' + h));
    console.log('    sizes', JSON.stringify(sizes));
    await page.click('.nav-btn[data-tab="fx"]');
    if (shots) await page.screenshot({ path: path.join(shots, 'fx.png') });
  });

  assert.deepEqual(errors, [], 'no console errors');
  console.log('\n' + results.length + ' browser checks passed, no console errors');
  await browser.close();
})().catch(e => { console.error('FAILED:', e.message); process.exit(1); });
