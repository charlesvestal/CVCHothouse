# iOS Archiving & TestFlight Distribution Guide

## Quick Start - TestFlight Distribution

### 1. Archive from Xcode

**Which scheme to use:**
- Archive: **TapeScamVST_Standalone**
- This builds BOTH:
  - ✅ TapeScam.app (Standalone container)
  - ✅ TapeScam.appex (embedded AUv3 plugin)

**Steps:**
1. In Xcode, select **"Any iOS Device"** as the destination (or a connected device)
2. **Product → Archive**
3. Wait for the build to complete

**Note:** The post-build script now automatically fixes the embedded AUv3 plugin during the build!

### 2. Export for TestFlight

Once the archive appears in Xcode Organizer:

1. Click **Distribute App**
2. Select **TestFlight & App Store**
3. Choose **Upload**
4. Select your team and signing options:
   - **Automatically manage signing** (recommended)
   - Or manually select your certificates/profiles
5. Click **Upload**

The archive will be uploaded to App Store Connect and appear in TestFlight within a few minutes.

## Code Signing Setup

For TestFlight distribution, you need:

### In Xcode Project Settings:
1. Select the **TapeScamVST_Standalone** target
2. Go to **Signing & Capabilities**
3. Set your **Team**
4. Enable **Automatically manage signing**
5. Repeat for the **TapeScamVST_AUv3** target

### Required:
- Apple Developer account
- Development or Distribution certificate
- App ID for: `com.vestal.TapeScam`
- App ID for AUv3: `com.vestal.TapeScam.AUv3` (if needed)

## Troubleshooting

**"Archive validation failed"**
- If the embedded appex is still invalid, run `./scripts/fix_archive.sh` as a fallback
- The post-build script should fix this automatically now

**"Code signing error"**
- Make sure you've set up code signing in Xcode (see above)
- Check that you have valid certificates in Keychain Access
- Ensure your Apple Developer account is active

**"Could not find TapeScam.appex"**
- Make sure you're archiving the **TapeScamVST_Standalone** scheme
- This scheme builds both the app and the embedded plugin

## Alternative: Manual Archive Creation

If Xcode archive fails validation, you can still create a manual archive:

1. Archive from Xcode (even if validation fails)
2. Run: `./scripts/fix_archive.sh`
3. The script creates a fixed archive on your Desktop
4. Import it into Xcode Organizer: **Window → Organizer → Archives → +**

## Why This Setup?

Your project is on a symlinked volume (`/Volumes/ExtFS`), which causes CMake to generate paths that don't always resolve correctly during Xcode's archive process. The post-build script now automatically fixes the embedded AUv3 plugin, but the fallback manual script is available if needed.

### Long-term Solution

To avoid any archiving issues entirely, move your project to a non-symlinked location:
```bash
# Instead of /Volumes/ExtFS/charlesvestal/github/...
# Use /Users/charlesvestal/github/... directly
```

Then regenerate your CMake project.

## Summary

**For TestFlight:**
- ✅ Archive: **TapeScamVST_Standalone** scheme
- ✅ Destination: **Any iOS Device**
- ✅ The post-build script auto-fixes the AUv3 plugin
- ✅ Export via Organizer → Distribute → TestFlight & App Store

**Fallback (if needed):**
```bash
./scripts/fix_archive.sh
```
