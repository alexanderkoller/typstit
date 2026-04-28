import Foundation

enum Preamble {
    static func wrap(_ source: String, font: String, size: Double, colorHex: String) -> String {
        let sizeStr = size.truncatingRemainder(dividingBy: 1) == 0
            ? "\(Int(size))pt"
            : String(format: "%.1fpt", size)
        return """
        #set page(width: auto, height: auto, margin: 0pt)
        #set text(font: "\(font)", size: \(sizeStr), top-edge: "bounds", bottom-edge: "bounds", fill: rgb("\(colorHex)"))

        """ + source
    }
}
