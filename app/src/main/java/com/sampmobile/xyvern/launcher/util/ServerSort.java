package com.hopepride.samp.util;

/** Comparisons for the optional server browser sort modes. */
public final class ServerSort {
    private ServerSort() { }

    public static int compareNames(String first, String second) {
        return SampColorText.strip(first).trim().compareToIgnoreCase(SampColorText.strip(second).trim());
    }

    public static int compareEndpoints(String firstHost, int firstPort, String secondHost, int secondPort) {
        String first = firstHost == null ? "" : firstHost.trim();
        String second = secondHost == null ? "" : secondHost.trim();
        int hostOrder = first.compareToIgnoreCase(second);
        return hostOrder != 0 ? hostOrder : Integer.compare(firstPort, secondPort);
    }
}
