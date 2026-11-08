package com.hopepride.samp.game;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileAlreadyExistsException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;

/** Copies only missing font files needed by the GTA engine before native startup. */
public final class StartupAssets {
    public interface AssetSource { InputStream open(String name) throws IOException; }

    private static final String[] REQUIRED = {"RussianFont.met", "RussianFont.png"};
    private StartupAssets() {}

    public static synchronized void prepare(File root, AssetSource assets) throws IOException {
        if (root == null || !root.isDirectory()) throw new IOException("Penyimpanan game tidak tersedia.");
        File directory = new File(root, "Textures/Fonts");
        if (!directory.isDirectory() && !directory.mkdirs() && !directory.isDirectory()) {
            throw new IOException("Tidak dapat menyiapkan folder font game.");
        }
        for (String name : REQUIRED) {
            File destination = new File(directory, name);
            if (destination.exists()) {
                requireNonEmptyFile(destination);
                continue; // Never replace a user-supplied file.
            }
            Path temporary = Files.createTempFile(directory.toPath(), ".hopepride-font-", ".tmp");
            try {
                try (InputStream in = assets.open("Textures/fonts/" + name)) {
                    Files.copy(in, temporary, StandardCopyOption.REPLACE_EXISTING);
                }
                requireNonEmptyFile(temporary.toFile());
                try {
                    // No REPLACE_EXISTING: preserve a file created by someone else.
                    Files.move(temporary, destination.toPath());
                } catch (FileAlreadyExistsException e) {
                    requireNonEmptyFile(destination);
                }
                requireNonEmptyFile(destination);
            } finally {
                Files.deleteIfExists(temporary);
            }
        }
    }

    private static void requireNonEmptyFile(File file) throws IOException {
        if (!file.isFile() || !file.canRead() || file.length() == 0) {
            throw new IOException("File font tidak dapat dibaca atau kosong: " + file.getName());
        }
    }
}
