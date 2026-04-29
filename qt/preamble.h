#pragma once
#include <QString>
#include <QColor>

namespace Preamble {

inline constexpr const char *sourceKeyword = "TypstitSource=";

inline QString wrap(const QString &source, const QString &font, int size, const QColor &color)
{
    QString colorHex = QString("%1%2%3")
        .arg(color.red(),   2, 16, QChar('0'))
        .arg(color.green(), 2, 16, QChar('0'))
        .arg(color.blue(),  2, 16, QChar('0'));

    // Embed raw source as invisible placed text so it survives apps that strip PDF metadata.
    QString encoded = source.toUtf8().toBase64();
    // Zero-width box: invisible to humans, in-flow so getSelectionAtIndex finds it.
    QString hidden  = QString(
        "#box(width: 0pt)[#text(fill: rgb(0,0,0,0), size: 0.5pt)[TypstitSource=%1!]]"
    ).arg(encoded);

    return QString(
        "#set page(width: auto, height: auto, margin: 0pt, fill: none)\n"
        "#set text(font: \"%1\", size: %2pt, top-edge: \"bounds\", bottom-edge: \"bounds\", fill: rgb(\"%3\"))\n"
        "%4\n\n"
    ).arg(font).arg(size).arg(colorHex).arg(hidden)
     + source;
}

} // namespace Preamble
