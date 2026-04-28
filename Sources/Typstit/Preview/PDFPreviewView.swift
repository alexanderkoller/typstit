import SwiftUI

struct PDFPreviewView: NSViewRepresentable {
    let pdfData: Data?

    func makeNSView(context: Context) -> DraggablePDFView {
        DraggablePDFView()
    }

    func updateNSView(_ view: DraggablePDFView, context: Context) {
        view.pdfData = pdfData
    }
}
