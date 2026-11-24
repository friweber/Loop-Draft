import SwiftUI

struct ContentView: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState

    var body: some View {
        GeometryReader { geo in
            // SIZE scales everything at once, text included: the layout is done at another size and then zoomed.
            let k = state.uiScaleFactor
            let w = geo.size.width / k
            let h = geo.size.height / k
            mainFrame(wide: w > h && w >= 560)
                .frame(width: w, height: h)
                .scaleEffect(k, anchor: .topLeading)
                .frame(width: geo.size.width, height: geo.size.height, alignment: .topLeading)
        }
        .background(skin.chassis.ignoresSafeArea())
        .onChange(of: state.tab) { _ in state.rememberTab() }
        .sheet(isPresented: $state.showSkinStudio) {
            SkinStudioSheet(state: state)
                .environment(\.skin, SkinTheme.resolve(state.skinConfig))
        }
        .background(
            Color.clear.sheet(isPresented: $state.showMenu) {
                MenuSheet(state: state)
                    .environment(\.skin, SkinTheme.resolve(state.skinConfig))
            }
        )
    }

    @ViewBuilder
    private func mainFrame(wide: Bool) -> some View {
        if wide {
            // Landscape: the transport rail on the side of the hand that is free, the screen in two columns.
            HStack(spacing: 0) {
                if !state.leftHanded { sideRail }
                VStack(spacing: 6) {
                    pickBanner
                    tabContent(wide: true)
                }
                .padding(6)
                .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                if state.leftHanded { sideRail }
            }
            .background(skin.chassis)
        } else {
            HStack(spacing: 0) {
                WalnutRail()
                VStack(spacing: 0) {
                    topBar
                    sceneStrip
                    pickBanner.padding(.horizontal, 8)
                    ScrollView {
                        tabContent(wide: false).padding(8)
                    }
                    bottomNav
                }
                .background(skin.chassis)
                WalnutRail()
            }
        }
    }

    @ViewBuilder
    private func tabContent(wide: Bool) -> some View {
        if state.tab == 0 {
            DrumsScreen(state: state, wide: wide)
        } else if state.tab == 1 {
            SynthScreen(state: state, wide: wide)
        } else if state.tab == 2 {
            fxTab(wide)
        } else {
            SongScreen(state: state, wide: wide)
        }
    }

    /// Shown while a control is being picked for the song automation.
    @ViewBuilder
    private var pickBanner: some View {
        if state.pickAuto {
            HStack(spacing: 6) {
                SilkLabel(text: "TAP A CONTROL WITH A YELLOW FRAME", color: HwColor.yellow, size: 10)
                Spacer()
                HwButton(text: "CANCEL", ledColor: skin.danger, height: 26) { state.pickAuto = false; state.tab = 3 }.frame(width: 76)
            }
            .padding(6)
            .background(skin.recess)
            .overlay(RoundedRectangle(cornerRadius: skin.buttonRadius).stroke(HwColor.yellow, lineWidth: 1))
            .cornerRadius(skin.buttonRadius)
        }
    }

    private var logo: some View {
        HStack(spacing: 0) {
            SilkLabel(text: "LOOP", color: skin.silk, size: 15)
            SilkLabel(text: "DRAFT", color: skin.primary, size: 15)
        }
    }

    private func setBpm(_ v: Int) {
        state.bridge.beginEdit()
        state.bridge.setParam(ParamId.BPM, index: 0, value: Float(min(max(v, 40), 240)))
        state.rev += 1
    }

    private var playButton: some View {
        HwButton(text: state.playing ? "STOP" : "PLAY", on: state.playing, ledColor: skin.positive, height: 36, showLed: true) {
            if state.playing { state.bridge.stopTransport() } else { state.bridge.playScene(state.scene) }
        }
    }

    private var recButton: some View {
        HwButton(text: "REC", on: state.rec, ledColor: skin.danger, height: 36, showLed: true) {
            state.rec.toggle()
            state.bridge.setRecording(state.rec)
        }
    }

    // ---- Top Transport Bar (portrait) ----
    private var topBar: some View {
        let bpm = Int(state.bridge.getParam(ParamId.BPM, index: 0).rounded())
        return VStack(spacing: 6) {
            HStack(spacing: 5) {
                logo
                Spacer()
                HwButton(text: "MENU", on: state.showMenu, ledColor: skin.primary, height: 28) { state.showMenu = true }.frame(width: 50)
                HwButton(text: "SETUP", on: state.showSkinStudio, ledColor: skin.tertiary, height: 28) { state.showSkinStudio = true }.frame(width: 52)
                HwButton(text: "UNDO", enabled: state.bridge.canUndo(), height: 28) { if state.bridge.undo() { state.rev += 1 } }.frame(width: 50)
                HwButton(text: "REDO", enabled: state.bridge.canRedo(), height: 28) { if state.bridge.redo() { state.rev += 1 } }.frame(width: 50)
            }
            HStack(spacing: 6) {
                playButton
                recButton.frame(width: 66)
                HwButton(text: "-", height: 36) { setBpm(bpm - 1) }.frame(width: 36)
                SevenSegmentBpm(bpm: bpm)
                HwButton(text: "+", height: 36) { setBpm(bpm + 1) }.frame(width: 36)
            }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 6)
        .background(skin.faceplate)
    }

    private func sceneButton(_ i: Int) -> some View {
        let colors = [skin.primary, skin.secondary, skin.tertiary, skin.positive]
        let bars = state.bridge.sceneBars(Int32(i))
        return HwButton(text: "\(SCENE_NAMES[i]) (\(bars)B)",
                        on: state.scene == Int32(i) || state.queued == Int32(i),
                        ledColor: state.queued == Int32(i) ? skin.tertiary : colors[i],
                        height: 30) {
            if state.playing && state.songMode { return }
            state.bridge.queueScene(Int32(i))
            if !state.playing { state.scene = Int32(i); state.drumBar = 0; state.rollPage = 0 }
        }
    }

    private func navButton(_ i: Int) -> some View {
        let items: [(String, Color)] = [("DRUMS", skin.primary), ("SYNTH", skin.secondary), ("FX", skin.tertiary), ("SONG", skin.positive)]
        return HwButton(text: items[i].0, on: state.tab == i, ledColor: items[i].1, height: 38, showLed: true) { state.tab = i }
    }

    // ---- Scene Strip ----
    private var sceneStrip: some View {
        HStack(spacing: 5) {
            ForEach(0..<4, id: \.self) { i in sceneButton(i) }
        }
        .padding(.horizontal, 8)
        .padding(.vertical, 5)
    }

    // ---- Bottom Nav ----
    private var bottomNav: some View {
        HStack(spacing: 6) {
            ForEach(0..<4, id: \.self) { i in navButton(i) }
        }
        .padding(6)
        .background(skin.faceplate)
    }

    // ---- Side rail (landscape): everything the top bar, scene strip and bottom bar hold ----
    private var sideRail: some View {
        let bpm = Int(state.bridge.getParam(ParamId.BPM, index: 0).rounded())
        return ScrollView {
            VStack(spacing: 5) {
                HStack(spacing: 4) {
                    logo
                    Spacer()
                }
                HStack(spacing: 5) {
                    HwButton(text: "MENU", on: state.showMenu, ledColor: skin.primary, height: 26) { state.showMenu = true }
                    HwButton(text: "SETUP", on: state.showSkinStudio, ledColor: skin.tertiary, height: 26) { state.showSkinStudio = true }
                }
                HStack(spacing: 5) {
                    playButton
                    recButton.frame(width: 66)
                }
                HStack(spacing: 5) {
                    HwButton(text: "-", height: 36) { setBpm(bpm - 1) }
                    SevenSegmentBpm(bpm: bpm)
                    HwButton(text: "+", height: 36) { setBpm(bpm + 1) }
                }
                HStack(spacing: 5) {
                    HwButton(text: "UNDO", enabled: state.bridge.canUndo(), height: 30) { if state.bridge.undo() { state.rev += 1 } }
                    HwButton(text: "REDO", enabled: state.bridge.canRedo(), height: 30) { if state.bridge.redo() { state.rev += 1 } }
                }
                HStack(spacing: 5) { sceneButton(0); sceneButton(1) }
                HStack(spacing: 5) { sceneButton(2); sceneButton(3) }
                HStack(spacing: 5) { navButton(0); navButton(1) }
                HStack(spacing: 5) { navButton(2); navButton(3) }
            }
            .padding(6)
        }
        .frame(width: 210)
        .background(skin.faceplate)
    }

    // ---- FX TAB (Guitar Pedalboard Chain) ----
    @ViewBuilder
    private func fxTab(_ wide: Bool) -> some View {
        let activeTracks = 1 + min(max(Int(state.bridge.getParam(ParamId.ACTIVE_SYNTHS, index: 0).rounded()), 2), 4)
        let t = min(state.fxTrack, Int32(activeTracks - 1))
        let mute = state.bridge.getParam(ParamId.FX_MUTE, index: t) > 0.5

        if wide {
            Panes(left: { fxRig(t, activeTracks) }, right: { fxChannel(t, mute) })
        } else {
            VStack(spacing: 8) { fxRig(t, activeTracks); fxChannel(t, mute) }
        }
    }

    @ViewBuilder
    private func fxRig(_ t: Int32, _ activeTracks: Int) -> some View {
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
    }

    @ViewBuilder
    private func fxChannel(_ t: Int32, _ mute: Bool) -> some View {

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
