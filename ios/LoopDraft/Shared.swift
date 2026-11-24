import SwiftUI

// Names and colours shared by all screens.
let PAD_NAMES = ["Kick", "Snare", "Clap", "CH", "OH", "Rim", "Tom", "Perc"]
let PAD_SHORT = ["Kik", "Snr", "Clp", "CH", "OH", "Rim", "Tom", "Prc"]
let PAD_COLORS: [Color] = [
    Color(red: 0.91, green: 0.28, blue: 0.22),
    Color(red: 1.0, green: 0.54, blue: 0.14),
    Color(red: 1.0, green: 0.79, blue: 0.16),
    Color(red: 0.16, green: 0.83, blue: 0.91),
    Color(red: 0.37, green: 0.66, blue: 1.0),
    Color(red: 0.29, green: 0.88, blue: 0.54),
    Color(red: 0.75, green: 0.42, blue: 1.0),
    Color(red: 1.0, green: 0.36, blue: 0.58)
]
let SCENE_NAMES = ["A", "B", "C", "D"]
let TRACK_NAMES = ["Drums", "Bass", "Lead", "Syn 3", "Syn 4"]
let SYNTH_SHORT = ["BS", "LD", "S3", "S4"]
let SYNTH_TAG = ["B", "L", "3", "4"]
let SYNTH_COLORS: [Color] = [
    Color(red: 1.0, green: 0.58, blue: 0.15),
    Color(red: 0.16, green: 0.83, blue: 0.91),
    Color(red: 1.0, green: 0.30, blue: 0.65),
    Color(red: 0.37, green: 0.91, blue: 0.44)
]
let DRUM_ENGINES = ["808 Classic", "909 Punch", "Lo-Fi Acoustic", "FM Industrial", "Jungle Break", "Hardcore Gabber", "Warehouse Rumble", "Neuro Techstep", "EBM Electro", "Glitch Breakcore"]
let INSTRUMENTS = ["Custom", "Analog Bass", "Acid 303", "Reese DnB", "FM Bell", "FM E-Piano", "Super Pad", "Pluck Lead"]
let DSYN_TEMPLATES = ["Kick", "Snare", "Hat", "Gabber", "Zap", "Clang"]
let NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
let WAVE_NAMES = ["Saw", "Sqr", "Tri", "Sin"]

func noteName(_ midi: Int32) -> String {
    let m = Int(midi)
    return NOTE_NAMES[((m % 12) + 12) % 12] + "\(m / 12 - 1)"
}
func pct(_ v: Float) -> String { "\(Int((v * 100).rounded()))%" }
func signed(_ v: Float, _ unit: String) -> String { "\(v >= 0 ? "+" : "")\(Int(v.rounded()))\(unit)" }
func secs(_ v: Float) -> String { String(format: "%.2fs", v) }
func panText(_ v: Float) -> String {
    if v < -0.02 { return "L\(Int((-v * 100).rounded()))" }
    if v > 0.02 { return "R\(Int((v * 100).rounded()))" }
    return "CTR"
}
func oneDecimal(_ v: Float) -> String { String(format: "%.1f", v) }
func whole(_ v: Float) -> String { "\(Int(v.rounded()))" }
func millis(_ v: Float) -> String { "\(Int((v * 1000).rounded()))ms" }

/// A control the song can move over time: its name and the range its Start and End controls cover.
struct AutoInfo {
    let label: String
    let lo: Float
    let hi: Float
}

