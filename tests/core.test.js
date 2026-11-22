// Unit tests for the pure logic in prototype/index.html (<script id="core">).
// Run: node --test tests/core.test.js
const test = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const html = fs.readFileSync(path.join(__dirname, '..', 'prototype', 'index.html'), 'utf8');
const src = html.match(/<script id="core">([\s\S]*?)<\/script>/)[1];
const C = {};
vm.createContext(C);
vm.runInContext(src, C);

function parseMidi(u8) {
  let o = 0;
  const str = n => { const s = String.fromCharCode(...u8.slice(o, o + n)); o += n; return s; };
  const u32 = () => { const v = (u8[o] << 24) | (u8[o + 1] << 16) | (u8[o + 2] << 8) | u8[o + 3]; o += 4; return v; };
  const u16 = () => { const v = (u8[o] << 8) | u8[o + 1]; o += 2; return v; };
  assert.equal(str(4), 'MThd'); assert.equal(u32(), 6);
  const format = u16(), ntrks = u16(), tpq = u16(), tracks = [];
  for (let t = 0; t < ntrks; t++) {
    assert.equal(str(4), 'MTrk');
    const len = u32(), end = o + len; let tick = 0; const evs = [];
    while (o < end) {
      let d = 0, b; do { b = u8[o++]; d = (d << 7) | (b & 0x7f); } while (b & 0x80);
      tick += d;
      const s = u8[o++];
      if (s === 0xFF) { const type = u8[o++], len = u8[o++]; evs.push({ tick, meta: type, data: Array.from(u8.slice(o, o + len)) }); o += len; }
      else { evs.push({ tick, status: s, note: u8[o++], vel: u8[o++] }); }
    }
    assert.equal(o, end, 'track length matches its header');
    tracks.push(evs);
  }
  assert.equal(o, u8.length, 'no trailing bytes');
  return { format, ntrks, tpq, tracks };
}

test('scale degrees map to the right notes', () => {
  const st = C.defaultState();
  assert.deepEqual([0,1,2,3,4,5,6,7].map(d => C.noteName(C.degreeToMidi(st, d))), ['D3','E3','F3','G3','A3','A#3','C4','D4']);
  st.scale = 'major'; st.root = 0;
  assert.deepEqual([0,1,2,3,4,5,6,7].map(d => C.degreeToMidi(st, d)), [48,50,52,53,55,57,59,60]);
  st.scale = 'pentatonic';
  assert.equal(C.degreeToMidi(st, 5), 60, 'pentatonic wraps after five notes');
});

test('chord mode stays inside the scale for every degree and scale', () => {
  for (const scale of Object.keys(C.SCALES)) for (let root = 0; root < 12; root++) {
    const st = C.defaultState(); st.scale = scale; st.root = root; st.chord = true;
    const allowed = new Set(C.SCALES[scale].map(i => (i + root) % 12));
    for (let d = 0; d < 8; d++) {
      const ch = C.chordMidis(st, d);
      assert.equal(ch.length, 3);
      ch.forEach(m => assert.ok(allowed.has(m % 12), `${scale} root ${root} degree ${d}: ${m} out of scale`));
    }
  }
  const st = C.defaultState(); st.chord = true;   // D minor: degree 2 is F, must be F major (F A C), not F minor
  assert.deepEqual(Array.from(C.chordMidis(st, 2)), [53, 57, 60]);
  st.chord = false;
  assert.deepEqual(Array.from(C.chordMidis(st, 2)), [53]);
});

test('swing delays only the off-beat sixteenths', () => {
  const st = C.defaultState(); st.swing = 0.3;
  assert.equal(C.swingOffset(st, 0), 0); assert.equal(C.swingOffset(st, 1), 0.3);
  assert.equal(C.swingOffset(st, 2), 0); assert.equal(C.swingOffset(st, 15), 0.3);
  const ev = C.buildEvents(st, [0, 1]);
  assert.equal(ev.totalSteps, 32);
  assert.ok(ev.events.some(e => e.pos === 16), 'second bar starts at step 16');
  assert.ok(ev.events.every(e => Number.isInteger(e.pos) || Math.abs((e.pos % 1) - 0.3) < 1e-9));
});

test('MIDI export is a valid format-1 file with GM drums, tempo and chords', () => {
  const st = C.defaultState(); st.bpm = 120;
  const m = parseMidi(C.buildMidi(st, [1]));
  assert.equal(m.format, 1); assert.equal(m.ntrks, 3); assert.equal(m.tpq, 480);
  const tempo = m.tracks[0].find(e => e.meta === 0x51).data;
  assert.equal((tempo[0] << 16) | (tempo[1] << 8) | tempo[2], 500000);
  const drumOn = m.tracks[1].filter(e => (e.status & 0xF0) === 0x90);
  assert.ok(drumOn.every(e => (e.status & 0x0F) === 9), 'drums on channel 10');
  assert.deepEqual([...new Set(drumOn.map(e => e.note))].sort((a, b) => a - b), [36, 38, 42], 'kick, snare, closed hat as GM notes');
  assert.equal(drumOn.filter(e => e.note === 36).length, 5);
  assert.deepEqual(drumOn.filter(e => e.note === 38).map(e => e.tick), [480, 1440], 'snare on beats 2 and 4');
  const synOn = m.tracks[2].filter(e => (e.status & 0xF0) === 0x90);
  assert.equal(synOn.length, 6);
  assert.equal(m.tracks[2].filter(e => (e.status & 0xF0) === 0x80).length, 6, 'every note is released');

  st.chord = true;
  assert.equal(parseMidi(C.buildMidi(st, [1])).tracks[2].filter(e => (e.status & 0xF0) === 0x90).length, 18, 'chords are written out');
  const song = parseMidi(C.buildMidi(st, st.chain));
  assert.ok(Math.max(...song.tracks[1].map(e => e.tick)) > 3 * 1920, 'song export spans all four bars');
});

