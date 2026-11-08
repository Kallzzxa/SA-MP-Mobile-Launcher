package com.hopepride.samp.launcher.util;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.Locale;

/** Local filtering and optional sorting; never mutates the server response. */
public final class PlayerListFilter {
    public enum Order { SERVER, NAME, SCORE, PING, ID }

    private PlayerListFilter() { }

    public static List<ServerPlayerQuery.Player> apply(List<ServerPlayerQuery.Player> players,
                                                       String search, Order order) {
        String needle = search == null ? "" : search.trim().toLowerCase(Locale.ROOT);
        List<ServerPlayerQuery.Player> visible = new ArrayList<>();
        for (ServerPlayerQuery.Player player : players) {
            if (player.name.toLowerCase(Locale.ROOT).contains(needle)) visible.add(player);
        }
        Comparator<ServerPlayerQuery.Player> comparator = null;
        if (order == Order.NAME) {
            comparator = (a, b) -> String.CASE_INSENSITIVE_ORDER.compare(a.name, b.name);
        } else if (order == Order.SCORE) {
            comparator = (a, b) -> Integer.compare(b.score, a.score);
        } else if (order == Order.PING) {
            comparator = (a, b) -> Integer.compare(a.ping, b.ping);
        } else if (order == Order.ID) {
            comparator = (a, b) -> Integer.compare(a.id, b.id);
        }
        if (comparator != null) Collections.sort(visible, comparator);
        return visible;
    }
}
