import SwiftUI

enum HwColor {
    static let chassis    = Color(red: 0.09, green: 0.094, blue: 0.11)
    static let faceplate  = Color(red: 0.125, green: 0.133, blue: 0.153)
    static let recess     = Color(red: 0.075, green: 0.078, blue: 0.094)
    static let line       = Color(red: 0.208, green: 0.224, blue: 0.259)
    static let silk       = Color(red: 0.922, green: 0.898, blue: 0.835)
    static let muted      = Color(red: 0.588, green: 0.569, blue: 0.522)
    static let red        = Color(red: 0.898, green: 0.224, blue: 0.208)
    static let orange     = Color(red: 1.0, green: 0.427, blue: 0.122)
    static let yellow     = Color(red: 1.0, green: 0.792, blue: 0.157)
    static let cream      = Color(red: 0.929, green: 0.902, blue: 0.831)
    static let cyan       = Color(red: 0.161, green: 0.827, blue: 0.91)
    static let green      = Color(red: 0.231, green: 0.878, blue: 0.514)
    static let ledOff     = Color(red: 0.20, green: 0.15, blue: 0.15)
    static let padSurface = Color(red: 0.835, green: 0.808, blue: 0.741)
    static let padInk     = Color(red: 0.106, green: 0.11, blue: 0.125)
}

struct WalnutRail: View {
    @Environment(\.skin) var skin
    var body: some View {
        Rectangle()
            .fill(LinearGradient(colors: [skin.railDark, skin.railLight, skin.railDark],
                                 startPoint: .leading, endPoint: .trailing))
            .frame(width: 7)
    }
}

struct SilkLabel: View {
    @Environment(\.skin) var skin
    let text: String
    var color: Color? = nil
    var size: CGFloat = 10
    var mono: Bool = false

    var body: some View {
        Text(text)
            .font(mono ? .system(size: size, weight: .bold, design: .monospaced)
                       : .system(size: size, weight: .bold, design: .default))
            .foregroundColor(color ?? skin.muted)
            .lineLimit(1)
            .minimumScaleFactor(0.72)
    }
}

struct LayerSwitcher: View {
    @Environment(\.skin) var skin
    let layers: [String]
    let selected: Int
    let accent: Color
    let onSelect: (Int) -> Void

    var body: some View {
        HStack(spacing: 4) {
            ForEach(0..<layers.count, id: \.self) { idx in
                let on = selected == idx
                Button { onSelect(idx) } label: {
                    SilkLabel(text: layers[idx], color: on ? skin.onAccentInk : skin.muted, size: 10)
                        .frame(maxWidth: .infinity, minHeight: 30, maxHeight: 30)
                        .background(on ? accent : Color.clear)
                        .cornerRadius(max(3, skin.buttonRadius - 1))
                }
                .buttonStyle(.plain)
            }
        }
        .padding(3)
        .background(skin.recess)
        .overlay(RoundedRectangle(cornerRadius: skin.panelRadius).stroke(skin.line, lineWidth: 1))
        .cornerRadius(skin.panelRadius)
    }
}

struct ModulePanel<Trailing: View, Content: View>: View {
    @Environment(\.skin) var skin
    let title: String
    var accent: Color? = nil
    @ViewBuilder let trailing: () -> Trailing
    @ViewBuilder let content: () -> Content

    init(_ title: String, accent: Color? = nil,
         @ViewBuilder trailing: @escaping () -> Trailing = { EmptyView() },
         @ViewBuilder content: @escaping () -> Content) {
        self.title = title
        self.accent = accent
        self.trailing = trailing
        self.content = content
    }

    var body: some View {
        let acc = accent ?? skin.primary
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                RoundedRectangle(cornerRadius: 2)
                    .fill(acc)
                    .frame(width: 4, height: 14)
                SilkLabel(text: title.uppercased(), color: skin.silk, size: 11)
                Spacer(minLength: 4)
                trailing()
            }
            Rectangle()
                .fill(acc.opacity(0.25))
                .frame(height: 1)
            content()
        }
        .padding(10)
        .background(skin.faceplate)
        .overlay(RoundedRectangle(cornerRadius: skin.panelRadius).stroke(acc.opacity(0.55), lineWidth: 1.2))
        .cornerRadius(skin.panelRadius)
    }
}

