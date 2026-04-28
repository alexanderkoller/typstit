#!/bin/bash
set -euo pipefail

APP_NAME="Typstit"
BUNDLE_ID="com.typstit.app"
VERSION="1.0"
APP_DIR="${APP_NAME}.app"

echo "==> Building release binary..."
swift build -c release

BINARY=".build/release/${APP_NAME}"
if [ ! -f "$BINARY" ]; then
    echo "Error: binary not found at $BINARY" >&2
    exit 1
fi

echo "==> Creating app bundle..."
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Resources"

cp "$BINARY" "$APP_DIR/Contents/MacOS/$APP_NAME"

cat > "$APP_DIR/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>${APP_NAME}</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>13.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

echo "==> Signing with ad-hoc signature..."
codesign --force --deep --sign - "$APP_DIR"

echo ""
echo "Done: $(pwd)/${APP_DIR}"
echo ""
echo "To run:            open ${APP_DIR}"
echo "To install system-wide: cp -r ${APP_DIR} /Applications/"
echo ""
echo "Note: first launch may require right-click → Open to bypass Gatekeeper."
