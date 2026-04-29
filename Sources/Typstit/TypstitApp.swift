import SwiftUI
import AppKit

class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSWindow.allowsAutomaticWindowTabbing = false
        NSApp.setActivationPolicy(.regular)
        NSApp.activate(ignoringOtherApps: true)
        if let url = Bundle.main.url(forResource: "AppIcon", withExtension: "icns"),
           let icon = NSImage(contentsOf: url) {
            NSApp.applicationIconImage = icon
        }
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
            CommandGroup(replacing: .appInfo) {
                Button("About Typstit") {
                    let credits = NSMutableAttributedString(
                        string: "github.com/alexanderkoller/typstit",
                        attributes: [
                            .link: URL(string: "https://github.com/alexanderkoller/typstit")!,
                            .font: NSFont.systemFont(ofSize: NSFont.smallSystemFontSize)
                        ]
                    )
                    NSApp.orderFrontStandardAboutPanel(options: [
                        .credits: credits,
                        .applicationIcon: NSApp.applicationIconImage as Any,
                        .version: ""
                    ])
                }
            }
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
            CommandGroup(after: .windowList) {
                Divider()
                ShowHistoryButton()
            }
        }

        Window("History", id: "history") {
            HistoryView()
                .environmentObject(model)
                .environmentObject(model.historyStore)
        }
        .defaultSize(width: 640, height: 500)
        .commandsRemoved()
    }
}

private struct ShowHistoryButton: View {
    @Environment(\.openWindow) private var openWindow
    var body: some View {
        Button("History") { openWindow(id: "history") }
            .keyboardShortcut("y", modifiers: .command)
    }
}
