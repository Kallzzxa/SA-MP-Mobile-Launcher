package com.sampmobile.xyvern.cef.omp;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.LinkedList;
import java.util.List;

/*
 * KCP - a reliable UDP protocol, ported to pure Java.
 *
 * Algorithm source: skywind3000/kcp (ikcp.c / ikcp.h), MIT licensed.
 * https://github.com/skywind3000/kcp
 *
 * This file is a close port of that implementation, not a new protocol:
 * the segment header, ack semantics, RTO calculation, fast retransmit,
 * congestion-window growth, window probing and fragmentation behaviour
 * are kept identical so that a peer running the original C ikcp (for
 * example the aurora-mp/omp-cef server plugin) can talk to this class.
 *
 * Differences from the C original (none material to the wire):
 *  - No pluggable congestion-control ops (ccops): only the built-in
 *    algorithm from ikcp.c is implemented, which is what ikcp uses
 *    when no ops are installed.
 *  - No stream mode: send() always fragments with the frg field, as
 *    ikcp does when stream == 0. The peer is a message-oriented KCP
 *    user, so stream mode is not needed here.
 *  - Timestamps, sequence numbers and window values are Java ints.
 *    All comparisons use 32-bit wrapping difference (later - earlier),
 *    exactly like ikcp's _itimediff, so wrap-around behaves the same.
 *  - send() returns 0 on success (ikcp returns the byte count) and a
 *    negative value on error, per the API required by this project.
 *  - recv() returns a complete reassembled message (or null), instead
 *    of filling a caller buffer and returning a negative errno.
 *  - This class is not thread-safe, exactly like ikcpcb: callers must
 *    serialize send/input/update/recv calls (e.g. from one thread).
 *
 * Segment header, little-endian, 24 bytes (IKCP_OVERHEAD):
 *   conv u32, cmd u8, frg u8, wnd u16, ts u32, sn u32, una u32, len u32,
 *   then len bytes of data.
 *   cmd: PUSH=81, ACK=82, WASK=83, WINS=84.
 */
public class Kcp {

    /** Called with one UDP datagram to transmit. Implementations must
     * send the bytes verbatim (one call = one datagram). */
    @FunctionalInterface
    public interface Output {
        void send(byte[] data);
    }

    // --- constants from ikcp.c ---
    private static final int RTO_NDL = 30;       // no-delay min RTO
    private static final int RTO_MIN = 100;      // normal min RTO
    private static final int RTO_DEF = 200;
    private static final int RTO_MAX = 60000;
    private static final int CMD_PUSH = 81;
    private static final int CMD_ACK = 82;
    private static final int CMD_WASK = 83;
    private static final int CMD_WINS = 84;
    private static final int ASK_SEND = 1;       // need to send WASK
    private static final int ASK_TELL = 2;       // need to send WINS
    private static final int WND_SND = 32;
    private static final int WND_RCV = 128;      // must >= max fragment size
    private static final int MTU_DEF = 1400;
    private static final int INTERVAL = 100;
    private static final int OVERHEAD = 24;
    private static final int DEADLINK = 20;
    private static final int THRESH_INIT = 2;
    private static final int THRESH_MIN = 2;
    private static final int PROBE_INIT = 5000;
    private static final int PROBE_LIMIT = 120000;
    private static final int FASTACK_LIMIT = 5;

    private static final class Seg {
        int conv;
        int cmd;
        int frg;
        int wnd;
        int ts;
        int sn;
        int una;
        int resendts;
        int rto;
        int fastack;
        int xmit;
        byte[] data;

        Seg(byte[] data) {
            this.data = data;
        }
    }

    private static final class Ack {
        final int sn;
        final int ts;

        Ack(int sn, int ts) {
            this.sn = sn;
            this.ts = ts;
        }
    }

    private final int conv;
    private final Output output;

    private int mtu = MTU_DEF;
    private int mss = MTU_DEF - OVERHEAD;
    private int state;

    private int sndUna;
    private int sndNxt;
    private int rcvNxt;

    private int rxRttval;
    private int rxSrtt;
    private int rxRto = RTO_DEF;
    private int rxMinrto = RTO_MIN;

    private int sndWnd = WND_SND;
    private int rcvWnd = WND_RCV;
    private int rmtWnd = WND_RCV;
    private int cwnd;
    private int incr;
    private int probe;

