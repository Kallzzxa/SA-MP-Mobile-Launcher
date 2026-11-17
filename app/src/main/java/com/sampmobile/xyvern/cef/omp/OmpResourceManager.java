package com.sampmobile.xyvern.cef.omp;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * omp-cef resource download + VFS for Android, ported from the PC client
 * (aurora-mp/omp-cef, MIT) src/client/core/system/resource_manager.cpp.
 *
 * Differences from the PC client, all deliberate:
 * - The .pak is a plain ZIP (miniz on PC, ZipInputStream here), so no miniz
 *   dependency is needed.
 * - AES-CBC decryption of entries uses javax.crypto (PKCS5Padding is the
 *   same PKCS7 padding the server uses, block size 16).
 * - Manifest parsing uses org.json on Android; this class instead parses
 *   the manifest with a tiny hand-rolled parser (see parseManifest) so the
 *   core stays testable on a plain JDK without Android classes. The format
 *   is: { "resourceName": [ {"path": "<resource>.pak", "size": N,
 *   "hash": "<sha256 hex>"}, ... ], ... }.
 *
 * Server layout this mirrors: CEF_AddResource("my_ui") registers
 * scriptfiles/cef/my_ui/ on the server; the server packs it to
 * scriptfiles/cef/my_ui.pak and the manifest lists one entry per resource,
 * path "<resource>.pak", hash = SHA-256 of the whole .pak file.
 *
 * Entry format inside the .pak (src/shared/crypto.hpp):
 * - "OCEF2" magic (5 bytes) + 1 flag byte.
 * - Flag 0: plaintext follows.
 * - Flag 1 (encrypted, used for .html/.css/.js/.json/.xml/.svg):
 *   16-byte IV, then AES-CBC ciphertext with the master resource key
 *   (from the ServerConfig packet), PKCS7 padded.
 * - Entries without the magic are treated as the legacy format: the whole
 *   entry is IV + AES-CBC ciphertext with the master key.
 *
 * Cache layout on disk: <cacheDir>/<host>_<gamePort>/<resource>.pak, plus
 * the unpacked VFS in memory. A cached .pak is reused only if its SHA-256
 * matches the manifest hash, exactly like the PC client.
 */
public final class OmpResourceManager {
    public interface ProgressListener {
        void onDownloadStart(List<String> files);
        void onFileProgress(String fileName, long downloadedBytes, long totalBytes);
        void onDownloadComplete();
    }

    public static final class ManifestEntry {
        public final String resource;
        public final String path;
        public final long size;
        public final String hash;

        public ManifestEntry(String resource, String path, long size, String hash) {
            this.resource = resource;
            this.path = path;
            this.size = size;
            this.hash = hash;
        }
    }

    private final File cacheDir;
    private final ProgressListener progress;
    private byte[] masterKey;
    private final Map<String, ManifestEntry> manifest = new LinkedHashMap<>();
    /** resource -> (internal path -> file bytes) */
    private final Map<String, Map<String, byte[]>> vfs = new HashMap<>();
    private final Set<String> completed = new HashSet<>();
    private final Map<String, List<byte[]>> assembling = new HashMap<>();
    private final Map<String, Integer> assemblingTotal = new HashMap<>();
    private final Map<String, String> assemblingHash = new HashMap<>();
    private final Map<String, Long> assemblingSize = new HashMap<>();
    private boolean downloadStarted;

    public OmpResourceManager(File cacheDir, ProgressListener progress) {
        this.cacheDir = cacheDir;
        this.progress = progress;
    }

    public void setMasterKey(byte[] key) {
        this.masterKey = key == null ? null : key.clone();
    }

    public byte[] getMasterKey() {
        return masterKey == null ? null : masterKey.clone();
    }

    /** Parse the manifest JSON from JoinResponse. Returns the .pak entries. */
    public List<ManifestEntry> onManifest(String manifestJson) {
        manifest.clear();
        List<ManifestEntry> out = parseManifest(manifestJson);
        for (ManifestEntry e : out) manifest.put(e.resource + "/" + e.path, e);
        return out;
    }

    /**
     * Decide which .pak files still need downloading, reusing cached .pak
     * files whose SHA-256 matches. Returns the RequestFiles list
     * ([resource, path] pairs). Call after setMasterKey, like the PC client
     * (it waits for the master key before triggering a download).
     */
    public List<String[]> filesToRequest() {
        List<String[]> request = new ArrayList<>();
        List<String> names = new ArrayList<>();
        for (ManifestEntry e : manifest.values()) {
            File cached = pakFile(e);
            boolean ok = false;
            if (cached != null && cached.isFile()) {
                try {
                    String localHash = sha256Hex(readAll(cached));
                    if (localHash.equalsIgnoreCase(e.hash) && loadPakIntoVfs(e.resource, readAll(cached))) {
                        ok = true;
                        completed.add(e.resource + "/" + e.path);
                    }
                } catch (Exception ignored) {
                }
            }
            if (!ok) {
                request.add(new String[]{e.resource, e.path});
                names.add(e.path);
            }
        }
        if (request.isEmpty()) {
            if (progress != null) progress.onDownloadComplete();
        } else if (!downloadStarted) {
            downloadStarted = true;
            if (progress != null) progress.onDownloadStart(names);
        }
        return request;
    }

