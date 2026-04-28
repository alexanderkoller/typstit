import Foundation

struct HistoryEntry: Identifiable, Codable {
    let id: UUID
    let date: Date
    let source: String
    let fontName: String
    let fontSize: Double
    let colorHex: String
}
