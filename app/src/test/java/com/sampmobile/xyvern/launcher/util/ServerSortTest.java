package com.hopepride.samp.launcher.util;

import org.junit.Test;
import static org.junit.Assert.*;

public class ServerSortTest {
    @Test public void namesIgnoreColorTagsCaseAndOuterSpaces() {
        assertEquals(0, ServerSort.compareNames(" {ff0000}Alpha ", "alpha"));
        assertTrue(ServerSort.compareNames("{FFFFFF}Bravo", "alpha") > 0);
        assertTrue(ServerSort.compareNames(null, "alpha") < 0);
    }

    @Test public void endpointsUseCaseInsensitiveHostThenNumericPort() {
        assertEquals(0, ServerSort.compareEndpoints(" EXAMPLE.COM ", 7777, "example.com", 7777));
        assertTrue(ServerSort.compareEndpoints("example.com", 999, "example.com", 1000) < 0);
        assertTrue(ServerSort.compareEndpoints("a.example", 9999, "b.example", 1) < 0);
        assertTrue(ServerSort.compareEndpoints(null, 1, "a", 1) < 0);
    }
}