    private int current;
    private int interval = INTERVAL;
    private int tsFlush = INTERVAL;
    private boolean updated;

    private int tsProbe;
    private int probeWait;

    private int ssthresh = THRESH_INIT;
    private int fastresend;
    private int fastlimit = FASTACK_LIMIT;
    private int nocwnd;
    private int nodelay;
    private int xmit;
    private final int deadLink = DEADLINK;

    private final LinkedList<Seg> sndQueue = new LinkedList<>();
    private final LinkedList<Seg> rcvQueue = new LinkedList<>();
    private final LinkedList<Seg> sndBuf = new LinkedList<>();
    private final LinkedList<Seg> rcvBuf = new LinkedList<>();
    private final List<Ack> acklist = new ArrayList<>();

    public Kcp(int conv, Output output) {
        if (output == null) {
            throw new IllegalArgumentException("output must not be null");
        }
        this.conv = conv;
        this.output = output;
    }

    public int getConv() {
        return conv;
    }

    /** ikcp_nodelay: nodelay enables the no-delay RTO floor (30ms),
     * interval is clamped to [10, 5000]ms, resend enables fast resend,
     * nocwnd disables congestion control. Mirrors ikcp_nodelay. */
    public void setNodelay(boolean nodelay, int intervalMs, int resend, boolean nocwnd) {
        this.nodelay = nodelay ? 1 : 0;
        this.rxMinrto = nodelay ? RTO_NDL : RTO_MIN;
        if (intervalMs >= 0) {
            if (intervalMs > 5000) {
                intervalMs = 5000;
            } else if (intervalMs < 10) {
                intervalMs = 10;
            }
            this.interval = intervalMs;
        }
        if (resend >= 0) {
            this.fastresend = resend;
        }
        this.nocwnd = nocwnd ? 1 : 0;
    }

    /** ikcp_wndsize. Note ikcp forces rcvWnd to be at least 128. */
    public void setWindowSize(int sndWnd, int rcvWnd) {
        if (sndWnd > 0) {
            this.sndWnd = sndWnd;
        }
        if (rcvWnd > 0) {
            this.rcvWnd = Math.max(rcvWnd, WND_RCV);
        }
    }

    /** ikcp_setmtu. Invalid values are ignored (ikcp returns -1). */
    public void setMtu(int mtu) {
        if (mtu < 50 || mtu < OVERHEAD) {
            return;
        }
        this.mtu = mtu;
        this.mss = mtu - OVERHEAD;
    }

    /** ikcp_waitsnd: segments in the send buffer plus the send queue. */
    public int waitSnd() {
        return sndBuf.size() + sndQueue.size();
    }

    // ------------------------------------------------------------------
    // 32-bit wrapping time difference, ikcp _itimediff.
    // Java int subtraction wraps exactly like (IINT32)(later - earlier).
    // ------------------------------------------------------------------
    private static int timediff(int later, int earlier) {
        return later - earlier;
    }

    private static int imin(int a, int b) {
        return a <= b ? a : b;
    }

    private static int imax(int a, int b) {
        return a >= b ? a : b;
    }

    private static int ibound(int lower, int middle, int upper) {
        return imin(imax(lower, middle), upper);
    }

    // ------------------------------------------------------------------
    // Little-endian encode / decode, ikcp_encode*/ikcp_decode*.
    // ------------------------------------------------------------------
    private static void put32(ByteArrayOutputStream out, int value) {
        out.write(value & 0xff);
        out.write((value >> 8) & 0xff);
        out.write((value >> 16) & 0xff);
        out.write((value >> 24) & 0xff);
    }

    private static void put16(ByteArrayOutputStream out, int value) {
        out.write(value & 0xff);
        out.write((value >> 8) & 0xff);
    }

    private static void put8(ByteArrayOutputStream out, int value) {
        out.write(value & 0xff);
    }

    private static int get32(byte[] data, int off) {
        return (data[off] & 0xff)
                | ((data[off + 1] & 0xff) << 8)
                | ((data[off + 2] & 0xff) << 16)
                | ((data[off + 3] & 0xff) << 24);
    }

    private static int get16(byte[] data, int off) {
        return (data[off] & 0xff) | ((data[off + 1] & 0xff) << 8);
    }

