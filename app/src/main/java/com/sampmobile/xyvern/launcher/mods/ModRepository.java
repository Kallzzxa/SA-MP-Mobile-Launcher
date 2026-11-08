package com.sampmobile.xyvern.launcher.mods;

import android.content.Context;
import android.net.Uri;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * File level management of the mods the launcher stages for the game.
 *
 * This layer only moves, validates and records files: there is no supported way yet
 * to make the engine load a custom model or texture, so nothing here is presented as
 * if it were already applied inside the game. That limit is printed on the screen.
 */
public final class ModRepository {

    public static final String ACTIVE_DIR = "mods";
    public static final String DISABLED_DIR = "mods_disabled";

    private static final String HISTORY_PREFS = "hopepride_mods";
    private static final String HISTORY_KEY = "history";
    private static final int HISTORY_LIMIT = 120;
    private static final long HASH_LIMIT = 96L * 1024 * 1024;

    private final Context context;

    public ModRepository(Context context) {
        this.context = context.getApplicationContext();
    }

    public File activeDir() {
        return ensure(new File(root(), ACTIVE_DIR));
    }

    public File disabledDir() {
        return ensure(new File(root(), DISABLED_DIR));
    }

    private File root() {
        File external = context.getExternalFilesDir(null);
        return external != null ? external : context.getFilesDir();
    }

    private static File ensure(File dir) {
        if (!dir.exists()) {
            //noinspection ResultOfMethodCallIgnored
            dir.mkdirs();
        }
        return dir;
    }

    public List<ModEntry> scan() {
        List<ModEntry> result = new ArrayList<>();
        collect(activeDir(), true, result);
        collect(disabledDir(), false, result);
        return result;
    }

    private void collect(File dir, boolean enabled, List<ModEntry> into) {
        File[] files = dir.listFiles();
        if (files == null) return;

        for (File file : files) {
            if (file.isDirectory()) {
                collect(file, enabled, into);
                continue;
            }
            if (file.getName().startsWith(".") || file.getName().endsWith(".part")) continue;
            into.add(new ModEntry(file, enabled, categoryOf(file.getName()), problemOf(file)));
        }
    }

    public static String categoryOf(String name) {
        switch (extensionOf(name)) {
            case "dff":
            case "txd":
            case "col":
            case "ipl":
            case "ide":
                return "Model dan peta";
            case "wav":
            case "mp3":
            case "ogg":
                return "Suara";
            case "cs":
            case "csi":
            case "cleo":
                return "Skrip";
            case "png":
            case "jpg":
            case "jpeg":
                return "Gambar antarmuka";
            default:
                return "Lainnya";
        }
    }

    public static String extensionOf(String name) {
        int dot = name.lastIndexOf('.');
        return dot < 0 ? "" : name.substring(dot + 1).toLowerCase(Locale.ROOT);
    }

    private static String problemOf(File file) {
        if (!file.canRead()) return "tidak dapat dibaca";
        if (file.length() == 0L) return "file kosong (0 byte)";

        String ext = extensionOf(file.getName());
        if (ext.isEmpty()) return "tanpa ekstensi";

        switch (ext) {
            case "exe":
            case "dll":
                return "format Windows, tidak dapat dipakai di Android";
            case "zip":
            case "rar":
            case "7z":
                return "arsip, ekstrak isinya terlebih dahulu";
            default:
                return null;
        }
    }

    /** Copies the picked document into the active folder, refusing exact duplicates. */
    public ImportResult importFrom(Uri uri, String displayName) throws IOException {
        File target = new File(activeDir(), sanitize(displayName));

        InputStream source = context.getContentResolver().openInputStream(uri);
        if (source == null) throw new IOException("Sumber tidak dapat dibaca");

        File temporary = new File(activeDir(), target.getName() + ".part");
        long copied = 0L;
        MessageDigest digest = sha256();

        try (InputStream in = source; FileOutputStream out = new FileOutputStream(temporary)) {
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
                digest.update(buffer, 0, read);
                copied += read;
            }
        } catch (IOException error) {
            //noinspection ResultOfMethodCallIgnored
            temporary.delete();
            throw error;
        }

        if (copied == 0L) {
            //noinspection ResultOfMethodCallIgnored
            temporary.delete();
            return ImportResult.rejected(displayName, "File kosong (0 byte)");
        }

        String incomingHash = toHex(digest.digest());

        for (ModEntry existing : scan()) {
            if (existing.size != copied) continue;
            if (existing.size <= HASH_LIMIT && incomingHash.equalsIgnoreCase(hashOf(existing.file))) {
                //noinspection ResultOfMethodCallIgnored
                temporary.delete();
                return ImportResult.rejected(displayName, "Duplikat dari " + existing.name());
            }
        }

        File resolved = resolveNameConflict(target);
        if (!temporary.renameTo(resolved)) {
            //noinspection ResultOfMethodCallIgnored
            temporary.delete();
            throw new IOException("Gagal menyimpan " + resolved.getName());
        }

