import SwiftUI

private let PAD_NAMES = ["Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc"]
private let PAD_SHORT = ["Kik", "Snr", "Clp", "CH", "OH", "Rim", "Tom", "Prc"]
private let PAD_COLORS: [Color] = [
    Color(red: 0.91, green: 0.28, blue: 0.22),
    Color(red: 1.0, green: 0.54, blue: 0.14),
    Color(red: 1.0, green: 0.79, blue: 0.16),
    Color(red: 0.16, green: 0.83, blue: 0.91),
    Color(red: 0.37, green: 0.66, blue: 1.0),
    Color(red: 0.29, green: 0.88, blue: 0.54),
    Color(red: 0.75, green: 0.42, blue: 1.0),
    Color(red: 1.0, green: 0.36, blue: 0.58)
]
private let SCENE_NAMES = ["A", "B", "C", "D"]
private let TRACK_NAMES = ["Drums", "Bass", "Lead", "Syn 3", "Syn 4"]
private let SYNTH_SHORT = ["BS", "LD", "S3", "S4"]
private let SYNTH_TAG = ["B", "L", "3", "4"]
private let SYNTH_COLORS: [Color] = [
    Color(red: 1.0, green: 0.58, blue: 0.15),
    Color(red: 0.16, green: 0.83, blue: 0.91),
    Color(red: 1.0, green: 0.30, blue: 0.65),
    Color(red: 0.37, green: 0.91, blue: 0.44)
]
private let DRUM_ENGINES = ["808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break", "Hardcore Gabber", "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore"]
private let INSTRUMENTS = ["Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead"]
private let NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

private func noteName(_ midi: Int32) -> String {
    let idx = ((Int(midi) % 12) + 12) % 12
    return "\(NOTE_NAMES[idx])\(Int(midi) / 12 - 1)"
}
private func pct(_ v: Float) -> String { "\(Int((v * 100).rounded()))%" }
private func signed(_ v: Float, _ unit: String) -> String { "\(v >= 0 ? "+" : "")\(Int(v.rounded()))\(unit)" }
private func secs(_ v: Float) -> String { String(format: "%.2fs", v) }

