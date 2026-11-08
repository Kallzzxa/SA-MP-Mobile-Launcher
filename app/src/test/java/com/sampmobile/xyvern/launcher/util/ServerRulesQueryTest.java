package com.hopepride.samp.launcher.util;

import org.junit.Test;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.List;
import static org.junit.Assert.*;

public class ServerRulesQueryTest {
    private final byte[] header = {'S','A','M','P',127,0,0,1,0x61,0x1e,'r'};

    @Test public void parsesRulesAndEmptyResult() throws Exception {
        ByteBuffer p = ByteBuffer.allocate(128).order(ByteOrder.LITTLE_ENDIAN);
        p.put(header).putShort((short) 1).put((byte) 7).put("version".getBytes(StandardCharsets.UTF_8));
        p.put((byte) 5).put("0.3.7".getBytes(StandardCharsets.UTF_8));
        List<ServerRulesQuery.Rule> rules = ServerRulesQuery.parse(p.array(), p.position(), header);
        assertEquals(1, rules.size());
        assertEquals("version", rules.get(0).name);
        assertEquals("0.3.7", rules.get(0).value);
        p.clear(); p.put(header).putShort((short) 0);
        assertTrue(ServerRulesQuery.parse(p.array(), p.position(), header).isEmpty());
    }

    @Test public void rejectsInvalidOrTruncatedPackets() throws Exception {
        ByteBuffer p = ByteBuffer.allocate(64).order(ByteOrder.LITTLE_ENDIAN);
        p.put(header).putShort((short) 1).put((byte) 1).put((byte) 'x').put((byte) 20).put((byte) 'y');
        try { ServerRulesQuery.parse(p.array(), p.position(), header); fail(); } catch (IOException expected) { }
        p.clear(); p.put(header).putShort((short) 0); p.array()[10] = 'i';
        try { ServerRulesQuery.parse(p.array(), p.position(), header); fail(); } catch (IOException expected) { }
    }
}
