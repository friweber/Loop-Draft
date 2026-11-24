import SwiftUI

private let PAD_NAMES = ["Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc"]
private let PAD_SHORT = ["Kik", "Snr", "Clp", "CH", "OH", "Rim", "Tom", "Prc"]
private let SCENE_NAMES = ["A", "B", "C", "D"]
private let TRACK_NAMES = ["Drums", "Bass", "Lead", "Syn 3", "Syn 4"]
private let DRUM_ENGINES = ["808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break", "Hardcore Gabber", "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore"]
private let DSYN_TEMPLATES = ["Kick", "Snare", "Hat", "Gabber", "Zap", "Clang"]
private let INSTRUMENTS = ["Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead"]
private let FX_SLOTS = ["Off", "Drive", "Crush", "Chorus", "Phaser", "Filter", "Comp"]
private let NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

private func noteName(_ midi: Int32) -> String {
    let idx = ((Int(midi) % 12) + 12) % 12
    return "\(NOTE_NAMES[idx])\(Int(midi) / 12 - 1)"
}
private func pct(_ v: Float) -> String { "\(Int((v * 100).rounded()))%" }
private func signed(_ v: Float, _ unit: String) -> String { "\(v >= 0 ? "+" : "")\(Int(v.rounded()))\(unit)" }
private func secs(_ v: Float) -> String { String(format: "%.2fs", v) }

struct ContentView: View {
    @ObservedObject var state: EngineState

    var body: some View {
        HStack(spacing: 0) {
            WalnutRail()
            VStack(spacing: 0) {
                topBar
                sceneStrip
                ScrollView {
                    VStack(spacing: 8) {
                        if state.showSettings {
                            settingsExpertView
                        } else {
                            switch state.tab {
                            case 0: drumsTab
                            case 1: synthTab
                            case 2: fxTab
                            default: songTab
                            }
                        }
                    }
                    .padding(8)
                }
                bottomNav
            }
            .background(HwColor.chassis)
            WalnutRail()
        }
        .background(HwColor.chassis.ignoresSafeArea())
    }

