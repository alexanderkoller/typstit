#!/usr/bin/swift
import AppKit
import Foundation

func createIcon(pixels: Int) -> Data? {
    let size = CGFloat(pixels)

    guard let rep = NSBitmapImageRep(
        bitmapDataPlanes: nil,
        pixelsWide: pixels, pixelsHigh: pixels,
        bitsPerSample: 8, samplesPerPixel: 4,
        hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0
    ) else { return nil }

    NSGraphicsContext.saveGraphicsState()
    defer { NSGraphicsContext.restoreGraphicsState() }
    guard let nsctx = NSGraphicsContext(bitmapImageRep: rep) else { return nil }
    NSGraphicsContext.current = nsctx
    let ctx = nsctx.cgContext

    // ── Background ─────────────────────────────────────────────────────
    let fullRect = CGRect(origin: .zero, size: CGSize(width: size, height: size))
    ctx.addPath(CGPath(roundedRect: fullRect,
                       cornerWidth: size * 0.225, cornerHeight: size * 0.225,
                       transform: nil))
    ctx.clip()

    let cs = CGColorSpaceCreateDeviceRGB()
    let bg = CGGradient(
        colorsSpace: cs,
        colors: [CGColor(red: 0.13, green: 0.67, blue: 0.68, alpha: 1.0),
                 CGColor(red: 0.04, green: 0.28, blue: 0.33, alpha: 1.0)] as CFArray,
        locations: [0, 1])!
    ctx.drawLinearGradient(bg,
                           start: CGPoint(x: 0, y: size),
                           end:   CGPoint(x: size, y: 0),
                           options: [])

    // ── "T" setup ──────────────────────────────────────────────────────
    let fontSize = size * 0.72
    let font = NSFont(name: "TimesNewRomanPS-BoldItalicMT", size: fontSize)
            ?? NSFont(name: "Georgia-BoldItalic",             size: fontSize)
            ?? NSFont.boldSystemFont(ofSize: fontSize)

    // The sharp T sits slightly right of centre — feels like it's moving right
    let tAttrs: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: NSColor.white]
    let tStr  = NSAttributedString(string: "T", attributes: tAttrs)
    let tSize = tStr.size()

    let sharpX = (size - tSize.width)  / 2 + size * 0.06
    let sharpY = (size - tSize.height) / 2 + size * 0.02

    // ── Motion-blur trail (ghosts behind the T, trailing left) ─────────
    // The T is "moving" to the upper-right, so the trail fans to the lower-left.
    let steps   = 16
    let trailDX = -size * 0.26   // total leftward reach of the trail
    let trailDY = -size * 0.06   // slight downward reach

    for i in 0 ..< steps {
        let t     = CGFloat(i + 1) / CGFloat(steps)   // 0 = near T, 1 = far end
        let alpha = (1 - t) * 0.48                    // fade to transparent
        let dx    = trailDX * t
        let dy    = trailDY * t

        let ghostAttrs: [NSAttributedString.Key: Any] = [
            .font:            font,
            .foregroundColor: NSColor.white.withAlphaComponent(alpha),
        ]
        NSAttributedString(string: "T", attributes: ghostAttrs)
            .draw(at: NSPoint(x: sharpX + dx, y: sharpY + dy))
    }

    // ── Sharp "T" on top with a soft glow so it pops ──────────────────
    // Glow: draw once at low opacity & larger size
    let glowSize  = fontSize * 1.04
    let glowFont  = NSFont(name: "TimesNewRomanPS-BoldItalicMT", size: glowSize)
                 ?? NSFont(name: "Georgia-BoldItalic",             size: glowSize)
                 ?? NSFont.boldSystemFont(ofSize: glowSize)
    let glowAttrs: [NSAttributedString.Key: Any] = [
        .font:            glowFont,
        .foregroundColor: NSColor.white.withAlphaComponent(0.18),
    ]
    let glowStr  = NSAttributedString(string: "T", attributes: glowAttrs)
    let glowSize2 = glowStr.size()
    glowStr.draw(at: NSPoint(
        x: sharpX - (glowSize2.width  - tSize.width)  / 2,
        y: sharpY - (glowSize2.height - tSize.height) / 2
    ))

    tStr.draw(at: NSPoint(x: sharpX, y: sharpY))

    return rep.representation(using: .png, properties: [:])
}

let entries: [(pixels: Int, filename: String)] = [
    (16,   "icon_16x16.png"),
    (32,   "icon_16x16@2x.png"),
    (32,   "icon_32x32.png"),
    (64,   "icon_32x32@2x.png"),
    (128,  "icon_128x128.png"),
    (256,  "icon_128x128@2x.png"),
    (256,  "icon_256x256.png"),
    (512,  "icon_256x256@2x.png"),
    (512,  "icon_512x512.png"),
    (1024, "icon_512x512@2x.png"),
]

let iconsetDir = "Typstit.iconset"
try! FileManager.default.createDirectory(atPath: iconsetDir, withIntermediateDirectories: true)
for (pixels, filename) in entries {
    guard let data = createIcon(pixels: pixels) else { print("FAILED \(filename)"); continue }
    try! data.write(to: URL(fileURLWithPath: "\(iconsetDir)/\(filename)"))
    print("  \(iconsetDir)/\(filename)")
}
print("Done.")
