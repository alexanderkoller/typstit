import SwiftUI
import AppKit

class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        // When launched via `swift run` the process starts as an accessory;
        // promoting to .regular gives it a Dock icon, a menu bar, and keyboard focus.
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
        }
        .commands {
            CommandGroup(replacing: .newItem) { }
            CommandMenu("Typeset") {
                Button("Typeset") {
                    Task { await model.typeset() }
                }
                .keyboardShortcut("t", modifiers: .command)
                .disabled(!model.typstAvailable || model.isCompiling)
            }
        }
    }
}
