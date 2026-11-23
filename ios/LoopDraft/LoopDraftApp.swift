import SwiftUI
import Combine

enum ParamId {
    static let BPM: Int32 = 0
    static let SWING: Int32 = 1
    static let MASTER: Int32 = 2
    static let DELAY_FB: Int32 = 3
    static let ROOT: Int32 = 4
    static let SCALE: Int32 = 5
    static let OCTAVE: Int32 = 6
    static let ACTIVE_SYNTHS: Int32 = 8
    static let PAD_VOL: Int32 = 10
    static let PAD_PAN: Int32 = 11
    static let PAD_PITCH: Int32 = 12
    static let PAD_DECAY: Int32 = 13
    static let PAD_CHOKE: Int32 = 14
    static let PAD_SLICE_START: Int32 = 15
    static let PAD_SLICE_END: Int32 = 16
    static let PAD_REVERSE: Int32 = 17
    static let SYN_WAVE1: Int32 = 20
    static let SYN_WAVE2: Int32 = 21
    static let SYN_DETUNE: Int32 = 22
    static let SYN_CUTOFF: Int32 = 23
    static let SYN_RES: Int32 = 24
    static let SYN_ATTACK: Int32 = 25
    static let SYN_DECAY: Int32 = 26
    static let SYN_SUSTAIN: Int32 = 27
    static let SYN_RELEASE: Int32 = 28
    static let SYN_LEVEL: Int32 = 29
    static let SYN_LFO_RATE: Int32 = 30
    static let SYN_LFO_DEPTH: Int32 = 31
    static let SYN_LFO_TARGET: Int32 = 32
    static let SYN_CHORD: Int32 = 34
    static let SYN_MODE: Int32 = 35
    static let SYN_FM_RATIO: Int32 = 36
    static let SYN_FM_INDEX: Int32 = 37
    static let SYN_FM_DECAY: Int32 = 38
    static let SYN_FM_FB: Int32 = 39
    static let FX_LEVEL: Int32 = 40
    static let FX_DRIVE: Int32 = 41
    static let FX_EQ_LOW: Int32 = 42
    static let FX_EQ_HIGH: Int32 = 43
    static let FX_DELAY: Int32 = 44
    static let FX_REVERB: Int32 = 45
    static let FX_MUTE: Int32 = 46
    static let SYN_INSTRUMENT: Int32 = 50
    static let SYN_WARMTH: Int32 = 51
    static let SYN_MOTION: Int32 = 52
    static let DRUM_ENGINE: Int32 = 53
    static let JUNGLE_PITCH: Int32 = 54
    static let JUNGLE_TIGHT: Int32 = 55
    static let JUNGLE_CRUNCH: Int32 = 56
}

final class EngineState: ObservableObject {
    let bridge = LoopDraftBridge.shared()
    @Published var rev: Int = 0
    @Published var tab: Int = 0
    @Published var scene: Int32 = 0
    @Published var selPad: Int32 = 0
    @Published var synth: Int32 = 1
    @Published var fxTrack: Int32 = 0
    @Published var drumBar: Int32 = 0
    @Published var rollPage: Int32 = 0
    @Published var rollMode: Bool = false
    @Published var accent: Bool = false
    @Published var rec: Bool = false
    @Published var songMode: Bool = false
    @Published var playing: Bool = false
    @Published var step: Int32 = -1
    @Published var queued: Int32 = -1
    @Published var chainPos: Int32 = 0
    @Published var peakL: Float = 0
    @Published var peakR: Float = 0
    @Published var audioInfo: String = "Starting..."
    @Published var inputOctave: [Int32] = [0, 0, 0, 0]
    @Published var drumRollMode: Int32 = 0
    @Published var selChainPos: Int32 = 0
    @Published var selAutoTrack: Int32 = 2
    @Published var selAutoLane: Int32 = 0

    private var timer: AnyCancellable?
    private var frames: Int = 0

    func start() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first?.path ?? NSTemporaryDirectory()
        _ = bridge.start(withFilesDir: docs)
        audioInfo = bridge.audioInfo()
        timer = Timer.publish(every: 1.0 / 30.0, on: .main, in: .common)
            .autoconnect()
            .sink { [weak self] _ in self?.tick() }
    }

    func stop() {
        timer?.cancel()
        bridge.stop()
    }

    private func tick() {
        if bridge.poll() { rev += 1 }
        playing = bridge.isPlaying()
        step = bridge.currentStep()
        queued = bridge.queuedScene()
        chainPos = bridge.chainPos()
        let sc = bridge.currentScene()
        if playing && sc != scene { scene = sc }
        peakL = bridge.peak(forChannel: 0)
        peakR = bridge.peak(forChannel: 1)
        frames += 1
        if frames % 90 == 0 { bridge.saveIfDirty() }
        if frames % 60 == 0 { audioInfo = bridge.audioInfo() }
    }

    func vel() -> Int32 { accent ? 2 : 1 }

    func degreeOffset(_ synthIdx: Int32) -> Int32 {
        let sc = Int(bridge.getParam(ParamId.SCALE, index: 0).rounded())
        let scaleLen: Int32 = sc == 3 ? 5 : 7
        return inputOctave[min(max(Int(synthIdx), 0), 3)] * scaleLen
    }
}

@main
struct LoopDraftApp: App {
    @StateObject private var state = EngineState()
    @Environment(\.scenePhase) private var phase

    var body: some Scene {
        WindowGroup {
            ContentView(state: state)
                .preferredColorScheme(.dark)
                .onAppear { state.start() }
                .onChange(of: phase) { newPhase in
                    if newPhase == .background { state.bridge.saveIfDirty() }
                    else if newPhase == .active { state.start() }
                }
        }
    }
}
