import SwiftUI

/// SONG: the chain of scenes, what moves over time in each slot, the mixer, the scene and the key.
struct SongScreen: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let wide: Bool

    private var chainArr: [Int] { state.bridge.chain().map { $0.intValue } }
    private var activeTracks: Int { 1 + min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4) }
    private var selPos: Int { min(max(Int(state.selChainPos), 0), max(0, chainArr.count - 1)) }
    private var autoTrk: Int { min(max(Int(state.selAutoTrack), 0), activeTracks - 1) }
    private var autoLn: Int { min(max(Int(state.selAutoLane), 0), 2) }

    var body: some View {
        if wide {
            Panes(left: {
                chainPanel
                automationPanel
            }, right: {
                mixerPanel
                filesPanel
                scenePanel
                keyPanel
            })
        } else {
            VStack(spacing: 8) {
                chainPanel
                automationPanel
                mixerPanel
                filesPanel
                scenePanel
                keyPanel
            }
        }
    }

    // ---- chain ----

    private var songButton: some View {
        HwButton(text: state.songMode ? "SONG ON" : "SONG OFF", on: state.songMode, ledColor: skin.positive, height: 26, showLed: true) {
            state.songMode.toggle()
            state.bridge.setSongMode(state.songMode)
            if state.playing { state.bridge.playScene(state.scene) }
        }.frame(width: 96)
    }

    private var chainPanel: some View {
        let chain = chainArr
        let sel = selPos
        return ModulePanel("Arrangement Chain", accent: skin.positive, trailing: { songButton }) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 5) {
                    ForEach(0..<chain.count, id: \.self) { pos in
                        let sc = min(max(chain[pos], 0), 3)
                        let playingHere = state.playing && state.songMode && Int(state.chainPos) == pos
                        HwButton(text: "\(pos + 1):\(SCENE_NAMES[sc])", on: playingHere || pos == sel, ledColor: playingHere ? skin.positive : skin.secondary, height: 34) {
                            state.selChainPos = Int32(pos)
                        }.fixedSize(horizontal: true, vertical: false)
                    }
                }
            }
            HStack(spacing: 4) {
                SilkLabel(text: "ADD", size: 9)
                ForEach(0..<4, id: \.self) { i in
                    HwButton(text: "+ \(SCENE_NAMES[i])", height: 30) { state.bridge.chainAdd(Int32(i)); state.rev += 1 }
                }
                HwButton(text: "DEL", ledColor: skin.danger, enabled: chain.count > 1, height: 30) {
                    state.bridge.chainRemove(at: Int32(sel))
                    state.selChainPos = Int32(max(0, sel - 1))
                    state.rev += 1
                }
            }
        }
    }

    // ---- automation ----

    private func laneIds(_ data: [NSNumber]) -> [Int32] {
        var out: [Int32] = []
        for ln in 0..<3 { out.append(data.count > ln * 3 ? data[ln * 3].int32Value : -1) }
        return out
    }

    private func laneName(_ id: Int32) -> String {
        if let info = AUTO_PARAMS[id] { return info.label }
        return id >= 0 ? "Control" : "empty"
    }

    private func autoTrackRow(_ trk: Int) -> some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                ForEach(0..<activeTracks, id: \.self) { t in
                    HwButton(text: TRACK_NAMES[t].uppercased(), on: trk == t, ledColor: skin.secondary, height: 26) {
                        state.selAutoTrack = Int32(t); state.autoMsg = ""
                    }.fixedSize(horizontal: true, vertical: false)
                }
            }
        }
    }

    private func laneRow(_ ids: [Int32], _ ln: Int) -> some View {
        HStack(spacing: 5) {
            ForEach(0..<3, id: \.self) { l in
                HwButton(text: "\(l + 1): \(laneName(ids[l]))", on: ln == l, ledColor: skin.primary, height: 30, showLed: ids[l] >= 0) {
                    state.selAutoLane = Int32(l)
                }
            }
        }
    }

    private func addRow(_ ids: [Int32], _ trk: Int) -> some View {
        let quick = trk == 0 ? AUTO_QUICK_DRUMS : AUTO_QUICK_SYNTH
        return ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 4) {
                SilkLabel(text: "ADD", size: 9)
                HwButton(text: "+ ANY CONTROL", on: state.pickAuto, ledColor: skin.tertiary, height: 30, showLed: true) {
                    state.pickAuto = true
                    state.autoMsg = ""
                    state.fxTrack = Int32(trk)
                    if trk > 0 { state.synth = Int32(trk - 1); state.tab = 2 } else { state.tab = 0 }
                }.fixedSize(horizontal: true, vertical: false)
                ForEach(0..<quick.count, id: \.self) { q in
                    let id = quick[q]
                    HwButton(text: laneName(id), on: ids.contains(id), ledColor: skin.tertiary, height: 30) {
                        state.addAutomation(id, autoIndexOf(id, Int32(trk)))
                    }.fixedSize(horizontal: true, vertical: false)
                }
            }
        }
    }

    private func laneEditor(pos: Int32, trk: Int32, ln: Int32, pid: Int32, start: Float, end: Float) -> some View {
        let info = AUTO_PARAMS[pid] ?? AutoInfo(label: "Control", lo: min(start, end, 0), hi: max(start, end, 1))
        let show: (Float) -> String = { v in (info.hi - info.lo) > 50 ? "\(Int(v.rounded()))" : String(format: "%.2f", v) }
        return VStack(spacing: 6) {
            HStack(spacing: 6) {
                ValueSlider(label: "Start", value: start, lo: info.lo, hi: info.hi, fmt: show,
                            onStart: { state.bridge.beginEdit() },
                            onChange: { v in state.bridge.setChainAutoLive(pos: pos, track: trk, lane: ln, paramId: pid, start: v, end: end); state.rev += 1 },
                            onEnd: { state.rev += 1 })
                ValueSlider(label: "End", value: end, lo: info.lo, hi: info.hi, fmt: show,
                            onStart: { state.bridge.beginEdit() },
                            onChange: { v in state.bridge.setChainAutoLive(pos: pos, track: trk, lane: ln, paramId: pid, start: start, end: v); state.rev += 1 },
                            onEnd: { state.rev += 1 })
            }
            HStack(spacing: 5) {
                HwButton(text: "START = NOW", ledColor: skin.secondary, height: 28) {
                    let now = min(max(state.bridge.getParam(pid, index: autoIndexOf(pid, trk)), info.lo), info.hi)
                    state.bridge.setChainAutoPos(pos, track: trk, lane: ln, paramId: pid, startVal: now, endVal: end); state.rev += 1
                }
                HwButton(text: "SWAP", ledColor: skin.tertiary, height: 28) {
                    state.bridge.setChainAutoPos(pos, track: trk, lane: ln, paramId: pid, startVal: end, endVal: start); state.rev += 1
                }
                HwButton(text: "REMOVE", ledColor: skin.danger, height: 28) {
                    state.bridge.setChainAutoPos(pos, track: trk, lane: ln, paramId: -1, startVal: 0, endVal: 0); state.autoMsg = ""; state.rev += 1
                }
            }
            hint("While slot #\(pos + 1) plays, \(info.label) of \(TRACK_NAMES[Int(trk)]) moves from Start to End.")
        }
    }

    private func hint(_ text: String, _ color: Color? = nil) -> some View {
        Text(text)
            .font(.system(size: 9))
            .foregroundColor(color ?? skin.muted)
            .frame(maxWidth: .infinity, alignment: .leading)
    }

    private var automationPanel: some View {
        let pos = selPos
        let trk = autoTrk
        let ln = autoLn
        let data = state.bridge.chainAutoPos(Int32(pos), track: Int32(trk))
        let ids = laneIds(data)
        let pid = ids[ln]
        let start = data.count > ln * 3 + 1 ? data[ln * 3 + 1].floatValue : 0
        let end = data.count > ln * 3 + 2 ? data[ln * 3 + 2].floatValue : 0
        return ModulePanel("Slot #\(pos + 1): moves over time", accent: skin.primary) {
            autoTrackRow(trk)
            laneRow(ids, ln)
            addRow(ids, trk)
            if !state.autoMsg.isEmpty { hint(state.autoMsg, skin.tertiary) }
            if pid >= 0 {
                laneEditor(pos: Int32(pos), trk: Int32(trk), ln: Int32(ln), pid: pid, start: start, end: end)
            } else {
                hint("Lane \(ln + 1) of \(TRACK_NAMES[trk]) is empty. Add a control above.")
            }
        }
    }

    // ---- mixer, files, scene, key ----

    private func mixerStrip(_ t: Int) -> some View {
        let mute = state.bridge.getParam(ParamId.FX_MUTE, index: Int32(t)) > 0.5
        return VStack(spacing: 5) {
            RotaryKnob(state: state, label: TRACK_NAMES[t], paramId: ParamId.FX_LEVEL, index: Int32(t), minVal: 0, maxVal: 1, fmt: pct)
            HwButton(text: mute ? "MUTED" : "MUTE", on: mute, ledColor: skin.danger, height: 28) {
                state.bridge.beginEdit(); state.bridge.setParam(ParamId.FX_MUTE, index: Int32(t), value: mute ? 0 : 1); state.rev += 1
            }
        }.frame(width: 96)
    }

    private var mixerPanel: some View {
        ModulePanel("Mixer", accent: skin.positive) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(alignment: .top, spacing: 6) {
                    ForEach(0..<activeTracks, id: \.self) { t in mixerStrip(t) }
                    RotaryKnob(state: state, label: "Master", paramId: ParamId.MASTER, index: 0, minVal: 0, maxVal: 1, fmt: pct).frame(width: 96)
                }
            }
        }
    }

    private var filesPanel: some View {
        ModulePanel("Projects, Library & Export", accent: skin.secondary) {
            HStack(spacing: 5) {
                HwButton(text: "PROJECTS", ledColor: skin.secondary, height: 34) { state.menuPage = 1; state.showMenu = true }
                HwButton(text: "LIBRARY", ledColor: skin.primary, height: 34) { state.menuPage = 0; state.showMenu = true }
                HwButton(text: "EXPORT", ledColor: skin.tertiary, height: 34) { state.menuPage = 2; state.showMenu = true }
            }
        }
    }

    private var scenePanel: some View {
        let sc = state.scene
        let bars = Int(state.bridge.sceneBars(sc))
        return ModulePanel("Scene \(SCENE_NAMES[Int(sc)])", accent: skin.secondary) {
            HStack(spacing: 4) {
                SilkLabel(text: "BARS", size: 9).frame(width: 38, alignment: .leading)
                ForEach(1..<9, id: \.self) { b in
                    HwButton(text: "\(b)", on: bars == b, ledColor: skin.secondary, height: 30) {
                        state.bridge.setSceneBars(sc, bars: Int32(b)); state.rev += 1
                    }
                }
            }
            HStack(spacing: 4) {
                SilkLabel(text: "COPY", size: 9).frame(width: 38, alignment: .leading)
                ForEach(0..<4, id: \.self) { i in
                    HwButton(text: "-> \(SCENE_NAMES[i])", enabled: Int32(i) != sc, height: 30) {
                        state.bridge.copyScene(from: sc, to: Int32(i)); state.rev += 1
                    }
                }
            }
            HStack(spacing: 6) {
                HwButton(text: "CLEAR SCENE", height: 32) { state.bridge.clearScene(sc); state.rev += 1 }
                HwButton(text: "RESET DEMO", height: 32) { state.bridge.resetToDemo(); state.rev += 1 }
            }
            HwButton(text: "LOAD PRESET: WAREHOUSE 303", ledColor: skin.positive, height: 32) {
                state.bridge.loadStartPreset()
                state.scene = 0; state.drumBar = 0; state.rollPage = 0
                state.rev += 1
            }
        }
    }

    private var keyPanel: some View {
        ModulePanel("Key & Scale", accent: skin.tertiary) {
            ChoiceRow(state: state, label: "Root", paramId: ParamId.ROOT, index: 0, options: NOTE_NAMES, accent: skin.tertiary)
            ChoiceRow(state: state, label: "Scale", paramId: ParamId.SCALE, index: 0, options: ["Minor", "Major", "Dorian", "Penta"], accent: skin.tertiary)
            ChoiceRow(state: state, label: "Base Oct", paramId: ParamId.OCTAVE, index: 0, options: ["Low", "Mid", "High"], offset: 2, accent: skin.tertiary)
        }
    }
}
