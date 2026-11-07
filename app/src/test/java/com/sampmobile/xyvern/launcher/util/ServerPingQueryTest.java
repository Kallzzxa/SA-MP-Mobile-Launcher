package com.sampmobile.xyvern.launcher.util;

import org.junit.Test;
import static org.junit.Assert.*;

public class ServerPingQueryTest {
    @Test public void validatesHeaderAndNonce() {
        byte[] request = {'S','A','M','P',127,0,0,1,0x61,0x1e,'p',1,2,3,4};
        byte[] response = request.clone();
        assertTrue(ServerPingQuery.matchesResponse(request, response, response.length));
        response[14] = 8;
        assertFalse(ServerPingQuery.matchesResponse(request, response, response.length));
        response = request.clone(); response[10] = 'i';
        assertFalse(ServerPingQuery.matchesResponse(request, response, response.length));
        assertFalse(ServerPingQuery.matchesResponse(request, response, 14));
        assertFalse(ServerPingQuery.matchesResponse(request, null, 15));
    }
}
