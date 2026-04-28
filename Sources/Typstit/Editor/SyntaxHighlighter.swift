import AppKit

enum SyntaxHighlighter {
    private static let monoFont = NSFont.monospacedSystemFont(ofSize: 13, weight: .regular)
    private static let boldFont = NSFont.monospacedSystemFont(ofSize: 13, weight: .bold)

    // Rules applied in order — later rules win on overlaps (comments highest priority).
    private static let rules: [(regex: NSRegularExpression, color: NSColor, bold: Bool)] = {
        let specs: [(String, NSRegularExpression.Options, NSColor, Bool)] = [
            (#"^=+[ \t][^\n]*"#,            .anchorsMatchLines, .systemBlue,   true),
            (#"\$[^$]+\$"#,                  [],                 .systemGreen,  false),
            (#"#[a-zA-Z][a-zA-Z0-9._-]*"#,  [],                 .systemPurple, false),
            (#"\"[^\"\n]*\""#,               [],                 .systemOrange, false),
            (#"//[^\n]*"#,                   [],                 .systemGray,   false),
        ]
        return specs.compactMap { pattern, options, color, bold in
            (try? NSRegularExpression(pattern: pattern, options: options))
                .map { ($0, color, bold) }
        }
    }()

    static func highlight(_ storage: NSTextStorage) {
        let str = storage.string
        let full = NSRange(location: 0, length: (str as NSString).length)

        storage.setAttributes([.foregroundColor: NSColor.labelColor, .font: monoFont], range: full)

        for (regex, color, bold) in rules {
            regex.enumerateMatches(in: str, range: full) { match, _, _ in
                guard let r = match?.range else { return }
                storage.addAttribute(.foregroundColor, value: color, range: r)
                if bold {
                    storage.addAttribute(.font, value: boldFont, range: r)
                }
            }
        }
    }
}
