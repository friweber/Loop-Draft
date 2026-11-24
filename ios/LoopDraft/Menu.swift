import SwiftUI
import UIKit
import UniformTypeIdentifiers

/// How the app feels in the hand, apart from how it looks. Kept in UserDefaults.
enum Feel {
    /// 0 fine, 1 normal, 2 fast: the finger travel of sliders and knobs.
    static var speed: Int = (UserDefaults.standard.object(forKey: "loopdraft_speed") as? Int) ?? 1 {
        didSet { UserDefaults.standard.set(speed, forKey: "loopdraft_speed") }
    }
    static var haptics: Bool = (UserDefaults.standard.object(forKey: "loopdraft_haptics") as? Bool) ?? true {
        didSet { UserDefaults.standard.set(haptics, forKey: "loopdraft_haptics") }
    }
    /// Multiplies the finger travel for a control's whole range: more travel is finer.
    static var travelFactor: Float { speed == 0 ? 1.8 : (speed == 2 ? 0.6 : 1.0) }

    static func tick() {
        if haptics { UIImpactFeedbackGenerator(style: .light).impactOccurred() }
    }
}

/// One file of the library: an instrument, a drum kit or a groove.
struct LibItem: Identifiable {
    let id: String          // the file's path
    let kind: String        // "instrument", "drumkit", "groove"
    let name: String
    let style: String
    let user: Bool
}

/// LIBRARY / PROJECTS / EXPORT in one sheet.
struct MenuSheet: View {
    @ObservedObject var state: EngineState
    @Environment(\.skin) var skin
    @Environment(\.dismiss) var dismiss

    @State private var page: Int = 0
    @State private var items: [LibItem] = []
    @State private var kind: String = "groove"
    @State private var style: String = ""
    @State private var tempoKey: Bool = true
    @State private var message: String = ""
    @State private var showImporter: Bool = false

    @State private var projectName: String = ""
    @State private var projects: [String] = []

    @State private var exporting: Bool = false
    @State private var exported: URL? = nil

