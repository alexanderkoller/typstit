import AppKit
import PDFKit

@MainActor
class HistoryStore: ObservableObject {

    @Published private(set) var entries: [HistoryEntry] = []

    private var historyDir: URL { AppDirectories.appSupport.appendingPathComponent("history") }

    init() {
        try? FileManager.default.createDirectory(at: historyDir, withIntermediateDirectories: true)
        load()
    }

    func add(source: String, pdfData: Data, fontName: String, fontSize: Double,
             colorHex: String, mathFont: String) {
        let trimmed = source.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !entries.contains(where: { $0.source.trimmingCharacters(in: .whitespacesAndNewlines) == trimmed }) else { return }

        let entry = HistoryEntry(id: UUID(), date: Date(), source: source,
                                 fontName: fontName, fontSize: fontSize,
                                 colorHex: colorHex, mathFont: mathFont)
        let dir = entryDir(for: entry.id)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        try? source.write(to: dir.appendingPathComponent("source.typ"), atomically: true, encoding: .utf8)
        try? pdfData.write(to: dir.appendingPathComponent("output.pdf"))
        writeMeta(entry, to: dir)
        entries.insert(entry, at: 0)
    }

    func remove(_ entry: HistoryEntry) {
        try? FileManager.default.removeItem(at: entryDir(for: entry.id))
        entries.removeAll { $0.id == entry.id }
    }

    func thumbnail(for entry: HistoryEntry) async -> NSImage? {
        let url = entryDir(for: entry.id).appendingPathComponent("output.pdf")
        // Return Data instead of NSImage across the task boundary — Data is Sendable on all targets.
        guard let tiff = await Task.detached(priority: .utility, operation: { () -> Data? in
            guard let data = try? Data(contentsOf: url),
                  let doc = PDFDocument(data: data),
                  let page = doc.page(at: 0) else { return nil }
            let rect = page.bounds(for: .mediaBox)
            guard rect.width > 0, rect.height > 0 else { return nil }
            let maxDim: CGFloat = 240
            let scale = min(maxDim / rect.width, maxDim / rect.height)
            return page.thumbnail(of: CGSize(width: rect.width * scale, height: rect.height * scale),
                                  for: .mediaBox).tiffRepresentation
        }).value else { return nil }
        return NSImage(data: tiff)
    }

    private func entryDir(for id: UUID) -> URL {
        historyDir.appendingPathComponent(id.uuidString)
    }

    private func writeMeta(_ entry: HistoryEntry, to dir: URL) {
        let encoder = JSONEncoder()
        encoder.dateEncodingStrategy = .iso8601
        if let data = try? encoder.encode(entry) {
            try? data.write(to: dir.appendingPathComponent("meta.json"))
        }
    }

    private func load() {
        guard let dirs = try? FileManager.default.contentsOfDirectory(
            at: historyDir, includingPropertiesForKeys: nil
        ) else { return }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        entries = dirs.compactMap { dir in
            guard let data = try? Data(contentsOf: dir.appendingPathComponent("meta.json")) else { return nil }
            return try? decoder.decode(HistoryEntry.self, from: data)
        }.sorted { $0.date > $1.date }
    }
}