test('MIDI export applies swing and accents', () => {
  const st = C.defaultState(); st.swing = 0.5;
  st.scenes[0] = C.emptyScene(); st.scenes[0].drums[0][1] = 2;
  const on = parseMidi(C.buildMidi(st, [0])).tracks[1].find(e => (e.status & 0xF0) === 0x90);
  assert.equal(on.tick, 180, 'step 2 moves from tick 120 to 180 at full swing');
  assert.equal(on.vel, 127);
});

test('WAV encoder writes a correct 16-bit stereo header and samples', () => {
  const L = new Float32Array([0, 1, -1, 0.5]), R = new Float32Array([0, -1, 1, 2]);
  const v = new DataView(C.encodeWav([L, R], 48000));
  const tag = o => String.fromCharCode(v.getUint8(o), v.getUint8(o + 1), v.getUint8(o + 2), v.getUint8(o + 3));
  assert.equal(tag(0), 'RIFF'); assert.equal(tag(8), 'WAVE'); assert.equal(tag(36), 'data');
  assert.equal(v.getUint16(22, true), 2); assert.equal(v.getUint32(24, true), 48000);
  assert.equal(v.getUint32(28, true), 192000); assert.equal(v.getUint16(34, true), 16);
  assert.equal(v.getUint32(40, true), 16); assert.equal(v.byteLength, 60);
  assert.equal(v.getInt16(44 + 4, true), 32767); assert.equal(v.getInt16(44 + 6, true), -32767);
  assert.equal(v.getInt16(44 + 14, true), 32767, 'values above 1.0 are clipped');
});

test('live hits snap to the nearest step', () => {
  const log = [{ step: 3, time: 1.0, scene: 0 }, { step: 4, time: 1.125, scene: 0 }];
  let q = C.quantizeHit(1.04, log, 0.125);
  assert.equal(q.step, 3); assert.equal(q.predicted, false); assert.equal(q.future, false);
  q = C.quantizeHit(1.10, log, 0.125);
  assert.equal(q.step, 4); assert.equal(q.future, true);
  q = C.quantizeHit(1.22, log, 0.125);
  assert.equal(q.step, 5); assert.equal(q.predicted, true);
  q = C.quantizeHit(5.0, [{ step: 15, time: 4.95, scene: 2 }], 0.125);
  assert.equal(q.step, 15);
  q = C.quantizeHit(5.07, [{ step: 15, time: 4.95, scene: 2 }], 0.125);
  assert.equal(q.step, 0, 'wraps to the next bar');
  assert.equal(C.quantizeHit(1, [], 0.125), null);
});

test('saved projects load, broken ones are rejected, missing fields are filled', () => {
  const st = C.defaultState();
  const round = C.migrateState(JSON.parse(JSON.stringify(st)));
  assert.deepEqual(JSON.parse(JSON.stringify(round)), JSON.parse(JSON.stringify(st)));
  assert.equal(C.migrateState(null), null);
  assert.equal(C.migrateState({ v: 1 }), null);
  const bad = JSON.parse(JSON.stringify(st)); bad.scenes[2].drums.pop();
  assert.equal(C.migrateState(bad), null);
  const old = JSON.parse(JSON.stringify(st)); delete old.synth.lfoDepth; delete old.fx; old.chain = [9]; old.bpm = 999; old.scale = 'nope';
  const m = C.migrateState(old);
  assert.equal(m.synth.lfoDepth, 0.45); assert.equal(m.fx.tracks.length, 2);
  assert.deepEqual(Array.from(m.chain), [0]); assert.equal(m.bpm, 240); assert.equal(m.scale, 'minor');
});

test('drive curve is linear at zero and bounded when driven', () => {
  const lin = C.makeDriveCurve(0, 9);
  assert.ok(Math.abs(lin[0] + 1) < 1e-6 && Math.abs(lin[4]) < 1e-6 && Math.abs(lin[8] - 1) < 1e-6 && Math.abs(lin[6] - 0.5) < 1e-6);
  const hot = C.makeDriveCurve(100, 1025);
  assert.ok(Array.from(hot).every(x => x >= -1.0001 && x <= 1.0001));
  assert.ok(hot[600] > 0.9, 'small signals are pushed up');
});
