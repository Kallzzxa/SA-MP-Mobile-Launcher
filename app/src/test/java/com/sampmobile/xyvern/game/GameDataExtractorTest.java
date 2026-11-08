package com.hopepride.samp.game;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

/**
 * Covers the extraction failures that produced DATA_DOWNLOAD_EXTRACT_FAILED on device,
 * including the file/directory clash that surfaced as a bare FileNotFoundException.
 */
public class GameDataExtractorTest {

    private File work;
    private File archive;
    private File root;

    @Before public void setUp() throws Exception {
        work = File.createTempFile("hopepride-extract", "");
        assertTrue(work.delete());
        assertTrue(work.mkdirs());
        archive = new File(work, "files.zip");
        root = new File(work, "root");
    }

    @After public void tearDown() {
        deleteRecursively(work);
    }

    private static void deleteRecursively(File file) {
        if (file == null || !file.exists()) return;
        File[] children = file.listFiles();
        if (children != null) for (File child : children) deleteRecursively(child);
        //noinspection ResultOfMethodCallIgnored
        file.delete();
    }

    private void writeZip(String[] names, String[] contents) throws IOException {
        try (ZipOutputStream out = new ZipOutputStream(new FileOutputStream(archive))) {
            for (int i = 0; i < names.length; i++) {
                if (names[i].endsWith("/")) {
                    out.putNextEntry(new ZipEntry(names[i]));
                    out.closeEntry();
                    continue;
                }
                out.putNextEntry(new ZipEntry(names[i]));
                if (contents[i] != null) {
                    out.write(contents[i].getBytes(StandardCharsets.UTF_8));
                }
                out.closeEntry();
            }
        }
    }

    @Test public void extractsFilesInsideSingleTopLevelFolder() throws Exception {
        writeZip(
                new String[]{"files/", "files/data/", "files/data/handling.cfg", "files/texdb/samp/samp.dxt.tmb"},
                new String[]{null, null, "isi", "tekstur"});

        GameDataExtractor.Summary summary = GameDataExtractor.extract(archive, root, null);

        File config = new File(root, "data/handling.cfg");
        assertTrue("config harus ada di akar tujuan", config.isFile());
        assertEquals("isi", read(config));
        assertTrue(new File(root, "texdb/samp/samp.dxt.tmb").isFile());
        assertFalse("folder pembungkus tidak boleh ikut dibuat", new File(root, "files").exists());
        assertEquals(2, summary.files);
        assertTrue(summary.bytes > 0);
    }

    @Test public void recoversWhenArchiveHasFileAndDirectoryWithSameName() throws Exception {
        // Exactly the shape that used to throw FileNotFoundException: "SAMP" is written
        // as a file first, then a child entry needs it to be a directory.
        writeZip(
                new String[]{"SAMP", "SAMP/settings.ini"},
                new String[]{"berkas", "[client]\nname=Pemain"});

        GameDataExtractor.Summary summary = GameDataExtractor.extract(archive, root, null);

        File settings = new File(root, "SAMP/settings.ini");
        assertTrue("berkas anak harus tertulis", settings.isFile());
        assertTrue(summary.recovered >= 1);
        assertTrue(summary.files >= 1);
    }

    @Test public void createsParentFoldersThatDoNotExistYet() throws Exception {
        writeZip(
                new String[]{"deep/deeper/deepest/file.bin"},
                new String[]{"x"});

        GameDataExtractor.extract(archive, root, null);
        assertTrue(new File(root, "deep/deeper/deepest/file.bin").isFile());
    }

    @Test public void refusesEntriesThatEscapeTheDestination() throws Exception {
        writeZip(
                new String[]{"../evil.txt", "ok.txt"},
                new String[]{"jahat", "aman"});

        GameDataExtractor.Summary summary = GameDataExtractor.extract(archive, root, null);

        assertFalse("berkas di luar tujuan tidak boleh dibuat", new File(work.getParentFile(), "evil.txt").exists());
        assertTrue(new File(root, "ok.txt").isFile());
        assertEquals(1, summary.skipped);
        assertFalse(summary.problems.isEmpty());
    }

    @Test public void keepsZeroByteEntries() throws Exception {
        writeZip(new String[]{"kosong.dat"}, new String[]{null});

        GameDataExtractor.Summary summary = GameDataExtractor.extract(archive, root, null);

        File file = new File(root, "kosong.dat");
        assertTrue(file.isFile());
        assertEquals(0L, file.length());
        assertEquals(1, summary.files);
    }

    @Test public void replacesStaleDirectoryWhereAFileMustGo() throws Exception {
        assertTrue(root.mkdirs());
        // An interrupted install can leave an empty folder behind where the archive
        // expects a file. Writing into it used to raise FileNotFoundException.
        File stale = new File(root, "data.txt");
        assertTrue(stale.mkdirs());

        writeZip(new String[]{"data.txt"}, new String[]{"isi"});
        GameDataExtractor.Summary summary = GameDataExtractor.extract(archive, root, null);

        assertTrue(stale.isFile());
        assertEquals("isi", read(stale));
        assertTrue(summary.recovered >= 1);
    }

    @Test public void overwritesExistingFileWithoutLeavingPartFiles() throws Exception {
        assertTrue(root.mkdirs());
        File existing = new File(root, "data.txt");
        try (FileOutputStream out = new FileOutputStream(existing)) {
            out.write("lama".getBytes(StandardCharsets.UTF_8));
        }

        writeZip(new String[]{"data.txt"}, new String[]{"baru"});
        GameDataExtractor.extract(archive, root, null);

        assertEquals("baru", read(existing));
        File[] leftovers = root.listFiles((dir, name) -> name.endsWith(".hopepart"));
        assertNotNull(leftovers);
        assertEquals("tidak boleh ada berkas sementara tertinggal", 0, leftovers.length);
    }

    @Test public void rejectsEmptyArchive() throws Exception {
        try (ZipOutputStream out = new ZipOutputStream(new FileOutputStream(archive))) {
            out.finish();
        }
        try {
            GameDataExtractor.extract(archive, root, null);
            fail("arsip kosong harus ditolak");
        } catch (IOException expected) {
            assertTrue(expected.getMessage().contains("kosong"));
        }
    }

    @Test public void detectsTopLevelPrefixOnlyWhenItReallyWrapsEverything() {
        List<ZipEntry> wrapped = new ArrayList<>();
        wrapped.add(new ZipEntry("files/a.txt"));
        wrapped.add(new ZipEntry("files/sub/b.txt"));
        assertEquals("files/", GameDataExtractor.detectStripPrefix(wrapped));

        List<ZipEntry> mixed = new ArrayList<>();
        mixed.add(new ZipEntry("files/a.txt"));
        mixed.add(new ZipEntry("README.txt"));
        assertEquals("", GameDataExtractor.detectStripPrefix(mixed));
    }

    @Test public void flagsUnsafeEntryNames() {
        assertNotNull(GameDataExtractor.nameProblem("../escape"));
        assertNotNull(GameDataExtractor.nameProblem("/absolute"));
        assertNotNull(GameDataExtractor.nameProblem("bad\u0000name"));
        assertEquals(null, GameDataExtractor.nameProblem("data/handling.cfg"));
    }

    private static String read(File file) throws IOException {
        byte[] bytes = java.nio.file.Files.readAllBytes(file.toPath());
        return new String(bytes, StandardCharsets.UTF_8);
    }
}
