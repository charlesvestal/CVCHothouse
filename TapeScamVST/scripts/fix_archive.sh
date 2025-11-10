#!/usr/bin/env bash
# Run this AFTER Xcode archive completes to fix the embedded appex and create proper archive
set -e

echo "🔧 Fixing iOS archive..."

# Find the most recent DerivedData for this project
DERIVED_DATA=$(find /Users/charlesvestal/Library/Developer/Xcode/DerivedData -name "TapeScamVST-*" -type d -maxdepth 1 2>/dev/null | head -1)

if [ -z "$DERIVED_DATA" ]; then
    echo "❌ Error: Could not find DerivedData for TapeScamVST"
    echo "Make sure you've run Product > Archive in Xcode first"
    exit 1
fi

echo "📁 Found DerivedData: $DERIVED_DATA"

# Determine which scheme was archived (Standalone or AUv3)
if [ -d "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_Standalone" ]; then
    SCHEME="TapeScamVST_Standalone"
elif [ -d "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_AUv3" ]; then
    SCHEME="TapeScamVST_AUv3"
else
    echo "❌ Error: No archive intermediates found"
    echo "Make sure you've run Product > Archive in Xcode first"
    exit 1
fi

echo "🎯 Archive scheme: $SCHEME"

# Set up symlinks for future builds
echo "🔗 Setting up symlinks for library linking..."
mkdir -p /Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST_artefacts/Release/AUv3
mkdir -p /Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST_artefacts/Release/Standalone

# Create symlinks to libraries (these will be used in next archive)
if [ -d "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_AUv3/IntermediateBuildFilesPath/UninstalledProducts/iphoneos" ]; then
    ln -sf "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_AUv3/IntermediateBuildFilesPath/UninstalledProducts/iphoneos/libTapeScam_SharedCode.a" \
           /Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST_artefacts/Release/AUv3/libTapeScam_SharedCode.a 2>/dev/null || true

    ln -sf "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_AUv3/IntermediateBuildFilesPath/UninstalledProducts/iphoneos/libTapeScamGUIAssets.a" \
           /Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST_artefacts/Release/AUv3/libTapeScamGUIAssets.a 2>/dev/null || true
fi

if [ -d "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_Standalone/IntermediateBuildFilesPath/UninstalledProducts/iphoneos" ]; then
    ln -sf "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_Standalone/IntermediateBuildFilesPath/UninstalledProducts/iphoneos/libTapeScam_SharedCode.a" \
           /Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST_artefacts/Release/Standalone/libTapeScam_SharedCode.a 2>/dev/null || true

    ln -sf "$DERIVED_DATA/ArchiveIntermediates/TapeScamVST_Standalone/IntermediateBuildFilesPath/UninstalledProducts/iphoneos/libTapeScamGUIAssets.a" \
           /Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST_artefacts/Release/Standalone/libTapeScamGUIAssets.a 2>/dev/null || true
fi

# Fix the embedded AUv3 plugin
APP_PATH="$DERIVED_DATA/ArchiveIntermediates/$SCHEME/InstallationBuildProductsLocation/Applications/TapeScam.app"
APPEX_SOURCE="$DERIVED_DATA/ArchiveIntermediates/$SCHEME/IntermediateBuildFilesPath/UninstalledProducts/iphoneos/TapeScam.appex"

if [ ! -d "$APPEX_SOURCE" ]; then
    echo "❌ Error: Could not find built TapeScam.appex at:"
    echo "   $APPEX_SOURCE"
    exit 1
fi

if [ ! -d "$APP_PATH" ]; then
    echo "❌ Error: Could not find TapeScam.app at:"
    echo "   $APP_PATH"
    exit 1
fi

echo "🔌 Fixing embedded AUv3 plugin..."
rm -rf "$APP_PATH/PlugIns/TapeScam.appex"
cp -R "$APPEX_SOURCE" "$APP_PATH/PlugIns/"

# Verify the fix worked
if [ ! -d "$APP_PATH/PlugIns/TapeScam.appex/TapeScam" ]; then
    echo "⚠️  Warning: TapeScam.appex may not have copied correctly"
fi

# Create the final archive
echo "📦 Creating archive..."
ARCHIVE_PATH=~/Desktop/TapeScam_$(date +%Y%m%d_%H%M%S).xcarchive

mkdir -p "$ARCHIVE_PATH/Products/Applications"
cp -R "$APP_PATH" "$ARCHIVE_PATH/Products/Applications/"

# Create Info.plist for the archive
cat > "$ARCHIVE_PATH/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>ApplicationProperties</key>
    <dict>
        <key>ApplicationPath</key>
        <string>Applications/TapeScam.app</string>
        <key>CFBundleIdentifier</key>
        <string>com.vestal.TapeScam</string>
        <key>CFBundleShortVersionString</key>
        <string>1.0.0</string>
    </dict>
    <key>ArchiveVersion</key>
    <integer>2</integer>
    <key>CreationDate</key>
    <date>$(date -u +"%Y-%m-%dT%H:%M:%SZ")</date>
    <key>Name</key>
    <string>TapeScam</string>
    <key>SchemeName</key>
    <string>$SCHEME</string>
</dict>
</plist>
EOF

echo ""
echo "✅ Archive created successfully!"
echo "📦 Location: $ARCHIVE_PATH"
echo ""
echo "📋 The archive contains:"
echo "   • TapeScam.app (Standalone container app)"
echo "   • TapeScam.appex (embedded AUv3 plugin)"
echo ""
echo "🎉 Next steps:"
echo "   1. Open in Xcode Organizer:"
echo "      Window → Organizer → Archives → + (import)"
echo "   2. Or double-click the archive to open it"
echo "   3. Click 'Distribute App' → TestFlight & App Store"
echo ""
echo "💡 Tip: Symlinks are now set up for your next archive!"
