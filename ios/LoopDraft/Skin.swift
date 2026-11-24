import SwiftUI

enum FrameSkin: Int, CaseIterable {
    case walnut80s = 0, studioRack, pocketCream, oledCyber, fieldOlive
    var title: String {
        switch self {
        case .walnut80s: return "WALNUT 80S"
        case .studioRack: return "STUDIO RACK"
        case .pocketCream: return "POCKET CREAM"
        case .oledCyber: return "OLED CYBER"
        case .fieldOlive: return "FIELD OLIVE"
        }
    }
}

enum KnobSkin: Int, CaseIterable {
    case hSlider = 0, ledBar, skirted80s, moogSilver, neonRing, bakelite
    var title: String {
        switch self {
        case .hSlider: return "H-SLIDER (L-R)"
        case .ledBar: return "LED BAR (L-R)"
        case .skirted80s: return "SKIRTED 80S"
        case .moogSilver: return "SILVER CAP"
        case .neonRing: return "NEON RING"
        case .bakelite: return "BAKELITE"
        }
    }
}

enum ButtonSkin: Int, CaseIterable {
    case caps808 = 0, mpcRubber, glassNeon, aluSwitch
    var title: String {
        switch self {
        case .caps808: return "808 CAPS"
        case .mpcRubber: return "MPC RUBBER"
        case .glassNeon: return "GLASS NEON"
        case .aluSwitch: return "ALU SWITCH"
        }
    }
}

enum AccentSkin: Int, CaseIterable {
    case amber808 = 0, acidLime, cyberPink, iceSteel, rubyGold
    var title: String {
        switch self {
        case .amber808: return "AMBER 808"
        case .acidLime: return "ACID LIME"
        case .cyberPink: return "CYBER PINK"
        case .iceSteel: return "ICE STEEL"
        case .rubyGold: return "RUBY GOLD"
        }
    }
}

struct SkinConfig: Equatable {
    var frame: FrameSkin = .walnut80s
    var knob: KnobSkin = .hSlider
    var button: ButtonSkin = .caps808
    var accent: AccentSkin = .amber808

    func encode() -> String {
        "\(frame.rawValue),\(knob.rawValue),\(button.rawValue),\(accent.rawValue)"
    }

    static func decode(_ raw: String?) -> SkinConfig {
        guard let raw = raw else { return SkinConfig() }
        let parts = raw.split(separator: ",").compactMap { Int($0) }
        guard parts.count >= 4 else { return SkinConfig() }
        return SkinConfig(
            frame: FrameSkin(rawValue: parts[0]) ?? .walnut80s,
            knob: KnobSkin(rawValue: parts[1]) ?? .hSlider,
            button: ButtonSkin(rawValue: parts[2]) ?? .caps808,
            accent: AccentSkin(rawValue: parts[3]) ?? .amber808
        )
    }

    static let presets: [(String, SkinConfig)] = [
        ("80S SLIDER", SkinConfig(frame: .walnut80s, knob: .hSlider, button: .caps808, accent: .amber808)),
        ("STUDIO RACK", SkinConfig(frame: .studioRack, knob: .hSlider, button: .aluSwitch, accent: .iceSteel)),
        ("POCKET CREAM", SkinConfig(frame: .pocketCream, knob: .ledBar, button: .mpcRubber, accent: .amber808)),
        ("CYBER OLED", SkinConfig(frame: .oledCyber, knob: .neonRing, button: .glassNeon, accent: .cyberPink)),
        ("ACID FIELD", SkinConfig(frame: .fieldOlive, knob: .bakelite, button: .caps808, accent: .acidLime)),
        ("CLASSIC 808", SkinConfig(frame: .walnut80s, knob: .skirted80s, button: .caps808, accent: .amber808))
    ]
}

struct SkinTheme {
    let config: SkinConfig
    let isLight: Bool
    let chassis: Color
    let faceplate: Color
    let recess: Color
    let line: Color
    let buttonOff: Color
    let railDark: Color
    let railLight: Color
    let silk: Color
    let muted: Color
    let padSurface: Color
    let padInk: Color
    let onAccentInk: Color
    let primary: Color
    let secondary: Color
    let tertiary: Color
    let positive: Color
    let danger: Color
    let ledOff: Color
    let scopeBg: Color
    let scopeTrace: Color
    let buttonRadius: CGFloat
    let panelRadius: CGFloat

