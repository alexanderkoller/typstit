import AppKit
import PDFKit

class DraggablePDFView: NSView, NSDraggingSource {
    var pdfData: Data? {
        didSet {
            pdfDocument = pdfData.flatMap { PDFDocument(data: $0) }
            needsDisplay = true
        }
    }

    private(set) var pdfDocument: PDFDocument?
    private var dragStartLocation: NSPoint?

    override func draw(_ dirtyRect: NSRect) {
        NSColor(white: 0.92, alpha: 1).setFill()
        bounds.fill()

        guard let page = pdfDocument?.page(at: 0),
              let context = NSGraphicsContext.current?.cgContext else { return }

        let pageRect = page.bounds(for: .mediaBox)
        guard pageRect.width > 0, pageRect.height > 0 else { return }

        let padding: CGFloat = 20
        let scale = min((bounds.width - padding * 2) / pageRect.width,
                        (bounds.height - padding * 2) / pageRect.height,
                        1.0)
        let scaledWidth = pageRect.width * scale
        let scaledHeight = pageRect.height * scale
        let offsetX = (bounds.width - scaledWidth) / 2
        let offsetY = (bounds.height - scaledHeight) / 2

        context.saveGState()
        context.translateBy(x: offsetX, y: offsetY)
        context.scaleBy(x: scale, y: scale)
        page.draw(with: .mediaBox, to: context)
        context.restoreGState()
    }

    override func mouseDown(with event: NSEvent) {
        dragStartLocation = convert(event.locationInWindow, from: nil)
    }

    override func mouseUp(with event: NSEvent) {
        dragStartLocation = nil
    }

    override func mouseDragged(with event: NSEvent) {
        guard let start = dragStartLocation, let pdfData = pdfData else { return }

        let current = convert(event.locationInWindow, from: nil)
        guard hypot(current.x - start.x, current.y - start.y) > 4 else { return }

        dragStartLocation = nil

        let dragImage = makeDragImage()
        let imageRect = NSRect(x: current.x - dragImage.size.width / 2,
                               y: current.y - dragImage.size.height / 2,
                               width: dragImage.size.width,
                               height: dragImage.size.height)

        let pasteboardItem = NSPasteboardItem()
        pasteboardItem.setData(pdfData, forType: .pdf)
        let item = NSDraggingItem(pasteboardWriter: pasteboardItem)
        item.setDraggingFrame(imageRect, contents: dragImage)

        beginDraggingSession(with: [item], event: event, source: self)
    }

    func draggingSession(_ session: NSDraggingSession,
                         sourceOperationMaskFor context: NSDraggingContext) -> NSDragOperation {
        .copy
    }

    override func resetCursorRects() {
        addCursorRect(bounds, cursor: .arrow)
    }

    override func cursorUpdate(with event: NSEvent) {
        NSCursor.arrow.set()
    }

    private func makeDragImage() -> NSImage {
        guard let page = pdfDocument?.page(at: 0) else {
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
