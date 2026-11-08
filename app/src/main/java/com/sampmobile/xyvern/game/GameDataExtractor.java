package com.hopepride.samp.game;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Extracts the downloaded game data archive into the app storage root.
 *
 * The previous implementation wrote every entry straight to its final path and let
 * any IOException escape as a bare FileNotFoundException, which is why the installer
 * failed with "DATA_DOWNLOAD_EXTRACT_FAILED" and no useful detail. The failures that
 * actually happen on device are:
 *
 *   - the archive contains both "data" (a file) and "data/..." (children), so the
 *     second write finds a file where it needs a directory;
 *   - a previous interrupted install left an empty directory where a file must go,
 *     or a truncated file where a directory must go;
 *   - the destination folder does not exist yet, or the app lost access to it;
 *   - the entry name is not safe to write (absolute path, "..", over-long path);
 *   - the volume runs out of space halfway through.
 *
 * This class handles each of those cases explicitly, writes through a temporary file
 * so a half-written file is never left behind, and reports which entry failed and why.
 * It has no Android dependency, so it is covered by JVM unit tests.
 */
public final class GameDataExtractor {

    public interface Listener {
        void onFile(int done, int total, String name);
    }

    public static final class Summary {
        public int files;
        public int directories;
        public int skipped;
        public int recovered;
        public long bytes;
        public final List<String> problems = new ArrayList<>();

        public String describe() {
            StringBuilder builder = new StringBuilder();
            builder.append(files).append(" berkas, ").append(directories).append(" folder, ")
                    .append(bytes).append(" byte");
            if (recovered > 0) builder.append(", ").append(recovered).append(" bentrok diperbaiki");
            if (skipped > 0) builder.append(", ").append(skipped).append(" entri dilewati");
            return builder.toString();
        }
    }

    private static final long MAX_ENTRY_BYTES = 2L * 1024 * 1024 * 1024;
    private static final int MAX_NAME_LENGTH = 240;
    private static final int MAX_PATH_LENGTH = 3500;

    private GameDataExtractor() { }

    public static Summary extract(File zipFile, File destination, Listener listener) throws IOException {
        if (zipFile == null || !zipFile.isFile()) {
            throw new IOException("Arsip data tidak ditemukan: " + (zipFile == null ? "null" : zipFile.getAbsolutePath()));
        }
        if (destination == null) throw new IOException("Folder tujuan tidak tersedia.");

        prepareDestination(destination);

        Summary summary = new Summary();
        String destinationCanonical = destination.getCanonicalPath();
        String prefix = destinationCanonical.endsWith(File.separator)
                ? destinationCanonical : destinationCanonical + File.separator;

        List<String> failures = new ArrayList<>();

        try (ZipFile zip = new ZipFile(zipFile)) {
            int totalEntries = zip.size();
            if (totalEntries <= 0) throw new IOException("Arsip kosong atau rusak.");

            List<ZipEntry> entries = new ArrayList<>(totalEntries);
            Enumeration<? extends ZipEntry> scan = zip.entries();
            while (scan.hasMoreElements()) entries.add(scan.nextElement());

            String stripPrefix = detectStripPrefix(entries);
            long needed = estimateNeededBytes(entries);
            long available = destination.getUsableSpace();
            if (needed > 0 && available > 0 && needed > available) {
                throw new IOException("Ruang penyimpanan tidak cukup: butuh " + needed
                        + " byte, tersedia " + available + " byte.");
            }

            int index = 0;
            for (ZipEntry entry : entries) {
                index++;
                String rawName = entry.getName();
                if (rawName == null || rawName.isEmpty()) continue;

                String name = strip(stripPrefix, rawName);
                if (name == null || name.isEmpty()) continue;

                String problem = nameProblem(name);
                if (problem != null) {
                    summary.skipped++;
                    if (summary.problems.size() < 20) summary.problems.add(name + ": " + problem);
                    continue;
                }

                File target;
                try {
                    target = new File(destination, name);
                } catch (RuntimeException badName) {
                    summary.skipped++;
                    if (summary.problems.size() < 20) summary.problems.add(name + ": nama tidak valid");
                    continue;
                }

                String canonical = target.getCanonicalPath();
                if (!canonical.equals(destinationCanonical) && !canonical.startsWith(prefix)) {
                    summary.skipped++;
                    if (summary.problems.size() < 20) summary.problems.add(name + ": keluar dari folder tujuan");
                    continue;
                }

                try {
                    if (entry.isDirectory()) {
                        makeDirectory(target, summary);
                    } else {
                        long written = writeFile(zip, entry, target, summary);
                        summary.files++;
                        summary.bytes += written;
                    }
                    if (listener != null) listener.onFile(index, totalEntries, name);
                } catch (IOException error) {
                    failures.add(name + " -> " + target.getAbsolutePath() + ": " + describe(error));
                    if (failures.size() >= 5) break;
                }
            }
        }

        if (!failures.isEmpty()) {
            throw new IOException("Gagal menulis " + failures.size() + " entri. "
                    + String.join(" | ", failures));
        }

        return summary;
    }

