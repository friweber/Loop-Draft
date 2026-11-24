import SwiftUI

private struct PedalSpec {
    let typeId: Int32
    let code: String
    let name: String
    let top: Color
    let bottom: Color
    let ink: Color
    let param1: String
    let param2: String
}

private let PEDAL_SPECS: [PedalSpec] = [
    PedalSpec(typeId: 1, code: "OD-1", name: "OVERDRIVE", top: Color(red: 0.88, green: 0.59, blue: 0.11), bottom: Color(red: 0.71, green: 0.43, blue: 0.05), ink: Color(red: 0.10, green: 0.08, blue: 0.03), param1: "DRIVE", param2: ""),
    PedalSpec(typeId: 2, code: "8-BIT", name: "BITCRUSH", top: Color(red: 0.13, green: 0.55, blue: 0.61), bottom: Color(red: 0.07, green: 0.36, blue: 0.41), ink: .white, param1: "CRUSH", param2: "RATE"),
    PedalSpec(typeId: 3, code: "CE-2", name: "CHORUS", top: Color(red: 0.17, green: 0.43, blue: 0.76), bottom: Color(red: 0.10, green: 0.27, blue: 0.51), ink: .white, param1: "DEPTH", param2: "SPEED"),
    PedalSpec(typeId: 4, code: "PH-90", name: "PHASER", top: Color(red: 0.85, green: 0.34, blue: 0.13), bottom: Color(red: 0.63, green: 0.21, blue: 0.05), ink: .white, param1: "DEPTH", param2: "SPEED"),
    PedalSpec(typeId: 5, code: "MF-101", name: "LO-PASS", top: Color(red: 0.12, green: 0.48, blue: 0.32), bottom: Color(red: 0.07, green: 0.30, blue: 0.20), ink: .white, param1: "CUTOFF", param2: "RESO"),
    PedalSpec(typeId: 6, code: "DYNA", name: "COMP", top: Color(red: 0.76, green: 0.18, blue: 0.20), bottom: Color(red: 0.53, green: 0.11, blue: 0.12), ink: .white, param1: "SQUASH", param2: "")
]

struct GuitarPedalboardView: View {
    @Environment(\.skin) var skin
    @ObservedObject var state: EngineState
    let track: Int32
    let trackName: String

    @State private var pickerSlot: Int = -1

    private func swapSlots(_ a: Int32, _ b: Int32) {
        let fa = state.bridge.fxSlotTrack(track, slot: a)
        let fb = state.bridge.fxSlotTrack(track, slot: b)
        let ta = fa.count > 0 ? fa[0].int32Value : 0
        let aa = fa.count > 1 ? fa[1].floatValue : 0.5
        let ra = fa.count > 2 ? fa[2].floatValue : 0.4
        let ba = fa.count > 3 ? (fa[3].floatValue > 0.5) : false
        let tb = fb.count > 0 ? fb[0].int32Value : 0
        let ab = fb.count > 1 ? fb[1].floatValue : 0.5
        let rb = fb.count > 2 ? fb[2].floatValue : 0.4
        let bb = fb.count > 3 ? (fb[3].floatValue > 0.5) : false
        state.bridge.setFxSlotTrack(track, slot: a, type: tb, amount: ab, rate: rb, bypass: bb)
        state.bridge.setFxSlotTrack(track, slot: b, type: ta, amount: aa, rate: ra, bypass: ba)
        state.rev += 1
    }

