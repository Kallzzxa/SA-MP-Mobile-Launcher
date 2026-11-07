package com.sampmobile.xyvern.game;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.concurrent.atomic.AtomicInteger;
import static org.junit.Assert.*;

public class StartupAssetsTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    @Test public void installsMissingFilesAndKeepsExistingFiles() throws Exception {
        File root = temporary.newFolder();
        AtomicInteger opens = new AtomicInteger();
        StartupAssets.AssetSource source = name -> {
            opens.incrementAndGet();
            return new ByteArrayInputStream(name.getBytes(StandardCharsets.UTF_8));
        };
        StartupAssets.prepare(root, source);
        File metrics = new File(root, "Textures/Fonts/RussianFont.met");
        File image = new File(root, "Textures/Fonts/RussianFont.png");
        assertEquals("Textures/fonts/RussianFont.met", new String(Files.readAllBytes(metrics.toPath()), StandardCharsets.UTF_8));
        assertEquals("Textures/fonts/RussianFont.png", new String(Files.readAllBytes(image.toPath()), StandardCharsets.UTF_8));
        Files.write(metrics.toPath(), "custom".getBytes(StandardCharsets.UTF_8));
        StartupAssets.prepare(root, source);
        assertEquals(2, opens.get());
        assertEquals("custom", new String(Files.readAllBytes(metrics.toPath()), StandardCharsets.UTF_8));
    }

    @Test public void refusesEmptyExistingFileWithoutReplacingIt() throws Exception {
        File root = temporary.newFolder();
        File dir = new File(root, "Textures/Fonts");
        assertTrue(dir.mkdirs());
        File existing = new File(dir, "RussianFont.met");
        assertTrue(existing.createNewFile());
        try {
            StartupAssets.prepare(root, name -> new ByteArrayInputStream(new byte[] {1}));
            fail("empty existing file must be reported");
        } catch (IOException expected) {
            assertTrue(existing.exists());
            assertEquals(0, existing.length());
        }
    }

    @Test public void failedCopyDoesNotLeavePartialFile() throws Exception {
        File root = temporary.newFolder();
        try {
            StartupAssets.prepare(root, name -> { throw new IOException("missing asset"); });
            fail("copy should fail");
        } catch (IOException expected) {
            File dir = new File(root, "Textures/Fonts");
            assertFalse(new File(dir, "RussianFont.met").exists());
            assertNotNull(dir.list());
            assertEquals(0, dir.list().length);
        }
    }
}
