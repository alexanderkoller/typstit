import SwiftUI
import PDFKit

struct HistoryView: View {
    @EnvironmentObject var model: AppModel
    @EnvironmentObject var store: HistoryStore
    @Environment(\.dismiss) private var dismiss

    private let columns = [GridItem(.adaptive(minimum: 160, maximum: 220), spacing: 12)]

    var body: some View {
        Group {
            if store.entries.isEmpty {
                VStack(spacing: 8) {
                    Image(systemName: "clock")
                        .font(.system(size: 36))
                        .foregroundStyle(.tertiary)
                    Text("No history yet")
                        .foregroundStyle(.secondary)
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            } else {
                ScrollView {
                    LazyVGrid(columns: columns, spacing: 12) {
                        ForEach(store.entries) { entry in
                            HistoryCell(entry: entry)
                                .onTapGesture(count: 2) { restoreAndCompile(entry) }
                                .onTapGesture(count: 1) { restore(entry) }
                                .contextMenu {
                                    Button("Restore") { restore(entry) }
                                    Button("Open and Compile") { restoreAndCompile(entry) }
                                    Divider()
                                    Button("Delete", role: .destructive) { store.remove(entry) }
                                }
                        }
                    }
                    .padding()
                }
            }
        }
        .frame(minWidth: 520, minHeight: 360)
        .toolbar {
            ToolbarItem(placement: .automatic) {
                Button("Clear All", role: .destructive) {
                    for entry in store.entries { store.remove(entry) }
                }
                .disabled(store.entries.isEmpty)
            }
        }
    }

    private func restore(_ entry: HistoryEntry) {
        model.source = entry.source
        model.fontName = entry.fontName
        model.fontSize = entry.fontSize
        model.textColor = Color(hex: entry.colorHex)
    }

    private func restoreAndCompile(_ entry: HistoryEntry) {
        restore(entry)
        dismiss()
        Task { await model.typeset() }
    }
}

struct HistoryCell: View {
    @EnvironmentObject var store: HistoryStore
    let entry: HistoryEntry
    @State private var thumbnail: NSImage?
    @State private var isHovered = false

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            ZStack {
                Color.white
                if let img = thumbnail {
                    Image(nsImage: img)
                        .resizable()
                        .aspectRatio(contentMode: .fit)
                        .padding(6)
                } else {
                    ProgressView().scaleEffect(0.6)
                }
            }
            .frame(height: 88)
            .clipShape(RoundedRectangle(cornerRadius: 4))

            Text(entry.date.formatted(date: .abbreviated, time: .shortened))
                .font(.caption)
                .foregroundStyle(.primary)
        }
        .padding(8)
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(isHovered
                      ? Color.accentColor.opacity(0.12)
                      : Color(nsColor: .controlBackgroundColor))
        )
        .overlay(
            RoundedRectangle(cornerRadius: 8)
                .strokeBorder(isHovered ? Color.accentColor.opacity(0.4) : Color.clear, lineWidth: 1)
        )
        .onHover { isHovered = $0 }
        .task { thumbnail = await store.thumbnail(for: entry) }
    }
}
