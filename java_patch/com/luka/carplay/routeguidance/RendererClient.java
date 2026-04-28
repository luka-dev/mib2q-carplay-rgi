/*
 * RendererClient - TCP client for c_render (port 19800).
 *
 * Sends 48-byte cr_cmd_t packets matching c_render/protocol.h.
 * All methods are non-fatal (swallow exceptions, log only).
 *
 * Java 1.2 compatible (no generics, no autoboxing, no enhanced for).
 */
package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;
import java.io.OutputStream;
import java.net.Socket;

public class RendererClient {

    private static final String TAG = "RendererClient";
    private static final int PORT = 19800;
    private static final int PKT_SIZE = 48;

    /* Command IDs -- must match protocol.h */
    private static final byte CMD_MANEUVER    = 0x01;
    private static final byte CMD_SHUTDOWN    = 0x03;
    private static final byte CMD_PERSPECTIVE = 0x04;
    private static final byte CMD_BARGRAPH    = 0x06;

    /* CMD_MANEUVER flags */
    private static final byte MAN_FLAG_SET_PERSP = 0x01;
    private static final byte MAN_FLAG_BARGRAPH  = 0x02;

    private Socket sock;
    private OutputStream out;

    public boolean connect() {
        if (sock != null && !sock.isClosed()) return true;
        try {
            sock = new Socket("127.0.0.1", PORT);
            sock.setTcpNoDelay(true);
            sock.setSoTimeout(1000);
            out = sock.getOutputStream();
            Log.i(TAG, "Connected to c_render:" + PORT);
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Connect failed: " + e.getMessage());
            close();
            return false;
        }
    }

    public void disconnect() {
        sendShutdown();
        close();
    }

    private void close() {
        try {
            if (out != null) out.close();
        } catch (Exception e) { /* ignore */ }
        try {
            if (sock != null) sock.close();
        } catch (Exception e) { /* ignore */ }
        out = null;
        sock = null;
    }

    public boolean isConnected() {
        return sock != null && !sock.isClosed() && out != null;
    }

    /**
     * Send CMD_MANEUVER — push a new maneuver with transition.
     *
     * @param icon           ICON_* constant (0-7)
     * @param direction      -1=left, 0=center, +1=right
     * @param exitAngle      signed degrees
     * @param drivingSide    0=RHT, 1=LHT
     * @param junctionAngles signed degree array (may be null)
     * @param bargraphLevel  0-16 (0=empty, 16=full)
     * @param bargraphMode   0=off, 1=on, 2=blink
     * @param perspective    0=2D, 1=3D after transition (-1=don't change)
     */
    public boolean sendManeuver(int icon, int direction, int exitAngle,
                                int drivingSide, int[] junctionAngles,
                                int bargraphLevel, int bargraphMode,
                                int perspective) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_MANEUVER;
        byte flags = 0;
        if (bargraphMode > 0)  flags |= MAN_FLAG_BARGRAPH;
        if (perspective >= 0)  flags |= MAN_FLAG_SET_PERSP;
        pkt[1] = flags;

        /* payload[0]: icon */
        pkt[2] = (byte) (icon & 0xFF);
        /* payload[1]: direction (signed) */
        pkt[3] = (byte) direction;
        /* payload[2..3]: exit_angle (big-endian i16) */
        pkt[4] = (byte) ((exitAngle >> 8) & 0xFF);
        pkt[5] = (byte) (exitAngle & 0xFF);
        /* payload[4]: driving_side */
        pkt[6] = (byte) (drivingSide & 0xFF);
        /* payload[5]: junction_count */
        int jCount = 0;
        if (junctionAngles != null) {
            jCount = junctionAngles.length;
            if (jCount > 18) jCount = 18;  /* max 18: payload[6..41], keeps [42..45] for persp/bargraph */
        }
        pkt[7] = (byte) (jCount & 0xFF);
        /* payload[6..45]: junction_angles (big-endian i16 each) */
        for (int i = 0; i < jCount; i++) {
            int off = 8 + i * 2;
            if (off + 1 >= PKT_SIZE) break;
            int a = junctionAngles[i];
            pkt[off]     = (byte) ((a >> 8) & 0xFF);
            pkt[off + 1] = (byte) (a & 0xFF);
        }

        /* perspective in payload[43] when flag set */
        if (perspective >= 0) {
            pkt[45] = (byte) (perspective & 0xFF);
        }
        /* bargraph in payload[44..45] when flag set */
        if (bargraphMode > 0) {
            pkt[46] = (byte) (bargraphLevel & 0xFF);
            pkt[47] = (byte) (bargraphMode & 0xFF);
        }

        return sendPacket(pkt);
    }

    /**
     * Send standalone CMD_BARGRAPH update.
     *
     * @param level 0-16
     * @param mode  0=off, 1=on, 2=blink
     * @return true if sent successfully
     */
    public boolean sendBargraph(int level, int mode) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_BARGRAPH;
        pkt[2] = (byte) (level & 0xFF);   /* payload[0] = level */
        pkt[3] = (byte) (mode & 0xFF);    /* payload[1] = mode */
        return sendPacket(pkt);
    }

    /**
     * Send CMD_PERSPECTIVE to switch 3D/2D mode.
     *
     * @param enabled 0=flat 2D, 1=perspective 3D
     */
    public void sendPerspective(int enabled) {
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_PERSPECTIVE;
        pkt[2] = (byte) (enabled & 0xFF);   /* payload[0] = on/off */
        if (sendPacket(pkt)) {
            Log.i(TAG, "Sent CMD_PERSPECTIVE=" + enabled);
        }
    }

    /**
     * Send CMD_SHUTDOWN for graceful renderer exit.
     */
    public void sendShutdown() {
        if (!isConnected()) return;
        byte[] pkt = new byte[PKT_SIZE];
        pkt[0] = CMD_SHUTDOWN;
        sendPacket(pkt);
        Log.i(TAG, "Sent CMD_SHUTDOWN");
    }

    private boolean sendPacket(byte[] pkt) {
        /* Auto-reconnect if not connected */
        if (!isConnected()) {
            if (!connect()) return false;
        }
        try {
            out.write(pkt);
            out.flush();
            return true;
        } catch (Exception e) {
            Log.w(TAG, "Send failed: " + e.getMessage());
            close();
            return false;
        }
    }
}
