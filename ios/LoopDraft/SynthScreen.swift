import SwiftUI

/// SYNTH: 1. play and piano roll, 2. the engine and its filter, 3. envelope, LFO and wavefolder.
struct SynthScreen: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let wide: Bool

    private var activeSynths: Int { min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4) }
    private var syn: Int32 { min(max(state.synth, 0), Int32(activeSynths - 1)) }
    private var trackName: String { TRACK_NAMES[Int(syn) + 1] }
    private var synMode: Int { min(max(Int(state.bridge.getParam(ParamId.SYN_MODE, index: syn).rounded()), 0), 3) }
    private var lfoTarget: Int { Int(state.bridge.getParam(ParamId.SYN_LFO_TARGET, index: syn).rounded()) }
    private var bars: Int { max(1, Int(state.bridge.sceneBars(state.scene))) }
    private var page: Int { min(max(Int(state.rollPage), 0), bars * 2 - 1) }
    private var synColor: Color { SYNTH_COLORS[Int(syn) % 4] }

    var body: some View {
        if wide {
            Panes(left: {
                quickTonePanel
                sculptPanel
                deepModPanel
            }, right: {
                playPanel
            })
        } else {
            VStack(spacing: 8) {
                LayerSwitcher(layers: ["1. PLAY & ROLL", "2. SCULPT", "3. MOD & DEEP"], selected: state.synthLayer, accent: skin.secondary) {
                    state.synthLayer = $0
                }
                if state.synthLayer == 0 {
                    quickTonePanel
                    playPanel
                } else if state.synthLayer == 1 {
                    sculptPanel
                } else {
                    deepModPanel
                }
            }
        }
    }

    // ---- pieces ----

    private var trackSelectorRow: some View {
        let n = activeSynths
        let cur = syn
        return ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                ForEach(0..<n, id: \.self) { idx in
                    HwButton(text: TRACK_NAMES[idx + 1].uppercased(), on: cur == Int32(idx), ledColor: SYNTH_COLORS[idx], height: 28) {
                        state.synth = Int32(idx)
                    }.fixedSize(horizontal: true, vertical: false)
                }
                if n < 4 {
                    HwButton(text: "+TRK", ledColor: skin.positive, height: 28) {
                        state.bridge.setActiveSynths(Int32(n + 1))
                        state.synth = Int32(n)
                        state.rev += 1
                    }.fixedSize(horizontal: true, vertical: false)
                }
            }
        }
    }

    private var quickTonePanel: some View {
        let curInst = Int(state.bridge.getParam(ParamId.SYN_INSTRUMENT, index: syn).rounded())
        let modeName = ["SUB", "FM", "PLUCK", "VECTOR"][synMode]
        let s = syn
        return ModulePanel("Quick Tone: \(trackName) (\(modeName))", accent: skin.secondary) {
            trackSelectorRow
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 4) {
                    SilkLabel(text: "INST", size: 9)
                    ForEach(0..<INSTRUMENTS.count, id: \.self) { idx in
                        HwButton(text: INSTRUMENTS[idx], on: curInst == idx, ledColor: skin.tertiary, height: 26) {
                            state.bridge.applyInstrumentSynth(s, preset: Int32(idx)); state.rev += 1
                        }.fixedSize(horizontal: true, vertical: false)
                    }
                }
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: s, minVal: 80, maxVal: 8000, modulated: lfoTarget == 1, fmt: whole)
                RotaryKnob(state: state, label: "Warmth", paramId: ParamId.SYN_WARMTH, index: s, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Motion", paramId: ParamId.SYN_MOTION, index: s, minVal: 0, maxVal: 1, fmt: pct)
            }
        }
    }

    private var octaveButtons: some View {
        let s = Int(syn)
        let octVal = state.inputOctave[s]
        return HStack(spacing: 4) {
            HwButton(text: "OCT-", enabled: octVal > -2, height: 24) { state.inputOctave[s] = max(-2, octVal - 1) }.frame(width: 46)
            HwButton(text: "OCT+", enabled: octVal < 2, height: 24) { state.inputOctave[s] = min(2, octVal + 1) }.frame(width: 46)
        }
    }

    private var playModeRow: some View {
        let s = syn
        let chord = state.bridge.getParam(ParamId.SYN_CHORD, index: s) > 0.5
        return HStack(spacing: 5) {
            HwButton(text: chord ? "CHORD ON" : "CHORD", on: chord, ledColor: skin.tertiary, height: 30) {
                state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_CHORD, index: s, value: chord ? 0 : 1); state.rev += 1
            }
            HwButton(text: "ACCENT", on: state.accent, ledColor: skin.primary, height: 30) { state.accent.toggle() }
            HwButton(text: "SLIDE", on: state.slide, ledColor: skin.tertiary, height: 30) {
                state.slide.toggle()
                if state.slide { state.rollMode = true }
            }
            HwButton(text: state.rollMode ? "PADS" : "ROLL", on: state.rollMode, ledColor: skin.secondary, height: 30) { state.rollMode.toggle() }
        }
    }

    private func scalePad(_ deg: Int32) -> some View {
        let s = syn
        let name = noteName(state.bridge.degreeMidi(forSynth: s, degree: deg))
        return PressPad(onDown: {
            state.bridge.noteOnSynth(s, degree: deg, vel: state.vel())
        }, onUp: {
            state.bridge.noteOffSynth(s, degree: deg)
        }) {
            SilkLabel(text: name, color: skin.padInk, size: 12, mono: true)
                .frame(maxWidth: .infinity, minHeight: 50, maxHeight: 50)
                .background(skin.padSurface)
                .cornerRadius(9)
        }
    }

    private var playHint: String {
        if state.slide { return "SLIDE is on: tap a note to glide in (yellow bar). Tap again to clear." }
        if state.rollMode { return "Ghost layers show Drums + other Synths at \(Int((state.ghostAlpha * 100).rounded()))% alpha (^/v = octave offset)." }
        return "OCT-/+ shifts only \(trackName) without altering other scenes."
    }

    private var playPanel: some View {
        let s = syn
        let degOff = state.degreeOffset(s)
        let octVal = state.inputOctave[Int(s)]
        return ModulePanel("Play \(trackName) • OCT \(octVal >= 0 ? "+\(octVal)" : "\(octVal)")", accent: synColor, trailing: { octaveButtons }) {
            playModeRow
            if !state.rollMode {
                HStack(spacing: 6) { ForEach(0..<4, id: \.self) { col in scalePad(degOff + Int32(4 + col)) } }
                HStack(spacing: 6) { ForEach(0..<4, id: \.self) { col in scalePad(degOff + Int32(col)) } }
            } else {
                ghostPianoRoll(syn: s, degOff: degOff, bars: bars, page: page, activeSynths: activeSynths)
            }
            Text(playHint)
                .font(.system(size: 9))
                .foregroundColor(state.slide ? skin.tertiary : skin.muted)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var engineButtons: some View {
        let s = syn
        let cur = synMode
        let names = ["SUBTRACTIVE", "FM 2-OP", "PLUCK", "VECTOR"]
        return HStack(spacing: 4) {
            ForEach(0..<names.count, id: \.self) { m in
                HwButton(text: names[m], on: cur == m, ledColor: skin.secondary, height: 28) {
                    state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_MODE, index: s, value: Float(m)); state.rev += 1
                }
            }
        }
    }

    @ViewBuilder
    private var subKnobs: some View {
        let s = syn
        ChoiceRow(state: state, label: "Osc 1", paramId: ParamId.SYN_WAVE1, index: s, options: WAVE_NAMES, accent: skin.secondary)
        ChoiceRow(state: state, label: "Osc 2", paramId: ParamId.SYN_WAVE2, index: s, options: WAVE_NAMES, accent: skin.secondary)
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: s, minVal: 80, maxVal: 8000, modulated: lfoTarget == 1, fmt: whole)
            RotaryKnob(state: state, label: "Res", paramId: ParamId.SYN_RES, index: s, minVal: 0.5, maxVal: 15, fmt: oneDecimal)
        }
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Detune", paramId: ParamId.SYN_DETUNE, index: s, minVal: -30, maxVal: 30, modulated: lfoTarget == 2) { signed($0, "c") }
            RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: s, minVal: 0, maxVal: 1, modulated: lfoTarget == 3, fmt: pct)
        }
    }

    @ViewBuilder
    private var fmKnobs: some View {
        let s = syn
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Ratio", paramId: ParamId.SYN_FM_RATIO, index: s, minVal: 0.5, maxVal: 8) { String(format: "%.1fx", $0) }
            RotaryKnob(state: state, label: "Index", paramId: ParamId.SYN_FM_INDEX, index: s, minVal: 0, maxVal: 8, fmt: oneDecimal)
        }
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "ModDec", paramId: ParamId.SYN_FM_DECAY, index: s, minVal: 0.01, maxVal: 2, fmt: secs)
            RotaryKnob(state: state, label: "Fdbk", paramId: ParamId.SYN_FM_FB, index: s, minVal: 0, maxVal: 1, fmt: pct)
        }
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: s, minVal: 80, maxVal: 8000, modulated: lfoTarget == 1, fmt: whole)
            RotaryKnob(state: state, label: "Res", paramId: ParamId.SYN_RES, index: s, minVal: 0.5, maxVal: 15, fmt: oneDecimal)
            RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: s, minVal: 0, maxVal: 1, modulated: lfoTarget == 3, fmt: pct)
        }
    }

    @ViewBuilder
    private var pluckKnobs: some View {
        let s = syn
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Bright", paramId: ParamId.SYN_PLUCK_BRIGHT, index: s, minVal: 0, maxVal: 1, fmt: pct)
            RotaryKnob(state: state, label: "Ring", paramId: ParamId.SYN_DECAY, index: s, minVal: 0.03, maxVal: 1.5) { secs($0 * 4) }
        }
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: s, minVal: 80, maxVal: 8000, modulated: lfoTarget == 1, fmt: whole)
            RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: s, minVal: 0, maxVal: 1, modulated: lfoTarget == 3, fmt: pct)
        }
    }

    @ViewBuilder
    private var vectorKnobs: some View {
        let s = syn
        let w1 = WAVE_NAMES[min(max(Int(state.bridge.getParam(ParamId.SYN_WAVE1, index: s).rounded()), 0), 3)]
        let w2 = WAVE_NAMES[min(max(Int(state.bridge.getParam(ParamId.SYN_WAVE2, index: s).rounded()), 0), 3)]
        VectorPad(state: state, synth: s, corners: ["OSC 1 \(w1)", "OSC 2 \(w2)", "FM", "SUB"])
        ChoiceRow(state: state, label: "Osc 1", paramId: ParamId.SYN_WAVE1, index: s, options: WAVE_NAMES, accent: skin.secondary)
        ChoiceRow(state: state, label: "Osc 2", paramId: ParamId.SYN_WAVE2, index: s, options: WAVE_NAMES, accent: skin.secondary)
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "FM Ratio", paramId: ParamId.SYN_FM_RATIO, index: s, minVal: 0.5, maxVal: 8) { String(format: "%.1fx", $0) }
            RotaryKnob(state: state, label: "FM Index", paramId: ParamId.SYN_FM_INDEX, index: s, minVal: 0, maxVal: 8, fmt: oneDecimal)
            RotaryKnob(state: state, label: "Detune", paramId: ParamId.SYN_DETUNE, index: s, minVal: -30, maxVal: 30, modulated: lfoTarget == 2) { signed($0, "c") }
        }
        HStack(spacing: 6) {
            RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: s, minVal: 80, maxVal: 8000, modulated: lfoTarget == 1, fmt: whole)
            RotaryKnob(state: state, label: "Res", paramId: ParamId.SYN_RES, index: s, minVal: 0.5, maxVal: 15, fmt: oneDecimal)
            RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: s, minVal: 0, maxVal: 1, modulated: lfoTarget == 3, fmt: pct)
        }
    }

    private var scope: some View {
        let s = syn
        let wave1 = Int(state.bridge.getParam(ParamId.SYN_WAVE1, index: s).rounded())
        let cutoff = state.bridge.getParam(ParamId.SYN_CUTOFF, index: s)
        let fmIndex = state.bridge.getParam(ParamId.SYN_FM_INDEX, index: s)
        let fmRatio = state.bridge.getParam(ParamId.SYN_FM_RATIO, index: s)
        return OledScopeDisplay(isFm: synMode == 1, wave: wave1, cutoffNorm: min(max((cutoff - 80) / 7920, 0.08), 1), fmIndex: fmIndex, fmRatio: fmRatio)
    }

    private var sculptPanel: some View {
        let mode = synMode
        return ModulePanel("Engine & Filter: \(trackName)", accent: skin.secondary) {
            trackSelectorRow
            scope
            engineButtons
            if mode == 2 {
                pluckKnobs
            } else if mode == 3 {
                vectorKnobs
            } else if mode == 1 {
                fmKnobs
            } else {
                subKnobs
            }
        }
    }

    private var deepModPanel: some View {
        let s = syn
        return ModulePanel("Envelope, LFO & Wavefolder", accent: skin.secondary) {
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Attack", paramId: ParamId.SYN_ATTACK, index: s, minVal: 0.002, maxVal: 0.4, fmt: millis)
                RotaryKnob(state: state, label: "Decay", paramId: ParamId.SYN_DECAY, index: s, minVal: 0.03, maxVal: 1.5, fmt: secs)
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Sustain", paramId: ParamId.SYN_SUSTAIN, index: s, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Release", paramId: ParamId.SYN_RELEASE, index: s, minVal: 0.01, maxVal: 1.5, fmt: secs)
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "LFO Hz", paramId: ParamId.SYN_LFO_RATE, index: s, minVal: 0.1, maxVal: 12, fmt: oneDecimal)
                RotaryKnob(state: state, label: "LFO Dep", paramId: ParamId.SYN_LFO_DEPTH, index: s, minVal: 0, maxVal: 1, fmt: pct)
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Fold", paramId: ParamId.SYN_FOLD, index: s, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Slide", paramId: ParamId.SYN_SLIDE_TIME, index: s, minVal: 0.01, maxVal: 0.4, fmt: millis)
            }
            ChoiceRow(state: state, label: "LFO ->", paramId: ParamId.SYN_LFO_TARGET, index: s, options: ["Off", "Cutoff", "Pitch", "Amp"], accent: skin.secondary)
        }
    }

    // ---- piano roll ----

    /// The notes of this synth that glide in, as step * 1000 + degree + 500.
    private func slideKeys(_ s: Int32) -> Set<Int> {
        let flat = state.bridge.slideNotes(scene: state.scene, synth: s).map { $0.intValue }
        var out = Set<Int>()
        var k = 0
        while k + 1 < flat.count { out.insert(flat[k] * 1000 + flat[k + 1] + 500); k += 2 }
        return out
    }

    @ViewBuilder
    private func slideMark(_ on: Bool) -> some View {
        if on {
            HStack(spacing: 0) {
                Rectangle().fill(HwColor.yellow).frame(width: 5)
                Spacer(minLength: 0)
            }
            .cornerRadius(2)
        }
    }

    private func ghostPianoRoll(syn: Int32, degOff: Int32, bars: Int, page: Int, activeSynths: Int) -> some View {
        let myNotes = state.bridge.notes(forScene: state.scene, synth: syn).map { $0.intValue }
        let slides = slideKeys(syn)
        let drumBytes = [UInt8](state.bridge.drumGrid(forScene: state.scene))
        let activeColor = SYNTH_COLORS[Int(syn) % 4]
        var otherNotes: [(Int, [Int])] = []
        for other in 0..<activeSynths where Int32(other) != syn {
            if (state.ghostSynthMask & (1 << other)) != 0 {
                otherNotes.append((other, state.bridge.notes(forScene: state.scene, synth: Int32(other)).map { $0.intValue }))
            }
        }

        return VStack(spacing: 4) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 4) {
                    SilkLabel(text: "GHOST", color: skin.muted, size: 8, mono: true)
                    HwButton(text: "DRM", on: state.ghostDrums && state.ghostAlpha > 0.05, ledColor: skin.primary, height: 24) {
                        state.ghostDrums.toggle()
                    }.fixedSize(horizontal: true, vertical: false)
                    ForEach(0..<activeSynths, id: \.self) { o in
                        if Int32(o) != syn {
                            let bit = 1 << o
                            HwButton(text: SYNTH_SHORT[o], on: (state.ghostSynthMask & bit) != 0 && state.ghostAlpha > 0.05, ledColor: SYNTH_COLORS[o], height: 24) {
                                state.ghostSynthMask ^= bit
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                    let aTxt = state.ghostAlpha < 0.05 ? "OFF" : "\(Int((state.ghostAlpha * 100).rounded()))%"
                    HwButton(text: "ALPHA \(aTxt)", on: state.ghostAlpha > 0.05, ledColor: skin.tertiary, height: 24) {
                        if state.ghostAlpha >= 0.75 { state.ghostAlpha = 0.35 }
                        else if state.ghostAlpha >= 0.55 { state.ghostAlpha = 0.80 }
                        else if state.ghostAlpha >= 0.25 { state.ghostAlpha = 0.0 }
                        else { state.ghostAlpha = 0.60 }
                    }.fixedSize(horizontal: true, vertical: false)
                }
            }

            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 4) {
                    ForEach(0..<(bars * 2), id: \.self) { pg in
                        HwButton(text: "\(pg / 2 + 1)\(pg % 2 == 0 ? "a" : "b")", on: pg == page, ledColor: skin.secondary, height: 24) {
                            state.rollPage = Int32(pg)
                        }.fixedSize(horizontal: true, vertical: false)
                    }
                }
            }

            ForEach((0..<8).reversed(), id: \.self) { i in
                let deg = degOff + Int32(i)
                let rowMidi = state.bridge.degreeMidi(forSynth: syn, degree: deg)
                HStack(spacing: 3) {
                    SilkLabel(text: noteName(rowMidi), color: skin.silk, size: 9, mono: true).frame(width: 28, alignment: .leading)
                    ForEach(0..<8, id: \.self) { col in
                        let st = page * 8 + col
                        let head = state.playing && state.step == Int32(st)
                        let vel = findNoteVel(myNotes, step: st, deg: Int(deg))
                        let ghost = state.ghostAlpha > 0.05 ? findGhost(otherNotes, step: st, rowMidi: rowMidi) : nil
                        let kick = state.ghostDrums && state.ghostAlpha > 0.05 && st < drumBytes.count && drumBytes[st] > 0
                        let snare = state.ghostDrums && state.ghostAlpha > 0.05 && (128 + st) < drumBytes.count && drumBytes[128 + st] > 0
                        let bg: Color = kick ? PAD_COLORS[0].opacity(min(0.24, state.ghostAlpha * 0.25)) : (snare ? PAD_COLORS[1].opacity(min(0.20, state.ghostAlpha * 0.20)) : skin.recess)

                        Button {
                            if state.bridge.toggleNote(scene: state.scene, synth: syn, step: Int32(st), degree: deg, accent: state.accent, slide: state.slide) && !state.slide {
                                state.bridge.noteOnSynth(syn, degree: deg, vel: state.vel())
                                DispatchQueue.main.asyncAfter(deadline: .now() + 0.15) { state.bridge.noteOffSynth(syn, degree: deg) }
                            }
                            state.rev += 1
                        } label: {
                            ZStack {
                                RoundedRectangle(cornerRadius: 4).fill(bg)
                                if let g = ghost {
                                    RoundedRectangle(cornerRadius: 3)
                                        .fill(g.0.opacity(g.2 ? state.ghostAlpha : state.ghostAlpha * 0.52))
                                        .padding(g.2 ? 1.5 : 3)
                                    if vel == 0 {
                                        SilkLabel(text: g.1, color: .white, size: 7, mono: true)
                                    }
                                }
                                if vel > 0 {
                                    RoundedRectangle(cornerRadius: 4)
                                        .fill(vel == 2 ? activeColor : activeColor.opacity(0.80))
                                }
                                slideMark(vel > 0 && slides.contains(st * 1000 + Int(deg) + 500))
                            }
                            .frame(maxWidth: .infinity, minHeight: 28, maxHeight: 28)
                            .overlay(RoundedRectangle(cornerRadius: 4).stroke(head ? skin.positive : (vel == 2 ? Color.white : skin.line.opacity(0.7)), lineWidth: head ? 2 : 1))
                        }.buttonStyle(.plain)
                    }
                }
            }

            if state.ghostDrums && state.ghostAlpha > 0.05 {
                HStack(spacing: 3) {
                    SilkLabel(text: "DRM", color: skin.primary, size: 8, mono: true).frame(width: 28, alignment: .leading)
                    ForEach(0..<8, id: \.self) { col in
                        let st = page * 8 + col
                        let k = st < drumBytes.count && drumBytes[st] > 0
                        let s = (128 + st) < drumBytes.count && drumBytes[128 + st] > 0
                        let h = (3 * 128 + st) < drumBytes.count && drumBytes[3 * 128 + st] > 0
                        HStack(spacing: 2) {
                            if k { Circle().fill(PAD_COLORS[0]).frame(width: 6, height: 6) }
                            if s { Circle().fill(PAD_COLORS[1]).frame(width: 6, height: 6) }
                            if h { Circle().fill(PAD_COLORS[3]).frame(width: 4, height: 4) }
                        }
                        .frame(maxWidth: .infinity, minHeight: 16, maxHeight: 16)
                        .background(skin.recess)
                        .cornerRadius(4)
                    }
                }
            }
        }
    }

    private func findNoteVel(_ arr: [Int], step: Int, deg: Int) -> Int {
        var k = 0
        while k + 3 < arr.count {
            if arr[k] == step && arr[k + 1] == deg { return arr[k + 3] }
            k += 4
        }
        return 0
    }

    private func findGhost(_ others: [(Int, [Int])], step: Int, rowMidi: Int32) -> (Color, String, Bool)? {
        var fallback: (Color, String, Bool)? = nil
        for (otherIdx, arr) in others {
            var k = 0
            while k + 3 < arr.count {
                let nSt = arr[k], nDeg = arr[k + 1], nLen = max(1, arr[k + 2])
                if step >= nSt && step < nSt + nLen {
                    let om = state.bridge.degreeMidi(forSynth: Int32(otherIdx), degree: Int32(nDeg))
                    if om == rowMidi {
                        return (SYNTH_COLORS[otherIdx % 4], SYNTH_TAG[otherIdx % 4], true)
                    } else if (om - rowMidi) % 12 == 0 && fallback == nil {
                        let arrow = om > rowMidi ? "^" : "v"
                        fallback = (SYNTH_COLORS[otherIdx % 4], "\(SYNTH_TAG[otherIdx % 4])\(arrow)", false)
                    }
                }
                k += 4
            }
        }
        return fallback
    }
}
