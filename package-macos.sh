#!/bin/bash
set -e

APP_NAME="Revel"
APP_BUNDLE="$APP_NAME.app"
CONTENTS="$APP_BUNDLE/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"

echo "Building revel..."
make

echo "Creating app bundle structure..."
mkdir -p "$MACOS" "$RESOURCES"

echo "Converting PNG to icns..."
# Create iconset directory
ICONSET="revel.iconset"
mkdir -p "$ICONSET"

# Generate different icon sizes from revel.png
sips -z 16 16     revel.png --out "$ICONSET/icon_16x16.png"
sips -z 32 32     revel.png --out "$ICONSET/icon_16x16@2x.png"
sips -z 32 32     revel.png --out "$ICONSET/icon_32x32.png"
sips -z 64 64     revel.png --out "$ICONSET/icon_32x32@2x.png"
sips -z 128 128   revel.png --out "$ICONSET/icon_128x128.png"
sips -z 256 256   revel.png --out "$ICONSET/icon_128x128@2x.png"
sips -z 256 256   revel.png --out "$ICONSET/icon_256x256.png"
sips -z 512 512   revel.png --out "$ICONSET/icon_256x256@2x.png"
sips -z 512 512   revel.png --out "$ICONSET/icon_512x512.png"
sips -z 1024 1024 revel.png --out "$ICONSET/icon_512x512@2x.png"

# Convert iconset to icns
iconutil -c icns "$ICONSET" -o "$RESOURCES/revel.icns"
rm -rf "$ICONSET"

echo "Copying executable..."
cp revel "$MACOS/"

echo "App bundle created: $APP_BUNDLE"
echo "You can now run: open $APP_BUNDLE"