    private static void encodeSeg(ByteArrayOutputStream out, int convValue, int cmd,
            int frg, int wnd, int ts, int sn, int una, int len) {
        put32(out, convValue);
        put8(out, cmd);
        put8(out, frg);
        put16(out, wnd);
        put32(out, ts);
        put32(out, sn);
        put32(out, una);
        put32(out, len);
    }

    // ------------------------------------------------------------------
    // send: fragment and queue. Returns 0 on success, -1 for a negative
    // length, -2 if the message needs >= IKCP_WND_RCV fragments (as in
    // ikcp_send). Stream mode is off, so frg counts down to 0.
    // ------------------------------------------------------------------
    public int send(byte[] data) {
        if (data == null || data.length < 0) {
            return -1;
        }
        int len = data.length;
        int count;
        if (len <= mss) {
            count = 1;
        } else {
            count = (len + mss - 1) / mss;
        }
        if (count >= WND_RCV) {
            return -2;
        }
        if (count == 0) {
            count = 1;
        }
        int offset = 0;
        int remaining = len;
        for (int i = 0; i < count; i++) {
            int size = remaining > mss ? mss : remaining;
            byte[] chunk = new byte[size];
            if (size > 0) {
                System.arraycopy(data, offset, chunk, 0, size);
            }
            Seg seg = new Seg(chunk);
            seg.frg = count - i - 1;
            sndQueue.addLast(seg);
            offset += size;
            remaining -= size;
        }
        return 0;
    }

    // ------------------------------------------------------------------
    // peekSize: ikcp_peeksize, or -1 if no complete message is ready.
    // ------------------------------------------------------------------
    public int peekSize() {
        if (rcvQueue.isEmpty()) {
            return -1;
        }
        Seg seg = rcvQueue.getFirst();
        if (seg.frg == 0) {
            return seg.data.length;
        }
        if (rcvQueue.size() < seg.frg + 1) {
            return -1;
        }
        int length = 0;
        for (Seg s : rcvQueue) {
            length += s.data.length;
            if (s.frg == 0) {
                break;
            }
        }
        return length;
    }

    // ------------------------------------------------------------------
    // recv: ikcp_recv (non-peek). Returns one complete reassembled
    // message, or null if none is ready. Consumes its fragments and
    // moves newly in-order segments from rcvBuf into rcvQueue.
    // ------------------------------------------------------------------
    public byte[] recv() {
        if (rcvQueue.isEmpty()) {
            return null;
        }
        int peeksize = peekSize();
        if (peeksize < 0) {
            return null;
        }
        boolean recover = rcvQueue.size() >= rcvWnd;

        ByteArrayOutputStream out = new ByteArrayOutputStream(peeksize);
        java.util.Iterator<Seg> it = rcvQueue.iterator();
        while (it.hasNext()) {
            Seg seg = it.next();
            out.write(seg.data, 0, seg.data.length);
            int fragment = seg.frg;
            it.remove();
            if (fragment == 0) {
                break;
            }
        }

        moveRcvBufToQueue();

        if (rcvQueue.size() < rcvWnd && recover) {
            // Ready to send back IKCP_CMD_WINS in flush.
            probe |= ASK_TELL;
        }
        return out.toByteArray();
    }

    private void moveRcvBufToQueue() {
        while (!rcvBuf.isEmpty()) {
            Seg seg = rcvBuf.getFirst();
            if (seg.sn == rcvNxt && rcvQueue.size() < rcvWnd) {
                rcvBuf.removeFirst();
                rcvQueue.addLast(seg);
                rcvNxt++;
            } else {
                break;
            }
        }
    }

    // ------------------------------------------------------------------
    // ack / una / fastack parsing, ikcp_parse_ack / parse_una /
    // parse_fastack (with IKCP_FASTACK_CONSERVE, as in ikcp.c).
    // ------------------------------------------------------------------
    private void updateAck(int rtt) {
        int rto;
        if (rxSrtt == 0) {
            rxSrtt = rtt;
            rxRttval = rtt / 2;
        } else {
            int delta = rtt - rxSrtt;
            if (delta < 0) {
                delta = -delta;
            }
            rxRttval = (3 * rxRttval + delta) / 4;
            rxSrtt = (7 * rxSrtt + rtt) / 8;
            if (rxSrtt < 1) {
                rxSrtt = 1;
            }
        }
        rto = rxSrtt + imax(interval, 4 * rxRttval);
        rxRto = ibound(rxMinrto, rto, RTO_MAX);
    }

