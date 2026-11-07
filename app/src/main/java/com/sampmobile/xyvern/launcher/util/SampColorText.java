package com.sampmobile.xyvern.launcher.util;

/** SA-MP six-digit color tags are data, not arbitrary HTML. */
public final class SampColorText {
    private SampColorText() {}

    private static boolean isColor(String text, int start) {
        if (start + 7 >= text.length() || text.charAt(start) != '{' || text.charAt(start + 7) != '}') return false;
        for (int i = start + 1; i < start + 7; i++) {
            char c = text.charAt(i);
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
        }
        return true;
    }

    public static String strip(String text) {
        if (text == null) return "";
        StringBuilder result = new StringBuilder();
        for (int i = 0; i < text.length(); i++) {
            if (isColor(text, i)) i += 7;
            else result.append(text.charAt(i));
        }
        return result.toString();
    }

    public static String toHtml(String text) {
        if (text == null) return "";
        StringBuilder result = new StringBuilder();
        boolean colorOpen = false;
        for (int i = 0; i < text.length(); i++) {
            if (isColor(text, i)) {
                if (colorOpen) result.append("</font>");
                result.append("<font color=\"#").append(text, i + 1, i + 7).append("\">");
                colorOpen = true;
                i += 7;
                continue;
            }
            char c = text.charAt(i);
            if (c == '&') result.append("&amp;");
            else if (c == '<') result.append("&lt;");
            else if (c == '>') result.append("&gt;");
            else if (c == '\n') result.append("<br/>");
            else if (c == ' ' && i + 1 < text.length() && text.charAt(i + 1) == ' ') result.append("&#160;");
            else result.append(c);
        }
        if (colorOpen) result.append("</font>");
        return result.toString();
    }
}
