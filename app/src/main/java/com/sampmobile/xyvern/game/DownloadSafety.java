package com.sampmobile.xyvern.game;

import java.io.File;
import java.io.IOException;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Validation shared by the download and extraction paths. */
final class DownloadSafety {
    private static final Pattern CONTENT_RANGE = Pattern.compile("bytes (\\d+)-(\\d+)/(\\d+|\\*)");
    private static final Pattern UNSATISFIED_RANGE = Pattern.compile("bytes \\*/(\\d+)");

    private DownloadSafety() {}

    static String resolvedKey(String version) {
        return "resolved_" + (version == null || version.isEmpty() ? "1" : version);
    }

    static boolean matchesRange(String header, long expectedStart, long contentLength) {
        if (header == null) return false;
        Matcher m = CONTENT_RANGE.matcher(header.trim());
        if (!m.matches()) return false;
        try {
            long start = Long.parseLong(m.group(1));
            long end = Long.parseLong(m.group(2));
            if (start != expectedStart || end < start || end == Long.MAX_VALUE) return false;
            if (contentLength >= 0 && contentLength != end - start + 1) return false;
            return "*".equals(m.group(3)) || Long.parseLong(m.group(3)) == end + 1;
        } catch (NumberFormatException e) {
            return false;
        }
    }

    /** Bytes described by a valid Content-Range, or -1 for malformed/overflow input. */
    static long rangeLength(String header) {
        if (header == null) return -1;
        Matcher m = CONTENT_RANGE.matcher(header.trim());
        if (!m.matches()) return -1;
        try {
            long start = Long.parseLong(m.group(1));
            long end = Long.parseLong(m.group(2));
            return end < start || end == Long.MAX_VALUE ? -1 : end - start + 1;
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    static boolean isCompleteRange(String header, long existing, long expectedSize) {
        if (header == null || existing <= 0) return false;
        Matcher m = UNSATISFIED_RANGE.matcher(header.trim());
        if (!m.matches()) return false;
        try {
            long total = Long.parseLong(m.group(1));
            return total == existing && (expectedSize <= 0 || expectedSize == total);
        } catch (NumberFormatException e) {
            return false;
        }
    }

    static File safeZipTarget(File root, String name) throws IOException {
        File target = new File(root, name);
        String rootPath = root.getCanonicalPath();
        String targetPath = target.getCanonicalPath();
        if (!targetPath.startsWith(rootPath + File.separator)) {
            throw new IOException("Entri zip tidak valid: " + name);
        }
        return target;
    }
}
