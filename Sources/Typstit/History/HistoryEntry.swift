import Foundation

struct HistoryEntry: Identifiable, Codable {
    let id: UUID
    let date: Date
    let source: String
    let fontName: String
    let fontSize: Double
    let colorHex: String
    let mathFont: String

    init(id: UUID, date: Date, source: String, fontName: String,
         fontSize: Double, colorHex: String, mathFont: String) {
        self.id = id; self.date = date; self.source = source
        self.fontName = fontName; self.fontSize = fontSize
        self.colorHex = colorHex; self.mathFont = mathFont
    }

    // Backward-compatible decoder: old entries on disk lack `mathFont`.
    init(from decoder: Decoder) throws {
        let c = try decoder.container(keyedBy: CodingKeys.self)
        id       = try c.decode(UUID.self,   forKey: .id)
        date     = try c.decode(Date.self,   forKey: .date)
        source   = try c.decode(String.self, forKey: .source)
        fontName = try c.decode(String.self, forKey: .fontName)
        fontSize = try c.decode(Double.self, forKey: .fontSize)
        colorHex = try c.decode(String.self, forKey: .colorHex)
        mathFont = try c.decodeIfPresent(String.self, forKey: .mathFont)
                   ?? "New Computer Modern Math"
    }
}
