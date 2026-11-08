package com.hopepride.samp.game.ui;

import org.junit.Test;
import static org.junit.Assert.*;

public class InputHistoryTest {
    @Test public void historyNavigatesAndRestoresDraft() {
        InputHistory history = new InputHistory(2);
        history.add("/one");
        history.add("hello");
        assertEquals("hello", history.previous("draft"));
        assertEquals("/one", history.previous("hello"));
        assertEquals("/one", history.previous("/one"));
        assertEquals("hello", history.next("/one"));
        assertEquals("draft", history.next("hello"));
        assertEquals("draft", history.next("draft"));
    }

    @Test public void boundedAndSkipsEmptyMessages() {
        InputHistory history = new InputHistory(2);
        history.add("first"); history.add("second"); history.add("third"); history.add(" ");
        assertEquals("third", history.previous(""));
        assertEquals("second", history.previous("third"));
        assertEquals("second", history.previous("second"));
    }
}
