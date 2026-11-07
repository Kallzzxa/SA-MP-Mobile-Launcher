package com.sampmobile.xyvern.game.ui.dialog;

import org.junit.Test;
import static org.junit.Assert.*;

public class DialogColumnsTest {
    @Test public void preservesEmptyColumns() {
        assertArrayEquals(new String[]{"name", "", "score", ""}, DialogColumns.split("name\t\tscore\t"));
        assertArrayEquals(new String[]{"", ""}, DialogColumns.split("\t"));
        assertArrayEquals(new String[]{"one"}, DialogColumns.split("one"));
    }
}