let AUTO_PARAMS: [Int32: AutoInfo] = {
    var m: [Int32: AutoInfo] = [:]
    let add: (Int32, String, Float, Float) -> Void = { id, label, lo, hi in m[id] = AutoInfo(label: label, lo: lo, hi: hi) }
    add(ParamId.FX_LEVEL, "Level", 0, 1)
    add(ParamId.FX_DRIVE, "Drive", 0, 1)
    add(ParamId.FX_EQ_LOW, "Low EQ", -12, 12)
    add(ParamId.FX_EQ_HIGH, "High EQ", -12, 12)
    add(ParamId.FX_DELAY, "Delay send", 0, 0.8)
    add(ParamId.FX_REVERB, "Reverb send", 0, 0.8)
    add(ParamId.FX_SLOT1_AMOUNT, "FX1 amount", 0, 1)
    add(ParamId.FX_SLOT1_AMOUNT + 1, "FX1 rate", 0, 1)
    add(ParamId.FX_SLOT1_AMOUNT + 2, "FX2 amount", 0, 1)
    add(ParamId.FX_SLOT1_AMOUNT + 3, "FX2 rate", 0, 1)
    add(ParamId.FX_SLOT1_AMOUNT + 4, "FX3 amount", 0, 1)
    add(ParamId.FX_SLOT1_AMOUNT + 5, "FX3 rate", 0, 1)
    add(ParamId.KIT_HARDNESS, "Hardness", 0, 1)
    add(ParamId.SYN_CUTOFF, "Cutoff", 80, 8000)
    add(ParamId.SYN_RES, "Resonance", 0.5, 15)
    add(ParamId.SYN_DETUNE, "Detune", -30, 30)
    add(ParamId.SYN_ATTACK, "Attack", 0.001, 1)
    add(ParamId.SYN_DECAY, "Decay", 0.02, 3)
    add(ParamId.SYN_SUSTAIN, "Sustain", 0, 1)
    add(ParamId.SYN_RELEASE, "Release", 0.01, 3)
    add(ParamId.SYN_LEVEL, "Synth level", 0, 1)
    add(ParamId.SYN_LFO_RATE, "LFO rate", 0.05, 20)
    add(ParamId.SYN_LFO_DEPTH, "LFO depth", 0, 1)
    add(ParamId.SYN_FM_RATIO, "FM ratio", 0.5, 8)
    add(ParamId.SYN_FM_INDEX, "FM index", 0, 8)
    add(ParamId.SYN_FM_DECAY, "FM decay", 0.01, 3)
    add(ParamId.SYN_FM_FB, "FM feedback", 0, 1)
    add(ParamId.SYN_WARMTH, "Warmth", 0, 1)
    add(ParamId.SYN_MOTION, "Motion", 0, 1)
    add(ParamId.SYN_FOLD, "Fold", 0, 1)
    add(ParamId.SYN_VEC_X, "Vector X", 0, 1)
    add(ParamId.SYN_VEC_Y, "Vector Y", 0, 1)
    add(ParamId.SYN_PLUCK_BRIGHT, "Pluck bright", 0, 1)
    return m
}()

func autoIsSynth(_ id: Int32) -> Bool { (id >= 20 && id <= 39) || (id >= 50 && id <= 61) }
func autoTrackOf(_ id: Int32, _ index: Int32) -> Int32 {
    if id == ParamId.KIT_HARDNESS { return 0 }
    return autoIsSynth(id) ? index + 1 : index
}
func autoIndexOf(_ id: Int32, _ track: Int32) -> Int32 {
    if id == ParamId.KIT_HARDNESS { return 0 }
    return autoIsSynth(id) ? track - 1 : track
}
let AUTO_QUICK_DRUMS: [Int32] = [ParamId.KIT_HARDNESS, ParamId.FX_LEVEL, ParamId.FX_REVERB, ParamId.FX_DELAY, ParamId.FX_EQ_HIGH]
let AUTO_QUICK_SYNTH: [Int32] = [ParamId.SYN_CUTOFF, ParamId.SYN_RES, ParamId.SYN_FOLD, ParamId.FX_DRIVE, ParamId.FX_REVERB, ParamId.FX_DELAY, ParamId.FX_LEVEL]

/// Landscape: two columns that scroll on their own.
struct Panes<L: View, R: View>: View {
    let left: L
    let right: R

    init(@ViewBuilder left: () -> L, @ViewBuilder right: () -> R) {
        self.left = left()
        self.right = right()
    }

    var body: some View {
        HStack(alignment: .top, spacing: 8) {
            ScrollView { VStack(spacing: 8) { left } }.frame(maxWidth: .infinity)
            ScrollView { VStack(spacing: 8) { right } }.frame(maxWidth: .infinity)
        }
    }
}

