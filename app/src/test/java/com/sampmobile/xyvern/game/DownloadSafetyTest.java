package com.sampmobile.xyvern.game;

import org.junit.Test;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import static org.junit.Assert.*;

public class DownloadSafetyTest {
    @Test public void validatesPartialResponse() {
        assertTrue(DownloadSafety.matchesRange("bytes 100-199/200", 100, 100));
        assertTrue(DownloadSafety.matchesRange("bytes 0-199/*", 0, -1));
        assertFalse(DownloadSafety.matchesRange("bytes 0-199/200", 100, 200));
        assertFalse(DownloadSafety.matchesRange("bytes 100-199/300", 100, 100));
        assertFalse(DownloadSafety.matchesRange("bytes 100-199/200", 100, 99));
        assertFalse(DownloadSafety.matchesRange(null, 100, 100));
        assertFalse(DownloadSafety.matchesRange("bytes 999999999999999999999-2/3", 0, -1));
    }

    @Test public void acceptsOnlyMatchingCompletedRange() {
        assertTrue(DownloadSafety.isCompleteRange("bytes */200", 200, 200));
        assertTrue(DownloadSafety.isCompleteRange("bytes */200", 200, 0));
        assertFalse(DownloadSafety.isCompleteRange("bytes */200", 100, 0));
        assertFalse(DownloadSafety.isCompleteRange("bytes */200", 200, 300));
        assertFalse(DownloadSafety.isCompleteRange("bytes */0", 0, 0));
    }

    @Test public void rejectsZipTraversalAndSiblingPrefix() throws Exception {
        File parent = Files.createTempDirectory("download-safety").toFile();
        File root = new File(parent, "files");
        assertTrue(root.mkdir());
        assertEquals(new File(root, "data/gta.dat").getCanonicalFile(),
                DownloadSafety.safeZipTarget(root, "data/gta.dat").getCanonicalFile());
        for (String path : new String[]{"../outside", "../files-other/payload", "."}) {
            try {
                DownloadSafety.safeZipTarget(root, path);
                fail("Accepted unsafe entry: " + path);
            } catch (IOException expected) { /* rejected */ }
        }
        assertTrue(root.delete());
        assertTrue(parent.delete());
    }
}