struct HwButton: View {
    @Environment(\.skin) var skin
    let text: String
    var on: Bool = false
    var ledColor: Color? = nil
    var enabled: Bool = true
    var height: CGFloat = 34
    var showLed: Bool = false
    let action: () -> Void

    var body: some View {
        let activeCol = ledColor ?? skin.primary
        let isGlass = skin.config.button == .glassNeon
        let bg: Color = !enabled ? skin.recess : (isGlass ? (on ? activeCol.opacity(0.22) : skin.recess) : (on ? activeCol : skin.buttonOff))
        let fg: Color = !enabled ? skin.muted.opacity(0.4) : (isGlass && on ? activeCol : (on ? skin.onAccentInk : skin.silk))

        Button(action: action) {
            HStack(spacing: 4) {
                if showLed {
                    Circle()
                        .fill(on ? (isGlass ? activeCol : Color.white) : skin.ledOff)
                        .frame(width: 6, height: 6)
                }
                SilkLabel(text: text, color: fg, size: 10)
            }
            .padding(.horizontal, 7)
            .frame(maxWidth: .infinity, minHeight: height, maxHeight: height)
            .background(bg)
            .overlay(RoundedRectangle(cornerRadius: skin.buttonRadius).stroke(on ? activeCol : skin.line, lineWidth: isGlass && on ? 1.5 : 1))
            .cornerRadius(skin.buttonRadius)
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
    }
}

struct SevenSegmentBpm: View {
    let bpm: Int
    var body: some View {
        Text(String(format: "%3d", bpm))
            .font(.system(size: 18, weight: .heavy, design: .monospaced))
            .foregroundColor(Color(red: 1.0, green: 0.23, blue: 0.19))
            .padding(.horizontal, 10)
            .padding(.vertical, 4)
            .background(Color(red: 0.04, green: 0.02, blue: 0.02))
            .overlay(RoundedRectangle(cornerRadius: 6).stroke(Color(red: 0.26, green: 0.10, blue: 0.10), lineWidth: 1))
            .cornerRadius(6)
    }
}

/// Skin-aware control: renders Left-Right Horizontal Slider (`hSlider`), LED Bar (`ledBar`),
/// or Rotary Knobs (`skirted80s`, `moogSilver`, `neonRing`, `bakelite`). All drag left <-> right!
struct RotaryKnob: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let label: String
    let paramId: Int32
    let index: Int32
    let minVal: Float
    let maxVal: Float
    var capColor: Color = Color(red: 0.20, green: 0.21, blue: 0.25)
    var modulated: Bool = false
    let fmt: (Float) -> String

    @State private var dragStartVal: Float? = nil