        record("impor", resolved.getName());
        return ImportResult.accepted(resolved, incomingHash, !resolved.equals(target));
    }

    private File resolveNameConflict(File target) {
        if (!target.exists()) return target;

        String name = target.getName();
        int dot = name.lastIndexOf('.');
        String base = dot < 0 ? name : name.substring(0, dot);
        String ext = dot < 0 ? "" : name.substring(dot);

        for (int index = 2; index < 1000; index++) {
            File candidate = new File(target.getParentFile(), base + " (" + index + ")" + ext);
            if (!candidate.exists()) return candidate;
        }

        return new File(target.getParentFile(), base + "-" + System.currentTimeMillis() + ext);
    }

    private static String sanitize(String displayName) {
        String name = displayName == null ? "mod" : displayName;
        name = name.replace('\\', '_').replace('/', '_').replace(':', '_').replace('\u0000', '_');

        int dot = name.lastIndexOf('.');
        String ext = dot < 0 ? "" : name.substring(dot + 1).replaceAll("[^A-Za-z0-9]", "");
        String base = dot < 0 ? name : name.substring(0, dot);
        base = base.replaceAll("[^A-Za-z0-9 _()\\-]", "_").trim();

        if (base.isEmpty()) base = "mod";
        if (base.length() > 64) base = base.substring(0, 64);

        return ext.isEmpty() ? base : base + "." + ext.toLowerCase(Locale.ROOT);
    }

    public boolean setEnabled(ModEntry entry, boolean enabled) {
        File targetDir = enabled ? activeDir() : disabledDir();
        File target = resolveNameConflict(new File(targetDir, entry.name()));

        boolean moved = entry.file.renameTo(target);
        if (moved) record(enabled ? "aktifkan" : "nonaktifkan", entry.name());
        return moved;
    }

    public boolean delete(ModEntry entry) {
        boolean deleted = entry.file.delete();
        if (deleted) record("hapus", entry.name());
        return deleted;
    }

    public static String hashOf(File file) {
        if (!file.isFile() || file.length() > HASH_LIMIT) return "";

        try (FileInputStream in = new FileInputStream(file)) {
            MessageDigest digest = sha256();
            byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = in.read(buffer)) > 0) digest.update(buffer, 0, read);
            return toHex(digest.digest());
        } catch (Exception ignored) {
            return "";
        }
    }

    private static MessageDigest sha256() {
        try {
            return MessageDigest.getInstance("SHA-256");
        } catch (Exception impossible) {
            throw new IllegalStateException(impossible);
        }
    }

    private static String toHex(byte[] bytes) {
        StringBuilder builder = new StringBuilder(bytes.length * 2);
        for (byte value : bytes) builder.append(String.format("%02x", value));
        return builder.toString();
    }

    public static String readableSize(long bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1024 * 1024) return String.format(Locale.GERMANY, "%.1f KB", bytes / 1024.0);
        if (bytes < 1024L * 1024 * 1024) return String.format(Locale.GERMANY, "%.1f MB", bytes / (1024.0 * 1024));
        return String.format(Locale.GERMANY, "%.2f GB", bytes / (1024.0 * 1024 * 1024));
    }

    public void record(String action, String name) {
        JSONArray history = historyArray();
        JSONObject item = new JSONObject();
        try {
            item.put("waktu", new SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.GERMANY).format(new Date()));
            item.put("aksi", action);
            item.put("nama", name);
        } catch (Exception ignored) {
            return;
        }

        JSONArray updated = new JSONArray();
        updated.put(item);
        for (int i = 0; i < history.length() && i < HISTORY_LIMIT - 1; i++) {
            updated.put(history.opt(i));
        }

        context.getSharedPreferences(HISTORY_PREFS, Context.MODE_PRIVATE)
                .edit().putString(HISTORY_KEY, updated.toString()).apply();
    }

    private JSONArray historyArray() {
        String raw = context.getSharedPreferences(HISTORY_PREFS, Context.MODE_PRIVATE)
                .getString(HISTORY_KEY, "[]");
        try {
            return new JSONArray(raw);
        } catch (Exception ignored) {
            return new JSONArray();
        }
    }

    public List<String> history() {
        JSONArray array = historyArray();
        List<String> lines = new ArrayList<>();
        for (int i = 0; i < array.length(); i++) {
            JSONObject item = array.optJSONObject(i);
            if (item == null) continue;
            lines.add(item.optString("waktu") + "  •  " + item.optString("aksi") + "  •  " + item.optString("nama"));
        }
        return lines;
    }

    public static final class ImportResult {
        public final boolean accepted;
        public final File file;
        public final String detail;

        private ImportResult(boolean accepted, File file, String detail) {
            this.accepted = accepted;
            this.file = file;
            this.detail = detail;
        }

        static ImportResult accepted(File file, String hash, boolean renamed) {
            return new ImportResult(true, file,
                    "Tersimpan sebagai " + file.getName() + (renamed ? " (nama bentrok, diberi akhiran)" : "")
                            + ", SHA-256 " + (hash.isEmpty() ? "tidak dihitung" : hash.substring(0, 16) + "…"));
        }

        static ImportResult rejected(String name, String reason) {
            return new ImportResult(false, null, name + ": " + reason);
        }
    }
}
