#!/usr/bin/env bash
# Copies the primary app icon entry into the AUv3 NSExtensionIcon block so hosts display the right artwork.
set -euo pipefail

INFO_PLIST=${1:-}

if [[ -z "$INFO_PLIST" ]]; then
    echo "Usage: $0 <Info.plist path>" >&2
    exit 1
fi

if [[ ! -f "$INFO_PLIST" ]]; then
    echo "[set_auv3_icon] Info.plist not found: $INFO_PLIST (skipping)"
    exit 0
fi

PLIST_BUDDY=/usr/libexec/PlistBuddy

echo "[set_auv3_icon] Updating $INFO_PLIST"

if ! $PLIST_BUDDY -c "Print :CFBundleIcons:CFBundlePrimaryIcon" "$INFO_PLIST" >/dev/null 2>&1; then
    echo "[set_auv3_icon] No CFBundlePrimaryIcon found; cannot copy icon data"
    exit 0
fi

if ! $PLIST_BUDDY -c "Print :NSExtension:NSExtensionAttributes" "$INFO_PLIST" >/dev/null 2>&1; then
    echo "[set_auv3_icon] AUv3 extension attributes not present; skipping"
    exit 0
fi

$PLIST_BUDDY -c "Delete :NSExtension:NSExtensionAttributes:NSExtensionIcon" "$INFO_PLIST" >/dev/null 2>&1 || true
$PLIST_BUDDY -c "Copy :CFBundleIcons:CFBundlePrimaryIcon :NSExtension:NSExtensionAttributes:NSExtensionIcon" "$INFO_PLIST"

# Audio Unit hosts sometimes only honor the NSExtensionIconFile string, so mirror the 60pt entry.
ICON_FILE="AppIcon60x60"
if ! $PLIST_BUDDY -c "Print :CFBundleIcons:CFBundlePrimaryIcon:CFBundleIconFiles" "$INFO_PLIST" | grep -q "$ICON_FILE"; then
    # pick the first icon file as a fallback
    ICON_FILE=$($PLIST_BUDDY -c "Print :CFBundleIcons:CFBundlePrimaryIcon:CFBundleIconFiles:0" "$INFO_PLIST" 2>/dev/null || echo "AppIcon")
fi
$PLIST_BUDDY -c "Delete :NSExtension:NSExtensionAttributes:NSExtensionIconFile" "$INFO_PLIST" >/dev/null 2>&1 || true
$PLIST_BUDDY -c "Add :NSExtension:NSExtensionAttributes:NSExtensionIconFile string $ICON_FILE" "$INFO_PLIST"

echo "[set_auv3_icon] NSExtensionIcon now mirrors AppIcon"