    var body: some View {
        let cur = state.bridge.getParam(paramId, index: index)
        let span = max(maxVal - minVal, 1e-6)
        let norm = Double(min(max((cur - minVal) / span, 0), 1))
        let isBipolar = minVal < -1e-4 && maxVal > 1e-4
        let zeroNorm = isBipolar ? Double((-minVal) / span) : 0.0
        let ring = modulated ? skin.secondary : skin.primary
        let isSlider = skin.config.knob == .hSlider || skin.config.knob == .ledBar

        VStack(spacing: isSlider ? 4 : 3) {
            if isSlider {
                HStack {
                    SilkLabel(text: label.uppercased(), color: modulated ? skin.secondary : skin.muted, size: 8)
                    Spacer(minLength: 2)
                    SilkLabel(text: fmt(cur), color: dragStartVal != nil ? ring : skin.silk, size: 9, mono: true)
                }
                if skin.config.knob == .hSlider {
                    Canvas { ctx, size in
                        let padX: CGFloat = 7
                        let trackW = max(size.width - padX * 2, 8)
                        let trackH: CGFloat = 7
                        let cy = size.height / 2
                        let trackRect = CGRect(x: padX, y: cy - trackH / 2, width: trackW, height: trackH)
                        ctx.fill(Path(roundedRect: trackRect, cornerRadius: trackH / 2),
                                 with: .color(skin.isLight ? Color(red: 0.74, green: 0.71, blue: 0.65) : Color(red: 0.04, green: 0.04, blue: 0.05)))
                        let thumbX = padX + trackW * CGFloat(norm)
                        let fStart = isBipolar ? padX + trackW * CGFloat(min(zeroNorm, norm)) : padX
                        let fEnd = isBipolar ? padX + trackW * CGFloat(max(zeroNorm, norm)) : thumbX
                        if fEnd - fStart > 1 {
                            let fillRect = CGRect(x: fStart, y: cy - trackH / 2 + 1, width: fEnd - fStart, height: trackH - 2)
                            ctx.fill(Path(roundedRect: fillRect, cornerRadius: trackH / 2), with: .color(ring))
                        }
                        let tw: CGFloat = 12, th: CGFloat = 19
                        let capRect = CGRect(x: thumbX - tw / 2, y: cy - th / 2, width: tw, height: th)
                        ctx.fill(Path(roundedRect: capRect, cornerRadius: 3), with: .color(skin.isLight ? Color(red: 0.2, green: 0.21, blue: 0.24) : Color(red: 0.86, green: 0.84, blue: 0.78)))
                        var line = Path()
                        line.move(to: CGPoint(x: thumbX, y: cy - th / 2 + 3))
                        line.addLine(to: CGPoint(x: thumbX, y: cy + th / 2 - 3))
                        ctx.stroke(line, with: .color(skin.isLight ? .white : .black), style: StrokeStyle(lineWidth: 2, lineCap: .round))
                    }
                    .frame(height: 24)
                } else {
                    Canvas { ctx, size in
                        let segs = 12
                        let gap: CGFloat = 2
                        let sw = max((size.width - gap * CGFloat(segs - 1)) / CGFloat(segs), 2)
                        let bh: CGFloat = 13
                        let topY = (size.height - bh) / 2
                        let activeIdx = Int((norm * Double(segs)).rounded())
                        let zeroIdx = Int((zeroNorm * Double(segs)).rounded())
                        for i in 0..<segs {
                            let lit = isBipolar ? (activeIdx >= zeroIdx ? (i >= zeroIdx && i < activeIdx) : (i >= activeIdx && i < zeroIdx)) : (i < activeIdx)
                            let col = lit ? ring : (skin.isLight ? Color(red: 0.77, green: 0.75, blue: 0.69) : Color(red: 0.11, green: 0.12, blue: 0.16))
                            ctx.fill(Path(roundedRect: CGRect(x: CGFloat(i) * (sw + gap), y: topY, width: sw, height: bh), cornerRadius: 2), with: .color(col))
                        }
                    }
                    .frame(height: 24)
                }
            } else {
                SilkLabel(text: label.uppercased(), color: modulated ? skin.secondary : skin.muted, size: 9)
                Canvas { ctx, size in
                    let c = CGPoint(x: size.width / 2, y: size.height / 2)
                    let r = min(size.width, size.height) / 2 - 4
                    let startDeg = 135.0
                    let sweepDeg = 270.0
                    var track = Path()
                    track.addArc(center: c, radius: r, startAngle: .degrees(startDeg), endAngle: .degrees(startDeg + sweepDeg), clockwise: false)
                    ctx.stroke(track, with: .color(skin.line), style: StrokeStyle(lineWidth: 3, lineCap: .round))
                    var active = Path()
                    active.addArc(center: c, radius: r, startAngle: .degrees(startDeg), endAngle: .degrees(startDeg + sweepDeg * norm), clockwise: false)
                    ctx.stroke(active, with: .color(ring), style: StrokeStyle(lineWidth: 3.5, lineCap: .round))
                    let bodyR = r * 0.72
                    let fillCol: Color = skin.config.knob == .moogSilver ? Color(red: 0.76, green: 0.80, blue: 0.86) : (skin.config.knob == .bakelite ? Color(red: 0.23, green: 0.16, blue: 0.12) : capColor)
                    ctx.fill(Path(ellipseIn: CGRect(x: c.x - bodyR, y: c.y - bodyR, width: bodyR * 2, height: bodyR * 2)), with: .color(fillCol))
                    let rad = (startDeg + sweepDeg * norm) * .pi / 180.0
                    var ptr = Path()
                    ptr.move(to: CGPoint(x: c.x + CGFloat(cos(rad)) * bodyR * 0.22, y: c.y + CGFloat(sin(rad)) * bodyR * 0.22))
                    ptr.addLine(to: CGPoint(x: c.x + CGFloat(cos(rad)) * bodyR * 0.88, y: c.y + CGFloat(sin(rad)) * bodyR * 0.88))
                    let ptrCol: Color = skin.config.knob == .moogSilver ? .black : (skin.config.knob == .neonRing ? ring : Color(red: 0.92, green: 0.90, blue: 0.84))
                    ctx.stroke(ptr, with: .color(ptrCol), style: StrokeStyle(lineWidth: 2.4, lineCap: .round))
                }
                .frame(width: 46, height: 46)
                SilkLabel(text: fmt(cur), color: skin.silk, size: 9, mono: true)
            }
        }
        .padding(.vertical, 5)
        .padding(.horizontal, isSlider ? 6 : 2)
        .frame(maxWidth: .infinity)
        .background(skin.recess)
        .overlay(RoundedRectangle(cornerRadius: skin.buttonRadius).stroke(dragStartVal != nil ? skin.silk : (modulated ? skin.secondary.opacity(0.75) : skin.line), lineWidth: 1))
        .cornerRadius(skin.buttonRadius)
        .gesture(
            DragGesture(minimumDistance: 4)
                .onChanged { g in
                    if dragStartVal == nil {
                        state.bridge.beginEdit()
                        dragStartVal = state.bridge.getParam(paramId, index: index)
                    }
                    let start = dragStartVal ?? cur
                    let travel: Float = isSlider ? 130.0 : 180.0
                    let delta = Float(g.translation.width) / travel * span
                    let nv = min(max(start + delta, minVal), maxVal)
                    state.bridge.setParam(paramId, index: index, value: nv)
                    state.rev += 1
                }
                .onEnded { _ in
                    dragStartVal = nil
                    state.rev += 1
                }
        )
    }
}

