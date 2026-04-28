import SwiftUI
import PDFKit

struct PDFPreviewView: NSViewRepresentable {
    let pdfData: Data?

    func makeNSView(context: Context) -> DraggablePDFView {
        let view = DraggablePDFView()
        view.autoScales = true
        view.displayMode = .singlePage
        view.displaysPageBreaks = false
        view.backgroundColor = NSColor.controlBackgroundColor
        return view
    }

    func updateNSView(_ view: DraggablePDFView, context: Context) {
        view.pdfData = pdfData
        if let data = pdfData, let document = PDFDocument(data: data) {
            if view.document == nil || view.document?.dataRepresentation() != data {
                view.document = document
            }
        } else if pdfData == nil {
            view.document = nil
        }
    }
}