    static func resolve(_ cfg: SkinConfig) -> SkinTheme {
        let primary: Color
        let secondary: Color
        let tertiary: Color
        let positive: Color
        switch cfg.accent {
        case .amber808:
            primary = Color(red: 1.0, green: 0.427, blue: 0.122)
            secondary = Color(red: 0.161, green: 0.827, blue: 0.91)
            tertiary = Color(red: 1.0, green: 0.792, blue: 0.157)
            positive = Color(red: 0.231, green: 0.878, blue: 0.514)
        case .acidLime:
            primary = Color(red: 0.706, green: 0.941, blue: 0.18)
            secondary = Color(red: 0.094, green: 0.886, blue: 0.949)
            tertiary = Color(red: 1.0, green: 0.847, blue: 0.231)
            positive = Color(red: 0.243, green: 0.949, blue: 0.608)
        case .cyberPink:
            primary = Color(red: 1.0, green: 0.22, blue: 0.596)
            secondary = Color(red: 0.173, green: 0.91, blue: 0.961)
            tertiary = Color(red: 1.0, green: 0.784, blue: 0.271)
            positive = Color(red: 0.259, green: 0.961, blue: 0.655)
        case .iceSteel:
            primary = Color(red: 0.302, green: 0.651, blue: 1.0)
            secondary = Color(red: 0.22, green: 0.851, blue: 0.753)
            tertiary = Color(red: 1.0, green: 0.718, blue: 0.302)
            positive = Color(red: 0.298, green: 0.878, blue: 0.541)
        case .rubyGold:
            primary = Color(red: 0.91, green: 0.243, blue: 0.298)
            secondary = Color(red: 0.961, green: 0.722, blue: 0.239)
            tertiary = Color(red: 1.0, green: 0.541, blue: 0.278)
            positive = Color(red: 0.235, green: 0.851, blue: 0.525)
        }

        let btnR: CGFloat = cfg.button == .mpcRubber ? 10 : (cfg.button == .aluSwitch ? 3 : 7)
        let pnlR: CGFloat = cfg.frame == .studioRack ? 5 : (cfg.frame == .pocketCream ? 12 : 10)

        switch cfg.frame {
        case .walnut80s:
            return SkinTheme(
                config: cfg, isLight: false,
                chassis: Color(red: 0.082, green: 0.086, blue: 0.102),
                faceplate: Color(red: 0.122, green: 0.129, blue: 0.153),
                recess: Color(red: 0.071, green: 0.075, blue: 0.090),
                line: Color(red: 0.208, green: 0.224, blue: 0.259),
                buttonOff: Color(red: 0.169, green: 0.18, blue: 0.208),
                railDark: Color(red: 0.227, green: 0.129, blue: 0.067),
                railLight: Color(red: 0.42, green: 0.259, blue: 0.149),
                silk: Color(red: 0.922, green: 0.898, blue: 0.835),
                muted: Color(red: 0.588, green: 0.569, blue: 0.522),
                padSurface: Color(red: 0.835, green: 0.808, blue: 0.741),
                padInk: Color(red: 0.106, green: 0.11, blue: 0.125),
                onAccentInk: Color(red: 0.08, green: 0.085, blue: 0.10),
                primary: primary, secondary: secondary, tertiary: tertiary, positive: positive,
                danger: Color(red: 0.898, green: 0.224, blue: 0.208),
                ledOff: Color(red: 0.20, green: 0.15, blue: 0.15),
                scopeBg: Color(red: 0.027, green: 0.078, blue: 0.051),
                scopeTrace: positive, buttonRadius: btnR, panelRadius: pnlR
            )
        case .studioRack:
            return SkinTheme(
                config: cfg, isLight: false,
                chassis: Color(red: 0.071, green: 0.082, blue: 0.106),
                faceplate: Color(red: 0.106, green: 0.125, blue: 0.161),
                recess: Color(red: 0.059, green: 0.075, blue: 0.094),
                line: Color(red: 0.212, green: 0.255, blue: 0.322),
                buttonOff: Color(red: 0.149, green: 0.18, blue: 0.231),
                railDark: Color(red: 0.145, green: 0.173, blue: 0.22),
                railLight: Color(red: 0.239, green: 0.278, blue: 0.349),
                silk: Color(red: 0.929, green: 0.949, blue: 0.98),
                muted: Color(red: 0.549, green: 0.596, blue: 0.671),
                padSurface: Color(red: 0.812, green: 0.843, blue: 0.89),
                padInk: Color(red: 0.071, green: 0.086, blue: 0.118),
                onAccentInk: Color(red: 0.055, green: 0.067, blue: 0.090),
                primary: primary, secondary: secondary, tertiary: tertiary, positive: positive,
                danger: Color(red: 0.91, green: 0.271, blue: 0.271),
                ledOff: Color(red: 0.157, green: 0.184, blue: 0.231),
                scopeBg: Color(red: 0.027, green: 0.067, blue: 0.102),
                scopeTrace: secondary, buttonRadius: btnR, panelRadius: pnlR
            )
        case .pocketCream:
            return SkinTheme(
                config: cfg, isLight: true,
                chassis: Color(red: 0.894, green: 0.875, blue: 0.827),
                faceplate: Color(red: 0.953, green: 0.937, blue: 0.902),
                recess: Color(red: 0.863, green: 0.839, blue: 0.784),
                line: Color(red: 0.71, green: 0.682, blue: 0.624),
                buttonOff: Color(red: 0.902, green: 0.878, blue: 0.827),
                railDark: Color(red: 0.702, green: 0.678, blue: 0.627),
                railLight: Color(red: 0.839, green: 0.816, blue: 0.769),
                silk: Color(red: 0.122, green: 0.125, blue: 0.141),
                muted: Color(red: 0.388, green: 0.376, blue: 0.345),
                padSurface: Color.white,
                padInk: Color(red: 0.102, green: 0.106, blue: 0.118),
                onAccentInk: Color(red: 0.086, green: 0.090, blue: 0.102),
                primary: primary, secondary: secondary, tertiary: tertiary, positive: positive,
                danger: Color(red: 0.851, green: 0.188, blue: 0.173),
                ledOff: Color(red: 0.722, green: 0.690, blue: 0.627),
                scopeBg: Color(red: 0.078, green: 0.094, blue: 0.086),
                scopeTrace: positive, buttonRadius: btnR, panelRadius: pnlR
            )
        case .oledCyber:
            return SkinTheme(
                config: cfg, isLight: false,
                chassis: Color(red: 0.02, green: 0.024, blue: 0.031),
                faceplate: Color(red: 0.047, green: 0.055, blue: 0.078),
                recess: Color(red: 0.02, green: 0.024, blue: 0.039),
                line: Color(red: 0.153, green: 0.176, blue: 0.239),
                buttonOff: Color(red: 0.078, green: 0.094, blue: 0.141),
                railDark: Color(red: 0.031, green: 0.039, blue: 0.071),
                railLight: primary.opacity(0.45),
                silk: Color(red: 0.957, green: 0.965, blue: 1.0),
                muted: Color(red: 0.514, green: 0.549, blue: 0.651),
                padSurface: Color(red: 0.102, green: 0.122, blue: 0.18),
                padInk: Color(red: 0.957, green: 0.965, blue: 1.0),
                onAccentInk: Color(red: 0.035, green: 0.039, blue: 0.055),
                primary: primary, secondary: secondary, tertiary: tertiary, positive: positive,
                danger: Color(red: 1.0, green: 0.2, blue: 0.333),
                ledOff: Color(red: 0.141, green: 0.106, blue: 0.161),
                scopeBg: Color(red: 0.012, green: 0.02, blue: 0.031),
                scopeTrace: primary, buttonRadius: btnR, panelRadius: pnlR
            )
        case .fieldOlive:
            return SkinTheme(
                config: cfg, isLight: false,
                chassis: Color(red: 0.090, green: 0.102, blue: 0.082),
                faceplate: Color(red: 0.133, green: 0.153, blue: 0.122),
                recess: Color(red: 0.078, green: 0.090, blue: 0.071),
                line: Color(red: 0.243, green: 0.278, blue: 0.220),
                buttonOff: Color(red: 0.176, green: 0.20, blue: 0.161),
                railDark: Color(red: 0.173, green: 0.149, blue: 0.094),
                railLight: Color(red: 0.31, green: 0.267, blue: 0.161),
                silk: Color(red: 0.922, green: 0.906, blue: 0.839),
                muted: Color(red: 0.60, green: 0.604, blue: 0.525),
                padSurface: Color(red: 0.82, green: 0.80, blue: 0.718),
                padInk: Color(red: 0.102, green: 0.114, blue: 0.086),
                onAccentInk: Color(red: 0.075, green: 0.086, blue: 0.063),
                primary: primary, secondary: secondary, tertiary: tertiary, positive: positive,
                danger: Color(red: 0.878, green: 0.271, blue: 0.22),
                ledOff: Color(red: 0.20, green: 0.173, blue: 0.133),
                scopeBg: Color(red: 0.043, green: 0.078, blue: 0.039),
                scopeTrace: positive, buttonRadius: btnR, panelRadius: pnlR
            )
        }
    }
}