    public boolean isDownloadComplete() {
        return !manifest.isEmpty() && completed.size() >= manifest.size();
    }

    /** Feed one FileData packet. Returns true when the whole download just completed. */
    public boolean onFileData(OmpPacket.Packet p) throws IOException {
        String key = p.resourceName + "/" + p.relativePath;
        if (!assembling.containsKey(key)) {
            assembling.put(key, new ArrayList<>());
            for (int i = 0; i < p.totalChunks; i++) assembling.get(key).add(null);
            assemblingTotal.put(key, p.totalChunks);
            assemblingHash.put(key, p.fileHash);
            ManifestEntry e = manifest.get(key);
            assemblingSize.put(key, e == null ? 0L : e.size);
        }
        List<byte[]> chunks = assembling.get(key);
        if (p.chunkIndex < 0 || p.chunkIndex >= chunks.size()) return false;
        if (chunks.get(p.chunkIndex) == null) chunks.set(p.chunkIndex, p.data);
        long downloaded = 0;
        boolean all = true;
        for (byte[] c : chunks) {
            if (c == null) { all = false; } else downloaded += c.length;
        }
        if (progress != null) {
            progress.onFileProgress(p.relativePath, downloaded, assemblingSize.getOrDefault(key, 0L));
        }
        if (!all) return false;
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        for (byte[] c : chunks) out.write(c, 0, c.length);
        byte[] complete = out.toByteArray();
        String gotHash = sha256Hex(complete);
        if (!gotHash.equalsIgnoreCase(assemblingHash.getOrDefault(key, ""))) {
            // Hash mismatch: drop the assembly so the server's retransmit
            // (or a fresh request) can retry, like the PC client.
            assembling.remove(key);
            return false;
        }
        File f = pakFile(manifest.get(key));
        if (f != null) {
            f.getParentFile().mkdirs();
            try (FileOutputStream fos = new FileOutputStream(f)) {
                fos.write(complete);
            }
        }
        loadPakIntoVfs(p.resourceName, complete);
        completed.add(key);
        assembling.remove(key);
        if (isDownloadComplete()) {
            if (progress != null) progress.onDownloadComplete();
            return true;
        }
        return false;
    }

    /** VFS lookup for http://cef/<resource>/<internalPath>. Null if not loaded. */
    public byte[] getFileContent(String resource, String internalPath) {
        Map<String, byte[]> res = vfs.get(resource);
        if (res == null) return null;
        return res.get(internalPath);
    }

    public boolean hasResource(String resource) {
        return vfs.containsKey(resource);
    }

    // ---- .pak handling ----

    public boolean loadPakIntoVfs(String resource, byte[] pakBytes) {
        if (masterKey == null || masterKey.length == 0) return false;
        Map<String, byte[]> files = new HashMap<>();
        try (ZipInputStream zin = new ZipInputStream(new ByteArrayInputStream(pakBytes))) {
            ZipEntry entry;
            while ((entry = zin.getNextEntry()) != null) {
                if (entry.isDirectory()) continue;
                ByteArrayOutputStream out = new ByteArrayOutputStream();
                byte[] tmp = new byte[8192];
                int n;
                while ((n = zin.read(tmp)) > 0) out.write(tmp, 0, n);
                byte[] decoded = decodeResourceEntry(out.toByteArray(), masterKey);
                if (decoded == null) continue;
                files.put(entry.getName(), decoded);
            }
        } catch (Exception e) {
            return false;
        }
        if (files.isEmpty()) return false;
        vfs.put(resource, files);
        return true;
    }

    static byte[] decodeResourceEntry(byte[] input, byte[] masterKey) {
        if (input == null) return null;
        // OCEF2 magic + flag.
        if (input.length >= 6 && input[0] == 'O' && input[1] == 'C' && input[2] == 'E' && input[3] == 'F' && input[4] == '2') {
            int flags = input[5] & 0xFF;
            if ((flags & 1) == 0) {
                byte[] out = new byte[input.length - 6];
                System.arraycopy(input, 6, out, 0, out.length);
                return out;
            }
            if (input.length < 6 + 16) return null;
            byte[] iv = new byte[16];
            System.arraycopy(input, 6, iv, 0, 16);
            byte[] ct = new byte[input.length - 6 - 16];
            System.arraycopy(input, 6 + 16, ct, 0, ct.length);
            return aesCbcDecrypt(ct, masterKey, iv);
        }
        // Legacy: IV + ciphertext, no magic.
        if (input.length < 16) return null;
        byte[] iv = new byte[16];
        System.arraycopy(input, 0, iv, 0, 16);
        byte[] ct = new byte[input.length - 16];
        System.arraycopy(input, 16, ct, 0, ct.length);
        byte[] dec = aesCbcDecrypt(ct, masterKey, iv);
        if (dec == null) return null;
        return dec;
    }

