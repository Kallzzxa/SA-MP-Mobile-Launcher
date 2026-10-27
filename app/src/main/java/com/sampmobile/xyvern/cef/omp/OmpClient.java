package com.sampmobile.xyvern.cef.omp;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.security.SecureRandom;
import java.util.List;

/**
 * omp-cef client transport for Android, ported from the PC client
 * (aurora-mp/omp-cef, MIT) src/client/core/network/network_manager.cpp.
 *
 * Flow, exactly like the PC client:
 * 1. UDP to server host : gamePort + 2 (the omp-cef server plugin listens on
 *    game port + 2 by default; e.g. game 7777 -> CEF 7779).
 * 2. Raw (non-KCP) RequestJoin -> HandshakeChallenge -> HandshakeFinalize.
 *    Session keys: rx||tx = BLAKE2b-512(X25519 || clientPk || serverPk).
 * 3. Raw JoinResponse (accepted, kcpConvId, manifestJson) -> create KCP
 *    (nodelay 1,10,2,1, window 128,128, like the PC client).
 * 4. Everything after that goes through KCP, encrypted with secretbox
 *    (nonce prepended, 24 bytes, then tag + ciphertext, libsodium format):
 *    ServerConfig, FileData, EmitEvent / EmitBrowserEvent from the server;
 *    RequestFiles, DownloadComplete, ClientEmitEvent to the server.
 * 5. No handshake after 5 join attempts x 2s -> onFailed("non-cef server"),
 *    same behaviour as the PC client, so legacy MobileCEF (RakNet 252)
 *    servers keep working untouched.
 *
 * Threading: one receiver thread owns the UDP socket and the KCP instance
 * (Kcp is not thread-safe, like ikcpcb). A second thread calls update()
 * every 10ms. Public send methods are synchronised on the client lock and
 * hand work to the receiver thread's KCP via a queue-free design: they run
 * on the calling thread but under the same lock the receiver thread uses,
 * so KCP is never touched concurrently.
 */
public final class OmpClient {
    public interface Listener {
        /** KCP is up and the manifest arrived. */
        void onConnected(String manifestJson);
        void onFailed(String reason);
        void onDisconnected();
        void onServerConfig(byte[] masterResourceKey, boolean resourcesLoaderUi);
        void onFileData(OmpPacket.Packet fileData);
        void onEmitEvent(OmpPacket.Packet emit);
    }

    /**
     * UDP endpoint abstraction. Production uses a real DatagramSocket;
     * tests use in-memory queues (the VM sandbox blocks UDP send with
     * "Operation not permitted", so a loopback UDP test cannot run here).
     * receive() returns null on timeout.
     */
    public interface UdpEndpoint {
        void send(byte[] data) throws IOException;
        byte[] receive(long timeoutMs) throws IOException;
        void close();
    }

    private static final class DatagramEndpoint implements UdpEndpoint {
        private final DatagramSocket socket;
        private final InetAddress address;
        private final int port;

        DatagramEndpoint(DatagramSocket socket, InetAddress address, int port) {
            this.socket = socket;
            this.address = address;
            this.port = port;
        }

        public void send(byte[] data) throws IOException {
            socket.send(new DatagramPacket(data, data.length, address, port));
        }

        public byte[] receive(long timeoutMs) throws IOException {
            socket.setSoTimeout((int) timeoutMs);
            byte[] buf = new byte[65535];
            DatagramPacket dp = new DatagramPacket(buf, buf.length);
            try {
                socket.receive(dp);
            } catch (java.net.SocketTimeoutException e) {
                return null;
            }
            if (dp.getAddress() == null || !dp.getAddress().equals(address)) return new byte[0];
            byte[] out = new byte[dp.getLength()];
            System.arraycopy(dp.getData(), dp.getOffset(), out, 0, out.length);
            return out;
        }

        public void close() {
            socket.close();
        }
    }

    private static final int MAX_JOIN_ATTEMPTS = 5;
    private static final int JOIN_RETRY_MS = 2000;
    private static final int KCP_UPDATE_MS = 10;
    private static final int CEF_PORT_OFFSET = 2;

    private final Listener listener;
    private final SecureRandom random = new SecureRandom();
    private final Object lock = new Object();

    private UdpEndpoint endpoint;
    private int playerId;
    private volatile boolean running;
    private Thread recvThread;
    private Thread updateThread;

