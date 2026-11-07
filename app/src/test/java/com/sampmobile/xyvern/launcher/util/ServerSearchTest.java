package com.sampmobile.xyvern.launcher.util;

import org.junit.Test;
import static org.junit.Assert.*;

public class ServerSearchTest {
    @Test public void findsNamesAndAddressesCaseInsensitively() {
        assertTrue(ServerSearch.matches("hope", "{FF0000}HopePride", "play.example.org", 7777, null));
        assertTrue(ServerSearch.matches("EXAMPLE.ORG:7777", "Other", "play.example.org", 7777, null));
        assertTrue(ServerSearch.matches("NEW NAME", "Old", "host", 7777, "New Name"));
        assertFalse(ServerSearch.matches("absent", "Old", "host", 7777, null));
    }
    @Test public void blankAndNullQueriesMatchAll() {
        assertTrue(ServerSearch.matches(" ", null, null, 0, null));
        assertTrue(ServerSearch.matches(null, null, null, 0, null));
    }
}
