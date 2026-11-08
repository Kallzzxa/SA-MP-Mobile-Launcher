package com.hopepride.samp.launcher.util;

import org.junit.Test;
import java.util.Arrays;
import java.util.List;
import static org.junit.Assert.*;

public class PlayerListFilterTest {
    private final ServerPlayerQuery.Player a = new ServerPlayerQuery.Player(9, "Alpha", -2, 80);
    private final ServerPlayerQuery.Player b = new ServerPlayerQuery.Player(3, "bravo", 5, 40);
    private final ServerPlayerQuery.Player c = new ServerPlayerQuery.Player(7, "ALICE", 5, 90);
    private final List<ServerPlayerQuery.Player> input = Arrays.asList(b, a, c);

    @Test public void filtersIgnoringCaseAndSpaces() {
        assertEquals(Arrays.asList(a, c), PlayerListFilter.apply(input, "  al ", PlayerListFilter.Order.SERVER));
        assertTrue(PlayerListFilter.apply(input, "not here", PlayerListFilter.Order.SERVER).isEmpty());
    }

    @Test public void sortsWithoutChangingOriginalOrder() {
        assertEquals(Arrays.asList(b, a, c), PlayerListFilter.apply(input, null, PlayerListFilter.Order.SERVER));
        assertEquals(Arrays.asList(c, a, b), PlayerListFilter.apply(input, "", PlayerListFilter.Order.NAME));
        assertEquals(Arrays.asList(b, c, a), PlayerListFilter.apply(input, "", PlayerListFilter.Order.SCORE));
        assertEquals(Arrays.asList(b, a, c), PlayerListFilter.apply(input, "", PlayerListFilter.Order.PING));
        assertEquals(Arrays.asList(b, c, a), PlayerListFilter.apply(input, "", PlayerListFilter.Order.ID));
        assertEquals(Arrays.asList(b, a, c), input);
    }

    @Test public void integerExtremesDoNotOverflowScoreSort() {
        ServerPlayerQuery.Player low = new ServerPlayerQuery.Player(1, "low", Integer.MIN_VALUE, 1);
        ServerPlayerQuery.Player high = new ServerPlayerQuery.Player(2, "high", Integer.MAX_VALUE, 1);
        assertEquals(Arrays.asList(high, low), PlayerListFilter.apply(Arrays.asList(low, high), "", PlayerListFilter.Order.SCORE));
    }
}
