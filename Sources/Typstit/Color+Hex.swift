import SwiftUI
import AppKit

extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var value: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&value)
        self.init(red: Double((value >> 16) & 0xFF) / 255,
                  green: Double((value >> 8) & 0xFF) / 255,
                  blue: Double(value & 0xFF) / 255)
    }

    var typstHex: String {
        guard let srgb = NSColor(self).usingColorSpace(.sRGB) else { return "#000000" }
        let r = min(255, max(0, Int((srgb.redComponent * 255).rounded())))
        let g = min(255, max(0, Int((srgb.greenComponent * 255).rounded())))
        let b = min(255, max(0, Int((srgb.blueComponent * 255).rounded())))
        return String(format: "#%02x%02x%02x", r, g, b)
    }
}