    static byte[] aesCbcDecrypt(byte[] ct, byte[] key, byte[] iv) {
        try {
            Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5Padding");
            cipher.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "AES"), new IvParameterSpec(iv));
            return cipher.doFinal(ct);
        } catch (Exception e) {
            return null;
        }
    }

    // ---- helpers ----

    private File pakFile(ManifestEntry e) {
        if (e == null || cacheDir == null) return null;
        return new File(cacheDir, e.path);
    }

    public void setCacheSubdir(String host, int gamePort) {
        // Cache is per-server, like the PC client's <cache>/<ip>_<port>/.
        // This class keeps cacheDir as given; callers should construct it
        // with the per-server subdir already appended.
    }

    static String sha256Hex(byte[] data) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] h = md.digest(data);
            StringBuilder sb = new StringBuilder();
            for (byte b : h) sb.append(String.format("%02x", b & 0xFF));
            return sb.toString();
        } catch (Exception e) {
            return "";
        }
    }

    private static byte[] readAll(File f) throws IOException {
        try (FileInputStream in = new FileInputStream(f);
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] tmp = new byte[8192];
            int n;
            while ((n = in.read(tmp)) > 0) out.write(tmp, 0, n);
            return out.toByteArray();
        }
    }

    /** Minimal JSON parser for the manifest shape only; not a general parser. */
    static List<ManifestEntry> parseManifest(String json) {
        List<ManifestEntry> out = new ArrayList<>();
        if (json == null) return out;
        int i = 0;
        int n = json.length();
        // Find each "resource": [ ... ] pair at top level.
        while (i < n) {
            int q1 = json.indexOf('"', i);
            if (q1 < 0) break;
            int q2 = json.indexOf('"', q1 + 1);
            if (q2 < 0) break;
            String resource = json.substring(q1 + 1, q2);
            int bracket = json.indexOf('[', q2);
            if (bracket < 0) break;
            int depth = 0;
            int j = bracket;
            for (; j < n; j++) {
                char c = json.charAt(j);
                if (c == '[' || c == '{') depth++;
                else if (c == ']' || c == '}') {
                    depth--;
                    if (depth == 0 && c == ']') break;
                }
            }
            if (j >= n) break;
            String arrayBody = json.substring(bracket + 1, j);
            // Each object inside: {"path": "...", "size": N, "hash": "..."}
            int k = 0;
            while (k < arrayBody.length()) {
                int o = arrayBody.indexOf('{', k);
                if (o < 0) break;
                int c = arrayBody.indexOf('}', o);
                if (c < 0) break;
                String obj = arrayBody.substring(o + 1, c);
                String path = stringField(obj, "path");
                String hash = stringField(obj, "hash");
                long size = longField(obj, "size");
                if (path != null && hash != null) {
                    out.add(new ManifestEntry(resource, path, size, hash));
                }
                k = c + 1;
            }
            i = j + 1;
        }
        return out;
    }

    private static String stringField(String obj, String name) {
        String needle = "\"" + name + "\"";
        int p = obj.indexOf(needle);
        if (p < 0) return null;
        int colon = obj.indexOf(':', p + needle.length());
        if (colon < 0) return null;
        int q1 = obj.indexOf('"', colon);
        if (q1 < 0) return null;
        int q2 = obj.indexOf('"', q1 + 1);
        if (q2 < 0) return null;
        return obj.substring(q1 + 1, q2);
    }

    private static long longField(String obj, String name) {
        String needle = "\"" + name + "\"";
        int p = obj.indexOf(needle);
        if (p < 0) return 0;
        int colon = obj.indexOf(':', p + needle.length());
        if (colon < 0) return 0;
        int s = colon + 1;
        while (s < obj.length() && (obj.charAt(s) == ' ')) s++;
        int e = s;
        while (e < obj.length() && (obj.charAt(e) == '-' || (obj.charAt(e) >= '0' && obj.charAt(e) <= '9'))) e++;
        if (e == s) return 0;
        try {
            return Long.parseLong(obj.substring(s, e));
        } catch (NumberFormatException ex) {
            return 0;
        }
    }
}
