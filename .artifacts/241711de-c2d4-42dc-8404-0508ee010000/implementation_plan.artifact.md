# Remove Update Functionality and Adjust UI

This plan aims to disable the APK update feature, remove specific text from the loading screen, and change the menu orientation to portrait.

## Proposed Changes

### Update Functionality

#### [MODIFY] [UpdateService.java](file:///C:/Users/Admin/Desktop/concac-JohnPeriaX/app/src/main/java/com/rstarx/hexrays/launcher/UpdateService.java)
- Modify `isGameUpdateExists()` to always return `false`. This prevents the launcher from detecting and downloading `update.apk`.

### UI Changes

#### [MODIFY] [activity_splash.xml](file:///C:/Users/Admin/Desktop/concac-JohnPeriaX/app/src/main/res/layout/activity_splash.xml)
- Remove or hide the `TextView` with ID `launcher_orig_text` containing "Using original launcher".

#### [MODIFY] [AndroidManifest.xml](file:///C:/Users/Admin/Desktop/concac-JohnPeriaX/app/src/main/AndroidManifest.xml)
- Change `android:screenOrientation="landscape"` to `android:screenOrientation="portrait"` for the following activities:
    - `.launcher.MainActivity`
    - `.launcher.SplashActivity`
    - `.launcher.UpdateActivity`

## Verification Plan

### Manual Verification
- Deploy the app to a device or emulator.
- Verify that the loading screen no longer displays "Using original launcher".
- Verify that the app starts in portrait mode and the main menu is in portrait mode.
- Verify that no update prompt for the APK appears, even if the version code on the server is higher.
