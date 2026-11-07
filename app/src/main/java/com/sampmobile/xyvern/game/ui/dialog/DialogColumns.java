package com.sampmobile.xyvern.game.ui.dialog;

/** Preserve empty tablist cells, including the last column, as sent by the server. */
public final class DialogColumns {
    private DialogColumns() {}

    public static String[] split(String row) {
        return row.split("\t", -1);
    }
}
