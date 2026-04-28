import SwiftUI
import AppKit

class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
    }
}

@main
struct TypstitApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(model)
                .environmentObject(model.historyStore)
        }
        .commands {
            CommandGroup(replacing: .newItem) { }
            CommandGroup(after: .pasteboard) {
                Button("Paste Typst Source") {
                    model.pasteTypstSource()
                }
                .keyboardShortcut("v", modifiers: [.command, .shift])
            }
            CommandMenu("Typeset") {
                Button("Typeset") {
                    Task { await model.typeset() }
                }
                .keyboardShortcut("t", modifiers: .command)
                .disabled(!model.typstAvailable || model.isCompiling)

                Toggle("Auto", isOn: $model.autoCompile)
                    .keyboardShortcut("t", modifiers: [.command, .shift])
            }
            CommandMenu("History") {
                ShowHistoryButton()
            }
        }

        Window("History", id: "history") {
            HistoryView()
                .environmentObject(model)
                .environmentObject(model.historyStore)
        }
        .defaultSize(width: 640, height: 500)
    }
}

private struct ShowHistoryButton: View {
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        Button("Show History") { openWindow(id: "history") }
            .keyboardShortcut("y", modifiers: .command)
    }
}
