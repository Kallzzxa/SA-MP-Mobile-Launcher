package com.sampmobile.xyvern.launcher.util;

import org.junit.Test;
import static org.junit.Assert.assertEquals;

public class LauncherVersionTest {
    @Test public void preservesSupportedSelections() {
        assertEquals(0, LauncherVersion.normalizeIndex(0, 3));
        assertEquals(1, LauncherVersion.normalizeIndex(1, 3));
        assertEquals(2, LauncherVersion.normalizeIndex(2, 3));
    }

    @Test public void handlesLegacyAndInvalidSelections() {
        assertEquals(2, LauncherVersion.normalizeIndex(4, 3));
        assertEquals(0, LauncherVersion.normalizeIndex(-1, 3));
        assertEquals(0, LauncherVersion.normalizeIndex(3, 3));
        assertEquals(0, LauncherVersion.normalizeIndex(4, 2));
    }
}