private struct SkinThemeKey: EnvironmentKey {
    static let defaultValue = SkinTheme.resolve(SkinConfig())
}

extension EnvironmentValues {
    var skin: SkinTheme {
        get { self[SkinThemeKey.self] }
        set { self[SkinThemeKey.self] = newValue }
    }
}

struct SkinStudioSheet: View {
    @ObservedObject var state: EngineState
    @Environment(\.skin) var skin
    @Environment(\.dismiss) var dismiss

    var body: some View {
        ScrollView {
            VStack(spacing: 10) {
                HStack {
                    SilkLabel(text: "SKIN STUDIO", color: skin.silk, size: 15)
                    Spacer()
                    HwButton(text: "DONE", on: true, ledColor: skin.positive, height: 30) {
                        dismiss()
                    }.frame(width: 74)
                }

                ModulePanel("Complete Rig Presets", accent: skin.secondary) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 5) {
                            ForEach(0..<SkinConfig.presets.count, id: \.self) { idx in
                                let item = SkinConfig.presets[idx]
                                HwButton(text: item.0, on: state.skinConfig == item.1, ledColor: skin.secondary, height: 28) {
                                    state.setSkin(item.1)
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                }

                ModulePanel("1. Controls (Left-Right Sliders & Rotary Knobs)", accent: skin.primary) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 5) {
                            ForEach(KnobSkin.allCases, id: \.rawValue) { k in
                                HwButton(text: k.title, on: state.skinConfig.knob == k, ledColor: skin.primary, height: 30, showLed: true) {
                                    var cfg = state.skinConfig; cfg.knob = k; state.setSkin(cfg)
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                }

                ModulePanel("2. Main Frame & Side Rails", accent: skin.tertiary) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 5) {
                            ForEach(FrameSkin.allCases, id: \.rawValue) { f in
                                HwButton(text: f.title, on: state.skinConfig.frame == f, ledColor: skin.tertiary, height: 28) {
                                    var cfg = state.skinConfig; cfg.frame = f; state.setSkin(cfg)
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                }

                ModulePanel("3. Buttons & Pads", accent: skin.secondary) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 5) {
                            ForEach(ButtonSkin.allCases, id: \.rawValue) { b in
                                HwButton(text: b.title, on: state.skinConfig.button == b, ledColor: skin.secondary, height: 28) {
                                    var cfg = state.skinConfig; cfg.button = b; state.setSkin(cfg)
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                }

                ModulePanel("4. Accent Palette", accent: skin.positive) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 5) {
                            ForEach(AccentSkin.allCases, id: \.rawValue) { a in
                                HwButton(text: a.title, on: state.skinConfig.accent == a, ledColor: skin.positive, height: 28) {
                                    var cfg = state.skinConfig; cfg.accent = a; state.setSkin(cfg)
                                }.fixedSize(horizontal: true, vertical: false)
                            }
                        }
                    }
                }
            }
            .padding(12)
        }
        .background(skin.chassis.ignoresSafeArea())
    }
}
