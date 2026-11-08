package com.hopepride.samp.util;

import org.junit.Test;
import static org.junit.Assert.*;

public class SampColorTextTest {
    @Test public void escapesTextAndPreservesSampColors() {
        assertEquals("<font color=\"#FF0000\">&lt;b&gt;&amp;</font><font color=\"#00ff00\">ok</font>",
                SampColorText.toHtml("{FF0000}<b>&{00ff00}ok"));
        assertEquals("<b>&ok", SampColorText.strip("{FF0000}<b>&{00ff00}ok"));
    }

    @Test public void invalidTagsAreNotRemoved() {
        assertEquals("{xyz123} <tag>", SampColorText.strip("{xyz123} <tag>"));
        assertEquals("{xyz123} &lt;tag&gt;", SampColorText.toHtml("{xyz123} <tag>"));
        assertEquals("a&#160; b<br/>c", SampColorText.toHtml("a  b\nc"));
    }
}
