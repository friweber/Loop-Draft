import SwiftUI

// ---- 80s Analog Hardware Palette (TR-808 / Juno-106 inspired) ----
enum HwColor {
    static let chassis    = Color(red: 0.09, green: 0.094, blue: 0.11)
    static let faceplate  = Color(red: 0.125, green: 0.133, blue: 0.153)
    static let recess     = Color(red: 0.075, green: 0.078, blue: 0.094)
    static let line       = Color(red: 0.208, green: 0.224, blue: 0.259)
    static let woodLight  = Color(red: 0.42, green: 0.259, blue: 0.149)
    static let woodDark   = Color(red: 0.227, green: 0.129, blue: 0.067)
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
    static let padActive  = Color(red: 1.0, green: 0.957, blue: 0.863)
    static let padInk     = Color(red: 0.106, green: 0.11, blue: 0.125)
}

struct WalnutRail: View {
    var body: some View {
        Rectangle()
            .fill(LinearGradient(colors: [HwColor.woodDark, HwColor.woodLight, HwColor.woodDark],
                                 startPoint: .leading, endPoint: .trailing))
            .frame(width: 7)
    }
}

struct SilkLabel: View {
    let text: String
    var color: Color = HwColor.muted
    var size: CGFloat = 10
    var mono: Bool = false

    var body: some View {
        Text(text)
            .font(mono ? .system(size: size, weight: .bold, design: .monospaced)
                       : .system(size: size, weight: .bold, design: .default))
            .foregroundColor(color)
            .lineLimit(1)
            .minimumScaleFactor(0.75)
    }
}

struct ModulePanel<Trailing: View, Content: View>: View {
    let title: String
    var accent: Color = HwColor.orange
    @ViewBuilder let trailing: () -> Trailing
    @ViewBuilder let content: () -> Content

    init(_ title: String, accent: Color = HwColor.orange,
         @ViewBuilder trailing: @escaping () -> Trailing = { EmptyView() },
         @ViewBuilder content: @escaping () -> Content) {
        self.title = title
        self.accent = accent
        self.trailing = trailing
        self.content = content
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 6) {
                RoundedRectangle(cornerRadius: 2)
                    .fill(accent)
                    .frame(width: 4, height: 14)
                SilkLabel(text: title.uppercased(), color: HwColor.silk, size: 11)
                Spacer(minLength: 4)
                trailing()
            }
            Rectangle()
                .fill(accent.opacity(0.25))
                .frame(height: 1)
            content()
        }
        .padding(10)
        .background(HwColor.faceplate)
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(accent.opacity(0.55), lineWidth: 1.2))
        .cornerRadius(10)
    }
}

struct HwButton: View {
    let text: String
    var on: Bool = false
    var ledColor: Color = HwColor.orange
    var enabled: Bool = true
    var height: CGFloat = 34
    var showLed: Bool = false
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 4) {
                if showLed {
                    Circle()
                        .fill(on ? Color.white : HwColor.ledOff)
                        .frame(width: 6, height: 6)
                }
                SilkLabel(text: text,
                          color: !enabled ? HwColor.muted.opacity(0.4) : (on ? HwColor.padInk : HwColor.silk),
                          size: 10)
            }
            .padding(.horizontal, 7)
            .frame(maxWidth: .infinity, minHeight: height, maxHeight: height)
            .background(!enabled ? HwColor.recess : (on ? ledColor : Color(red: 0.17, green: 0.18, blue: 0.21)))
            .overlay(RoundedRectangle(cornerRadius: 7).stroke(on ? ledColor : HwColor.line, lineWidth: 1))
            .cornerRadius(7)
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

struct RotaryKnob: View {
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
        let norm = Double(min(max((cur - minVal) / (maxVal - minVal), 0), 1))

