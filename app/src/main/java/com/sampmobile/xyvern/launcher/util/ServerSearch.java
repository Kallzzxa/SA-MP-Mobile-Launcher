package com.hopepride.samp.util;

import java.util.Locale;

/** Local server-browser search; it never changes the saved favorites. */
public final class ServerSearch {
    private ServerSearch() {}

    public static boolean matches(String query, String name, String address, int port, String cachedName) {
        String needle = normalize(query).trim();
        if (needle.isEmpty()) return true;
        String endpoint = (address == null ? "" : address) + ":" + port;
        return normalize(name).contains(needle) || normalize(cachedName).contains(needle)
                || normalize(endpoint).contains(needle);
    }

    private static String normalize(String value) {
        return SampColorText.strip(value).toLowerCase(Locale.ROOT);
    }
}
