#import <AppKit/AppKit.h>
#include "clipboard_mac.h"

QByteArray macClipboardPdfData()
{
    NSPasteboard *pb = [NSPasteboard generalPasteboard];

    // Try the standard PDF pasteboard type first, then the UTI.
    for (NSPasteboardType t in @[NSPasteboardTypePDF, @"com.adobe.pdf", @"public.pdf"]) {
        NSData *d = [pb dataForType:t];
        if (d && d.length > 0)
            return QByteArray(reinterpret_cast<const char *>(d.bytes),
                              static_cast<qsizetype>(d.length));
    }
    return {};
}
