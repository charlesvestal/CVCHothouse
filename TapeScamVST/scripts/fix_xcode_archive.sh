#!/usr/bin/env bash
# Script to fix Xcode project for iOS archiving after CMake generation
# Run this after regenerating the Xcode project with CMake

set -e

PROJECT_FILE="/Users/charlesvestal/github/CVCHothouse/TapeScamVST/build_ios/TapeScamVST.xcodeproj/project.pbxproj"

if [ ! -f "$PROJECT_FILE" ]; then
    echo "Error: Project file not found: $PROJECT_FILE"
    exit 1
fi

echo "Backing up project file..."
cp "$PROJECT_FILE" "$PROJECT_FILE.backup"

echo "Fixing library paths in OTHER_LDFLAGS for iOS archiving..."

# Use Perl to replace absolute library paths with $(BUILT_PRODUCTS_DIR)
perl -i -pe '
# Replace paths for libTapeScam_SharedCode.a
s{,/[^,]*/TapeScamVST_artefacts/(Debug|Release|MinSizeRel|RelWithDebInfo)/libTapeScam_SharedCode\.a,}
 {,"\$(BUILT_PRODUCTS_DIR)/libTapeScam_SharedCode.a",}g;

# Replace paths for libTapeScamGUIAssets.a
s{,/[^,]*/build_ios/(Debug|Release|MinSizeRel|RelWithDebInfo)/libTapeScamGUIAssets\.a,}
 {,"\$(BUILT_PRODUCTS_DIR)/libTapeScamGUIAssets.a",}g;
' "$PROJECT_FILE"

echo "Done! Run this script after every CMake regeneration."
echo "Note: Library paths now use \$(BUILT_PRODUCTS_DIR)"