    var body: some View {
        VStack(spacing: 8) {
            HStack(spacing: 5) {
                HwButton(text: "LIBRARY", on: page == 0, ledColor: skin.primary, height: 30) { page = 0 }
                HwButton(text: "PROJECTS", on: page == 1, ledColor: skin.secondary, height: 30) { page = 1 }
                HwButton(text: "EXPORT", on: page == 2, ledColor: skin.tertiary, height: 30) { page = 2 }
                HwButton(text: "DONE", on: true, ledColor: skin.positive, height: 30) { dismiss() }.frame(width: 64)
            }
            if !message.isEmpty {
                Text(message)
                    .font(.system(size: 11, weight: .bold))
                    .foregroundColor(skin.silk)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            ScrollView {
                VStack(spacing: 8) {
                    if page == 0 { libraryPage } else if page == 1 { projectsPage } else { exportPage }
                }
            }
        }
        .padding(10)
        .background(skin.chassis.ignoresSafeArea())
        .onAppear { loadLibrary(); loadProjects() }
        .fileImporter(isPresented: $showImporter, allowedContentTypes: [.json]) { result in
            if case .success(let url) = result { importOwnFile(url) }
        }
    }

    // ---- Library ----

    private var libraryPage: some View {
        let ofKind = items.filter { $0.kind == kind }
        let styles = Array(Set(ofKind.map { $0.style }.filter { !$0.isEmpty })).sorted()
        let shown = ofKind.filter { style.isEmpty || $0.style == style }
        return VStack(spacing: 8) {
            ModulePanel("Library: \(shown.count) of \(items.count)", accent: skin.primary) {
                HStack(spacing: 5) {
                    HwButton(text: "GROOVES", on: kind == "groove", ledColor: skin.primary, height: 28) { kind = "groove"; style = "" }
                    HwButton(text: "SYNTHS", on: kind == "instrument", ledColor: skin.primary, height: 28) { kind = "instrument"; style = "" }
                    HwButton(text: "DRUM KITS", on: kind == "drumkit", ledColor: skin.primary, height: 28) { kind = "drumkit"; style = "" }
                }
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 4) {
                        HwButton(text: "ALL STYLES", on: style.isEmpty, ledColor: skin.secondary, height: 26) { style = "" }
                            .fixedSize(horizontal: true, vertical: false)
                        ForEach(styles, id: \.self) { st in
                            HwButton(text: st.uppercased(), on: style == st, ledColor: skin.secondary, height: 26) { style = st }
                                .fixedSize(horizontal: true, vertical: false)
                        }
                    }
                }
                if kind == "groove" {
                    HwButton(text: tempoKey ? "GROOVES BRING THEIR TEMPO AND KEY" : "KEEP MY TEMPO AND KEY", on: tempoKey, ledColor: skin.positive, height: 28, showLed: true) {
                        tempoKey.toggle()
                    }
                }
                SilkLabel(text: targetHint, color: skin.muted, size: 9)
                HwButton(text: "LOAD MY OWN .JSON FILE", height: 28) { showImporter = true }
            }
            ForEach(shown) { item in
                Button(action: { apply(item) }) {
                    HStack {
                        VStack(alignment: .leading, spacing: 2) {
                            SilkLabel(text: item.name, color: skin.silk, size: 12)
                            SilkLabel(text: item.style.isEmpty ? item.kind : item.style, color: skin.muted, size: 9)
                        }
                        Spacer()
                        SilkLabel(text: "LOAD", color: skin.primary, size: 10)
                    }
                    .padding(.horizontal, 10)
                    .padding(.vertical, 8)
                    .background(skin.recess)
                    .overlay(RoundedRectangle(cornerRadius: skin.buttonRadius).stroke(skin.line, lineWidth: 1))
                    .cornerRadius(skin.buttonRadius)
                }
                .buttonStyle(.plain)
            }
        }
    }

    private var targetHint: String {
        if kind == "groove" { return "Loads into scene \(state.scene + 1). UNDO takes it back." }
        if kind == "instrument" { return "Loads into synth \(state.synth + 1). UNDO takes it back." }
        return "Replaces the drum kit. UNDO takes it back."
    }

    private func apply(_ item: LibItem) {
        guard let json = try? String(contentsOfFile: item.id, encoding: .utf8) else { message = "\(item.name): could not be read"; return }
        let err = state.bridge.importContent(json, scene: state.scene, synth: state.synth, tempoKey: tempoKey)
        message = err.isEmpty ? "Loaded: \(item.name)" : "\(item.name): \(err)"
        if err.isEmpty { Feel.tick() }
        state.rev += 1
    }

    private func importOwnFile(_ url: URL) {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        guard let json = try? String(contentsOf: url, encoding: .utf8) else { message = "The file could not be read."; return }
        let err = state.bridge.importContent(json, scene: state.scene, synth: state.synth, tempoKey: tempoKey)
        if err.isEmpty {
            // keep a copy, so it shows up in the library next time
            let dir = MenuSheet.documents().appendingPathComponent("library", isDirectory: true)
            try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
            try? json.write(to: dir.appendingPathComponent(url.lastPathComponent), atomically: true, encoding: .utf8)
            message = "Loaded: \(url.lastPathComponent)"
            loadLibrary()
        } else {
            message = "\(url.lastPathComponent): \(err)"
        }
        state.rev += 1
    }

    private func loadLibrary() {
        var dirs: [(URL, Bool)] = []
        if let builtIn = Bundle.main.resourceURL?.appendingPathComponent("library", isDirectory: true) {
            for sub in ["grooves", "instruments", "drumkits"] { dirs.append((builtIn.appendingPathComponent(sub, isDirectory: true), false)) }
        }
        dirs.append((MenuSheet.documents().appendingPathComponent("library", isDirectory: true), true))
        DispatchQueue.global(qos: .userInitiated).async {
            var found: [LibItem] = []
            for (dir, user) in dirs {
                let names = (try? FileManager.default.contentsOfDirectory(atPath: dir.path)) ?? []
                for n in names where n.hasSuffix(".json") {
                    let path = dir.appendingPathComponent(n).path
                    guard let data = FileManager.default.contents(atPath: path),
                          let obj = try? JSONSerialization.jsonObject(with: data),
                          let dict = obj as? [String: Any],
                          let k = dict["kind"] as? String else { continue }
                    let nm = (dict["name"] as? String) ?? n
                    let st = (dict["style"] as? String) ?? ""
                    found.append(LibItem(id: path, kind: k, name: nm, style: st, user: user))
                }
            }
            found.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
            DispatchQueue.main.async { items = found }
        }
    }

    // ---- Projects ----

    private var projectsPage: some View {
        VStack(spacing: 8) {
            ModulePanel("Save this project", accent: skin.secondary) {
                TextField("Name", text: $projectName)
                    .textFieldStyle(.roundedBorder)
                    .autocorrectionDisabled(true)
                HwButton(text: "SAVE", on: !cleanName.isEmpty, ledColor: skin.positive, enabled: !cleanName.isEmpty, height: 32) { saveProject() }
                SilkLabel(text: "The app also keeps what you are working on by itself.", color: skin.muted, size: 9)
            }
            ModulePanel("Open (\(projects.count))", accent: skin.secondary) {
                if projects.isEmpty { SilkLabel(text: "No saved projects yet.", color: skin.muted, size: 10) }
                ForEach(projects, id: \.self) { name in
                    HwButton(text: name, height: 34) { openProject(name) }
                }
                SilkLabel(text: "UNDO goes back to what was open before.", color: skin.muted, size: 9)
            }
        }
    }

    private var cleanName: String {
        let allowed = CharacterSet.alphanumerics.union(CharacterSet(charactersIn: " -_"))
        return String(projectName.unicodeScalars.filter { allowed.contains($0) }).trimmingCharacters(in: .whitespaces)
    }

    private static func documents() -> URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first ?? URL(fileURLWithPath: NSTemporaryDirectory())
    }

    private func projectsDir() -> URL {
        let dir = MenuSheet.documents().appendingPathComponent("projects", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }

    private func loadProjects() {
        let names = (try? FileManager.default.contentsOfDirectory(atPath: projectsDir().path)) ?? []
        projects = names.filter { $0.hasSuffix(".ldp") }.map { String($0.dropLast(4)) }.sorted()
    }

    private func saveProject() {
        let name = cleanName
        if name.isEmpty { return }
        let ok = state.bridge.saveProjectCopy(projectsDir().appendingPathComponent(name + ".ldp").path)
        message = ok ? "Saved: \(name)" : "Could not save \(name)."
        if ok { Feel.tick() }
        loadProjects()
    }

    private func openProject(_ name: String) {
        let ok = state.bridge.openProject(projectsDir().appendingPathComponent(name + ".ldp").path)
        message = ok ? "Opened: \(name)" : "\(name) could not be opened."
        state.rev += 1
    }

    // ---- Export ----

    private var exportPage: some View {
        VStack(spacing: 8) {
            ModulePanel("Export", accent: skin.tertiary) {
                HStack(spacing: 5) {
                    HwButton(text: "WAV: THIS SCENE", enabled: !exporting, height: 34) { export(midi: false, song: false) }
                    HwButton(text: "WAV: WHOLE SONG", enabled: !exporting, height: 34) { export(midi: false, song: true) }
                }
                HStack(spacing: 5) {
                    HwButton(text: "MIDI: THIS SCENE", enabled: !exporting, height: 34) { export(midi: true, song: false) }
                    HwButton(text: "MIDI: WHOLE SONG", enabled: !exporting, height: 34) { export(midi: true, song: true) }
                }
                if exporting { SilkLabel(text: "Rendering...", color: skin.silk, size: 11) }
                if let url = exported {
                    ShareLink(item: url) {
                        Text("SHARE \(url.lastPathComponent)")
                            .font(.system(size: 12, weight: .bold))
                            .foregroundColor(skin.onAccentInk)
                            .frame(maxWidth: .infinity, minHeight: 38)
                            .background(skin.positive)
                            .cornerRadius(skin.buttonRadius)
                    }
                }
                SilkLabel(text: "The whole song is the chain of the SONG tab, played once.", color: skin.muted, size: 9)
            }
        }
    }

    private func export(midi: Bool, song: Bool) {
        let name = "LoopDraft-\(song ? "song" : "scene\(state.scene + 1)").\(midi ? "mid" : "wav")"
        let url = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent(name)
        exporting = true
        exported = nil
        message = ""
        state.bridge.export(to: url.path, midi: midi, song: song, scene: state.scene) { ok in
            exporting = false
            if ok { exported = url; message = "Ready: \(name)" } else { message = "The export did not work." }
        }
    }
}
