// swift-tools-version: 6.0
import PackageDescription

let package = Package(
    name: "Typstit",
    platforms: [.macOS(.v13)],
    targets: [
        .executableTarget(
            name: "Typstit",
            path: "Sources/Typstit",
            swiftSettings: [
                .swiftLanguageMode(.v5)
            ]
        )
    ]
)
