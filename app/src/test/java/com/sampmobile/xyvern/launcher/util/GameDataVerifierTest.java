package com.hopepride.samp.launcher.util;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import static org.junit.Assert.*;

/** Regression coverage for file layouts without touching Android user data. */
public class GameDataVerifierTest {
    @Rule public TemporaryFolder temporary = new TemporaryFolder();

    private void put(File root, String path, String content) throws Exception {
        File file = new File(root, path);
        Files.createDirectories(file.getParentFile().toPath());
        Files.write(file.toPath(), content.getBytes(StandardCharsets.UTF_8));
    }

    private File rootWithCoreFiles() throws Exception {
        File root = temporary.newFolder();
        for (String file : new String[] {"texdb/gta3.img", "data/gta.dat", "data/surface.dat",
                "data/fonts.dat", "data/handling.cfg"}) put(root, file, "x");
        return root;
    }

    @Test public void findsAUniqueCaseVariantButRejectsMissingAndEmpty() throws Exception {
        File root = rootWithCoreFiles();
        assertFalse(GameDataVerifier.verifyGameDataRoot(root).isValid);
        put(root, "audio/config/bankslot.dat", "valid");
        assertTrue(GameDataVerifier.verifyGameDataRoot(root).isValid);
        put(root, "audio/config/bankslot.dat", "");
        assertFalse(GameDataVerifier.verifyGameDataRoot(root).isValid);
    }

    @Test public void refusesAmbiguousVariantsAndKeepsExactPriority() throws Exception {
        File root = rootWithCoreFiles();
        put(root, "audio/config/bankslot.dat", "first");
        put(root, "AuDio/conFIG/BanKsLot.DaT", "second");
        assertFalse(GameDataVerifier.verifyGameDataRoot(root).isValid);
        put(root, "audio/CONFIG/BankSlot.dat", "known");
        assertTrue(GameDataVerifier.verifyGameDataRoot(root).isValid);
        put(root, "AUDIO/CONFIG/BANKSLOT.DAT", "");
        assertFalse(GameDataVerifier.verifyGameDataRoot(root).isValid);
        assertEquals("first", new String(Files.readAllBytes(
                new File(root, "audio/config/bankslot.dat").toPath()), StandardCharsets.UTF_8));
    }
}