    var body: some View {
        ModulePanel("Pedalboard: \(trackName) (IN -> FX1 -> FX2 -> FX3 -> OUT)", accent: skin.secondary) {
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 0) {
                    jackView("IN", color: skin.positive)
                    patchCable(active: true)

                    ForEach(0..<3, id: \.self) { slot in
                        let fd = state.bridge.fxSlotTrack(track, slot: Int32(slot))
                        let fxType = fd.count > 0 ? fd[0].int32Value : 0
                        let fxAmt = fd.count > 1 ? fd[1].floatValue : 0.5
                        let fxRate = fd.count > 2 ? fd[2].floatValue : 0.4
                        let fxByp = fd.count > 3 ? (fd[3].floatValue > 0.5) : false

                        if fxType > 0, let spec = PEDAL_SPECS.first(where: { $0.typeId == fxType }) {
                            stompbox(slot: slot, spec: spec, amt: fxAmt, rate: fxRate, bypass: fxByp)
                        } else {
                            emptyBay(slot: slot)
                        }
                        patchCable(active: fxType > 0 && !fxByp)
                    }

                    jackView("OUT", color: skin.primary)
                }
                .padding(.vertical, 8)
                .padding(.horizontal, 6)
            }
            .background(Color(red: 0.08, green: 0.09, blue: 0.11))
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(Color(red: 0.20, green: 0.22, blue: 0.27), lineWidth: 1.5))
            .cornerRadius(10)

            if pickerSlot >= 0 && pickerSlot < 3 {
                let fd = state.bridge.fxSlotTrack(track, slot: Int32(pickerSlot))
                let curType = fd.count > 0 ? fd[0].int32Value : 0
                let curAmt = fd.count > 1 ? fd[1].floatValue : 0.55
                let curRate = fd.count > 2 ? fd[2].floatValue : 0.42
                VStack(alignment: .leading, spacing: 6) {
                    HStack {
                        SilkLabel(text: "CHOOSE PEDAL FOR SLOT \(pickerSlot + 1)", color: skin.silk, size: 10)
                        Spacer()
                        HwButton(text: "CLOSE", height: 24) { pickerSlot = -1 }.frame(width: 64)
                    }
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 6) {
                            ForEach(0..<PEDAL_SPECS.count, id: \.self) { idx in
                                let sp = PEDAL_SPECS[idx]
                                Button {
                                    state.bridge.setFxSlotTrack(track, slot: Int32(pickerSlot), type: sp.typeId, amount: curAmt, rate: curRate, bypass: false)
                                    pickerSlot = -1
                                    state.rev += 1
                                } label: {
                                    VStack(spacing: 2) {
                                        SilkLabel(text: sp.code, color: sp.ink, size: 11, mono: true)
                                        SilkLabel(text: sp.name, color: sp.ink.opacity(0.85), size: 8)
                                    }
                                    .frame(width: 78, height: 42)
                                    .background(LinearGradient(colors: [sp.top, sp.bottom], startPoint: .top, endPoint: .bottom))
                                    .overlay(RoundedRectangle(cornerRadius: 7).stroke(curType == sp.typeId ? Color.white : Color.black.opacity(0.4), lineWidth: curType == sp.typeId ? 2 : 1))
                                    .cornerRadius(7)
                                }.buttonStyle(.plain)
                            }
                        }
                    }
                }
                .padding(8)
                .background(skin.recess)
                .cornerRadius(8)
            }
        }
    }

    private func jackView(_ label: String, color: Color) -> some View {
        VStack(spacing: 3) {
            ZStack {
                Circle().fill(Color(red: 0.25, green: 0.27, blue: 0.31)).frame(width: 18, height: 18)
                    .overlay(Circle().stroke(Color(red: 0.65, green: 0.68, blue: 0.74), lineWidth: 2))
                Circle().fill(Color.black).frame(width: 7, height: 7)
            }
            SilkLabel(text: label, color: color, size: 8, mono: true)
        }
        .frame(width: 28)
    }

    private func patchCable(active: Bool) -> some View {
        Canvas { ctx, size in
            let y = size.height / 2
            ctx.fill(Path(CGRect(x: 0, y: y - 5, width: 4, height: 10)), with: .color(Color(red: 0.56, green: 0.60, blue: 0.66)))
            ctx.fill(Path(CGRect(x: size.width - 4, y: y - 5, width: 4, height: 10)), with: .color(Color(red: 0.56, green: 0.60, blue: 0.66)))
            var p = Path(); p.move(to: CGPoint(x: 4, y: y)); p.addLine(to: CGPoint(x: size.width - 4, y: y))
            ctx.stroke(p, with: .color(active ? Color(red: 0.84, green: 0.86, blue: 0.90) : Color(red: 0.28, green: 0.30, blue: 0.35)), style: StrokeStyle(lineWidth: 3, lineCap: .round))
        }
        .frame(width: 20, height: 34)
    }

    private func stompbox(slot: Int, spec: PedalSpec, amt: Float, rate: Float, bypass: Bool) -> some View {
        let engaged = !bypass
        let alpha = engaged ? 1.0 : 0.56
        return VStack(spacing: 6) {
            HStack {
                HStack(spacing: 3) {
                    if slot > 0 { miniChip("<") { swapSlots(Int32(slot), Int32(slot - 1)) } }
                    if slot < 2 { miniChip(">") { swapSlots(Int32(slot), Int32(slot + 1)) } }
                }
                Spacer()
                Circle()
                    .fill(engaged ? Color(red: 1.0, green: 0.22, blue: 0.19) : Color(red: 0.20, green: 0.08, blue: 0.08))
                    .frame(width: 10, height: 10)
                    .overlay(Circle().stroke(Color.black.opacity(0.6), lineWidth: 1.2))
                Spacer()
                HStack(spacing: 3) {
                    miniChip("SWP") { pickerSlot = (pickerSlot == slot ? -1 : slot) }
                    miniChip("X") {
                        state.bridge.setFxSlotTrack(track, slot: Int32(slot), type: 0, amount: amt, rate: rate, bypass: false)
                        if pickerSlot == slot { pickerSlot = -1 }
                        state.rev += 1
                    }
                }
            }
            HStack {
                SilkLabel(text: spec.code, color: spec.ink, size: 11, mono: true)
                Spacer()
                SilkLabel(text: spec.name, color: spec.ink.opacity(0.85), size: 8)
            }
            RotaryKnob(state: state, label: spec.param1, paramId: ParamId.FX_SLOT1_AMOUNT + Int32(slot * 2), index: track, minVal: 0, maxVal: 1) {
                "\(Int(($0 * 100).rounded()))%"
            }
            if !spec.param2.isEmpty {
                RotaryKnob(state: state, label: spec.param2, paramId: ParamId.FX_SLOT1_AMOUNT + Int32(slot * 2 + 1), index: track, minVal: 0, maxVal: 1) {
                    "\(Int(($0 * 100).rounded()))%"
                }
            }
            Button {
                state.bridge.setFxSlotTrack(track, slot: Int32(slot), type: spec.typeId, amount: amt, rate: rate, bypass: !bypass)
                state.rev += 1
            } label: {
                HStack {
                    Circle()
                        .fill(RadialGradient(colors: [.white, Color.gray], center: .center, startRadius: 1, endRadius: 8))
                        .frame(width: 15, height: 15)
                    Spacer()
                    SilkLabel(text: engaged ? "ENGAGED" : "BYPASS", color: engaged ? HwColor.green : Color.gray, size: 8, mono: true)
                }
                .padding(.horizontal, 8)
                .frame(height: 32)
                .background(Color(red: 0.09, green: 0.10, blue: 0.12))
                .overlay(RoundedRectangle(cornerRadius: 6).stroke(engaged ? Color.white.opacity(0.4) : Color.gray.opacity(0.4), lineWidth: 1))
                .cornerRadius(6)
            }.buttonStyle(.plain)
        }
        .padding(7)
        .frame(width: 146)
        .background(LinearGradient(colors: [spec.top.opacity(alpha), spec.bottom.opacity(alpha)], startPoint: .top, endPoint: .bottom))
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(engaged ? Color.white.opacity(0.35) : Color.gray.opacity(0.3), lineWidth: 1.5))
        .cornerRadius(10)
    }

    private func miniChip(_ title: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            SilkLabel(text: title, color: .white, size: 8, mono: true)
                .padding(.horizontal, 4)
                .frame(height: 18)
                .background(Color.black.opacity(0.42))
                .cornerRadius(4)
        }.buttonStyle(.plain)
    }

    private func emptyBay(slot: Int) -> some View {
        Button { pickerSlot = (pickerSlot == slot ? -1 : slot) } label: {
            VStack(spacing: 6) {
                ZStack {
                    Circle().fill(pickerSlot == slot ? skin.secondary : Color(red: 0.16, green: 0.18, blue: 0.22)).frame(width: 28, height: 28)
                    SilkLabel(text: "+", color: pickerSlot == slot ? skin.onAccentInk : .white, size: 15)
                }
                SilkLabel(text: "SLOT \(slot + 1)", color: Color.gray, size: 9, mono: true)
                SilkLabel(text: "ADD PEDAL", color: pickerSlot == slot ? skin.secondary : Color.white, size: 9)
            }
            .frame(width: 104, height: 142)
            .background(Color(red: 0.11, green: 0.12, blue: 0.15).opacity(0.7))
            .overlay(RoundedRectangle(cornerRadius: 10).stroke(pickerSlot == slot ? skin.secondary : Color(red: 0.22, green: 0.25, blue: 0.30), lineWidth: 1.5))
            .cornerRadius(10)
        }.buttonStyle(.plain)
    }
}
