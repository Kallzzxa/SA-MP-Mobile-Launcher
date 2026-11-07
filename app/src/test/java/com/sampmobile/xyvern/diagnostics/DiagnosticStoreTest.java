package com.sampmobile.xyvern.diagnostics;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import static org.junit.Assert.*;

public class DiagnosticStoreTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    @Test public void recordsFramesButNotExceptionMessages() throws Exception {
        File dir = temporary.newFolder();
        DiagnosticStore log = new DiagnosticStore(dir, "game");
        log.append("DATA_CONFIG_FAILED", new RuntimeException("private-token-and-password"));
        String result = new String(Files.readAllBytes(new File(dir, "game.log").toPath()), StandardCharsets.UTF_8);
        assertTrue(result.contains("DATA_CONFIG_FAILED"));
        assertTrue(result.contains("RuntimeException"));
        assertTrue(result.contains("DiagnosticStoreTest"));
        assertFalse(result.contains("private-token-and-password"));
    }

    @Test public void rollsWithinBoundAndSeparatesProcesses() throws Exception {
        File dir = temporary.newFolder();
        DiagnosticStore launcher = new DiagnosticStore(dir, "launcher");
        DiagnosticStore game = new DiagnosticStore(dir, "game");
        for (int i = 0; i < 5000; i++) launcher.append("LAUNCHER_TEST_EVENT", null);
        game.append("GAME_TEST_EVENT", null);
        File current = new File(dir, "launcher.log");
        File previous = new File(dir, "launcher.previous.log");
        assertTrue(previous.exists());
        assertTrue(previous.length() <= DiagnosticStore.MAX_BYTES);
        assertTrue(current.length() <= DiagnosticStore.MAX_BYTES);
        assertFalse(new String(Files.readAllBytes(current.toPath()), StandardCharsets.UTF_8).contains("GAME_TEST_EVENT"));
        assertTrue(new File(dir, "game.log").isFile());
    }

    @Test public void rejectsUntrustedEventAndProcessNames() throws Exception {
        try { new DiagnosticStore(temporary.newFolder(), "../other"); fail(); }
        catch (IllegalArgumentException expected) { }
        DiagnosticStore log = new DiagnosticStore(temporary.newFolder(), "launcher");
        try { log.append("PASSWORD=secret", null); fail(); }
        catch (IllegalArgumentException expected) { }
    }
}
