import AppKit
import PDFKit
import UniformTypeIdentifiers

// Separate delegate class avoids @MainActor isolation conflicts with background-queue callback.
final class PDFFilePromiseDelegate: NSObject, NSFilePromiseProviderDelegate {
    var pdfData: Data?

    func filePromiseProvider(_ filePromiseProvider: NSFilePromiseProvider,
                             fileNameForType fileType: String) -> String {
        "typstit-output.pdf"
    }

    func filePromiseProvider(_ filePromiseProvider: NSFilePromiseProvider,
                             writePromiseTo url: URL,
                             completionHandler: @escaping (Error?) -> Void) {
        guard let data = pdfData else {
            completionHandler(NSError(domain: "Typstit", code: 1,
                                     userInfo: [NSLocalizedDescriptionKey: "No PDF data available"]))
            return
        }
        do {
            try data.write(to: url)
            completionHandler(nil)
        } catch {
            completionHandler(error)
        }
    }
}

class DraggablePDFView: PDFView, NSDraggingSource {
    var pdfData: Data?
    private var dragStartLocation: NSPoint?
    private let filePromiseDelegate = PDFFilePromiseDelegate()

    override func mouseDown(with event: NSEvent) {
        // Don't call super — that would arm PDFView's text-selection gesture,
        // which would win the mouseDragged race every time.
        dragStartLocation = convert(event.locationInWindow, from: nil)
    }

    override func mouseUp(with event: NSEvent) {
        dragStartLocation = nil
        super.mouseUp(with: event)
    }

    override func mouseDragged(with event: NSEvent) {
        guard let start = dragStartLocation, let pdfData = pdfData else { return }

        let current = convert(event.locationInWindow, from: nil)
        let distance = hypot(current.x - start.x, current.y - start.y)

        guard distance > 4 else { return }

        dragStartLocation = nil
        filePromiseDelegate.pdfData = pdfData

        let dragImage = makeDragImage()
        let imageRect = NSRect(
            x: current.x - dragImage.size.width / 2,
            y: current.y - dragImage.size.height / 2,
            width: dragImage.size.width,
            height: dragImage.size.height
        )

        // Item 1: raw PDF data — for inline embedding (Pages, Keynote, Word)
        let pasteboardItem = NSPasteboardItem()
        pasteboardItem.setData(pdfData, forType: .pdf)
        let item1 = NSDraggingItem(pasteboardWriter: pasteboardItem)
        item1.setDraggingFrame(imageRect, contents: dragImage)

        // Item 2: file promise — for drops onto Finder / Mail
        let filePromise = NSFilePromiseProvider(fileType: UTType.pdf.identifier, delegate: filePromiseDelegate)
        let item2 = NSDraggingItem(pasteboardWriter: filePromise)
        item2.setDraggingFrame(imageRect, contents: dragImage)

        beginDraggingSession(with: [item1, item2], event: event, source: self)
    }

    func draggingSession(_ session: NSDraggingSession,
                         sourceOperationMaskFor context: NSDraggingContext) -> NSDragOperation {
        return .copy
    }

    override func resetCursorRects() {
        // Don't call super — PDFView would install a text-beam cursor over the content.
        addCursorRect(bounds, cursor: .arrow)
    }

    override func cursorUpdate(with event: NSEvent) {
        NSCursor.arrow.set()
    }

    private func makeDragImage() -> NSImage {
        guard let page = document?.page(at: 0) else {
            return NSImage(systemSymbolName: "doc.richtext.fill", accessibilityDescription: nil) ?? NSImage()
        }
        let pageRect = page.bounds(for: .mediaBox)
        let maxDim: CGFloat = 160
        let scale = min(maxDim / pageRect.width, maxDim / pageRect.height)
        return page.thumbnail(of: CGSize(width: pageRect.width * scale,
                                         height: pageRect.height * scale),
                              for: .mediaBox)
    }
}
