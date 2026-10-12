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
            "data/handling.cfg"
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

    public static VerificationResult verifyGameData(Context context) {
        File baseDir = context.getExternalFilesDir(null);
        if (baseDir == null || !baseDir.exists()) {
            List<String> missing = new ArrayList<>();
            for (String f : ESSENTIAL_FILES) missing.add(f);
            return new VerificationResult(false, missing, 0);
        }

        List<String> missingFiles = new ArrayList<>();
        long totalSize = 0;

        for (String relativePath : ESSENTIAL_FILES) {
            File target = new File(baseDir, relativePath);
            if (!target.exists() || target.length() == 0) {
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
