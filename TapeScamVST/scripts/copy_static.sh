#!/usr/bin/env bash
set -euo pipefail
SRC="$1"
BASE="$2"
CONFIG="${CONFIG:-Release}"
OUT_DIR="$BASE/$CONFIG/static"

# Create output directory
mkdir -p "$OUT_DIR"

# Check if source file exists
if [ ! -f "$SRC" ] && [ ! -L "$SRC" ]; then
    echo "Skipping copy: $SRC does not exist (likely during archive build)"
    exit 0
fi

# Handle symlinks properly during archive builds
if [ -L "$SRC" ]; then
    TARGET=$(readlink "$SRC")
    echo "Source is a symlink: $SRC -> $TARGET"

    # If the symlink target is relative, make it absolute relative to the symlink's directory
    if [[ ! "$TARGET" = /* ]]; then
        TARGET="$(cd "$(dirname "$SRC")" && cd "$(dirname "$TARGET")" && pwd)/$(basename "$TARGET")"
    fi

    # If target still doesn't exist, try resolving through /Users path instead of /Volumes/ExtFS
    if [ ! -f "$TARGET" ]; then
        # Replace /Volumes/ExtFS prefix with nothing to get the actual path
        ALTERNATE_TARGET="${TARGET/\/Volumes\/ExtFS/}"
        if [ -f "$ALTERNATE_TARGET" ]; then
            TARGET="$ALTERNATE_TARGET"
            echo "Using alternate path: $TARGET"
        fi
    fi

    if [ -f "$TARGET" ]; then
        echo "Copying resolved file: $TARGET"
        cp "$TARGET" "$OUT_DIR/lib$(basename "$SRC")"
    else
        echo "Warning: Target $TARGET does not exist, skipping copy (archive build)"
        exit 0
    fi
else
    cp "$SRC" "$OUT_DIR/lib$(basename "$SRC")"
fi