    // Handshake state, only touched under lock / by recv thread.
    private int state; // 0 idle, 1 sending join, 2 awaiting acceptance, 3 connected
    private int joinAttempts;
    private long nextJoinAtMs;
    private byte[] cookie;
    private byte[] clientPublicKey;
    private byte[] clientSecretKey;
    private byte[] rxKey;
    private byte[] txKey;
    private Kcp kcp;

    public OmpClient(Listener listener) {
        if (listener == null) throw new IllegalArgumentException("listener");
        this.listener = listener;
    }

    /** Connect to an omp-cef server plugin. gamePort is the SA-MP port; CEF port = gamePort + 2. */
    public void connect(String host, int gamePort, int playerId) throws IOException {
        connectToCefPort(host, gamePort + CEF_PORT_OFFSET, playerId);
    }

    /** Connect directly to a CEF port (use when the server plugin was configured with a custom listen port). */
    public void connectToCefPort(String host, int cefPort, int playerId) throws IOException {
        InetAddress addr = InetAddress.getByName(host);
        DatagramSocket sock = new DatagramSocket();
        connectWithEndpoint(new DatagramEndpoint(sock, addr, cefPort), playerId);
    }

    /** Test / custom-transport entry point. The endpoint must already target the CEF port. */
    public void connectWithEndpoint(UdpEndpoint endpoint, int playerId) {
        synchronized (lock) {
            if (running) throw new IllegalStateException("already running");
            this.endpoint = endpoint;
            this.playerId = playerId;
            this.running = true;
            this.state = 1;
            this.joinAttempts = 0;
            this.nextJoinAtMs = 0;
            this.kcp = null;
            this.rxKey = null;
            this.txKey = null;
        }
        recvThread = new Thread(this::recvLoop, "omp-cef-recv");
        recvThread.setDaemon(true);
        recvThread.start();
        updateThread = new Thread(this::updateLoop, "omp-cef-update");
        updateThread.setDaemon(true);
        updateThread.start();
    }

    public void disconnect() {
        Thread rt, ut;
        UdpEndpoint ep;
        synchronized (lock) {
            running = false;
            ep = endpoint;
            endpoint = null;
            rt = recvThread;
            ut = updateThread;
            recvThread = null;
            updateThread = null;
            kcp = null;
        }
        if (ep != null) ep.close();
        if (rt != null) rt.interrupt();
        if (ut != null) ut.interrupt();
        listener.onDisconnected();
    }

    public boolean isConnected() {
        synchronized (lock) {
            return state == 3;
        }
    }

    // ---- outgoing (KCP, encrypted) ----

    public void sendRequestFiles(List<String[]> files) {
        sendKcpPacket(OmpPacket.encodeRequestFiles(files));
    }

    public void sendDownloadComplete() {
        sendKcpPacket(OmpPacket.encodeDownloadComplete());
    }

    public void sendClientEmitEvent(int browserId, String name, List<OmpPacket.Argument> args) {
        sendKcpPacket(OmpPacket.encodeClientEmitEvent(browserId, name, args));
    }

    private void sendKcpPacket(byte[] plain) {
        byte[] boxed;
        synchronized (lock) {
            if (!running || state != 3 || kcp == null || txKey == null) return;
            byte[] nonce = new byte[24];
            random.nextBytes(nonce);
            byte[] enc = OmpCrypto.secretboxEncrypt(plain, nonce, txKey);
            if (enc == null) return;
            boxed = new byte[nonce.length + enc.length];
            System.arraycopy(nonce, 0, boxed, 0, nonce.length);
            System.arraycopy(enc, 0, boxed, nonce.length, enc.length);
            kcp.send(boxed);
            kcp.flush();
        }
    }

    private void sendRaw(byte[] data) {
        try {
            UdpEndpoint ep;
            synchronized (lock) {
                ep = endpoint;
            }
            if (ep == null) return;
            ep.send(data);
        } catch (IOException e) {
            // Do not fail silently: on Android this goes to logcat and is
            // the first thing to check when a handshake never arrives.
            System.err.println("[OmpClient] UDP send failed: " + e);
        }
    }

    // ---- threads ----

