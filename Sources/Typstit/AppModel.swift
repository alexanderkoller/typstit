import SwiftUI

@MainActor
class AppModel: ObservableObject {
    @Published var source: String = ""
    @Published var pdfData: Data? = nil
    @Published var statusMessage: String = ""
    @Published var statusIsError: Bool = false
    @Published var isCompiling: Bool = false
    @Published var typstAvailable: Bool = false
    @Published var autoCompile: Bool = false

    @Published var fontName: String = "Libertinus Serif"
    @Published var fontSize: Double = 36
    @Published var textColor: Color = .black

    // Fonts bundled inside the typst binary — available for compilation even if not
    // installed as system fonts (so NSFontManager won't list them).
    private static let typstBundledFonts = [
        "Libertinus Serif", "Libertinus Sans", "Libertinus Mono",
        "New Computer Modern", "New Computer Modern Math",
        "DejaVu Sans Mono",
    ]

    static let availableFonts: [String] = {
        let system = NSFontManager.shared.availableFontFamilies.sorted()
        let systemSet = Set(system)
        let bundledOnly = typstBundledFonts.filter { !systemSet.contains($0) }
        return bundledOnly + system
    }()

    private let compiler = TypstCompiler()
    private var autoCompileTask: Task<Void, Never>? = nil

    init() {
        Task {
            self.typstAvailable = await compiler.checkAvailability()
            if !self.typstAvailable {
                self.statusMessage = "typst not found — install with: brew install typst"
                self.statusIsError = true
            }
        }
    }

    func typeset() async {
        guard typstAvailable, !isCompiling else { return }
        isCompiling = true
        let start = Date()
        let colorHex = textColor.typstHex
        do {
            let data = try await compiler.compile(
                source: source,
                font: fontName,
                size: fontSize,
                colorHex: colorHex
            )
            pdfData = data
            let elapsed = Date().timeIntervalSince(start)
            statusMessage = String(format: "Compiled in %.2fs", elapsed)
            statusIsError = false
        } catch let error as CompileError {
            statusMessage = error.stderr
            statusIsError = true
        } catch {
            statusMessage = error.localizedDescription
            statusIsError = true
        }
        isCompiling = false
    }

    func scheduleAutoCompile() {
        guard autoCompile else { return }
        autoCompileTask?.cancel()
        autoCompileTask = Task { @MainActor in
            try? await Task.sleep(for: .milliseconds(500))
            guard !Task.isCancelled else { return }
            await typeset()
        }
    }
}