struct Tr808StepButton: View {
    @Environment(\.skin) var skin
    let stepInBar: Int
    let vel: Int
    let playhead: Bool
    let onTap: () -> Void

    private var groupColor: Color {
        switch (stepInBar / 4) % 4 {
        case 0: return HwColor.red
        case 1: return skin.primary
        case 2: return skin.tertiary
        default: return HwColor.cream
        }
    }

    var body: some View {
        let isRoll = vel >= 3
        let bg: Color = isRoll ? skin.secondary : (vel == 2 ? groupColor : (vel == 1 ? groupColor.opacity(0.72) : skin.buttonOff))
        let lbl: String = vel == 3 ? "2x" : (vel == 4 ? "3x" : (vel == 5 ? "4x" : "\(stepInBar + 1)"))

        Button(action: onTap) {
            VStack {
                Circle()
                    .fill(playhead ? skin.positive : (vel >= 2 ? Color.white : (vel == 1 ? HwColor.red : skin.ledOff)))
                    .frame(width: 6, height: 6)
                Spacer()
                SilkLabel(text: lbl, color: vel > 0 ? skin.onAccentInk : skin.muted, size: 9, mono: true)
            }
            .padding(.vertical, 4)
            .frame(maxWidth: .infinity, minHeight: 44, maxHeight: 44)
            .background(bg)
            .overlay(RoundedRectangle(cornerRadius: skin.buttonRadius).stroke(playhead ? skin.positive : (vel >= 2 ? Color.white : skin.line), lineWidth: playhead ? 2 : 1))
            .cornerRadius(skin.buttonRadius)
        }
        .buttonStyle(.plain)
    }
}

struct OledScopeDisplay: View {
    @Environment(\.skin) var skin
    let isFm: Bool
    let wave: Int
    let cutoffNorm: Float
    let fmIndex: Float
    let fmRatio: Float

