import SwiftUI
import UniformTypeIdentifiers

/// DRUMS: 1. the 8x16 grid, 2. pads and tone, 3. the kit's sound, the slicer and the drum synth.
struct DrumsScreen: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let wide: Bool
    @State private var showBreakImporter = false

    private var curEng: Int { min(max(Int(state.bridge.getParam(ParamId.DRUM_ENGINE, index: 0).rounded()), 0), DRUM_ENGINES.count - 1) }
    private var isJungle: Bool { curEng == 4 }
    private var accent: Color { isJungle ? skin.secondary : skin.primary }
    private var bars: Int { max(1, Int(state.bridge.sceneBars(state.scene))) }
    private var bar: Int { min(max(Int(state.drumBar), 0), bars - 1) }
    private var grid: [UInt8] { [UInt8](state.bridge.drumGrid(forScene: state.scene)) }
    private var pad: Int { min(max(Int(state.selPad), 0), 7) }
    private var padLabel: String { isJungle ? "Slice \(pad + 1)" : PAD_NAMES[pad] }
    private var playingBar: Int { state.playing ? Int(state.step) / 16 : -1 }
    private var effectiveLayer: Int { min(max(state.drumLayer, 0), 2) }

    var body: some View {
        Group {
            if wide {
                Panes(left: {
                    LayerSwitcher(layers: ["PADS & TONE", "SOUND & SYNTH"], selected: effectiveLayer == 2 ? 1 : 0, accent: accent) {
                        state.drumLayer = $0 == 1 ? 2 : 1
                    }
                    if effectiveLayer == 2 { soundSection } else { padsSection }
                }, right: {
                    gridPanel
                    quickTrimPanel
                })
            } else {
                VStack(spacing: 8) {
                    LayerSwitcher(layers: ["1. GRID 8x16", "2. PADS", isJungle ? "3. SLICER" : "3. SYNTH"], selected: effectiveLayer, accent: accent) {
                        state.drumLayer = $0
                    }
                    if effectiveLayer == 0 {
                        kitPanel
                        gridPanel
                        selectedTrackPanel
                    } else if effectiveLayer == 1 {
                        padsSection
                    } else {
                        soundSection
                    }
                }
            }
        }
        .fileImporter(isPresented: $showBreakImporter, allowedContentTypes: [.wav, .audio]) { result in
            if case .success(let url) = result { loadBreak(url) }
        }
    }

    // ---- actions ----

    private func bounce() {
        state.drumMsg = state.bridge.bounceScene(state.scene) ? "" : "Nothing to bounce: the scene needs drum steps and must be shorter than 24 s."
        state.rev += 1
    }

    private func loadBreak(_ url: URL) {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let tmp = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("break-import.wav")
        try? FileManager.default.removeItem(at: tmp)
        do {
            try FileManager.default.copyItem(at: url, to: tmp)
            state.drumMsg = state.bridge.loadJungleBreakWav(tmp.path) ? "" : "This file could not be used. It needs to be a WAV file."
        } catch {
            state.drumMsg = "The file could not be read."
        }
        state.rev += 1
    }

    private func tapStep(pad p: Int, step st: Int, value v: Int) {
        if state.drumRollMode > 0 {
            state.bridge.setDrumStepScene(state.scene, pad: Int32(p), step: Int32(st), value: v == Int(state.drumRollMode) ? 0 : state.drumRollMode)
        } else {
            _ = state.bridge.toggleStepScene(state.scene, pad: Int32(p), step: Int32(st), accent: state.accent)
        }
        state.rev += 1
    }

    private func nextRollMode() {
        state.drumRollMode = state.drumRollMode == 0 ? 3 : (state.drumRollMode == 3 ? 4 : (state.drumRollMode == 4 ? 5 : 0))
    }

    // ---- pieces ----

    /// The row of kits. It scrolls to the kit in use.
    private var kitStrip: some View {
        let cur = curEng
        return ScrollViewReader { proxy in
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 4) {
                    SilkLabel(text: "KIT", size: 9)
                    ForEach(0..<DRUM_ENGINES.count, id: \.self) { idx in
                        HwButton(text: DRUM_ENGINES[idx], on: cur == idx, ledColor: idx == 4 ? skin.secondary : skin.primary, height: 28) {
                            if cur != idx { state.bridge.setDrumEngine(Int32(idx)); state.drumMsg = ""; state.rev += 1 }
                        }
                        .fixedSize(horizontal: true, vertical: false)
                        .id(idx)
                    }
                }
            }
            .onAppear { proxy.scrollTo(cur, anchor: .center) }
            .onChange(of: cur) { v in withAnimation { proxy.scrollTo(v, anchor: .center) } }
        }
    }

    @ViewBuilder
    private var drumMessage: some View {
        if !state.drumMsg.isEmpty {
            Text(state.drumMsg)
                .font(.system(size: 10, weight: .regular))
                .foregroundColor(skin.tertiary)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private var waveform: some View {
        let peaks = state.bridge.padWave(Int32(pad), count: 96).map { $0.floatValue }
        let s0 = state.bridge.getParam(ParamId.PAD_SLICE_START, index: state.selPad)
        let s1 = state.bridge.getParam(ParamId.PAD_SLICE_END, index: state.selPad)
        return WaveformDisplay(peaks: peaks, sliceStart: s0, sliceEnd: s1, color: accent)
    }

    private var kitPanel: some View {
        ModulePanel(isJungle ? "Kit: Break Slicer" : "Kit: \(DRUM_ENGINES[curEng])", accent: accent) {
            kitStrip
            HStack(spacing: 6) {
                if isJungle {
                    HwButton(text: "WRITE BREAK", ledColor: skin.secondary, height: 34) { state.bridge.writeBreakPattern(state.scene); state.rev += 1 }
                    HwButton(text: "BOUNCE \(SCENE_NAMES[Int(state.scene)])", ledColor: skin.tertiary, height: 34) { bounce() }
                } else {
                    RotaryKnob(state: state, label: "Hardness", paramId: ParamId.KIT_HARDNESS, index: 0, minVal: 0, maxVal: 1, fmt: pct)
                    RotaryKnob(state: state, label: "\(padLabel) Vol", paramId: ParamId.PAD_VOL, index: state.selPad, minVal: 0, maxVal: 1, fmt: pct)
                }
            }
            drumMessage
        }
    }

    private var rollLabel: String {
        state.drumRollMode == 3 ? "2X" : (state.drumRollMode == 4 ? "3X" : (state.drumRollMode == 5 ? "4X" : "ROLL"))
    }

    private var rollAndAccent: some View {
        HStack(spacing: 4) {
            HwButton(text: rollLabel, on: state.drumRollMode > 0, ledColor: skin.secondary, height: 24) { nextRollMode() }.frame(width: 52)
            HwButton(text: "ACC", on: state.accent, ledColor: skin.tertiary, height: 24) { state.accent.toggle() }.frame(width: 44)
        }
    }

    /// The numbers 1 to 4 over the beats, and a dot where the song is.
    private var beatRuler: some View {
        HStack(spacing: 2) {
            SilkLabel(text: "TRK", color: skin.muted, size: 8).frame(width: 34)
            ForEach(0..<16, id: \.self) { stInBar in
                let st = bar * 16 + stInBar
                let head = state.playing && state.step == Int32(st)
                ZStack {
                    if head {
                        Circle().fill(skin.positive).frame(width: 6, height: 6)
                    } else if stInBar % 4 == 0 {
                        SilkLabel(text: "\(stInBar / 4 + 1)", color: skin.silk, size: 8, mono: true)
                    } else {
                        Circle().fill(skin.muted.opacity(0.4)).frame(width: 2.5, height: 2.5)
                    }
                }
                .frame(maxWidth: .infinity, minHeight: 10, maxHeight: 10)
            }
        }
    }

    private func gridRow(_ p: Int, _ bytes: [UInt8]) -> some View {
        let rowCol = PAD_COLORS[p]
        let selected = pad == p
        return HStack(spacing: 2) {
            Button {
                state.selPad = Int32(p)
                state.bridge.triggerPad(Int32(p), vel: state.vel())
            } label: {
                SilkLabel(text: (isJungle ? "S\(p + 1)" : PAD_SHORT[p]).uppercased(), color: selected ? skin.onAccentInk : skin.silk, size: 8, mono: true)
                    .frame(width: 34, height: 24)
                    .background(selected ? rowCol : skin.buttonOff)
                    .cornerRadius(4)
            }.buttonStyle(.plain)
            ForEach(0..<16, id: \.self) { stInBar in
                gridCell(p, stInBar, bytes, rowCol)
            }
        }
    }

    private func gridCell(_ p: Int, _ stInBar: Int, _ bytes: [UInt8], _ rowCol: Color) -> some View {
        let st = bar * 16 + stInBar
        let idx = p * 128 + st
        let v = idx < bytes.count ? Int(bytes[idx]) : 0
        let head = state.playing && state.step == Int32(st)
        let bg: Color = v >= 3 ? skin.secondary : (v == 2 ? rowCol : (v == 1 ? rowCol.opacity(0.75) : skin.recess))
        let edge: Color = head ? skin.positive : (v == 2 ? Color.white : skin.line.opacity(0.6))
        return Button {
            state.selPad = Int32(p)
            tapStep(pad: p, step: st, value: v)
        } label: {
            ZStack {
                RoundedRectangle(cornerRadius: 3).fill(bg)
                if v >= 3 { SilkLabel(text: "\(v - 1)", color: skin.onAccentInk, size: 7, mono: true) }
            }
            .frame(maxWidth: .infinity, minHeight: 24, maxHeight: 24)
            .overlay(RoundedRectangle(cornerRadius: 3).stroke(edge, lineWidth: head ? 1.5 : 0.7))
            .padding(.leading, stInBar > 0 && stInBar % 4 == 0 ? 2 : 0)
        }.buttonStyle(.plain)
    }

    private var gridPanel: some View {
        let bytes = grid
        return ModulePanel("Drum Grid 8x16 • Bar \(bar + 1)/\(bars)", accent: accent, trailing: { rollAndAccent }) {
            BarPicker(bars: bars, current: bar, playingBar: playingBar) { state.drumBar = Int32($0) }
            beatRuler
            VStack(spacing: 3) {
                ForEach(0..<8, id: \.self) { p in gridRow(p, bytes) }
            }
        }
    }

    private var selectedTrackPanel: some View {
        ModulePanel("Selected Track: \(padLabel)", accent: PAD_COLORS[pad]) {
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Pitch", paramId: ParamId.PAD_PITCH, index: state.selPad, minVal: -12, maxVal: 12) { signed($0, "st") }
                RotaryKnob(state: state, label: "Decay", paramId: ParamId.PAD_DECAY, index: state.selPad, minVal: 0.03, maxVal: 1.5, fmt: secs)
                RotaryKnob(state: state, label: "Pan", paramId: ParamId.PAD_PAN, index: state.selPad, minVal: -1, maxVal: 1, fmt: panText)
            }
        }
    }

    private var quickTrimPanel: some View {
        ModulePanel("Quick Trim: \(padLabel)", accent: accent) {
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Vol", paramId: ParamId.PAD_VOL, index: state.selPad, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Pitch", paramId: ParamId.PAD_PITCH, index: state.selPad, minVal: -12, maxVal: 12) { signed($0, "st") }
                RotaryKnob(state: state, label: "Decay", paramId: ParamId.PAD_DECAY, index: state.selPad, minVal: 0.03, maxVal: 1.5, fmt: secs)
                if !isJungle {
                    RotaryKnob(state: state, label: "Hardness", paramId: ParamId.KIT_HARDNESS, index: 0, minVal: 0, maxVal: 1, fmt: pct)
                }
            }
        }
    }

    private func livePad(_ i: Int32) -> some View {
        let on = state.selPad == i
        return PressPad(onDown: {
            state.bridge.triggerPad(i, vel: state.vel())
            state.selPad = i
        }) {
            VStack {
                HStack {
                    Circle().fill(on ? PAD_COLORS[Int(i)] : skin.ledOff).frame(width: 7, height: 7)
                    Spacer()
                }
                Spacer()
                SilkLabel(text: isJungle ? "S\(i + 1)" : PAD_NAMES[Int(i)].uppercased(), color: skin.padInk, size: 10)
            }
            .padding(6)
            .frame(maxWidth: .infinity, minHeight: 56, maxHeight: 56)
            .background(skin.padSurface)
            .overlay(RoundedRectangle(cornerRadius: 9).stroke(on ? PAD_COLORS[Int(i)] : skin.line, lineWidth: on ? 2 : 1))
            .cornerRadius(9)
        }
    }

    private var padSelectorRow: some View {
        HStack(spacing: 3) {
            ForEach(0..<8, id: \.self) { i in
                HwButton(text: isJungle ? "S\(i + 1)" : PAD_SHORT[i], on: pad == i, ledColor: accent, height: 30) { state.selPad = Int32(i) }
            }
        }
    }

    private func stepStripRow(_ row: Int, _ bytes: [UInt8]) -> some View {
        HStack(spacing: 4) {
            ForEach(0..<8, id: \.self) { col in
                let stInBar = row * 8 + col
                let st = bar * 16 + stInBar
                let idx = pad * 128 + st
                let curV = idx < bytes.count ? Int(bytes[idx]) : 0
                Tr808StepButton(stepInBar: stInBar, vel: curV, playhead: state.playing && state.step == Int32(st)) {
                    tapStep(pad: pad, step: st, value: curV)
                }
            }
        }
    }

    /// Layer 2: the pads with their tone controls, and the step strip of the chosen pad.
    @ViewBuilder
    private var padsSection: some View {
        let bytes = grid
        ModulePanel("Live Pads & Tone: \(padLabel)", accent: accent) {
            kitStrip
            waveform
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Vol", paramId: ParamId.PAD_VOL, index: state.selPad, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Pitch", paramId: ParamId.PAD_PITCH, index: state.selPad, minVal: -12, maxVal: 12) { signed($0, "st") }
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Decay", paramId: ParamId.PAD_DECAY, index: state.selPad, minVal: 0.03, maxVal: 1.5, fmt: secs)
                RotaryKnob(state: state, label: "Pan", paramId: ParamId.PAD_PAN, index: state.selPad, minVal: -1, maxVal: 1, fmt: panText)
            }
            if !isJungle {
                ChoiceRow(state: state, label: "Choke", paramId: ParamId.PAD_CHOKE, index: state.selPad, options: ["Off", "Grp 1", "Grp 2"], accent: skin.primary)
            }
            HStack(spacing: 7) { ForEach(0..<4, id: \.self) { col in livePad(Int32(col)) } }
            HStack(spacing: 7) { ForEach(0..<4, id: \.self) { col in livePad(Int32(4 + col)) } }
        }
        ModulePanel("808 Step Strip: \(padLabel)", accent: skin.primary, trailing: { rollAndAccent }) {
            padSelectorRow
            BarPicker(bars: bars, current: bar, playingBar: playingBar) { state.drumBar = Int32($0) }
            stepStripRow(0, bytes)
            stepStripRow(1, bytes)
        }
    }

    private var slicerControls: some View {
        let ownBreak = state.bridge.hasOwnBreak()
        let revOn = state.bridge.getParam(ParamId.PAD_REVERSE, index: state.selPad) > 0.5
        return VStack(spacing: 6) {
            HStack(spacing: 5) {
                HwButton(text: "BUILT-IN", on: !ownBreak, ledColor: skin.secondary, height: 30) {
                    if ownBreak { state.bridge.setDrumEngine(4); state.drumMsg = ""; state.rev += 1 }
                }
                HwButton(text: "LOAD WAV", ledColor: skin.secondary, height: 30) { showBreakImporter = true }
                HwButton(text: "BOUNCE \(SCENE_NAMES[Int(state.scene)])", ledColor: skin.tertiary, height: 30) { bounce() }
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Repitch", paramId: ParamId.JUNGLE_PITCH, index: 0, minVal: -12, maxVal: 12) { signed($0, "st") }
                RotaryKnob(state: state, label: "Tight", paramId: ParamId.JUNGLE_TIGHT, index: 0, minVal: 0.08, maxVal: 1.5, fmt: secs)
                RotaryKnob(state: state, label: "Crunch", paramId: ParamId.JUNGLE_CRUNCH, index: 0, minVal: 0, maxVal: 1, fmt: pct)
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Slice S", paramId: ParamId.PAD_SLICE_START, index: state.selPad, minVal: 0, maxVal: 0.95, fmt: pct)
                RotaryKnob(state: state, label: "Slice E", paramId: ParamId.PAD_SLICE_END, index: state.selPad, minVal: 0.05, maxVal: 1, fmt: pct)
            }
            HStack(spacing: 6) {
                HwButton(text: revOn ? "REVERSE ON" : "REVERSE", on: revOn, ledColor: skin.secondary, height: 34, showLed: true) {
                    state.bridge.beginEdit(); state.bridge.setParam(ParamId.PAD_REVERSE, index: state.selPad, value: revOn ? 0 : 1); state.rev += 1
                }
                HwButton(text: "WRITE BREAK", ledColor: skin.secondary, height: 34) { state.bridge.writeBreakPattern(state.scene); state.rev += 1 }
            }
        }
    }

    private func dk(_ label: String, _ id: Int32, _ lo: Float, _ hi: Float, _ fmt: @escaping (Float) -> String) -> some View {
        let p = Int32(pad)
        return RotaryKnob(state: state, label: label, paramId: id, index: p, minVal: lo, maxVal: hi, fmt: fmt, onEnd: { state.bridge.triggerPad(p, vel: 2) })
    }

    private var drumSynthPanel: some View {
        let p = Int32(pad)
        let padSynth = state.bridge.padIsSynth(p)
        return ModulePanel("Drum Synth: \(padLabel)", accent: skin.tertiary, trailing: {
            HwButton(text: padSynth ? "USE KIT SOUND" : "KIT SOUND", ledColor: skin.primary, enabled: padSynth, height: 24) {
                state.bridge.clearCustomPad(p); state.rev += 1; state.bridge.triggerPad(p, vel: 2)
            }.fixedSize(horizontal: true, vertical: false)
        }) {
            HStack(spacing: 3) {
                ForEach(0..<8, id: \.self) { i in
                    HwButton(text: PAD_SHORT[i], on: pad == i, ledColor: skin.tertiary, height: 26) {
                        state.selPad = Int32(i); state.bridge.triggerPad(Int32(i), vel: 2)
                    }
                }
            }
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 4) {
                    SilkLabel(text: "START", size: 9)
                    ForEach(0..<DSYN_TEMPLATES.count, id: \.self) { idx in
                        HwButton(text: DSYN_TEMPLATES[idx], ledColor: skin.primary, height: 28) {
                            state.bridge.applyDrumSynthTemplate(p, template: Int32(idx)); state.rev += 1
                        }.fixedSize(horizontal: true, vertical: false)
                    }
                }
            }
            HStack(spacing: 6) { dk("Pitch", ParamId.DSYN_FREQ, 25, 1200) { "\(Int($0.rounded()))Hz" }; dk("Drop", ParamId.DSYN_DROP, 0, 600) { "\(Int($0.rounded()))Hz" } }
            HStack(spacing: 6) { dk("Drop Time", ParamId.DSYN_PDEC, 0.005, 0.30, millis); dk("Length", ParamId.DSYN_LEN, 0.05, 1.5, secs) }
            HStack(spacing: 6) { dk("Tone", ParamId.DSYN_TONE, 0, 1, pct); dk("Noise", ParamId.DSYN_NOISE, 0, 1, pct) }
            HStack(spacing: 6) { dk("Noise Hz", ParamId.DSYN_NFREQ, 300, 10000, whole); dk("Click", ParamId.DSYN_CLICK, 0, 1, pct) }
            HStack(spacing: 6) {
                dk("FM", ParamId.DSYN_FM, 0, 8, oneDecimal)
                dk("FM Ratio", ParamId.DSYN_FMRATIO, 0.5, 8) { String(format: "%.1fx", $0) }
                dk("Drive", ParamId.DSYN_DRIVE, 0, 1, pct)
            }
            Text(padSynth ? "This pad plays its own synthesized drum. It stays when you change the kit."
                          : "This pad plays the kit's sound. Pick a start or move a control to give it its own drum.")
                .font(.system(size: 9))
                .foregroundColor(skin.muted)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    /// Layer 3: the character of the kit or the break slicer, and the drum synth of the chosen pad.
    @ViewBuilder
    private var soundSection: some View {
        ModulePanel(isJungle ? "Break Slicer: \(padLabel)" : "Kit Character & Slicer", accent: accent) {
            kitStrip
            waveform
            if isJungle {
                slicerControls
            } else {
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Hardness", paramId: ParamId.KIT_HARDNESS, index: 0, minVal: 0, maxVal: 1, fmt: pct)
                    HwButton(text: "BOUNCE LOOP TO SLICER", ledColor: skin.secondary, height: 46) { bounce() }
                }
            }
            drumMessage
        }
        if !isJungle { drumSynthPanel }
    }
}