    private static void prepareDestination(File destination) throws IOException {
        if (!destination.exists()) {
            if (!destination.mkdirs() && !destination.isDirectory()) {
                throw new IOException("Folder tujuan tidak dapat dibuat: " + destination.getAbsolutePath());
            }
        }
        if (!destination.isDirectory()) {
            throw new IOException("Folder tujuan bukan direktori: " + destination.getAbsolutePath());
        }

        File probe = new File(destination, ".hopepride_write_probe");
        try {
            if (!probe.createNewFile()) {
                // An old probe may already exist (interrupted run); reuse it.
                if (!probe.isFile()) throw new IOException("Folder tujuan tidak dapat ditulis.");
            }
            try (OutputStream out = new FileOutputStream(probe)) {
                out.write(1);
            }
        } catch (IOException error) {
            throw new IOException("Folder tujuan tidak dapat ditulis: " + destination.getAbsolutePath()
                    + " (" + describe(error) + ")");
        } finally {
            //noinspection ResultOfMethodCallIgnored
            probe.delete();
        }
    }

    private static void makeDirectory(File target, Summary summary) throws IOException {
        if (target.exists() && !target.isDirectory()) {
            if (!target.delete()) {
                throw new IOException("Ada berkas dengan nama folder yang sama dan tidak dapat dihapus");
            }
            summary.recovered++;
        }
        if (!target.isDirectory() && !target.mkdirs() && !target.isDirectory()) {
            throw new IOException("Folder tidak dapat dibuat");
        }
        summary.directories++;
    }