/// A row of buttons for a control with a few named settings.
struct ChoiceRow: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let label: String
    let paramId: Int32
    let index: Int32
    let options: [String]
    var offset: Int = 0
    var accent: Color? = nil

    var body: some View {
        let cur = Int(state.bridge.getParam(paramId, index: index).rounded())
        HStack(spacing: 6) {
            SilkLabel(text: label.uppercased(), size: 9).frame(width: 56, alignment: .leading)
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 4) {
                    ForEach(0..<options.count, id: \.self) { i in
                        HwButton(text: options[i], on: cur == i + offset, ledColor: accent ?? skin.primary, height: 28) {
                            state.bridge.beginEdit()
                            state.bridge.setParam(paramId, index: index, value: Float(i + offset))
                            state.rev += 1
                        }.fixedSize(horizontal: true, vertical: false)
                    }
                }
            }
        }
    }
}

/// B1 B2 ... for scenes longer than one bar.
struct BarPicker: View {
    @Environment(\.skin) var skin
    let bars: Int
    let current: Int
    let playingBar: Int
    let onPick: (Int) -> Void

    var body: some View {
        if bars > 1 {
            HStack(spacing: 4) {
                ForEach(0..<bars, id: \.self) { b in
                    HwButton(text: "B\(b + 1)", on: b == current, ledColor: b == playingBar ? skin.positive : skin.primary, height: 28) { onPick(b) }
                }
            }
        }
    }
}

/// Outline of a pad's sound, with the part between slice start and end lit.
struct WaveformDisplay: View {
    @Environment(\.skin) var skin
    let peaks: [Float]
    let sliceStart: Float
    let sliceEnd: Float
    let color: Color

    var body: some View {
        Canvas { ctx, size in
            let n = max(1, peaks.count)
            let bw = size.width / CGFloat(n)
            for i in 0..<peaks.count {
                let h = max(1, CGFloat(min(max(peaks[i], 0), 1)) * size.height * 0.92)
                let pos = Float(i) / Float(n)
                let inside = pos >= sliceStart && pos <= sliceEnd
                let r = CGRect(x: CGFloat(i) * bw, y: (size.height - h) / 2, width: max(1, bw - 1), height: h)
                ctx.fill(Path(r), with: .color(inside ? color : color.opacity(0.28)))
            }
        }
        .frame(height: 54)
        .frame(maxWidth: .infinity)
        .background(skin.scopeBg)
        .cornerRadius(6)
    }
}

/// A pad that sounds when the finger goes down, not when it lifts.
struct PressPad<Label: View>: View {
    let onDown: () -> Void
    let onUp: () -> Void
    let label: Label
    @State private var down = false

    init(onDown: @escaping () -> Void, onUp: @escaping () -> Void = {}, @ViewBuilder label: () -> Label) {
        self.onDown = onDown
        self.onUp = onUp
        self.label = label()
    }

    var body: some View {
        label
            .opacity(down ? 0.7 : 1.0)
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { _ in if !down { down = true; Feel.tick(); onDown() } }
                    .onEnded { _ in down = false; onUp() }
            )
    }
}

/// A slider for a value that is not an engine control of its own (the Start and End of an automation).
struct ValueSlider: View {
    @Environment(\.skin) var skin
    let label: String
    let value: Float
    let lo: Float
    let hi: Float
    let fmt: (Float) -> String
    let onStart: () -> Void
    let onChange: (Float) -> Void
    let onEnd: () -> Void
    @State private var startVal: Float? = nil

