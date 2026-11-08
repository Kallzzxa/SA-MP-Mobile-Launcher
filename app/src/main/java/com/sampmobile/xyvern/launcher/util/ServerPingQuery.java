package com.hopepride.samp.launcher.util;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.security.SecureRandom;
import java.util.concurrent.TimeUnit;

/** Measures the SA-MP UDP query round trip, not the player's in-game ping. */
public final class ServerPingQuery {
    private ServerPingQuery() {}

    public static long measure(String host, int port) throws IOException {
        if (!ServerInput.isValidHost(host) || !ServerInput.isValidPort(port)) throw new IOException("Alamat server tidak valid");
        InetAddress address = null;
        for (InetAddress candidate : InetAddress.getAllByName(host)) {
            if (candidate instanceof Inet4Address) { address = candidate; break; }
        }
        if (address == null) throw new IOException("Query memerlukan IPv4");
        byte[] ip = address.getAddress();
        byte[] packet = {'S', 'A', 'M', 'P', ip[0], ip[1], ip[2], ip[3], (byte) port, (byte) (port >> 8), 'p', 0, 0, 0, 0};
        byte[] nonce = new byte[4];
        new SecureRandom().nextBytes(nonce);
        System.arraycopy(nonce, 0, packet, 11, nonce.length);
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.connect(address, port);
            socket.setSoTimeout(1800);
            long start = System.nanoTime();
            socket.send(new DatagramPacket(packet, packet.length));
            byte[] received = new byte[64];
            DatagramPacket reply = new DatagramPacket(received, received.length);
            socket.receive(reply);
            if (!matchesResponse(packet, received, reply.getLength())) throw new IOException("Respons ping tidak cocok");
            return Math.max(0, TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - start));
        }
    }

    static boolean matchesResponse(byte[] request, byte[] response, int length) {
        if (request == null || request.length != 15 || response == null || length < 15 || length > response.length) return false;
        for (int i = 0; i < request.length; i++) {
            if (request[i] != response[i]) return false;
        }
        return true;
    }
}