    private void shrinkBuf() {
        if (!sndBuf.isEmpty()) {
            sndUna = sndBuf.getFirst().sn;
        } else {
            sndUna = sndNxt;
        }
    }

    private void parseAck(int sn) {
        if (timediff(sn, sndUna) < 0 || timediff(sn, sndNxt) >= 0) {
            return;
        }
        java.util.Iterator<Seg> it = sndBuf.iterator();
        while (it.hasNext()) {
            Seg seg = it.next();
            if (sn == seg.sn) {
                it.remove();
                break;
            }
            if (timediff(sn, seg.sn) < 0) {
                break;
            }
        }
    }

    private void parseUna(int una) {
        java.util.Iterator<Seg> it = sndBuf.iterator();
        while (it.hasNext()) {
            Seg seg = it.next();
            if (timediff(una, seg.sn) > 0) {
                it.remove();
            } else {
                break;
            }
        }
    }

    private void parseFastack(int sn, int ts) {
        if (timediff(sn, sndUna) < 0 || timediff(sn, sndNxt) >= 0) {
            return;
        }
        for (Seg seg : sndBuf) {
            if (timediff(sn, seg.sn) < 0) {
                break;
            } else if (sn != seg.sn) {
                // IKCP_FASTACK_CONSERVE: only count acks newer than the segment.
                if (timediff(ts, seg.ts) >= 0) {
                    seg.fastack++;
                }
            }
        }
    }

    private void ackPush(int sn, int ts) {
        acklist.add(new Ack(sn, ts));
    }

    // ------------------------------------------------------------------
    // parseData: ikcp_parse_data. Inserts in sn order into rcvBuf,
    // drops duplicates and segments outside [rcvNxt, rcvNxt + rcvWnd).
    // ------------------------------------------------------------------
    private void parseData(Seg newseg) {
        int sn = newseg.sn;
        if (timediff(sn, rcvNxt + rcvWnd) >= 0 || timediff(sn, rcvNxt) < 0) {
            return;
        }
        boolean repeat = false;
        int insertAt = 0; // default: at the front (smaller than all)
        boolean found = false;
        for (int i = rcvBuf.size() - 1; i >= 0; i--) {
            Seg seg = rcvBuf.get(i);
            if (seg.sn == sn) {
                repeat = true;
                found = true;
                break;
            }
            if (timediff(sn, seg.sn) > 0) {
                insertAt = i + 1;
                found = true;
                break;
            }
        }
        if (!found) {
            insertAt = 0;
        }
        if (!repeat) {
            rcvBuf.add(insertAt, newseg);
        }
        moveRcvBufToQueue();
    }

