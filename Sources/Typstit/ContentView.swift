import SwiftUI

struct ContentView: View {
    @EnvironmentObject var model: AppModel
    @Environment(\.openWindow) private var openWindow

    var body: some View {
        VStack(spacing: 0) {
            toolbar

            Divider()

            VSplitView {
                CodeEditorView(text: $model.source)
                    .frame(minHeight: 80)

                PDFPreviewView(pdfData: model.pdfData)
                    .frame(minHeight: 80)
            }

            Divider()

            statusBar
        }
        .frame(minWidth: 600, minHeight: 480)
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            // Fixed: never compress below natural size
            Button(action: { Task { await model.typeset() } }) {
                Label("Typeset", systemImage: "play.fill")
            }
            .keyboardShortcut("t", modifiers: .command)
            .disabled(!model.typstAvailable || model.isCompiling)
            .fixedSize()

            Toggle("Auto", isOn: $model.autoCompile)
                .toggleStyle(.checkbox)
                .fixedSize()

            Divider().frame(height: 18)

            Picker("Font", selection: $model.fontName) {
                ForEach(AppModel.availableFonts, id: \.self) { Text($0).tag($0) }
            }
            .labelsHidden()
            .frame(width: 170)

            // Math font: flexible width — shows full name when space permits,
            // truncates to "…" as the window narrows. Capped at 170 so the
            // Spacer always fills the gap before the history button.
            Picker("Math Font", selection: $model.mathFont) {
                ForEach(AppModel.availableMathFonts, id: \.self) { Text($0).tag($0) }
            }
            .labelsHidden()
            .frame(minWidth: 60, maxWidth: 170)

            HStack(spacing: 2) {
                TextField("pt", value: $model.fontSize, format: .number)
                    .frame(width: 44)
                    .textFieldStyle(.roundedBorder)
                    .multilineTextAlignment(.trailing)
                Stepper("", value: $model.fontSize, in: 4...288, step: 1)
                    .labelsHidden()
            }
            .fixedSize()

            ColorPicker("Color", selection: $model.textColor)
                .labelsHidden()
                .frame(width: 32)

            Spacer()

            Button(action: { openWindow(id: "history") }) {
                Image(systemName: "clock")
            }
            .help("Show History (⌘Y)")
            .fixedSize()

            if model.isCompiling {
                ProgressView()
                    .controlSize(.small)
                    .padding(.trailing, 4)
                    .fixedSize()
            }
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 6)
    }

    private var statusBar: some View {
        HStack {
            Text(model.statusMessage.isEmpty ? " " : model.statusMessage)
                .font(.system(size: 11, design: .monospaced))
                .foregroundStyle(model.statusIsError ? Color.red : Color.secondary)
                .lineLimit(3)
                .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding(.horizontal, 10)
        .padding(.vertical, 4)
        .background(.bar)
    }
}
