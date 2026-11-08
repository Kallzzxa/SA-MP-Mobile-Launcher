package com.hopepride.samp.launcher.util;

import android.content.Context;
import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

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
        return verifyGameDataRoot(context.getExternalFilesDir(null), textureVariantForDevice());
    }

    /**
     * Texture container variant the GTA engine selects for this GPU. The engine
     * requests texdb/txd/txd.&lt;variant&gt;.tmb during InitialiseRenderWare and
     * dereferences the handle without a null check, so a pack built for another
     * GPU family crashes the game. Returning null (unknown SoC) skips the check
     * instead of guessing.
     */
    public static String textureVariantForDevice() {
        String identity = safeSocValue("SOC_MODEL") + " "
                + safeSocValue("SOC_MANUFACTURER") + " "
                + safeBuildField("HARDWARE") + " "
                + safeBuildField("BOARD");
        return textureVariantForSoc(identity);
    }

    private static String safeBuildField(String name) {
        try {
            if ("HARDWARE".equals(name)) return String.valueOf(android.os.Build.HARDWARE);
            if ("BOARD".equals(name)) return String.valueOf(android.os.Build.BOARD);
        } catch (Throwable ignored) { }
        return "";
    }

    private static String safeSocValue(String name) {
        try {
            // SOC_MODEL / SOC_MANUFACTURER exist from API 31 only.
            java.lang.reflect.Field field = android.os.Build.class.getField(name);
            Object value = field.get(null);
            return value == null ? "" : String.valueOf(value);
        } catch (Throwable ignored) {
            return "";
        }
    }

    // Package-visible: pure mapping so it can be unit tested without a device.
    static String textureVariantForSoc(String identity) {
        if (identity == null) return null;
        String value = identity.toLowerCase(Locale.ROOT);
        if (containsAny(value, "qcom", "qualcomm", "adreno", "msm", "sdm", "sm-", "sm8", "sm7", "sm6")) return "dxt";
        if (containsAny(value, "exynos", "mali", "mediatek", "helio", "mt6", "mt8", "kirin", "hi3", "unisoc", "sprd", "ums")) return "etc";
        if (containsAny(value, "powervr", "imagination", "imgtec")) return "pvr";
        return null;
    }

    private static boolean containsAny(String value, String... needles) {
        for (String needle : needles) {
            if (value.contains(needle)) return true;
        }
        return false;
    }

    // Package-visible to exercise the same checks without Android storage in unit tests.
    static VerificationResult verifyGameDataRoot(File baseDir) {
        return verifyGameDataRoot(baseDir, null);
    }

    static VerificationResult verifyGameDataRoot(File baseDir, String textureVariant) {
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

        // The engine loads texdb/txd/txd.<variant>.tmb before the world exists and
        // does not survive a failed open, so verify the variant this GPU needs.
        if (textureVariant != null) {
            File textureDir = new File(baseDir, "texdb/txd");
            if (textureDir.isDirectory()) {
                String relative = "texdb/txd/txd." + textureVariant + ".tmb";
                File variant = new File(baseDir, relative);
                if (!variant.isFile() || variant.length() == 0) {
                    missingFiles.add(relative);
                } else {
                    totalSize += variant.length();
                }
            }
        }

        // Kalau texdb/gta3.img ada atau data/gta.dat ada, cek juga folder models/data
        boolean isValid = missingFiles.isEmpty();
        return new VerificationResult(isValid, missingFiles, totalSize);
    }
}
