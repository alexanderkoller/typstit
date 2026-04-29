#include "highlighter.h"
#include <QFont>

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *doc)
    : QSyntaxHighlighter(doc)
{
    auto rule = [](const QString &pattern,
                   QRegularExpression::PatternOptions opts,
                   const QColor &color, bool bold = false) -> Rule
    {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        if (bold) fmt.setFontWeight(QFont::Bold);
        return { QRegularExpression(pattern, opts), fmt };
    };

    // Rules applied in order; later rules win on overlaps (comments highest priority).
    m_rules = {
        // Headings: = Title  (blue, bold)
        rule(R"(^=+[ \t][^\n]*)",
             QRegularExpression::MultilineOption, QColor("#007aff"), true),
        // Math: $...$  (green)
        rule(R"(\$[^$\n]+\$)",
             {}, QColor("#28a745")),
        // Functions/keywords: #ident  (purple)
        rule(R"(#[a-zA-Z][a-zA-Z0-9._-]*)",
             {}, QColor("#7c3aed")),
        // Strings: "..."  (orange)
        rule(R"("[^"\n]*")",
             {}, QColor("#c05c00")),
        // Comments: // ...  (gray)
        rule(R"(//[^\n]*)",
             {}, QColor("#8e8e93")),
    };
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &r : m_rules) {
        auto it = r.rx.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            setFormat(m.capturedStart(), m.capturedLength(), r.fmt);
        }
    }
}
