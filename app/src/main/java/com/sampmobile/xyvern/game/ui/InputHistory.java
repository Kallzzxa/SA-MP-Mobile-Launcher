package com.sampmobile.xyvern.game.ui;

import java.util.ArrayList;
import java.util.List;

/** PC-style input history: newest first, with the unfinished draft restored on return. */
public final class InputHistory {
    private final int limit;
    private final List<String> entries = new ArrayList<>();
    private int position = -1;
    private String draft = "";

    public InputHistory(int limit) {
        if (limit <= 0) throw new IllegalArgumentException("limit must be positive");
        this.limit = limit;
    }

    public void add(String input) {
        if (input != null && !input.trim().isEmpty()) {
            entries.add(0, input);
            if (entries.size() > limit) entries.remove(entries.size() - 1);
        }
        reset();
    }

    public void reset() {
        position = -1;
        draft = "";
    }

    public String previous(String current) {
        if (entries.isEmpty()) return current;
        if (position == -1) draft = current == null ? "" : current;
        if (position < entries.size() - 1) position++;
        return entries.get(position);
    }

    public String next(String current) {
        if (position == -1) return current;
        position--;
        return position == -1 ? draft : entries.get(position);
    }
}
