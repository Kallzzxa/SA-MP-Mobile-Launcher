package com.hopepride.samp.launcher.mods;

import java.io.File;
import java.util.Locale;

/** One staged mod file and everything the manager can say about it without guessing. */
public final class ModEntry {
    public final File file;
    public final boolean enabled;
    public final long size;
    public final String category;
    public final String problem;

    public ModEntry(File file, boolean enabled, String category, String problem) {
        this.file = file;
        this.enabled = enabled;
        this.size = file.length();
        this.category = category;
        this.problem = problem;
    }

    public String name() {
        return file.getName();
    }

    public String extension() {
        String name = name();
        int dot = name.lastIndexOf('.');
        return dot < 0 ? "" : name.substring(dot + 1).toLowerCase(Locale.ROOT);
    }

    public String readableSize() {
        return ModRepository.readableSize(size);
    }

    public boolean isValid() {
        return problem == null;
    }

    public String statusText() {
        if (!isValid()) return "Bermasalah: " + problem;
        return enabled ? "Aktif" : "Nonaktif";
    }
}