struct ContentView: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState

    var body: some View {
        HStack(spacing: 0) {
            WalnutRail()
            VStack(spacing: 0) {
                topBar
                sceneStrip
                ScrollView {
                    VStack(spacing: 8) {
                        switch state.tab {
                        case 0: drumsTab
                        case 1: synthTab
                        case 2: fxTab
                        default: songTab
                        }
                    }
                    .padding(8)
                }
                bottomNav
            }
            .background(skin.chassis)
            .sheet(isPresented: $state.showMenu) {
                MenuSheet(state: state)
                    .environment(\.skin, SkinTheme.resolve(state.skinConfig))
            }
            WalnutRail()
        }
        .background(skin.chassis.ignoresSafeArea())
        .sheet(isPresented: $state.showSkinStudio) {
            SkinStudioSheet(state: state)
                .environment(\.skin, SkinTheme.resolve(state.skinConfig))
        }
    }

    // ---- Top Transport Bar ----
    private var topBar: some View {
        let bpm = Int(state.bridge.getParam(ParamId.BPM, index: 0).rounded())
        return VStack(spacing: 6) {
            HStack(spacing: 5) {
                VStack(alignment: .leading, spacing: 1) {
                    HStack(spacing: 0) {
                        SilkLabel(text: "LOOP", color: skin.silk, size: 15)
                        SilkLabel(text: "DRAFT", color: skin.primary, size: 15)
                    }
                }
                Spacer()
                HwButton(text: "MENU", on: state.showMenu, ledColor: skin.primary, height: 28) {
                    state.showMenu = true
                }.frame(width: 50)
                HwButton(text: "SETUP", on: state.showSkinStudio, ledColor: skin.tertiary, height: 28) {
                    state.showSkinStudio = true
                }.frame(width: 52)
                HwButton(text: "UNDO", enabled: state.bridge.canUndo(), height: 28) {
                    if state.bridge.undo() { state.rev += 1 }
                }.frame(width: 50)
                HwButton(text: "REDO", enabled: state.bridge.canRedo(), height: 28) {
                    if state.bridge.redo() { state.rev += 1 }
                }.frame(width: 50)
            }
            HStack(spacing: 6) {
                HwButton(text: state.playing ? "STOP" : "PLAY", on: state.playing, ledColor: skin.positive, height: 36, showLed: true) {
                    if state.playing { state.bridge.stopTransport() } else { state.bridge.playScene(state.scene) }
                }
                HwButton(text: "REC", on: state.rec, ledColor: skin.danger, height: 36, showLed: true) {
                    state.rec.toggle()
                    state.bridge.setRecording(state.rec)
                }.frame(width: 66)
                HwButton(text: "-", height: 36) {
                    state.bridge.beginEdit(); state.bridge.setParam(ParamId.BPM, index: 0, value: Float(max(40, bpm - 1))); state.rev += 1
                }.frame(width: 36)
                SevenSegmentBpm(bpm: bpm)
                HwButton(text: "+", height: 36) {
                    state.bridge.beginEdit(); state.bridge.setParam(ParamId.BPM, index: 0, value: Float(min(240, bpm + 1))); state.rev += 1
                }.frame(width: 36)
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
        .background(skin.faceplate)
    }

    // ---- Scene Strip ----
    private var sceneStrip: some View {
        let colors = [skin.primary, skin.secondary, skin.tertiary, skin.positive]
        return HStack(spacing: 5) {
            ForEach(0..<4, id: \.self) { i in
                let bars = state.bridge.sceneBars(Int32(i))
                HwButton(text: "\(SCENE_NAMES[i]) (\(bars)B)",
                         on: state.scene == Int32(i) || state.queued == Int32(i),
                         ledColor: state.queued == Int32(i) ? skin.tertiary : colors[i],
                         height: 30) {
                    if state.playing && state.songMode { return }
                    state.bridge.queueScene(Int32(i))
                    if !state.playing { state.scene = Int32(i); state.drumBar = 0; state.rollPage = 0 }
                }
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 5)
    }

    // ---- Bottom Nav ----
    private var bottomNav: some View {
        let items: [(String, Color)] = [("DRUMS", skin.primary), ("SYNTH", skin.secondary), ("FX", skin.tertiary), ("SONG", skin.positive)]
        return HStack(spacing: 6) {
            ForEach(0..<4, id: \.self) { i in
                HwButton(text: items[i].0, on: state.tab == i, ledColor: items[i].1, height: 38, showLed: true) {
                    state.tab = i
                }
            }
        }
        .padding(6)
        .background(skin.faceplate)
    }

    // ---- DRUMS TAB (8x16 Grid Assembler + Layered Depth) ----
    private var drumsTab: some View {
        let curEng = min(max(Int(state.bridge.getParam(ParamId.DRUM_ENGINE, index: 0).rounded()), 0), 4)
        let isJungle = curEng == 4
        let jPitch = state.bridge.getParam(ParamId.JUNGLE_PITCH, index: 0)
        let jTight = state.bridge.getParam(ParamId.JUNGLE_TIGHT, index: 0)
        let jCrunch = state.bridge.getParam(ParamId.JUNGLE_CRUNCH, index: 0)
        let revOn = state.bridge.getParam(ParamId.PAD_REVERSE, index: state.selPad) > 0.5
        let gridBytes = [UInt8](state.bridge.drumGrid(forScene: state.scene))
        let bars = max(1, Int(state.bridge.sceneBars(state.scene)))
        let bar = min(max(Int(state.drumBar), 0), bars - 1)
        let rollLbl = state.drumRollMode == 3 ? "2X" : (state.drumRollMode == 4 ? "3X" : (state.drumRollMode == 5 ? "4X" : "ROLL"))
        let accent = isJungle ? skin.secondary : skin.primary

        return VStack(spacing: 8) {
            LayerSwitcher(layers: ["1. GRID 8x16", "2. PADS", isJungle ? "3. SLICER" : "3. TONE"], selected: state.drumLayer, accent: accent) {
                state.drumLayer = $0
            }

            if state.drumLayer == 0 {
                // 8x16 Grid Assembler
                ModulePanel("Kit: \(DRUM_ENGINES[curEng])", accent: accent) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 4) {
                            ForEach(0..<DRUM_ENGINES.count, id: \.self) { idx in
                                HwButton(text: DRUM_ENGINES[idx], on: curEng == idx, ledColor: idx == 4 ? skin.secondary : skin.primary, height: 26) {
                                    state.bridge.setDrumEngine(Int32(idx)); state.rev += 1
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                    if curEng != 4 {
                        RotaryKnob(state: state, label: "Hardness", paramId: ParamId.KIT_HARDNESS, index: 0, minVal: 0, maxVal: 1, fmt: pct)
                    }
                }

                ModulePanel("Drum Grid 8x16 • Bar \(bar + 1)/\(bars)", accent: accent, trailing: {
                    HStack(spacing: 4) {
                        HwButton(text: rollLbl, on: state.drumRollMode > 0, ledColor: skin.secondary, height: 24) {
                            state.drumRollMode = state.drumRollMode == 0 ? 3 : (state.drumRollMode == 3 ? 4 : (state.drumRollMode == 4 ? 5 : 0))
                        }.frame(width: 52)
                        HwButton(text: "ACC", on: state.accent, ledColor: skin.tertiary, height: 24) {
                            state.accent.toggle()
                        }.frame(width: 44)
                    }
                }) {
                    VStack(spacing: 3) {
                        ForEach(0..<8, id: \.self) { pad in
                            let rowCol = PAD_COLORS[pad]
                            let selected = state.selPad == Int32(pad)
                            HStack(spacing: 2) {
                                Button {
                                    state.selPad = Int32(pad)
                                    state.bridge.triggerPad(Int32(pad), vel: state.vel())
                                } label: {
                                    SilkLabel(text: (isJungle ? "S\(pad + 1)" : PAD_SHORT[pad]).uppercased(), color: selected ? skin.onAccentInk : skin.silk, size: 8, mono: true)
                                        .frame(width: 34, height: 24)
                                        .background(selected ? rowCol : skin.buttonOff)
                                        .cornerRadius(4)
                                }.buttonStyle(.plain)

                                ForEach(0..<16, id: \.self) { stInBar in
                                    let st = bar * 16 + stInBar
                                    let idx = pad * 128 + st
                                    let v = idx < gridBytes.count ? Int(gridBytes[idx]) : 0
                                    let head = state.playing && state.step == Int32(st)
                                    let bg: Color = v >= 3 ? skin.secondary : (v == 2 ? rowCol : (v == 1 ? rowCol.opacity(0.75) : skin.recess))
                                    Button {
                                        state.selPad = Int32(pad)
                                        if state.drumRollMode > 0 {
                                            state.bridge.setDrumStepScene(state.scene, pad: Int32(pad), step: Int32(st), value: v == Int(state.drumRollMode) ? 0 : state.drumRollMode)
                                        } else {
                                            _ = state.bridge.toggleStepScene(state.scene, pad: Int32(pad), step: Int32(st), accent: state.accent)
                                        }
                                        state.rev += 1
                                    } label: {
                                        ZStack {
                                            RoundedRectangle(cornerRadius: 3).fill(bg)
                                            if v >= 3 { SilkLabel(text: "\(v - 1)", color: skin.onAccentInk, size: 7, mono: true) }
                                        }
                                        .frame(maxWidth: .infinity, minHeight: 24, maxHeight: 24)
                                        .overlay(RoundedRectangle(cornerRadius: 3).stroke(head ? skin.positive : (v == 2 ? Color.white : skin.line.opacity(0.6)), lineWidth: head ? 1.5 : 0.7))
                                    }.buttonStyle(.plain)
                                }
                            }
                        }
                    }
                }

                ModulePanel("Selected: \(isJungle ? "Slice \(state.selPad + 1)" : PAD_NAMES[Int(state.selPad)])", accent: PAD_COLORS[Int(state.selPad) % 8]) {
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Vol", paramId: ParamId.PAD_VOL, index: state.selPad, minVal: 0, maxVal: 1, fmt: pct)
                        RotaryKnob(state: state, label: "Pitch", paramId: ParamId.PAD_PITCH, index: state.selPad, minVal: -12, maxVal: 12) { signed($0, "st") }
                        RotaryKnob(state: state, label: "Decay", paramId: ParamId.PAD_DECAY, index: state.selPad, minVal: 0.03, maxVal: 1.5, fmt: secs)
                    }
                }
            } else if state.drumLayer == 1 {
                ModulePanel("Live Pads: \(isJungle ? "Slice \(state.selPad + 1)" : PAD_NAMES[Int(state.selPad)])", accent: accent) {
                    ForEach(0..<2, id: \.self) { row in
                        HStack(spacing: 6) {
                            ForEach(0..<4, id: \.self) { col in
                                let i = Int32(row * 4 + col)
                                Button {
                                    state.bridge.triggerPad(i, vel: state.vel())
                                    state.selPad = i
                                } label: {
                                    VStack {
                                        HStack {
                                            Circle().fill(state.selPad == i ? PAD_COLORS[Int(i)] : skin.ledOff).frame(width: 7, height: 7)
                                            Spacer()
                                        }
                                        Spacer()
                                        SilkLabel(text: isJungle ? "SLICE \(i + 1)" : PAD_NAMES[Int(i)].uppercased(), color: skin.padInk, size: 10)
                                    }
                                    .padding(6)
                                    .frame(maxWidth: .infinity, minHeight: 56, maxHeight: 56)
                                    .background(skin.padSurface)
                                    .overlay(RoundedRectangle(cornerRadius: 9).stroke(state.selPad == i ? PAD_COLORS[Int(i)] : skin.line, lineWidth: state.selPad == i ? 2 : 1))
                                    .cornerRadius(9)
                                }.buttonStyle(.plain)
                            }
                        }
                    }
                }

                ModulePanel("808 Step Strip", accent: skin.primary) {
                    ForEach(0..<2, id: \.self) { row in
                        HStack(spacing: 4) {
                            ForEach(0..<8, id: \.self) { col in
                                let stInBar = row * 8 + col
                                let st = bar * 16 + stInBar
                                let idx = Int(state.selPad) * 128 + st
                                let curV = idx < gridBytes.count ? Int(gridBytes[idx]) : 0
                                Tr808StepButton(stepInBar: stInBar, vel: curV, playhead: state.playing && state.step == Int32(st)) {
                                    if state.drumRollMode > 0 {
                                        state.bridge.setDrumStepScene(state.scene, pad: state.selPad, step: Int32(st), value: curV == Int(state.drumRollMode) ? 0 : state.drumRollMode)
                                    } else {
                                        _ = state.bridge.toggleStepScene(state.scene, pad: state.selPad, step: Int32(st), accent: state.accent)
                                    }
                                    state.rev += 1
                                }
                            }
                        }
                    }
                }
            } else {
                ModulePanel(isJungle ? "Jungle Slicer Tuning" : "Pad Tone & Slicer Bounce", accent: accent) {
                    if isJungle {
                        HStack(spacing: 5) {
                            HwButton(text: "AMEN DEFAULT", ledColor: skin.secondary, height: 28) {
                                state.bridge.setDrumEngine(4); state.rev += 1
                            }
                            HwButton(text: "BOUNCE \(SCENE_NAMES[Int(state.scene)])", ledColor: skin.tertiary, height: 28) {
                                state.bridge.resampleScene(toJungle: state.scene); state.rev += 1
                            }
                        }
                        HStack(spacing: 4) {
                            SilkLabel(text: "PITCH \(signed(jPitch, "st"))", color: skin.silk, size: 9, mono: true)
                            HwButton(text: "-", height: 24) { state.bridge.applyJungleTuningPitch(jPitch - 1, tight: jTight, crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                            HwButton(text: "+", height: 24) { state.bridge.applyJungleTuningPitch(jPitch + 1, tight: jTight, crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                            SilkLabel(text: "TIGHT \(secs(jTight))", color: skin.silk, size: 9, mono: true)
                            HwButton(text: "-", height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: max(0.08, jTight - 0.1), crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                            HwButton(text: "+", height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: min(1.5, jTight + 0.1), crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                        }
                        HStack(spacing: 6) {
                            RotaryKnob(state: state, label: "Slice S", paramId: ParamId.PAD_SLICE_START, index: state.selPad, minVal: 0, maxVal: 0.95, fmt: pct)
                            RotaryKnob(state: state, label: "Slice E", paramId: ParamId.PAD_SLICE_END, index: state.selPad, minVal: 0.05, maxVal: 1, fmt: pct)
                        }
                        HwButton(text: revOn ? "REVERSE ON" : "REVERSE", on: revOn, ledColor: skin.secondary, height: 34, showLed: true) {
                            state.bridge.beginEdit(); state.bridge.setParam(ParamId.PAD_REVERSE, index: state.selPad, value: revOn ? 0 : 1); state.rev += 1
                        }
                    } else {
                        HwButton(text: "BOUNCE LOOP -> JUNGLE SLICER", ledColor: skin.secondary, height: 34) {
                            state.bridge.resampleScene(toJungle: state.scene); state.rev += 1
                        }
                        HStack(spacing: 6) {
                            RotaryKnob(state: state, label: "Vol", paramId: ParamId.PAD_VOL, index: state.selPad, minVal: 0, maxVal: 1, fmt: pct)
                            RotaryKnob(state: state, label: "Pan", paramId: ParamId.PAD_PAN, index: state.selPad, minVal: -1, maxVal: 1) {
                                $0 < -0.02 ? "L\(Int(-$0 * 100))" : ($0 > 0.02 ? "R\(Int($0 * 100))" : "CTR")
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- SYNTH TAB (Layered Depth + High-Alpha Ghost Piano Roll) ----
    private var synthTab: some View {
        let activeSynths = min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4)
        let syn = min(state.synth, Int32(activeSynths - 1))
        let synMode = Int(state.bridge.getParam(ParamId.SYN_MODE, index: syn).rounded())
        let isFm = synMode == 1
        let wave1 = Int(state.bridge.getParam(ParamId.SYN_WAVE1, index: syn).rounded())
        let cutoff = state.bridge.getParam(ParamId.SYN_CUTOFF, index: syn)
        let fmIndex = state.bridge.getParam(ParamId.SYN_FM_INDEX, index: syn)
        let fmRatio = state.bridge.getParam(ParamId.SYN_FM_RATIO, index: syn)
        let curInst = Int(state.bridge.getParam(ParamId.SYN_INSTRUMENT, index: syn).rounded())
        let degOff = state.degreeOffset(syn)
        let octVal = state.inputOctave[Int(syn)]
        let chord = state.bridge.getParam(ParamId.SYN_CHORD, index: syn) > 0.5
        let bars = max(1, Int(state.bridge.sceneBars(state.scene)))
        let page = min(max(Int(state.rollPage), 0), bars * 2 - 1)

        return VStack(spacing: 8) {
            LayerSwitcher(layers: ["1. PLAY & ROLL", "2. SCULPT", "3. MOD & DEEP"], selected: state.synthLayer, accent: skin.secondary) {
                state.synthLayer = $0
            }

            if state.synthLayer == 0 {
                ModulePanel("Quick Tone: \(TRACK_NAMES[Int(syn) + 1])", accent: SYNTH_COLORS[Int(syn) % 4], trailing: {
                    HStack(spacing: 3) {
                        ForEach(0..<activeSynths, id: \.self) { idx in
                            HwButton(text: SYNTH_SHORT[idx], on: syn == Int32(idx), ledColor: SYNTH_COLORS[idx], height: 24) {
                                state.synth = Int32(idx)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                        if activeSynths < 4 {
                            HwButton(text: "+TRK", ledColor: skin.positive, height: 24) {
                                state.bridge.setActiveSynths(Int32(activeSynths + 1))
                                state.synth = Int32(activeSynths)
                                state.rev += 1
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 4) {
                            ForEach(0..<INSTRUMENTS.count, id: \.self) { idx in
                                HwButton(text: INSTRUMENTS[idx], on: curInst == idx, ledColor: skin.tertiary, height: 26) {
                                    state.bridge.applyInstrumentSynth(syn, preset: Int32(idx)); state.rev += 1
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: syn, minVal: 80, maxVal: 8000) { "\(Int($0.rounded()))" }
                        RotaryKnob(state: state, label: "Warmth", paramId: ParamId.SYN_WARMTH, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                        RotaryKnob(state: state, label: "Motion", paramId: ParamId.SYN_MOTION, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                    }
                }

                ModulePanel("Play \(TRACK_NAMES[Int(syn) + 1]) • OCT \(octVal >= 0 ? "+\(octVal)" : "\(octVal)")", accent: SYNTH_COLORS[Int(syn) % 4], trailing: {
                    HStack(spacing: 4) {
                        HwButton(text: "OCT-", enabled: octVal > -2, height: 24) { state.inputOctave[Int(syn)] = max(-2, octVal - 1) }.frame(width: 46)
                        HwButton(text: "OCT+", enabled: octVal < 2, height: 24) { state.inputOctave[Int(syn)] = min(2, octVal + 1) }.frame(width: 46)
                    }
                }) {
                    HStack(spacing: 5) {
                        HwButton(text: chord ? "CHORD ON" : "CHORD", on: chord, ledColor: skin.tertiary, height: 28) {
                            state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_CHORD, index: syn, value: chord ? 0 : 1); state.rev += 1
                        }
                        HwButton(text: "ACC", on: state.accent, ledColor: skin.primary, height: 28) { state.accent.toggle() }
                        HwButton(text: state.rollMode ? "PADS" : "ROLL", on: state.rollMode, ledColor: skin.secondary, height: 28) { state.rollMode.toggle() }
                    }
                    if !state.rollMode {
                        ForEach([1, 0], id: \.self) { row in
                            HStack(spacing: 6) {
                                ForEach(0..<4, id: \.self) { col in
                                    let deg = degOff + Int32(row * 4 + col)
                                    let name = noteName(state.bridge.degreeMidi(forSynth: syn, degree: deg))
                                    Button {
                                        state.bridge.noteOnSynth(syn, degree: deg, vel: state.vel())
                                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.25) {
                                            state.bridge.noteOffSynth(syn, degree: deg)
                                        }
                                    } label: {
                                        SilkLabel(text: name, color: skin.padInk, size: 12, mono: true)
                                            .frame(maxWidth: .infinity, minHeight: 50, maxHeight: 50)
                                            .background(skin.padSurface)
                                            .cornerRadius(9)
                                    }.buttonStyle(.plain)
                                }
                            }
                        }
                    } else {
                        ghostPianoRoll(syn: syn, degOff: degOff, bars: bars, page: page, activeSynths: activeSynths)
                    }
                }
            } else if state.synthLayer == 1 {
                ModulePanel("Engine & Filter: \(TRACK_NAMES[Int(syn) + 1])", accent: skin.secondary) {
                    OledScopeDisplay(isFm: isFm, wave: wave1, cutoffNorm: min(max((cutoff - 80) / 7920, 0.08), 1), fmIndex: fmIndex, fmRatio: fmRatio)
                    engineButtons(syn: syn)
                    if synMode >= 2 {
                        extraEngineKnobs(syn: syn, mode: synMode)
                    } else if !isFm {
                        HStack(spacing: 6) {
                            RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: syn, minVal: 80, maxVal: 8000) { "\(Int($0.rounded()))" }
                            RotaryKnob(state: state, label: "Res", paramId: ParamId.SYN_RES, index: syn, minVal: 0.5, maxVal: 15) { String(format: "%.1f", $0) }
                        }
                        HStack(spacing: 6) {
                            RotaryKnob(state: state, label: "Detune", paramId: ParamId.SYN_DETUNE, index: syn, minVal: -30, maxVal: 30) { signed($0, "c") }
                            RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                        }
                    } else {
                        HStack(spacing: 6) {
                            RotaryKnob(state: state, label: "Ratio", paramId: ParamId.SYN_FM_RATIO, index: syn, minVal: 0.5, maxVal: 8) { String(format: "%.1fx", $0) }
                            RotaryKnob(state: state, label: "Index", paramId: ParamId.SYN_FM_INDEX, index: syn, minVal: 0, maxVal: 8) { String(format: "%.1f", $0) }
                        }
                        HStack(spacing: 6) {
                            RotaryKnob(state: state, label: "ModDec", paramId: ParamId.SYN_FM_DECAY, index: syn, minVal: 0.01, maxVal: 2, fmt: secs)
                            RotaryKnob(state: state, label: "Fdbk", paramId: ParamId.SYN_FM_FB, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                        }
                    }
                }
            } else {
                ModulePanel("Envelope, LFO & Wavefolder", accent: skin.secondary) {
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Attack", paramId: ParamId.SYN_ATTACK, index: syn, minVal: 0.002, maxVal: 0.4) { "\(Int(($0 * 1000).rounded()))ms" }
                        RotaryKnob(state: state, label: "Decay", paramId: ParamId.SYN_DECAY, index: syn, minVal: 0.03, maxVal: 1.5, fmt: secs)
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Sustain", paramId: ParamId.SYN_SUSTAIN, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                        RotaryKnob(state: state, label: "Release", paramId: ParamId.SYN_RELEASE, index: syn, minVal: 0.01, maxVal: 1.5, fmt: secs)
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "LFO Hz", paramId: ParamId.SYN_LFO_RATE, index: syn, minVal: 0.1, maxVal: 12) { String(format: "%.1f", $0) }
                        RotaryKnob(state: state, label: "LFO Dep", paramId: ParamId.SYN_LFO_DEPTH, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Fold", paramId: ParamId.SYN_FOLD, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                        RotaryKnob(state: state, label: "Slide", paramId: ParamId.SYN_SLIDE_TIME, index: syn, minVal: 0.01, maxVal: 0.4) { "\(Int(($0 * 1000).rounded()))ms" }
                    }
                }
            }
        }
    }

    private func engineButtons(syn: Int32) -> some View {
        let cur = Int(state.bridge.getParam(ParamId.SYN_MODE, index: syn).rounded())
        let names = ["SUBTRACTIVE", "FM 2-OP", "PLUCK", "VECTOR"]
        return HStack(spacing: 4) {
            ForEach(0..<names.count, id: \.self) { m in
                HwButton(text: names[m], on: cur == m, ledColor: skin.secondary, height: 28) {
                    state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_MODE, index: syn, value: Float(m)); state.rev += 1
                }
            }
        }
    }

    /// The controls of the two newer engines: Pluck (a plucked string) and Vector (a blend of four sources).
    @ViewBuilder
    private func extraEngineKnobs(syn: Int32, mode: Int) -> some View {
        if mode == 2 {
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Bright", paramId: ParamId.SYN_PLUCK_BRIGHT, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Ring", paramId: ParamId.SYN_DECAY, index: syn, minVal: 0.03, maxVal: 1.5) { secs($0 * 4) }
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: syn, minVal: 80, maxVal: 8000) { "\(Int($0.rounded()))" }
                RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: syn, minVal: 0, maxVal: 1, fmt: pct)
            }
        } else {
            SilkLabel(text: "X and Y blend four sources: OSC 1, OSC 2, FM and SUB.", color: skin.muted, size: 9)
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Vector X", paramId: ParamId.SYN_VEC_X, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                RotaryKnob(state: state, label: "Vector Y", paramId: ParamId.SYN_VEC_Y, index: syn, minVal: 0, maxVal: 1, fmt: pct)
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "FM Ratio", paramId: ParamId.SYN_FM_RATIO, index: syn, minVal: 0.5, maxVal: 8) { String(format: "%.1fx", $0) }
                RotaryKnob(state: state, label: "FM Index", paramId: ParamId.SYN_FM_INDEX, index: syn, minVal: 0, maxVal: 8) { String(format: "%.1f", $0) }
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: syn, minVal: 80, maxVal: 8000) { "\(Int($0.rounded()))" }
                RotaryKnob(state: state, label: "Res", paramId: ParamId.SYN_RES, index: syn, minVal: 0.5, maxVal: 15) { String(format: "%.1f", $0) }
            }
            HStack(spacing: 6) {
                RotaryKnob(state: state, label: "Detune", paramId: ParamId.SYN_DETUNE, index: syn, minVal: -30, maxVal: 30) { signed($0, "c") }
                RotaryKnob(state: state, label: "Level", paramId: ParamId.SYN_LEVEL, index: syn, minVal: 0, maxVal: 1, fmt: pct)
            }
        }
    }

    private func ghostPianoRoll(syn: Int32, degOff: Int32, bars: Int, page: Int, activeSynths: Int) -> some View {
        let myNotes = state.bridge.notes(forScene: state.scene, synth: syn).map { $0.intValue }
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
                            if state.bridge.toggleNoteScene(state.scene, synth: syn, step: Int32(st), degree: deg, accent: state.accent) {
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

    // ---- FX TAB (Guitar Pedalboard Chain) ----
    private var fxTab: some View {
        let activeTracks = 1 + min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4)
        let t = min(state.fxTrack, Int32(activeTracks - 1))
        let mute = state.bridge.getParam(ParamId.FX_MUTE, index: t) > 0.5

        return VStack(spacing: 8) {
            ModulePanel("Rig Track: \(TRACK_NAMES[Int(t)])", accent: skin.tertiary) {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<activeTracks, id: \.self) { i in
                            let trkCol = i == 0 ? skin.primary : SYNTH_COLORS[(i - 1) % 4]
                            HwButton(text: TRACK_NAMES[i].uppercased(), on: t == Int32(i), ledColor: trkCol, height: 28, showLed: true) {
                                state.fxTrack = Int32(i)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
            }

            GuitarPedalboardView(state: state, track: t, trackName: TRACK_NAMES[Int(t)])

            ModulePanel("Channel Amp & Sends: \(TRACK_NAMES[Int(t)])", accent: skin.tertiary, trailing: {
                HwButton(text: mute ? "MUTED" : "MUTE", on: mute, ledColor: skin.danger, height: 24, showLed: true) {
                    state.bridge.beginEdit(); state.bridge.setParam(ParamId.FX_MUTE, index: t, value: mute ? 0 : 1); state.rev += 1
                }.frame(width: 72)
            }) {
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Level", paramId: ParamId.FX_LEVEL, index: t, minVal: 0, maxVal: 1, fmt: pct)
                    RotaryKnob(state: state, label: "Drive", paramId: ParamId.FX_DRIVE, index: t, minVal: 0, maxVal: 1, fmt: pct)
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Low EQ", paramId: ParamId.FX_EQ_LOW, index: t, minVal: -12, maxVal: 12) { signed($0, "dB") }
                    RotaryKnob(state: state, label: "High EQ", paramId: ParamId.FX_EQ_HIGH, index: t, minVal: -12, maxVal: 12) { signed($0, "dB") }
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Dly Snd", paramId: ParamId.FX_DELAY, index: t, minVal: 0, maxVal: 0.8, fmt: pct)
                    RotaryKnob(state: state, label: "Rev Snd", paramId: ParamId.FX_REVERB, index: t, minVal: 0, maxVal: 0.8, fmt: pct)
                }
            }

            ModulePanel("Master Bus & Analog VU", accent: skin.primary) {
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Master", paramId: ParamId.MASTER, index: 0, minVal: 0, maxVal: 1, fmt: pct)
                    RotaryKnob(state: state, label: "Dly Fdbk", paramId: ParamId.DELAY_FB, index: 0, minVal: 0, maxVal: 0.8, fmt: pct)
                    RotaryKnob(state: state, label: "Swing", paramId: ParamId.SWING, index: 0, minVal: 0, maxVal: 0.5) { $0 < 0.005 ? "STR" : "\(Int((50 + $0 * 50).rounded()))%" }
                }
                AnalogVuMeter(peakL: state.peakL, peakR: state.peakR)
            }
        }
    }

    // ---- SONG TAB ----
    private var songTab: some View {
        let chainArr = state.bridge.chain().map { $0.intValue }
        let selPos = min(max(Int(state.selChainPos), 0), max(0, chainArr.count - 1))
        let activeTracks = 1 + min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4)
        let autoTrk = min(max(Int(state.selAutoTrack), 0), activeTracks - 1)
        let autoLn = min(max(Int(state.selAutoLane), 0), 2)
        let autoData = state.bridge.chainAutoPos(Int32(selPos), track: Int32(autoTrk))
        let curPid = autoData.count > autoLn * 3 ? autoData[autoLn * 3].intValue : -1
        let curStart = autoData.count > autoLn * 3 + 1 ? autoData[autoLn * 3 + 1].floatValue : 0
        let curEnd = autoData.count > autoLn * 3 + 2 ? autoData[autoLn * 3 + 2].floatValue : 0

        let choices: [(Int32, String, Float, Float)] = autoTrk == 0 ? [
            (-1, "Off", 0, 0), (ParamId.FX_LEVEL, "Level", 0.1, 1), (ParamId.FX_DRIVE, "Drive", 0, 0.95),
            (ParamId.FX_DELAY, "Delay", 0, 0.75), (ParamId.FX_REVERB, "Reverb", 0, 0.75)
        ] : [
            (-1, "Off", 0, 0), (ParamId.SYN_CUTOFF, "Cutoff", 140, 7500), (ParamId.SYN_RES, "Res", 0.8, 12),
            (ParamId.SYN_FM_INDEX, "FM Idx", 0.2, 7.5), (ParamId.SYN_WARMTH, "Warmth", 0, 1), (ParamId.SYN_MOTION, "Motion", 0, 1),
            (ParamId.FX_DRIVE, "Drive", 0, 0.95), (ParamId.FX_DELAY, "Delay", 0, 0.75), (ParamId.FX_REVERB, "Reverb", 0, 0.75)
        ]

        return VStack(spacing: 8) {
            ModulePanel("Arrangement Chain", accent: skin.positive, trailing: {
                HwButton(text: state.songMode ? "SONG ON" : "SONG OFF", on: state.songMode, ledColor: skin.positive, height: 26, showLed: true) {
                    state.songMode.toggle()
                    state.bridge.setSongMode(state.songMode)
                    if state.playing { state.bridge.playScene(state.scene) }
                }.frame(width: 96)
            }) {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 5) {
                        ForEach(0..<chainArr.count, id: \.self) { pos in
                            let sc = min(max(chainArr[pos], 0), 3)
                            let playingHere = state.playing && state.songMode && Int(state.chainPos) == pos
                            HwButton(text: "\(pos + 1):\(SCENE_NAMES[sc])", on: playingHere || pos == selPos, ledColor: playingHere ? skin.positive : skin.secondary, height: 32) {
                                state.selChainPos = Int32(pos)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                HStack(spacing: 4) {
                    ForEach(0..<4, id: \.self) { i in
                        HwButton(text: "+ \(SCENE_NAMES[i])", height: 28) { state.bridge.chainAdd(Int32(i)); state.rev += 1 }
                    }
                    HwButton(text: "DEL", ledColor: skin.danger, enabled: chainArr.count > 1, height: 28) {
                        state.bridge.chainRemove(at: Int32(selPos))
                        state.selChainPos = Int32(max(0, selPos - 1))
                        state.rev += 1
                    }
                }
            }

            ModulePanel("Slot #\(selPos + 1) Free Automation (3 / Track)", accent: skin.primary) {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<activeTracks, id: \.self) { t in
                            HwButton(text: TRACK_NAMES[t].uppercased(), on: autoTrk == t, ledColor: skin.secondary, height: 26) {
                                state.selAutoTrack = Int32(t)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                HStack(spacing: 5) {
                    ForEach(0..<3, id: \.self) { ln in
                        let hasVal = autoData.count > ln * 3 && autoData[ln * 3].intValue >= 0
                        HwButton(text: "LANE \(ln + 1)\(hasVal ? " *" : "")", on: autoLn == ln, ledColor: skin.primary, height: 26) {
                            state.selAutoLane = Int32(ln)
                        }
                    }
                }
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<choices.count, id: \.self) { idx in
                            let c = choices[idx]
                            HwButton(text: c.1, on: curPid == Int(c.0), ledColor: skin.tertiary, height: 26) {
                                state.bridge.setChainAutoPos(Int32(selPos), track: Int32(autoTrk), lane: Int32(autoLn), paramId: c.0, startVal: c.2, endVal: c.3)
                                state.rev += 1
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                if let active = choices.first(where: { Int($0.0) == curPid }), curPid >= 0 {
                    SilkLabel(text: String(format: "SWEEP: %.2f -> %.2f", curStart, curEnd), color: skin.silk, size: 10, mono: true)
                    HStack(spacing: 5) {
                        HwButton(text: "RISE ^", ledColor: skin.primary, height: 28) {
                            state.bridge.setChainAutoPos(Int32(selPos), track: Int32(autoTrk), lane: Int32(autoLn), paramId: active.0, startVal: active.2, endVal: active.3); state.rev += 1
                        }
                        HwButton(text: "DROP v", ledColor: skin.secondary, height: 28) {
                            state.bridge.setChainAutoPos(Int32(selPos), track: Int32(autoTrk), lane: Int32(autoLn), paramId: active.0, startVal: active.3, endVal: active.2); state.rev += 1
                        }
                        HwButton(text: "CLEAR", height: 28) {
                            state.bridge.setChainAutoPos(Int32(selPos), track: Int32(autoTrk), lane: Int32(autoLn), paramId: -1, startVal: 0, endVal: 0); state.rev += 1
                        }
                    }
                }
            }
        }
    }
}