        VStack(spacing: 3) {
            SilkLabel(text: label.uppercased(), color: modulated ? HwColor.cyan : HwColor.muted, size: 9)
            Canvas { ctx, size in
                let c = CGPoint(x: size.width / 2, y: size.height / 2)
                let r = min(size.width, size.height) / 2 - 4
                let startDeg = 135.0
                let sweepDeg = 270.0

                var track = Path()
                track.addArc(center: c, radius: r, startAngle: .degrees(startDeg), endAngle: .degrees(startDeg + sweepDeg), clockwise: false)
                ctx.stroke(track, with: .color(HwColor.line), style: StrokeStyle(lineWidth: 3, lineCap: .round))

                var active = Path()
                active.addArc(center: c, radius: r, startAngle: .degrees(startDeg), endAngle: .degrees(startDeg + sweepDeg * norm), clockwise: false)
                ctx.stroke(active, with: .color(modulated ? HwColor.cyan : HwColor.orange), style: StrokeStyle(lineWidth: 3.5, lineCap: .round))

                let bodyR = r * 0.72
                ctx.fill(Path(ellipseIn: CGRect(x: c.x - bodyR, y: c.y - bodyR, width: bodyR * 2, height: bodyR * 2)), with: .color(capColor))

                let rad = (startDeg + sweepDeg * norm) * .pi / 180.0
                var ptr = Path()
                ptr.move(to: CGPoint(x: c.x + CGFloat(cos(rad)) * bodyR * 0.25, y: c.y + CGFloat(sin(rad)) * bodyR * 0.25))
                ptr.addLine(to: CGPoint(x: c.x + CGFloat(cos(rad)) * bodyR * 0.88, y: c.y + CGFloat(sin(rad)) * bodyR * 0.88))
                ctx.stroke(ptr, with: .color(HwColor.silk), style: StrokeStyle(lineWidth: 2.4, lineCap: .round))
            }
            .frame(width: 46, height: 46)
            .gesture(
                DragGesture(minimumDistance: 0)
                    .onChanged { g in
                        if dragStartVal == nil {
                            state.bridge.beginEdit()
                            dragStartVal = state.bridge.getParam(paramId, index: index)
                        }
                        let start = dragStartVal ?? cur
                        let delta = Float(-g.translation.height / 140.0) * (maxVal - minVal)
                        let nv = min(max(start + delta, minVal), maxVal)
                        state.bridge.setParam(paramId, index: index, value: nv)
                        state.rev += 1
                    }
                    .onEnded { _ in
                        dragStartVal = nil
                        state.rev += 1
                    }
            )
            SilkLabel(text: fmt(cur), color: HwColor.silk, size: 9, mono: true)
        }
        .padding(.vertical, 5)
        .frame(maxWidth: .infinity)
        .background(HwColor.recess)
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(modulated ? HwColor.cyan.opacity(0.7) : HwColor.line, lineWidth: 1))
        .cornerRadius(8)
    }
}

struct Tr808StepButton: View {
    let stepInBar: Int
    let vel: Int
    let playhead: Bool
    let onTap: () -> Void

    private var groupColor: Color {
        switch (stepInBar / 4) % 4 {
        case 0: return HwColor.red
        case 1: return HwColor.orange
        case 2: return HwColor.yellow
        default: return HwColor.cream
        }
    }

    var body: some View {
        let isRoll = vel >= 3
        let bg: Color = isRoll ? HwColor.cyan : (vel == 2 ? groupColor : (vel == 1 ? groupColor.opacity(0.72) : Color(red: 0.16, green: 0.17, blue: 0.20)))
        let lbl: String = vel == 3 ? "2x" : (vel == 4 ? "3x" : (vel == 5 ? "4x" : "\(stepInBar + 1)"))

        Button(action: onTap) {
            VStack {
                Circle()
                    .fill(playhead ? HwColor.green : (vel >= 2 ? Color.white : (vel == 1 ? HwColor.red : HwColor.ledOff)))
                    .frame(width: 6, height: 6)
                Spacer()
                SilkLabel(text: lbl, color: vel > 0 ? HwColor.padInk : HwColor.muted, size: 9, mono: true)
            }
            .padding(.vertical, 4)
            .frame(maxWidth: .infinity, minHeight: 44, maxHeight: 44)
            .background(bg)
            .overlay(RoundedRectangle(cornerRadius: 6).stroke(playhead ? HwColor.green : (vel >= 2 ? Color.white : HwColor.line), lineWidth: playhead ? 2 : 1))
            .cornerRadius(6)
        }
        .buttonStyle(.plain)
    }
}

struct OledScopeDisplay: View {
    let isFm: Bool
    let wave: Int
    let cutoffNorm: Float
    let fmIndex: Float
    let fmRatio: Float

    var body: some View {
        ZStack(alignment: .topTrailing) {
            Canvas { ctx, size in
                let grid = Color(red: 0.08, green: 0.21, blue: 0.14)
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
                ctx.stroke(path, with: .color(HwColor.green), style: StrokeStyle(lineWidth: 2, lineCap: .round))
            }
            SilkLabel(text: isFm ? "FM 2-OP" : (["SAW", "SQR", "TRI", "SIN"].indices.contains(wave) ? ["SAW", "SQR", "TRI", "SIN"][wave] : "SAW"),
                      color: HwColor.green, size: 9, mono: true)
                .padding(5)
        }
        .frame(height: 60)
        .background(Color(red: 0.03, green: 0.08, blue: 0.05))
        .overlay(RoundedRectangle(cornerRadius: 8).stroke(Color(red: 0.15, green: 0.27, blue: 0.20), lineWidth: 1.5))
        .cornerRadius(8)
    }
}

struct AnalogVuMeter: View {
    let peakL: Float
    let peakR: Float

    var body: some View {
        let limiting = peakL > 0.91 || peakR > 0.91
        HStack(spacing: 8) {
            vuDial(label: "VU L", level: peakL)
            vuDial(label: "VU R", level: peakR)
            VStack(spacing: 4) {
                Circle()
                    .fill(limiting ? HwColor.red : HwColor.green)
                    .frame(width: 14, height: 14)
                SilkLabel(text: "LIMITER", color: HwColor.silk, size: 8)
                SilkLabel(text: limiting ? "ACTIVE" : "READY", color: limiting ? HwColor.orange : HwColor.green, size: 8)
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
