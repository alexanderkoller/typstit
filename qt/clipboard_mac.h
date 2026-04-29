#pragma once
#include <QByteArray>

// Returns raw PDF bytes from the macOS pasteboard, or empty if none present.
QByteArray macClipboardPdfData();