    private static long writeFile(ZipFile zip, ZipEntry entry, File target, Summary summary) throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory()) {
            if (parent.exists()) {
                if (!parent.delete()) throw new IOException("Induk berupa berkas dan tidak dapat dihapus: " + parent.getName());
                summary.recovered++;
            }
            if (!parent.mkdirs() && !parent.isDirectory()) {
                throw new IOException("Folder induk tidak dapat dibuat: " + parent.getName());
            }
        }

        if (target.isDirectory()) {
            String[] children = target.list();
            if (children == null || children.length > 0) {
                throw new IOException("Ada folder dengan nama berkas yang sama dan tidak kosong");
            }
            if (!target.delete()) throw new IOException("Folder kosong dengan nama berkas yang sama tidak dapat dihapus");
            summary.recovered++;
        }

        long declared = entry.getSize();
        if (declared > MAX_ENTRY_BYTES) throw new IOException("Ukuran entri tidak wajar (" + declared + " byte)");

        File temporary = new File(parent, target.getName() + ".hopepart");
        long written = 0;
        try (InputStream in = new BufferedInputStream(zip.getInputStream(entry));
             OutputStream out = new BufferedOutputStream(new FileOutputStream(temporary))) {
            byte[] buffer = new byte[128 * 1024];
            int read;
            while ((read = in.read(buffer)) != -1) {
                out.write(buffer, 0, read);
                written += read;
            }
        }

        if (target.exists() && !target.delete()) {
            //noinspection ResultOfMethodCallIgnored
            temporary.delete();
            throw new IOException("Berkas lama tidak dapat ditimpa");
        }

        if (!temporary.renameTo(target)) {
            // Same directory, so a rename should succeed; fall back to a copy so a
            // filesystem that refuses renames over an existing name still works.
            try (InputStream in = new java.io.FileInputStream(temporary);
                 OutputStream out = new BufferedOutputStream(new FileOutputStream(target))) {
                byte[] buffer = new byte[128 * 1024];
                int read;
                while ((read = in.read(buffer)) != -1) out.write(buffer, 0, read);
            } finally {
                //noinspection ResultOfMethodCallIgnored
                temporary.delete();
            }
        }

        return written;
    }

    private static String strip(String prefix, String name) {
        if (prefix.isEmpty()) return name;
        if (!name.startsWith(prefix)) return "";       // belongs to another root inside the archive
        String stripped = name.substring(prefix.length());
        return stripped.isEmpty() ? "" : stripped;
    }

    /**
     * First level names that identify a GTA San Andreas data tree. A wrapper folder is
     * only stripped when what is inside it actually looks like game data, so an archive
     * that legitimately ships a folder called "deep" keeps its structure.
     */
    private static final String[] GAME_MARKERS = {
            "samp", "data", "models", "texdb", "anim", "audio", "text", "txd", "android", "assets"
    };

    /** Archives are usually packed inside a single top level folder. */
    static String detectStripPrefix(List<ZipEntry> entries) {
        String common = null;
        for (ZipEntry entry : entries) {
            String name = entry.getName();
            if (name == null || name.isEmpty()) continue;

            int slash = name.indexOf('/');
            if (slash < 0) return "";                  // a file at the root: no prefix
            String top = name.substring(0, slash + 1);

            if (common == null) {
                common = top;
            } else if (!common.equals(top)) {
                return "";
            }
        }

        if (common == null) return "";

        // A file with the same name as the folder means the archive is flat-ish.
        for (ZipEntry entry : entries) {
            String name = entry.getName();
            if (name != null && !name.isEmpty() && name.equals(common.substring(0, common.length() - 1))) {
                return "";
            }
        }

        if (!containsGameDataBelow(entries, common)) return "";
        return common;
    }

    private static boolean containsGameDataBelow(List<ZipEntry> entries, String prefix) {
        for (ZipEntry entry : entries) {
            String name = entry.getName();
            if (name == null || !name.startsWith(prefix)) continue;

            String rest = name.substring(prefix.length());
            if (rest.isEmpty()) continue;

            int slash = rest.indexOf('/');
            String first = (slash < 0 ? rest : rest.substring(0, slash)).toLowerCase(java.util.Locale.ROOT);
            if (first.isEmpty()) continue;

            for (String marker : GAME_MARKERS) {
                if (marker.equals(first)) return true;
            }
        }
        return false;
    }

    static long estimateNeededBytes(List<ZipEntry> entries) {
        long total = 0;
        for (ZipEntry entry : entries) {
            long size = entry.getSize();
            if (size > 0 && total < Long.MAX_VALUE - size) total += size;
        }
        return total;
    }

    /** Returns a human readable reason when the entry must not be written. */
    static String nameProblem(String name) {
        if (name.indexOf('\u0000') >= 0) return "mengandung karakter nol";
        if (name.startsWith("/") || name.startsWith("\\")) return "path absolut";
        if (name.length() > MAX_PATH_LENGTH) return "path terlalu panjang";

        String[] parts = name.replace('\\', '/').split("/");
        for (String part : parts) {
            if (part.isEmpty() || part.equals(".")) continue;
            if (part.equals("..")) return "mencoba keluar dari folder tujuan";
            if (part.length() > MAX_NAME_LENGTH) return "nama komponen terlalu panjang";
            for (int i = 0; i < part.length(); i++) {
                char c = part.charAt(i);
                if (c < 0x20) return "mengandung karakter kontrol";
            }
        }
        return null;
    }

    private static String describe(Throwable error) {
        String message = error.getMessage();
        return error.getClass().getSimpleName() + (message == null ? "" : ": " + message);
    }
}