    // ------------------------------------------------------------------
    // input: ikcp_input. Feeds one received UDP datagram, which may
    // contain several segments. Returns 0 on success, -1 for a bad
    // header/conv, -2 for a truncated segment, -3 for a bad command.
    // ------------------------------------------------------------------
    public int input(byte[] data) {
        if (data == null || data.length < OVERHEAD) {
            return -1;
        }
        int prevUna = sndUna;
        int size = data.length;
        int off = 0;
        int flag = 0;
        int maxack = 0;
        int latestTs = 0;

        while (true) {
            if (size < OVERHEAD) {
                break;
            }
            int convValue = get32(data, off);
            if (convValue != conv) {
                return -1;
            }
            int cmd = data[off + 4] & 0xff;
            int frg = data[off + 5] & 0xff;
            int wnd = get16(data, off + 6);
            int ts = get32(data, off + 8);
            int sn = get32(data, off + 12);
            int una = get32(data, off + 16);
            int len = get32(data, off + 20);
            off += OVERHEAD;
            size -= OVERHEAD;

            if (size < len || len < 0) {
                return -2;
            }
            if (cmd != CMD_PUSH && cmd != CMD_ACK && cmd != CMD_WASK && cmd != CMD_WINS) {
                return -3;
            }

            rmtWnd = wnd;
            parseUna(una);
            shrinkBuf();

            if (cmd == CMD_ACK) {
                if (timediff(current, ts) >= 0) {
                    updateAck(timediff(current, ts));
                }
                parseAck(sn);
                shrinkBuf();
                if (flag == 0) {
                    flag = 1;
                    maxack = sn;
                    latestTs = ts;
                } else {
                    if (timediff(sn, maxack) > 0) {
                        // IKCP_FASTACK_CONSERVE
                        if (timediff(ts, latestTs) > 0) {
                            maxack = sn;
                            latestTs = ts;
                        }
                    }
                }
            } else if (cmd == CMD_PUSH) {
                if (timediff(sn, rcvNxt + rcvWnd) < 0) {
                    ackPush(sn, ts);
                    if (timediff(sn, rcvNxt) >= 0) {
                        byte[] payload = new byte[len];
                        if (len > 0) {
                            System.arraycopy(data, off, payload, 0, len);
                        }
                        Seg seg = new Seg(payload);
                        seg.conv = convValue;
                        seg.cmd = cmd;
                        seg.frg = frg;
                        seg.wnd = wnd;
                        seg.ts = ts;
                        seg.sn = sn;
                        seg.una = una;
                        parseData(seg);
                    }
                }
            } else if (cmd == CMD_WASK) {
                probe |= ASK_TELL;
            } else if (cmd == CMD_WINS) {
                // Do nothing, exactly as in ikcp_input.
            }

            off += len;
            size -= len;
        }

        if (flag != 0) {
            parseFastack(maxack, latestTs);
        }

        if (timediff(sndUna, prevUna) > 0) {
            // Built-in congestion control (no ccops installed), as in
            // the tail of ikcp_input.
            if (cwnd < rmtWnd) {
                int mssValue = mss;
                if (cwnd < ssthresh) {
                    cwnd++;
                    incr += mssValue;
                } else {
                    if (incr < mssValue) {
                        incr = mssValue;
                    }
                    incr += (mssValue * mssValue) / incr + (mssValue / 16);
                    if ((cwnd + 1) * mssValue <= incr) {
                        cwnd = (incr + mssValue - 1) / (mssValue > 0 ? mssValue : 1);
                    }
                }
                if (cwnd > rmtWnd) {
                    cwnd = rmtWnd;
                    incr = rmtWnd * mssValue;
                }
            }
        }
        return 0;
    }

    private int wndUnused() {
        if (rcvQueue.size() < rcvWnd) {
            return rcvWnd - rcvQueue.size();
        }
        return 0;
    }

