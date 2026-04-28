import SwiftUI
import AppKit

extension Color {
    var typstHex: String {
        guard let srgb = NSColor(self).usingColorSpace(.sRGB) else { return "#000000" }
        let r = min(255, max(0, Int((srgb.redComponent * 255).rounded())))
        let g = min(255, max(0, Int((srgb.greenComponent * 255).rounded())))
        let b = min(255, max(0, Int((srgb.blueComponent * 255).rounded())))
        return String(format: "#%02x%02x%02x", r, g, b)
    }
}
