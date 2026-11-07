package com.hopepride.samp.game;

import org.junit.Test;
import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import static org.junit.Assert.*;

public class DownloadSafetyTest {
    @Test public void resolvedUrlKeyUsesSameDefaultVersionAsDownload() {
        assertEquals("resolved_1", DownloadSafety.resolvedKey(null));
        assertEquals("resolved_1", DownloadSafety.resolvedKey(""));
        assertEquals("resolved_2", DownloadSafety.resolvedKey("2"));
    }

    @Test public void validatesPartialResponse() {
        assertTrue(DownloadSafety.matchesRange("bytes 100-199/200", 100, 100));
        assertTrue(DownloadSafety.matchesRange("bytes 0-199/*", 0, -1));
        assertFalse(DownloadSafety.matchesRange("bytes 0-199/200", 100, 200));
        assertFalse(DownloadSafety.matchesRange("bytes 100-199/300", 100, 100));
        assertFalse(DownloadSafety.matchesRange("bytes 100-199/200", 100, 99));
        assertFalse(DownloadSafety.matchesRange(null, 100, 100));
        assertFalse(DownloadSafety.matchesRange("bytes 999999999999999999999-2/3", 0, -1));
    }

    @Test public void extractsRangeLengthWhenContentLengthIsUnknown() {
        assertEquals(100, DownloadSafety.rangeLength("bytes 100-199/200"));
        assertEquals(200, DownloadSafety.rangeLength("bytes 0-199/*"));
        assertEquals(-1, DownloadSafety.rangeLength(null));
        assertEquals(-1, DownloadSafety.rangeLength("bytes 100-99/200"));
        assertEquals(-1, DownloadSafety.rangeLength("bytes 0-9223372036854775807/*"));
        assertEquals(-1, DownloadSafety.rangeLength("bytes 999999999999999999999-2/3"));
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