    var body: some View {
        ZStack(alignment: .topTrailing) {
            Canvas { ctx, size in
                let grid = skin.scopeTrace.opacity(0.18)
                for i in 1...3 {
                    let y = size.height * CGFloat(i) / 4
                    var p = Path(); p.move(to: CGPoint(x: 0, y: y)); p.addLine(to: CGPoint(x: size.width, y: y))
                    ctx.stroke(p, with: .color(grid), lineWidth: 1)
                }
                var path = Path()
                let samples = 100
                let midY = size.height / 2
                let amp = size.height * 0.38
                for i in 0...samples {
                    let u = Double(i) / Double(samples)
                    let t = (u * 2.0).truncatingRemainder(dividingBy: 1.0)
                    let raw: Double
                    if isFm {
                        raw = sin(2.0 * .pi * u * 2.0 + Double(fmIndex) * 0.65 * sin(2.0 * .pi * u * 2.0 * Double(fmRatio)))
                    } else {
                        switch wave {
                        case 0: raw = 2.0 * t - 1.0
                        case 1: raw = t < 0.5 ? 1.0 : -1.0
                        case 2: raw = 4.0 * abs(t - 0.5) - 1.0
                        default: raw = sin(2.0 * .pi * t)
                        }
                    }
                    let smooth = sin(2.0 * .pi * t)
                    let shaped = smooth * Double(1 - cutoffNorm) + raw * Double(cutoffNorm)
                    let pt = CGPoint(x: CGFloat(u) * size.width, y: midY - CGFloat(shaped) * amp)
                    if i == 0 { path.move(to: pt) } else { path.addLine(to: pt) }
                }
                ctx.stroke(path, with: .color(skin.scopeTrace), style: StrokeStyle(lineWidth: 2, lineCap: .round))
            }
            SilkLabel(text: isFm ? "FM 2-OP" : (["SAW", "SQR", "TRI", "SIN"].indices.contains(wave) ? ["SAW", "SQR", "TRI", "SIN"][wave] : "SAW"),
                      color: skin.scopeTrace, size: 9, mono: true)
                .padding(5)
        }
        .frame(height: 60)
        .background(skin.scopeBg)
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(skin.scopeTrace.opacity(0.35), lineWidth: 1.5))
        .cornerRadius(8)
    }
}

struct AnalogVuMeter: View {
    @Environment(\.skin) var skin
    let peakL: Float
    let peakR: Float

    var body: some View {
        let limiting = peakL > 0.91 || peakR > 0.91
        HStack(spacing: 8) {
            vuDial(label: "VU L", level: peakL)
            vuDial(label: "VU R", level: peakR)
            VStack(spacing: 4) {
                Circle()
                    .fill(limiting ? skin.danger : skin.positive)
                    .frame(width: 14, height: 14)
                SilkLabel(text: "LIMITER", color: skin.silk, size: 8)
                SilkLabel(text: limiting ? "ACTIVE" : "READY", color: limiting ? skin.primary : skin.positive, size: 8)
            }
            .frame(width: 52)
        }
    }

    private func vuDial(label: String, level: Float) -> some View {
        let f = Double(min(max(level / 0.944, 0), 1))
        return ZStack(alignment: .bottom) {
            Canvas { ctx, size in
                let pivot = CGPoint(x: size.width / 2, y: size.height * 1.15)
                let rad = size.height * 0.92
                var arc = Path()
                arc.addArc(center: pivot, radius: rad, startAngle: .degrees(228), endAngle: .degrees(312), clockwise: false)
                ctx.stroke(arc, with: .color(Color(red: 0.24, green: 0.19, blue: 0.09)), lineWidth: 2)
                let deg = (228.0 + 84.0 * f) * .pi / 180.0
                var needle = Path()
                needle.move(to: pivot)
                needle.addLine(to: CGPoint(x: pivot.x + CGFloat(cos(deg)) * rad * 0.95, y: pivot.y + CGFloat(sin(deg)) * rad * 0.95))
                ctx.stroke(needle, with: .color(.black), style: StrokeStyle(lineWidth: 2, lineCap: .round))
            }
            SilkLabel(text: label, color: Color(red: 0.23, green: 0.18, blue: 0.09), size: 9)
                .padding(.bottom, 3)
        }
        .frame(height: 58)
        .background(LinearGradient(colors: [Color(red: 0.95, green: 0.87, blue: 0.66), Color(red: 0.85, green: 0.72, blue: 0.42)],
                                   startPoint: .top, endPoint: .bottom))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(red: 0.29, green: 0.24, blue: 0.13), lineWidth: 1.5))
        .cornerRadius(8)
    }
}
