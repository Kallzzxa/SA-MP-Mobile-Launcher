package com.sampmobile.xyvern.launcher.util;

import android.content.Context;
import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class GameDataVerifier {

    // File-file inti GTA SA & SAMP yang esensial agar game bisa start tanpa crash
    public static final String[] ESSENTIAL_FILES = {
            "texdb/gta3.img",
            "data/gta.dat",
            "data/surface.dat",
            "data/fonts.dat",
            "data/handling.cfg",
            "AUDIO/CONFIG/BANKSLOT.DAT"
    };

    public static class VerificationResult {
        public final boolean isValid;
        public final List<String> missingFiles;
        public final long totalSizeBytes;

        public VerificationResult(boolean isValid, List<String> missingFiles, long totalSizeBytes) {
            this.isValid = isValid;
            this.missingFiles = missingFiles;
            this.totalSizeBytes = totalSizeBytes;
        }
    }

    // Only accept an unambiguous case variant. The native file loader uses the
    // same rule, so a green status cannot select a different audio file.
    private static File findUniqueBankSlot(File root) {
        File found = null;
        File[] audioDirs = root.listFiles();
        if (audioDirs == null) return null;
        for (File audio : audioDirs) {
            if (!audio.isDirectory() || !asciiNameEquals(audio.getName(), "audio")) continue;
            File[] configDirs = audio.listFiles();
            if (configDirs == null) continue;
            for (File config : configDirs) {
                if (!config.isDirectory() || !asciiNameEquals(config.getName(), "config")) continue;
                File[] files = config.listFiles();
                if (files == null) continue;
                for (File file : files) {
                    if (!file.isFile() || !asciiNameEquals(file.getName(), "bankslot.dat")) continue;
                    if (found != null) return null;
                    found = file;
                }
            }
        }
        return found;
    }

    private static boolean asciiNameEquals(String name, String expected) {
        if (name.length() != expected.length()) return false;
        for (int i = 0; i < name.length(); i++) {
            char c = name.charAt(i);
            if (c > 127) return false;
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c != expected.charAt(i)) return false;
        }
        return true;
    }

    public static VerificationResult verifyGameData(Context context) {
        return verifyGameDataRoot(context.getExternalFilesDir(null));
    }

    // Package-visible to exercise the same checks without Android storage in unit tests.
    static VerificationResult verifyGameDataRoot(File baseDir) {
        if (baseDir == null || !baseDir.exists()) {
            List<String> missing = new ArrayList<>();
            for (String f : ESSENTIAL_FILES) missing.add(f);
            return new VerificationResult(false, missing, 0);
        }

        List<String> missingFiles = new ArrayList<>();
        long totalSize = 0;

        for (String relativePath : ESSENTIAL_FILES) {
            File target = new File(baseDir, relativePath);
            // Match the native audio fallback without replacing either data file.
            if (relativePath.equals("AUDIO/CONFIG/BANKSLOT.DAT") && !target.exists()) {
                target = new File(baseDir, "audio/CONFIG/BankSlot.dat");
                if (!target.exists()) {
                    File alternate = findUniqueBankSlot(baseDir);
                    if (alternate != null) target = alternate;
                }
            }
            if (!target.isFile() || target.length() == 0) {
                missingFiles.add(relativePath);
            } else {
                totalSize += target.length();
            }
        }

        // Kalau texdb/gta3.img ada atau data/gta.dat ada, cek juga folder models/data
        boolean isValid = missingFiles.isEmpty();
        return new VerificationResult(isValid, missingFiles, totalSize);
    }
}
