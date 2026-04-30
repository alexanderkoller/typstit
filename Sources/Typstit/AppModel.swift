import SwiftUI
import PDFKit
import CoreText

@MainActor
class AppModel: ObservableObject {
    @Published var source: String = "" { didSet { scheduleAutoCompile() } }
    @Published var pdfData: Data? = nil
    @Published var statusMessage: String = ""
    @Published var statusIsError: Bool = false
    @Published var isCompiling: Bool = false
    @Published var typstAvailable: Bool = false
    @Published var autoCompile: Bool = true { didSet { if autoCompile { scheduleAutoCompile() } } }

    @Published var fontName: String = "Libertinus Serif"          { didSet { scheduleAutoCompile() } }
    @Published var fontSize: Double = 36                           { didSet { scheduleAutoCompile() } }
    @Published var textColor: Color = .black                       { didSet { scheduleAutoCompile() } }
    @Published var mathFont:  String = "New Computer Modern Math"  { didSet { scheduleAutoCompile() } }

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

    // Fonts that have an OpenType MATH table and are therefore suitable for
    // typesetting math equations. Typst-bundled math fonts are always listed first
    // even if not installed system-wide; system fonts with a MATH table follow.
    static let availableMathFonts: [String] = {
        let bundled = ["New Computer Modern Math", "Libertinus Math"]
        let mathTag: CTFontTableTag = 0x4D415448  // 'MATH'
        let system = NSFontManager.shared.availableFontFamilies.filter { family in
            let desc = NSFontDescriptor(fontAttributes: [.family: family])
            guard let font = NSFont(descriptor: desc, size: 12) else { return false }
            return CTFontCopyTable(font as CTFont, mathTag, []) != nil
        }.sorted()
        let systemSet = Set(system)
        return bundled.filter { !systemSet.contains($0) } + system
    }()

    let historyStore = HistoryStore()
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
                colorHex: colorHex,
                mathFont: mathFont
            )
            pdfData = data
            historyStore.add(source: source, pdfData: data,
                             fontName: fontName, fontSize: fontSize,
                             colorHex: colorHex, mathFont: mathFont)
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

    func pasteTypstSource() {
        let pb = NSPasteboard.general
        guard let data = pb.data(forType: .pdf) ?? pb.data(forType: NSPasteboard.PasteboardType("com.adobe.pdf")),
              let document = PDFDocument(data: data),
              let pageText = document.page(at: 0)?.string else {
            statusMessage = "No PDF found in clipboard"
            statusIsError = true
            return
        }

        let prefix = Preamble.sourceKeyword
        guard let markerRange = pageText.range(of: prefix) else {
            statusMessage = "No Typstit source found in clipboard PDF"
            statusIsError = true
            return
        }
        // Collect base64 chars up to the '!' terminator; strip any spaces/newlines Keynote may insert.
        let tail = pageText[markerRange.upperBound...]
        let base64Chars = CharacterSet(charactersIn: "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=")
            .union(.whitespacesAndNewlines)
        let rawB64 = String(tail.unicodeScalars.prefix(while: { base64Chars.contains($0) }))
        let base64 = rawB64.filter { !$0.isWhitespace }

        guard let sourceData = Data(base64Encoded: base64),
              let recovered = String(data: sourceData, encoding: .utf8) else {
            statusMessage = "Decode failed. raw=«\(String(tail.prefix(80)))»"
            statusIsError = true
            return
        }
        source = recovered
        statusMessage = "Source restored from PDF"
        statusIsError = false
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
