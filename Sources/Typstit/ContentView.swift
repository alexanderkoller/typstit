import SwiftUI

struct ContentView: View {
    @EnvironmentObject var model: AppModel

    var body: some View {
        VStack(spacing: 0) {
            toolbar

            Divider()

            VSplitView {
                CodeEditorView(text: $model.source, onChange: { model.scheduleAutoCompile() })
                    .frame(minHeight: 80)

                PDFPreviewView(pdfData: model.pdfData)
                    .frame(minHeight: 80)
                    .background(Color(nsColor: .controlBackgroundColor))
            }

            Divider()

            statusBar
        }
        .frame(minWidth: 600, minHeight: 480)
    }

    private var toolbar: some View {
        HStack(spacing: 8) {
            Button(action: { Task { await model.typeset() } }) {
                Label("Typeset", systemImage: "play.fill")
            }
            .keyboardShortcut("t", modifiers: .command)
            .disabled(!model.typstAvailable || model.isCompiling)

            Toggle("Auto", isOn: $model.autoCompile)
                .toggleStyle(.checkbox)

            Divider().frame(height: 18)

            Picker("Font", selection: $model.fontName) {
                ForEach(AppModel.availableFonts, id: \.self) { Text($0).tag($0) }
            }
            .labelsHidden()
            .frame(width: 170)
            .onChange(of: model.fontName) { _ in model.scheduleAutoCompile() }

            HStack(spacing: 2) {
                TextField("pt", value: $model.fontSize, format: .number)
                    .frame(width: 44)
                    .textFieldStyle(.roundedBorder)
                    .multilineTextAlignment(.trailing)
                    .onChange(of: model.fontSize) { _ in model.scheduleAutoCompile() }
                Stepper("", value: $model.fontSize, in: 4...288, step: 1)
                    .labelsHidden()
            }

            ColorPicker("Color", selection: $model.textColor)
                .labelsHidden()
                .frame(width: 32)
                .onChange(of: model.textColor) { _ in model.scheduleAutoCompile() }

            Spacer()

            if model.isCompiling {
                ProgressView()
                    .controlSize(.small)
                    .padding(.trailing, 4)
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
