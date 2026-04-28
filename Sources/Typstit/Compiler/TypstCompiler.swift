import Foundation

struct CompileError: Error {
    let stderr: String
}

actor TypstCompiler {
    private var typstPath: String?

    func checkAvailability() async -> Bool {
        // GUI apps launched from Finder get a minimal PATH that excludes Homebrew.
        // Probe known locations before falling back to `which`.
        let candidates = [
            "/opt/homebrew/bin/typst",  // Apple Silicon Homebrew
            "/usr/local/bin/typst",     // Intel Homebrew
            "/usr/bin/typst",
        ]
        for path in candidates {
            if FileManager.default.isExecutableFile(atPath: path) {
                typstPath = path
                return true
            }
        }
        // Fall back to `which` for custom installs (works when launched from terminal).
        let result = await runProcess(executable: "/usr/bin/env", arguments: ["which", "typst"])
        let path = result.stdout.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !path.isEmpty else { return false }
        typstPath = path
        return true
    }

    func compile(source: String, font: String, size: Double, colorHex: String) async throws -> Data {
        guard let typstPath = typstPath else {
            throw CompileError(stderr: "typst not found on $PATH")
        }

        // Use the stable workspace directory as cwd so snippets can reference
        // local files (CSV, images, etc.) with relative paths.
        let workspace  = AppDirectories.workspace
        let inputURL   = workspace.appendingPathComponent("input.typ")
        let outputURL  = workspace.appendingPathComponent("output.pdf")

        let wrapped = Preamble.wrap(source, font: font, size: size, colorHex: colorHex)
        try wrapped.write(to: inputURL, atomically: true, encoding: .utf8)

        let result = await runProcess(
            executable: typstPath,
            arguments:  ["compile", inputURL.path, outputURL.path],
            cwd:        workspace
        )

        guard result.exitCode == 0 else {
            let msg = result.stderr.trimmingCharacters(in: .whitespacesAndNewlines)
            throw CompileError(stderr: msg.isEmpty ? "Compilation failed (exit \(result.exitCode))" : msg)
        }

        return try Data(contentsOf: outputURL)
    }

    // MARK: – Private

    private struct ProcessResult {
        let exitCode: Int32
        let stdout: String
        let stderr: String
    }

    private func runProcess(executable: String, arguments: [String],
                            cwd: URL? = nil) async -> ProcessResult {
        await withCheckedContinuation { continuation in
            let process = Process()
            process.executableURL = URL(fileURLWithPath: executable)
            process.arguments = arguments
            if let cwd = cwd { process.currentDirectoryURL = cwd }

            let outPipe = Pipe()
            let errPipe = Pipe()
            process.standardOutput = outPipe
            process.standardError  = errPipe

            process.terminationHandler = { p in
                let out = String(data: outPipe.fileHandleForReading.readDataToEndOfFile(),
                                 encoding: .utf8) ?? ""
                let err = String(data: errPipe.fileHandleForReading.readDataToEndOfFile(),
                                 encoding: .utf8) ?? ""
                continuation.resume(returning: ProcessResult(exitCode: p.terminationStatus,
                                                             stdout: out,
                                                             stderr: err))
            }

            do {
                try process.run()
            } catch {
                continuation.resume(returning: ProcessResult(exitCode: -1, stdout: "",
                                                             stderr: error.localizedDescription))
            }
        }
    }
}
