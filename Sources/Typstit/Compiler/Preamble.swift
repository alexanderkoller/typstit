import Foundation

enum Preamble {
    static let sourceKeyword = "TypstitSource="

    static func wrap(_ source: String, font: String, size: Double, colorHex: String) -> String {
        let sizeStr = size.truncatingRemainder(dividingBy: 1) == 0
            ? "\(Int(size))pt"
            : String(format: "%.1fpt", size)
        // Base64-encode the raw snippet and store it in the PDF /Keywords field via
        // Typst's #set document(). This survives a round-trip through apps like Keynote
        // because the keyword is embedded in the PDF info dictionary at compile time,
        // without any post-processing that could drop it.
        let encoded = source.data(using: .utf8)?.base64EncodedString() ?? ""
        // Embed the raw source as transparent placed text so it survives a round-trip
        // through apps like Keynote that strip the PDF info dictionary.
        // #place takes content out of the layout flow, so it doesn't affect page bounds.
        let hidden = "#place(top + left)[#text(fill: rgb(0,0,0,0), size: 0.001pt)[\(sourceKeyword)\(encoded)!]]"
        return """
        #set page(width: auto, height: auto, margin: 0pt, fill: none)
        #set text(font: "\(font)", size: \(sizeStr), top-edge: "bounds", bottom-edge: "bounds", fill: rgb("\(colorHex)"))
        \(hidden)

        """ + source
    }
}