    // ------------------------------------------------------------------
    // flush: ikcp_flush.
    // ------------------------------------------------------------------
    public void flush() {
        if (!updated) {
            return;
        }
        int currentValue = current;
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        int priorCwnd = cwnd;

        // Template segment for ACK / WASK / WINS (ikcp's local seg).
        int templateWnd = wndUnused();
        int templateUna = rcvNxt;

        // Flush acknowledges.
        List<Ack> acks = new ArrayList<>(acklist);
        acklist.clear();
        for (Ack ack : acks) {
            if (buffer.size() + OVERHEAD > mtu) {
                output(buffer);
                buffer.reset();
            }
            encodeSeg(buffer, conv, CMD_ACK, 0, templateWnd, ack.ts, ack.sn, templateUna, 0);
        }

        // Probe window size (if remote window size equals zero).
        if (rmtWnd == 0) {
            if (probeWait == 0) {
                probeWait = PROBE_INIT;
                tsProbe = currentValue + probeWait;
            } else {
                if (timediff(currentValue, tsProbe) >= 0) {
                    if (probeWait < PROBE_INIT) {
                        probeWait = PROBE_INIT;
                    }
                    probeWait += probeWait / 2;
                    if (probeWait > PROBE_LIMIT) {
                        probeWait = PROBE_LIMIT;
                    }
                    tsProbe = currentValue + probeWait;
                    probe |= ASK_SEND;
                }
            }
        } else {
            tsProbe = 0;
            probeWait = 0;
        }

        if ((probe & ASK_SEND) != 0) {
            if (buffer.size() + OVERHEAD > mtu) {
                output(buffer);
                buffer.reset();
            }
            encodeSeg(buffer, conv, CMD_WASK, 0, templateWnd, 0, 0, templateUna, 0);
        }
        if ((probe & ASK_TELL) != 0) {
            if (buffer.size() + OVERHEAD > mtu) {
                output(buffer);
                buffer.reset();
            }
            encodeSeg(buffer, conv, CMD_WINS, 0, templateWnd, 0, 0, templateUna, 0);
        }
        probe = 0;

        // Calculate window size.
        int cwndValue = imin(sndWnd, rmtWnd);
        if (nocwnd == 0) {
            cwndValue = imin(cwnd, cwndValue);
        }

        // Move data from sndQueue to sndBuf.
        while (timediff(sndNxt, sndUna + cwndValue) < 0) {
            if (sndQueue.isEmpty()) {
                break;
            }
            Seg newseg = sndQueue.removeFirst();
            sndBuf.addLast(newseg);
            newseg.conv = conv;
            newseg.cmd = CMD_PUSH;
            newseg.wnd = templateWnd;
            newseg.ts = currentValue;
            newseg.sn = sndNxt++;
            newseg.una = rcvNxt;
            newseg.resendts = currentValue;
            newseg.rto = rxRto;
            newseg.fastack = 0;
            newseg.xmit = 0;
        }

        // Calculate resent (unsigned in C; use long here so that the
        // "never fast-retransmit" value 0xffffffff compares correctly).
        long resent = fastresend > 0 ? fastresend : 0xffffffffL;
        int rtomin = (nodelay == 0) ? (rxRto >> 3) : 0;

        int change = 0;
        boolean lost = false;
        for (Seg segment : sndBuf) {
            boolean needsend = false;
            if (segment.xmit == 0) {
                needsend = true;
                segment.xmit++;
                segment.rto = rxRto;
                segment.resendts = currentValue + segment.rto + rtomin;
            } else if (timediff(currentValue, segment.resendts) >= 0) {
                needsend = true;
                segment.xmit++;
                xmit++;
                if (nodelay == 0) {
                    segment.rto += imax(segment.rto, rxRto);
                } else {
                    int step = (nodelay < 2) ? segment.rto : rxRto;
                    segment.rto += step / 2;
                }
                segment.resendts = currentValue + segment.rto;
                lost = true;
            } else if (segment.fastack >= resent) {
                if (segment.xmit <= fastlimit || fastlimit <= 0) {
                    needsend = true;
                    segment.xmit++;
                    segment.fastack = 0;
                    segment.resendts = currentValue + segment.rto;
                    change++;
                }
            }

            if (needsend) {
                segment.ts = currentValue;
                segment.wnd = templateWnd;
                segment.una = rcvNxt;

                int size = buffer.size();
                int need = OVERHEAD + segment.data.length;
                if (size + need > mtu) {
                    output(buffer);
                    buffer.reset();
                }
                encodeSeg(buffer, segment.conv, segment.cmd, segment.frg,
                        segment.wnd, segment.ts, segment.sn, segment.una,
                        segment.data.length);
                if (segment.data.length > 0) {
                    buffer.write(segment.data, 0, segment.data.length);
                }
                if (segment.xmit >= deadLink) {
                    state = -1;
                }
            }
        }

        if (buffer.size() > 0) {
            output(buffer);
        }

        if (change != 0) {
            int inflight = sndNxt - sndUna;
            ssthresh = inflight / 2;
            if (ssthresh < THRESH_MIN) {
                ssthresh = THRESH_MIN;
            }
            cwnd = ssthresh + (fastresend > 0 ? fastresend : 0);
            incr = cwnd * mss;
        }

        if (lost) {
            ssthresh = priorCwnd / 2;
            if (ssthresh < THRESH_MIN) {
                ssthresh = THRESH_MIN;
            }
            cwnd = 1;
            incr = mss;
        }

        if (cwnd < 1) {
            cwnd = 1;
            incr = mss;
        }
    }

    private void output(ByteArrayOutputStream buffer) {
        if (buffer.size() == 0) {
            return;
        }
        output.send(buffer.toByteArray());
    }

    // ------------------------------------------------------------------
    // update: ikcp_update. Callers drive this about every 10ms with a
    // millisecond clock (only differences matter, so any epoch works).
    // ------------------------------------------------------------------
    public void update(long nowMs) {
        current = (int) nowMs;
        if (!updated) {
            updated = true;
            tsFlush = current;
        }
        int slap = timediff(current, tsFlush);
        if (slap >= 10000 || slap < -10000) {
            tsFlush = current;
            slap = 0;
        }
        if (slap >= 0) {
            tsFlush += interval;
            if (timediff(current, tsFlush) >= 0) {
                tsFlush = current + interval;
            }
            flush();
        }
    }
}
