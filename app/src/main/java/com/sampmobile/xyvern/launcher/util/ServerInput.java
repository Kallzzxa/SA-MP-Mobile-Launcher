package com.hopepride.samp.util;

/** Input constraints for the SA-MP client and its native INI reader. */
public final class ServerInput {
    private ServerInput() {}

    public static boolean isValidNickname(String value) {
        return value != null && value.matches("[A-Za-z0-9_\\[\\]()$@.=]{3,24}");
    }

    public static boolean isValidHost(String value) {
        return value != null && value.length() <= 127 && value.matches("[A-Za-z0-9._-]+");
    }

    public static boolean isValidPort(int value) {
        return value >= 1 && value <= 65535;
    }

    public static boolean isValidPassword(String value) {
        // The native INI reader trims whitespace and limits the value to 127 bytes.
        return value != null && value.matches("[\\x21-\\x7E]{0,127}");
    }
}