    private void updateLoop() {
        while (true) {
            synchronized (lock) {
                if (!running) return;
                long now = System.currentTimeMillis();
                if (state == 1 || state == 2) {
                    if (now >= nextJoinAtMs) {
                        joinAttempts++;
                        if (joinAttempts > MAX_JOIN_ATTEMPTS) {
                            running = false;
                            listener.onFailed("non-cef server: no handshake after " + MAX_JOIN_ATTEMPTS + " attempts");
                            return;
                        }
                        sendRaw(OmpPacket.encodeRequestJoin(playerId));
                        nextJoinAtMs = now + JOIN_RETRY_MS;
                    }
                }
                if (state == 3 && kcp != null) {
                    kcp.update(now);
                    drainKcp();
                }
            }
            try {
                Thread.sleep(KCP_UPDATE_MS);
            } catch (InterruptedException e) {
                return;
            }
        }
    }

    private void recvLoop() {
        while (true) {
            UdpEndpoint ep;
            synchronized (lock) {
                if (!running) return;
                ep = endpoint;
            }
            if (ep == null) return;
            byte[] data;
            try {
                data = ep.receive(200);
            } catch (IOException e) {
                synchronized (lock) {
                    if (!running) return;
                }
                continue;
            }
            if (data == null || data.length == 0) continue;
            synchronized (lock) {
                if (!running) return;
                handleUdp(data);
            }
        }
    }

    private void handleUdp(byte[] data) {
        if (state == 3 && kcp != null) {
            kcp.input(data);
            drainKcp();
            return;
        }
        // Handshake phase: raw packets only. A KCP segment arriving before
        // JoinResponse (UDP reordering) is dropped here; the server's KCP
        // retransmits it after we create our KCP instance.
        OmpPacket.Packet p = OmpPacket.decode(data);
        if (p == null) return;
        if (p.type == OmpPacket.TYPE_HANDSHAKE_CHALLENGE && state == 1) {
            state = 2;
            cookie = p.cookie;
            byte[][] kp = OmpCrypto.generateKeypair(random);
            clientPublicKey = kp[0];
            clientSecretKey = kp[1];
            try {
                byte[][] keys = OmpCrypto.kxClientSessionKeys(clientPublicKey, clientSecretKey, p.publicKey);
                rxKey = keys[0];
                txKey = keys[1];
            } catch (Exception e) {
                listener.onFailed("key exchange failed: " + e.getMessage());
                running = false;
                return;
            }
            // Wipe the secret key as soon as session keys exist, like the PC client.
            java.util.Arrays.fill(clientSecretKey, (byte) 0);
            sendRaw(OmpPacket.encodeHandshakeFinalize(cookie, clientPublicKey));
        } else if (p.type == OmpPacket.TYPE_JOIN_RESPONSE && (state == 2 || state == 1)) {
            if (!p.accepted) {
                listener.onFailed("join rejected: " + p.message + " (reason " + p.rejectReason + ")");
                running = false;
                return;
            }
            if (rxKey == null || txKey == null) {
                listener.onFailed("join accepted before key exchange");
                running = false;
                return;
            }
            final int conv = p.kcpConvId;
            kcp = new Kcp(conv, datagram -> sendRaw(datagram));
            kcp.setNodelay(true, 10, 2, false);
            kcp.setWindowSize(128, 128);
            state = 3;
            listener.onConnected(p.manifestJson);
        }
    }

    private void drainKcp() {
        if (kcp == null) return;
        byte[] msg;
        while ((msg = kcp.recv()) != null) {
            if (msg.length < 24 + 16) continue;
            byte[] nonce = new byte[24];
            byte[] boxed = new byte[msg.length - 24];
            System.arraycopy(msg, 0, nonce, 0, 24);
            System.arraycopy(msg, 24, boxed, 0, boxed.length);
            byte[] plain = OmpCrypto.secretboxDecrypt(boxed, nonce, rxKey);
            if (plain == null) continue;
            OmpPacket.Packet p = OmpPacket.decode(plain);
            if (p == null) continue;
            switch (p.type) {
                case OmpPacket.TYPE_SERVER_CONFIG:
                    listener.onServerConfig(p.masterResourceKey, p.resourcesLoaderUi);
                    break;
                case OmpPacket.TYPE_FILE_DATA:
                    listener.onFileData(p);
                    break;
                case OmpPacket.TYPE_EMIT_EVENT:
                case OmpPacket.TYPE_EMIT_BROWSER_EVENT:
                    listener.onEmitEvent(p);
                    break;
                default:
                    break;
            }
        }
    }
}
