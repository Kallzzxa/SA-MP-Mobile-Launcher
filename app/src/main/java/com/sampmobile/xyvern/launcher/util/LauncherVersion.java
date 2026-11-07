package com.sampmobile.xyvern.launcher.util;

/** Maps saved launcher version indices to the currently supported options. */
public final class LauncherVersion {
    private LauncherVersion() {}

    public static int normalizeIndex(int savedIndex, int optionCount) {
        if (savedIndex >= 0 && savedIndex < optionCount) return savedIndex;
        // Older builds used index 4 for 0.3.7-R5, now the third option.
        if (savedIndex == 4 && optionCount > 2) return 2;
        return 0;
    }
}