    // ---- Top Transport Bar ----
    private var topBar: some View {
        let bpm = Int(state.bridge.getParam(ParamId.BPM, index: 0).rounded())
        return VStack(spacing: 6) {
            HStack(spacing: 6) {
                VStack(alignment: .leading, spacing: 1) {
                    SilkLabel(text: "LOOPDRAFT", color: HwColor.silk, size: 14)
                    SilkLabel(text: state.audioInfo, color: HwColor.muted, size: 8)
                }
                Spacer()
                HwButton(text: "UNDO", enabled: state.bridge.canUndo(), height: 28) {
                    if state.bridge.undo() { state.rev += 1 }
                }.frame(width: 58)
                HwButton(text: "REDO", enabled: state.bridge.canRedo(), height: 28) {
                    if state.bridge.redo() { state.rev += 1 }
                }.frame(width: 52)
                HwButton(text: state.showSettings ? "BACK" : "SET ⚙", on: state.showSettings, ledColor: HwColor.cyan, height: 28) {
                    state.showSettings.toggle()
                }.frame(width: 56)
            }
            HStack(spacing: 6) {
                HwButton(text: state.playing ? "STOP" : "PLAY", on: state.playing, ledColor: HwColor.green, height: 36, showLed: true) {
                    if state.playing { state.bridge.stopTransport() } else { state.bridge.playScene(state.scene) }
                }
                HwButton(text: "REC", on: state.rec, ledColor: HwColor.red, height: 36, showLed: true) {
                    state.rec.toggle()
                    state.bridge.setRecording(state.rec)
                }.frame(width: 68)
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
        .background(HwColor.faceplate)
    }

    // ---- Scene Strip ----
    private var sceneStrip: some View {
        let colors = [HwColor.orange, HwColor.cyan, HwColor.yellow, HwColor.green]
        return HStack(spacing: 5) {
            ForEach(0..<4, id: \.self) { i in
                let bars = state.bridge.sceneBars(Int32(i))
                HwButton(text: "\(SCENE_NAMES[i]) (\(bars)B)",
                         on: state.scene == Int32(i) || state.queued == Int32(i),
                         ledColor: state.queued == Int32(i) ? HwColor.yellow : colors[i],
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
        let items: [(String, Color)] = [("DRUMS", HwColor.orange), ("SYNTH", HwColor.cyan), ("FX", HwColor.yellow), ("SONG", HwColor.green)]
        return HStack(spacing: 6) {
            ForEach(0..<4, id: \.self) { i in
                HwButton(text: items[i].0, on: state.tab == i, ledColor: items[i].1, height: 38, showLed: true) {
                    state.tab = i
                }
            }
        }
        .padding(6)
        .background(HwColor.faceplate)
    }

    // ---- DRUMS TAB ----
    private var drumsTab: some View {
        let curEng = min(max(Int(state.bridge.getParam(ParamId.DRUM_ENGINE, index: 0).rounded()), 0), 9)
        let kitHard = state.bridge.getParam(ParamId.KIT_HARDNESS, index: 0)
        let isJungle = curEng == 4
        let jPitch = state.bridge.getParam(ParamId.JUNGLE_PITCH, index: 0)
        let jTight = state.bridge.getParam(ParamId.JUNGLE_TIGHT, index: 0)
        let jCrunch = state.bridge.getParam(ParamId.JUNGLE_CRUNCH, index: 0)
        let revOn = state.bridge.getParam(ParamId.PAD_REVERSE, index: state.selPad) > 0.5
        let gridData = state.bridge.drumGrid(forScene: state.scene)
        let gridBytes = [UInt8](gridData)
        let bars = max(1, Int(state.bridge.sceneBars(state.scene)))
        let bar = min(max(Int(state.drumBar), 0), bars - 1)
        let rollLbl = state.drumRollMode == 3 ? "ROLL 2X" : (state.drumRollMode == 4 ? "ROLL 3X" : (state.drumRollMode == 5 ? "ROLL 4X" : "ROLL"))

        return VStack(spacing: 8) {
            ModulePanel(isJungle ? "Jungle Break Slicer — Slice \(state.selPad + 1)" : "\(DRUM_ENGINES[curEng]) — \(PAD_NAMES[Int(state.selPad)])",
                        accent: isJungle ? HwColor.cyan : HwColor.orange) {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<DRUM_ENGINES.count, id: \.self) { idx in
                            HwButton(text: DRUM_ENGINES[idx], on: curEng == idx, ledColor: idx == 4 ? HwColor.cyan : HwColor.orange, height: 26) {
                                state.bridge.setDrumEngine(Int32(idx)); state.rev += 1
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                if isJungle {
                    HStack(spacing: 5) {
                        HwButton(text: "AMEN DEFAULT", ledColor: HwColor.cyan, height: 28) {
                            state.bridge.setDrumEngine(4); state.rev += 1
                        }
                        HwButton(text: "BOUNCE SCENE \(SCENE_NAMES[Int(state.scene)]) → BREAK", ledColor: HwColor.yellow, height: 28) {
                            state.bridge.resampleScene(toJungle: state.scene); state.rev += 1
                        }
                    }
                    HStack(spacing: 4) {
                        SilkLabel(text: "PITCH \(signed(jPitch, "st"))", color: HwColor.silk, size: 9, mono: true)
                        HwButton(text: "-", height: 24) { state.bridge.applyJungleTuningPitch(jPitch - 1, tight: jTight, crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                        HwButton(text: "+", height: 24) { state.bridge.applyJungleTuningPitch(jPitch + 1, tight: jTight, crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                        SilkLabel(text: "TIGHT \(secs(jTight))", color: HwColor.silk, size: 9, mono: true)
                        HwButton(text: "-", height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: max(0.08, jTight - 0.1), crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                        HwButton(text: "+", height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: min(1.5, jTight + 0.1), crunch: jCrunch); state.rev += 1 }.frame(width: 28)
                    }
                    HStack(spacing: 4) {
                        SilkLabel(text: "CRUNCH \(pct(jCrunch))", color: HwColor.silk, size: 9, mono: true)
                        Spacer()
                        HwButton(text: "CLEAN", on: jCrunch < 0.15, ledColor: HwColor.green, height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: jTight, crunch: 0.05); state.rev += 1 }.frame(width: 58)
                        HwButton(text: "AKAI", on: jCrunch >= 0.15 && jCrunch <= 0.45, ledColor: HwColor.yellow, height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: jTight, crunch: 0.30); state.rev += 1 }.frame(width: 52)
                        HwButton(text: "RAW", on: jCrunch > 0.45, ledColor: HwColor.red, height: 24) { state.bridge.applyJungleTuningPitch(jPitch, tight: jTight, crunch: 0.75); state.rev += 1 }.frame(width: 50)
                    }
                } else {
                    HStack(spacing: 4) {
                        SilkLabel(text: "HARDNESS", color: HwColor.silk, size: 9)
                        HwButton(text: "CLEAN", on: kitHard < 0.15, ledColor: HwColor.green, height: 24) { state.bridge.setKitHardness(0.0); state.rev += 1 }
                        HwButton(text: "PUNCH", on: kitHard >= 0.15 && kitHard <= 0.55, ledColor: HwColor.yellow, height: 24) { state.bridge.setKitHardness(0.35); state.rev += 1 }
                        HwButton(text: "BRUTAL", on: kitHard > 0.55, ledColor: HwColor.red, height: 24) { state.bridge.setKitHardness(0.80); state.rev += 1 }
                    }
                    HStack(spacing: 5) {
                        HwButton(text: "BOUNCE → JUNGLE", ledColor: HwColor.cyan, height: 28) {
                            state.bridge.resampleScene(toJungle: state.scene); state.rev += 1
                        }
                        HwButton(text: "DRUM SYNTH ⚙", ledColor: HwColor.yellow, height: 28) {
                            state.expertPad = state.selPad; state.showExpertMenu = true; state.showSettings = true
                        }
                    }
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Vol", paramId: ParamId.PAD_VOL, index: state.selPad, minVal: 0, maxVal: 1, fmt: pct)
                    RotaryKnob(state: state, label: "Pitch", paramId: ParamId.PAD_PITCH, index: state.selPad, minVal: -12, maxVal: 12) { signed($0, "st") }
                    RotaryKnob(state: state, label: "Decay", paramId: ParamId.PAD_DECAY, index: state.selPad, minVal: 0.03, maxVal: 1.5, fmt: secs)
                    RotaryKnob(state: state, label: "Pan", paramId: ParamId.PAD_PAN, index: state.selPad, minVal: -1, maxVal: 1) {
                        $0 < -0.02 ? "L\(Int(-$0 * 100))" : ($0 > 0.02 ? "R\(Int($0 * 100))" : "CTR")
                    }
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Slice S", paramId: ParamId.PAD_SLICE_START, index: state.selPad, minVal: 0, maxVal: 0.95, capColor: Color(red: 0.12, green: 0.22, blue: 0.28), fmt: pct)
                    RotaryKnob(state: state, label: "Slice E", paramId: ParamId.PAD_SLICE_END, index: state.selPad, minVal: 0.05, maxVal: 1, capColor: Color(red: 0.12, green: 0.22, blue: 0.28), fmt: pct)
                    HwButton(text: revOn ? "REV ON" : "REV", on: revOn, ledColor: HwColor.cyan, height: 46, showLed: true) {
                        state.bridge.beginEdit(); state.bridge.setParam(ParamId.PAD_REVERSE, index: state.selPad, value: revOn ? 0 : 1); state.rev += 1
                    }
                }
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
                                        Circle().fill(state.selPad == i ? HwColor.red : HwColor.ledOff).frame(width: 7, height: 7)
                                        Spacer()
                                    }
                                    Spacer()
                                    SilkLabel(text: isJungle ? "SLICE \(i + 1)" : PAD_NAMES[Int(i)].uppercased(), color: HwColor.padInk, size: 10)
                                }
                                .padding(6)
                                .frame(maxWidth: .infinity, minHeight: 56, maxHeight: 56)
                                .background(HwColor.padSurface)
                                .overlay(RoundedRectangle(cornerRadius: 9).stroke(state.selPad == i ? HwColor.orange : Color.gray.opacity(0.5), lineWidth: state.selPad == i ? 2 : 1))
                                .cornerRadius(9)
                            }.buttonStyle(.plain)
                        }
                    }
                }
            }

            ModulePanel("16-Step Sequencer: \(isJungle ? "Slice \(state.selPad + 1)" : PAD_NAMES[Int(state.selPad)])", accent: HwColor.orange, trailing: {
                HwButton(text: rollLbl, on: state.drumRollMode > 0, ledColor: HwColor.cyan, height: 26) {
                    state.drumRollMode = state.drumRollMode == 0 ? 3 : (state.drumRollMode == 3 ? 4 : (state.drumRollMode == 4 ? 5 : 0))
                }.frame(width: 70)
                HwButton(text: "ACC", on: state.accent, ledColor: HwColor.yellow, height: 26) {
                    state.accent.toggle()
                }.frame(width: 48)
            }) {
                HStack(spacing: 3) {
                    ForEach(0..<8, id: \.self) { i in
                        HwButton(text: isJungle ? "S\(i + 1)" : PAD_SHORT[i], on: state.selPad == Int32(i), ledColor: isJungle ? HwColor.cyan : HwColor.orange, height: 26) {
                            state.selPad = Int32(i)
                        }
                    }
                }
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
        }
    }

    // ---- SYNTH TAB ----
    private var synthTab: some View {
        let activeSynths = min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4)
        let syn = min(state.synth, Int32(activeSynths - 1))
        let isFm = Int(state.bridge.getParam(ParamId.SYN_MODE, index: syn).rounded()) == 1
        let wave1 = Int(state.bridge.getParam(ParamId.SYN_WAVE1, index: syn).rounded())
        let cutoff = state.bridge.getParam(ParamId.SYN_CUTOFF, index: syn)
        let fmIndex = state.bridge.getParam(ParamId.SYN_FM_INDEX, index: syn)
        let fmRatio = state.bridge.getParam(ParamId.SYN_FM_RATIO, index: syn)
        let curInst = Int(state.bridge.getParam(ParamId.SYN_INSTRUMENT, index: syn).rounded())
        let degOff = state.degreeOffset(syn)
        let octVal = state.inputOctave[Int(syn)]
        let chord = state.bridge.getParam(ParamId.SYN_CHORD, index: syn) > 0.5

        return VStack(spacing: 8) {
            ModulePanel("Synth: \(TRACK_NAMES[Int(syn) + 1]) (\(isFm ? "FM" : "SUB"))", accent: HwColor.cyan, trailing: {
                HStack(spacing: 3) {
                    ForEach(0..<activeSynths, id: \.self) { idx in
                        HwButton(text: TRACK_NAMES[idx + 1].uppercased(), on: syn == Int32(idx), ledColor: HwColor.cyan, height: 24) {
                            state.synth = Int32(idx)
                        }.fixedSize(horizontal: true, vertical: false)
                    }
                    if activeSynths < 4 {
                        HwButton(text: "+TRK", ledColor: HwColor.green, height: 24) {
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
                            HwButton(text: INSTRUMENTS[idx], on: curInst == idx, ledColor: HwColor.yellow, height: 26) {
                                state.bridge.applyInstrumentSynth(syn, preset: Int32(idx)); state.rev += 1
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Warmth", paramId: ParamId.SYN_WARMTH, index: syn, minVal: 0, maxVal: 1, capColor: Color(red: 0.26, green: 0.16, blue: 0.10), fmt: pct)
                    RotaryKnob(state: state, label: "Motion", paramId: ParamId.SYN_MOTION, index: syn, minVal: 0, maxVal: 1, capColor: Color(red: 0.12, green: 0.22, blue: 0.28), fmt: pct)
                }
                OledScopeDisplay(isFm: isFm, wave: wave1, cutoffNorm: min(max((cutoff - 80) / 7920, 0.08), 1), fmIndex: fmIndex, fmRatio: fmRatio)
                HStack(spacing: 6) {
                    HwButton(text: "SUBTRACTIVE", on: !isFm, ledColor: HwColor.cyan, height: 28) {
                        state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_MODE, index: syn, value: 0); state.rev += 1
                    }
                    HwButton(text: "FM 2-OP", on: isFm, ledColor: HwColor.cyan, height: 28) {
                        state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_MODE, index: syn, value: 1); state.rev += 1
                    }
                }
                if !isFm {
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Cutoff", paramId: ParamId.SYN_CUTOFF, index: syn, minVal: 80, maxVal: 8000) { "\(Int($0.rounded()))" }
                        RotaryKnob(state: state, label: "Res", paramId: ParamId.SYN_RES, index: syn, minVal: 0.5, maxVal: 15) { String(format: "%.1f", $0) }
                        RotaryKnob(state: state, label: "Detune", paramId: ParamId.SYN_DETUNE, index: syn, minVal: -30, maxVal: 30) { signed($0, "c") }
                        RotaryKnob(state: state, label: "Decay", paramId: ParamId.SYN_DECAY, index: syn, minVal: 0.03, maxVal: 1.5, fmt: secs)
                    }
                } else {
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Ratio", paramId: ParamId.SYN_FM_RATIO, index: syn, minVal: 0.5, maxVal: 8) { String(format: "%.1fx", $0) }
                        RotaryKnob(state: state, label: "Index", paramId: ParamId.SYN_FM_INDEX, index: syn, minVal: 0, maxVal: 8) { String(format: "%.1f", $0) }
                        RotaryKnob(state: state, label: "ModDec", paramId: ParamId.SYN_FM_DECAY, index: syn, minVal: 0.01, maxVal: 2, fmt: secs)
                        RotaryKnob(state: state, label: "Fdbk", paramId: ParamId.SYN_FM_FB, index: syn, minVal: 0, maxVal: 1, fmt: pct)
                    }
                }
            }

            ModulePanel("Play \(TRACK_NAMES[Int(syn) + 1]) • OCT \(octVal >= 0 ? "+\(octVal)" : "\(octVal)")", accent: HwColor.cyan, trailing: {
                HStack(spacing: 4) {
                    HwButton(text: "OCT-", enabled: octVal > -2, height: 26) { state.inputOctave[Int(syn)] = max(-2, octVal - 1) }.frame(width: 50)
                    HwButton(text: "OCT+", enabled: octVal < 2, height: 26) { state.inputOctave[Int(syn)] = min(2, octVal + 1) }.frame(width: 50)
                }
            }) {
                HStack(spacing: 5) {
                    HwButton(text: chord ? "CHORD ON" : "CHORD", on: chord, ledColor: HwColor.yellow, height: 28) {
                        state.bridge.beginEdit(); state.bridge.setParam(ParamId.SYN_CHORD, index: syn, value: chord ? 0 : 1); state.rev += 1
                    }
                    HwButton(text: "ACCENT", on: state.accent, ledColor: HwColor.orange, height: 28) { state.accent.toggle() }
                }
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
                                SilkLabel(text: name, color: HwColor.padInk, size: 12, mono: true)
                                    .frame(maxWidth: .infinity, minHeight: 50, maxHeight: 50)
                                    .background(HwColor.padSurface)
                                    .cornerRadius(9)
                            }.buttonStyle(.plain)
                        }
                    }
                }
            }
        }
    }

    // ---- FX TAB ----
    private var fxTab: some View {
        let activeTracks = 1 + min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4)
        let t = min(state.fxTrack, Int32(activeTracks - 1))
        let mute = state.bridge.getParam(ParamId.FX_MUTE, index: t) > 0.5

        return VStack(spacing: 8) {
            ModulePanel("Channel Strip: \(TRACK_NAMES[Int(t)])", accent: HwColor.yellow) {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<activeTracks, id: \.self) { i in
                            HwButton(text: TRACK_NAMES[i].uppercased(), on: t == Int32(i), ledColor: HwColor.yellow, height: 26) {
                                state.fxTrack = Int32(i)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Level", paramId: ParamId.FX_LEVEL, index: t, minVal: 0, maxVal: 1, fmt: pct)
                    RotaryKnob(state: state, label: "Drive", paramId: ParamId.FX_DRIVE, index: t, minVal: 0, maxVal: 1, fmt: pct)
                    RotaryKnob(state: state, label: "Low EQ", paramId: ParamId.FX_EQ_LOW, index: t, minVal: -12, maxVal: 12) { signed($0, "dB") }
                    RotaryKnob(state: state, label: "High EQ", paramId: ParamId.FX_EQ_HIGH, index: t, minVal: -12, maxVal: 12) { signed($0, "dB") }
                }
                HStack(spacing: 6) {
                    RotaryKnob(state: state, label: "Dly Snd", paramId: ParamId.FX_DELAY, index: t, minVal: 0, maxVal: 0.8, fmt: pct)
                    RotaryKnob(state: state, label: "Rev Snd", paramId: ParamId.FX_REVERB, index: t, minVal: 0, maxVal: 0.8, fmt: pct)
                    HwButton(text: mute ? "MUTED" : "MUTE", on: mute, ledColor: HwColor.red, height: 46, showLed: true) {
                        state.bridge.beginEdit(); state.bridge.setParam(ParamId.FX_MUTE, index: t, value: mute ? 0 : 1); state.rev += 1
                    }
                }
            }

            ModulePanel("Modular FX Chain: \(TRACK_NAMES[Int(t)]) (FX1 → FX2 → FX3)", accent: HwColor.cyan) {
                ForEach(0..<3, id: \.self) { slot in
                    let fd = state.bridge.fxSlotTrack(t, slot: Int32(slot))
                    let fxType = fd.count > 0 ? fd[0].intValue : 0
                    let fxAmt = fd.count > 1 ? fd[1].floatValue : 0.5
                    let fxRate = fd.count > 2 ? fd[2].floatValue : 0.4
                    let fxByp = fd.count > 3 ? (fd[3].floatValue > 0.5) : false

                    VStack(spacing: 4) {
                        ScrollView(.horizontal, showsIndicators: false) {
                            HStack(spacing: 3) {
                                SilkLabel(text: "FX\(slot + 1)", color: HwColor.cyan, size: 9)
                                ForEach(0..<FX_SLOTS.count, id: \.self) { idx in
                                    HwButton(text: FX_SLOTS[idx], on: fxType == idx, ledColor: HwColor.cyan, height: 24) {
                                        state.bridge.setFxSlotTrack(t, slot: Int32(slot), type: Int32(idx), amount: fxAmt, rate: fxRate, bypass: false); state.rev += 1
                                    }.fixedSize(horizontal: true, vertical: false)
                                }
                            }
                        }
                        if fxType > 0 {
                            HStack(spacing: 4) {
                                SilkLabel(text: "AMT \(pct(fxAmt))", color: HwColor.silk, size: 9, mono: true)
                                HwButton(text: "-", height: 24) { state.bridge.setFxSlotTrack(t, slot: Int32(slot), type: Int32(fxType), amount: max(0, fxAmt - 0.15), rate: fxRate, bypass: fxByp); state.rev += 1 }.frame(width: 28)
                                HwButton(text: "+", height: 24) { state.bridge.setFxSlotTrack(t, slot: Int32(slot), type: Int32(fxType), amount: min(1, fxAmt + 0.15), rate: fxRate, bypass: fxByp); state.rev += 1 }.frame(width: 28)
                                SilkLabel(text: "RT \(pct(fxRate))", color: HwColor.silk, size: 9, mono: true)
                                HwButton(text: "-", height: 24) { state.bridge.setFxSlotTrack(t, slot: Int32(slot), type: Int32(fxType), amount: fxAmt, rate: max(0, fxRate - 0.15), bypass: fxByp); state.rev += 1 }.frame(width: 28)
                                HwButton(text: "+", height: 24) { state.bridge.setFxSlotTrack(t, slot: Int32(slot), type: Int32(fxType), amount: fxAmt, rate: min(1, fxRate + 0.15), bypass: fxByp); state.rev += 1 }.frame(width: 28)
                                HwButton(text: fxByp ? "BYP" : "ON", on: !fxByp, ledColor: HwColor.green, height: 24) {
                                    state.bridge.setFxSlotTrack(t, slot: Int32(slot), type: Int32(fxType), amount: fxAmt, rate: fxRate, bypass: !fxByp); state.rev += 1
                                }.frame(width: 44)
                            }
                        }
                    }
                }
            }

            ModulePanel("Master Bus & Analog VU", accent: HwColor.orange) {
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
            ModulePanel("Arrangement Chain", accent: HwColor.green, trailing: {
                HwButton(text: state.songMode ? "SONG ON" : "SONG OFF", on: state.songMode, ledColor: HwColor.green, height: 26, showLed: true) {
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
                            HwButton(text: "\(pos + 1):\(SCENE_NAMES[sc])", on: playingHere || pos == selPos, ledColor: playingHere ? HwColor.green : HwColor.cyan, height: 32) {
                                state.selChainPos = Int32(pos)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                HStack(spacing: 4) {
                    ForEach(0..<4, id: \.self) { i in
                        HwButton(text: "+ \(SCENE_NAMES[i])", height: 28) { state.bridge.chainAdd(Int32(i)); state.rev += 1 }
                    }
                    HwButton(text: "DEL", ledColor: HwColor.red, enabled: chainArr.count > 1, height: 28) {
                        state.bridge.chainRemove(at: Int32(selPos))
                        state.selChainPos = Int32(max(0, selPos - 1))
                        state.rev += 1
                    }
                }
            }

            ModulePanel("Slot #\(selPos + 1) Free Automation (3 / Track)", accent: HwColor.orange) {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<activeTracks, id: \.self) { t in
                            HwButton(text: TRACK_NAMES[t].uppercased(), on: autoTrk == t, ledColor: HwColor.cyan, height: 26) {
                                state.selAutoTrack = Int32(t)
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                HStack(spacing: 5) {
                    ForEach(0..<3, id: \.self) { ln in
                        let hasVal = autoData.count > ln * 3 && autoData[ln * 3].intValue >= 0
                        HwButton(text: "LANE \(ln + 1)\(hasVal ? " ●" : "")", on: autoLn == ln, ledColor: HwColor.orange, height: 26) {
                            state.selAutoLane = Int32(ln)
                        }
                    }
                }
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        ForEach(0..<choices.count, id: \.self) { idx in
                            let c = choices[idx]
                            HwButton(text: c.1, on: curPid == Int(c.0), ledColor: HwColor.yellow, height: 26) {
                                state.bridge.setChainAutoPos(Int32(selPos), track: Int32(autoTrk), lane: Int32(autoLn), paramId: c.0, startVal: c.2, endVal: c.3)
                                state.rev += 1
                            }.fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                if let active = choices.first(where: { Int($0.0) == curPid }), curPid >= 0 {
                    SilkLabel(text: String(format: "SWEEP: %.2f → %.2f", curStart, curEnd), color: HwColor.silk, size: 10, mono: true)
                    HStack(spacing: 5) {
                        HwButton(text: "RISE ↑", ledColor: HwColor.orange, height: 28) {
                            state.bridge.setChainAutoPos(Int32(selPos), track: Int32(autoTrk), lane: Int32(autoLn), paramId: active.0, startVal: active.2, endVal: active.3); state.rev += 1
                        }
                        HwButton(text: "DROP ↓", ledColor: HwColor.cyan, height: 28) {
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

    // ---- SETTINGS & EXPERT MENU ----
    private var settingsExpertView: some View {
        let kitHard = state.bridge.getParam(ParamId.KIT_HARDNESS, index: 0)
        let p = min(max(state.expertPad, 0), 7)

        return VStack(spacing: 8) {
            ModulePanel("Settings & System", accent: HwColor.cyan, trailing: {
                HwButton(text: "CLOSE", ledColor: HwColor.orange, height: 26) { state.showSettings = false }.frame(width: 64)
            }) {
                SilkLabel(text: "AUDIO: \(state.audioInfo) • FORMAT v6", color: HwColor.silk, size: 9, mono: true)
                HStack(spacing: 4) {
                    SilkLabel(text: "KIT DRIVE", size: 9)
                    HwButton(text: "CLEAN", on: kitHard < 0.15, ledColor: HwColor.green, height: 26) { state.bridge.setKitHardness(0.0); state.rev += 1 }
                    HwButton(text: "PUNCH", on: kitHard >= 0.15 && kitHard <= 0.55, ledColor: HwColor.yellow, height: 26) { state.bridge.setKitHardness(0.35); state.rev += 1 }
                    HwButton(text: "BRUTAL", on: kitHard > 0.55, ledColor: HwColor.red, height: 26) { state.bridge.setKitHardness(0.80); state.rev += 1 }
                }
                HStack(spacing: 6) {
                    HwButton(text: state.showExpertMenu ? "EXPERT MENU: OPEN" : "OPEN EXPERT MENU", on: state.showExpertMenu, ledColor: HwColor.yellow, height: 32, showLed: true) {
                        state.showExpertMenu.toggle()
                    }
                    HwButton(text: "RESET DEMO", height: 32) { state.bridge.resetToDemo(); state.rev += 1 }
                }
            }

            if state.showExpertMenu {
                ModulePanel("Expert: Custom Drum Synth (Pad \(p + 1): \(PAD_NAMES[Int(p)]))", accent: HwColor.yellow, trailing: {
                    HwButton(text: "PLAY PAD", ledColor: HwColor.green, height: 26) { state.bridge.triggerPad(p, vel: 2) }.frame(width: 78)
                }) {
                    HStack(spacing: 3) {
                        ForEach(0..<8, id: \.self) { i in
                            HwButton(text: PAD_SHORT[i], on: p == Int32(i), ledColor: HwColor.yellow, height: 26) { state.expertPad = Int32(i) }
                        }
                    }
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 4) {
                            SilkLabel(text: "TMPL", size: 9)
                            ForEach(0..<DSYN_TEMPLATES.count, id: \.self) { idx in
                                HwButton(text: DSYN_TEMPLATES[idx], ledColor: HwColor.orange, height: 26) {
                                    state.bridge.applyDrumSynthTemplatePad(p, tmpl: Int32(idx)); state.rev += 1
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Base Hz", paramId: ParamId.DSYN_FREQ, index: p, minVal: 25, maxVal: 1200) { "\(Int($0.rounded()))" }
                        RotaryKnob(state: state, label: "P.Drop", paramId: ParamId.DSYN_DROP, index: p, minVal: 0, maxVal: 600) { "\(Int($0.rounded()))" }
                        RotaryKnob(state: state, label: "P.Dec", paramId: ParamId.DSYN_PDEC, index: p, minVal: 0.005, maxVal: 0.30) { "\(Int(($0 * 1000).rounded()))ms" }
                        RotaryKnob(state: state, label: "Length", paramId: ParamId.DSYN_LEN, index: p, minVal: 0.05, maxVal: 1.5, fmt: secs)
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "Tone", paramId: ParamId.DSYN_TONE, index: p, minVal: 0, maxVal: 1, fmt: pct)
                        RotaryKnob(state: state, label: "Noise", paramId: ParamId.DSYN_NOISE, index: p, minVal: 0, maxVal: 1, fmt: pct)
                        RotaryKnob(state: state, label: "N.Freq", paramId: ParamId.DSYN_NFREQ, index: p, minVal: 300, maxVal: 10000) { "\(Int($0.rounded()))" }
                        RotaryKnob(state: state, label: "Click", paramId: ParamId.DSYN_CLICK, index: p, minVal: 0, maxVal: 1, fmt: pct)
                    }
                    HStack(spacing: 6) {
                        RotaryKnob(state: state, label: "FM Amt", paramId: ParamId.DSYN_FM, index: p, minVal: 0, maxVal: 8, capColor: Color(red: 0.12, green: 0.22, blue: 0.28)) { String(format: "%.1f", $0) }
                        RotaryKnob(state: state, label: "FM Rat", paramId: ParamId.DSYN_FMRATIO, index: p, minVal: 0.5, maxVal: 8, capColor: Color(red: 0.12, green: 0.22, blue: 0.28)) { String(format: "%.1fx", $0) }
                        RotaryKnob(state: state, label: "Drive", paramId: ParamId.DSYN_DRIVE, index: p, minVal: 0, maxVal: 1, capColor: Color(red: 0.26, green: 0.16, blue: 0.10), fmt: pct)
                    }
                    HwButton(text: "SYNTHESIZE & APPLY TO PAD \(p + 1)", on: true, ledColor: HwColor.green, height: 38, showLed: true) {
                        state.bridge.synthesizeCustomPad(p, audition: true); state.rev += 1
                    }
                }
            }
        }
    }
}