    var body: some View {
        let span = max(hi - lo, 1e-6)
        let norm = CGFloat(min(max((value - lo) / span, 0), 1))
        VStack(spacing: 4) {
            HStack {
                SilkLabel(text: label.uppercased(), color: skin.muted, size: 8)
                Spacer()
                SilkLabel(text: fmt(value), color: skin.silk, size: 9, mono: true)
            }
            GeometryReader { g in
                ZStack(alignment: .leading) {
                    Capsule().fill(skin.line.opacity(0.6)).frame(height: 5)
                    Capsule().fill(skin.primary).frame(width: max(5, norm * g.size.width), height: 5)
                    RoundedRectangle(cornerRadius: 3).fill(skin.silk).frame(width: 9, height: 20)
                        .offset(x: min(max(norm * g.size.width - 4.5, 0), max(0, g.size.width - 9)))
                }
                .frame(height: 20)
            }
            .frame(height: 20)
        }
        .padding(.vertical, 5)
        .padding(.horizontal, 6)
        .frame(maxWidth: .infinity)
        .background(skin.recess)
        .overlay(RoundedRectangle(cornerRadius: skin.buttonRadius).stroke(startVal != nil ? skin.silk : skin.line, lineWidth: 1))
        .cornerRadius(skin.buttonRadius)
        .contentShape(Rectangle())
        .gesture(
            DragGesture(minimumDistance: 4)
                .onChanged { g in
                    if startVal == nil { startVal = value; onStart() }
                    let delta = Float(g.translation.width) / (130.0 * Feel.travelFactor) * span
                    onChange(min(max((startVal ?? value) + delta, lo), hi))
                }
                .onEnded { _ in startVal = nil; onEnd() }
        )
    }
}

/// The Vector engine's blend: one finger moves between four sources.
struct VectorPad: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let synth: Int32
    let corners: [String]
    @State private var dragging = false

    var body: some View {
        let x = CGFloat(min(max(state.bridge.getParam(ParamId.SYN_VEC_X, index: synth), 0), 1))
        let y = CGFloat(min(max(state.bridge.getParam(ParamId.SYN_VEC_Y, index: synth), 0), 1))
        GeometryReader { g in
            let w = g.size.width
            let h = g.size.height
            ZStack {
                skin.scopeBg
                Path { p in
                    for i in 1...3 {
                        let fx = w * CGFloat(i) / 4
                        let fy = h * CGFloat(i) / 4
                        p.move(to: CGPoint(x: fx, y: 0)); p.addLine(to: CGPoint(x: fx, y: h))
                        p.move(to: CGPoint(x: 0, y: fy)); p.addLine(to: CGPoint(x: w, y: fy))
                    }
                }.stroke(skin.scopeTrace.opacity(0.14), lineWidth: 1)
                Path { p in
                    p.move(to: CGPoint(x: x * w, y: 0)); p.addLine(to: CGPoint(x: x * w, y: h))
                    p.move(to: CGPoint(x: 0, y: y * h)); p.addLine(to: CGPoint(x: w, y: y * h))
                }.stroke(skin.scopeTrace.opacity(0.55), lineWidth: 1)
                Circle().fill(skin.scopeTrace.opacity(0.25)).frame(width: 32, height: 32).position(x: x * w, y: y * h)
                Circle().fill(skin.scopeTrace).frame(width: 14, height: 14)
                    .overlay(Circle().stroke(Color.white, lineWidth: 1.5))
                    .position(x: x * w, y: y * h)
                VStack {
                    HStack { cornerLabel(0); Spacer(); cornerLabel(1) }
                    Spacer()
                    HStack { cornerLabel(2); Spacer(); cornerLabel(3) }
                }.padding(6)
            }
            .contentShape(Rectangle())
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { v in
                        if !dragging { dragging = true; state.bridge.beginEdit() }
                        let nx = Float(min(max(v.location.x / max(1, w), 0), 1))
                        let ny = Float(min(max(v.location.y / max(1, h), 0), 1))
                        state.bridge.setParam(ParamId.SYN_VEC_X, index: synth, value: nx)
                        state.bridge.setParam(ParamId.SYN_VEC_Y, index: synth, value: ny)
                        state.rev += 1
                    }
                    .onEnded { _ in dragging = false; state.rev += 1 }
            )
        }
        .frame(height: 164)
        .cornerRadius(8)
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(dragging ? skin.scopeTrace : skin.line, lineWidth: 1))
    }

    private func cornerLabel(_ i: Int) -> some View {
        SilkLabel(text: i < corners.count ? corners[i] : "", color: skin.scopeTrace.opacity(0.85), size: 9)
    }
}
