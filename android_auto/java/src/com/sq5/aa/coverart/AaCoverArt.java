/*
 * SQ5 (MHI2Q MU0918) Android Auto cover art -> Virtual Cockpit media screen.
 * Port of luka-dev's "Cover art on the cluster" (mib2q-carplay-rgi docs/hook/cover-art.md).
 *
 * luka (CarPlay): native hook reassembles the iAP2 artwork -> 256x256 PNG on tmpfs ->
 *   EVT_COVERART -> CoverArt -> TerminalModeBapCombi merges it into the now-playing
 *   CombiBAPCurrentStationInfo -> AppConnectorTerminalMode pushes it to the VC picture manager.
 * Here (Android Auto): no native hook is needed.  gal already writes the phone's album art to
 *   /tmp/gal_albumArt_<n>.png and reports it through DSIAndroidAuto2.coverArtUrl
 *   (updateCoverArtUrl(ResourceLocator, valid); valid=2 / null locator = no art), which stock
 *   MU0918 ignores (only DSIAndroidAuto2DefaultListener implements it, as a no-op).  This class
 *   is that missing consumer: it listens on the rebuilt AndroidAuto2ListenerDistributor,
 *   normalises the file on its own worker thread to luka's verified format (PngCover:
 *   256x256 RGB PNG) under /tmp/sq5_coverart/, and tells the rebuilt AppConnectorTerminalMode
 *   (through AaCoverArtConnector, on the terminal-mode dispatcher) to push it to the VC.
 *
 * Lifecycle: only while an Android Auto device is ACTIVATING/ACTIVE (same IDeviceManager
 * listener AaLukaBridge uses).  Art reported before the session callback is kept and applied
 * at session start; session end drops the art, deletes our files and clears the VC mapping.
 * A phone "no art" (valid=2) is applied after a 400 ms grace, because gal clears and re-sets
 * the art on every track change (car log: valid=2 then valid=1 _2.png 9 ms later).
 *
 * Kill switch: sq5_coverart_off at the root of SD1 (/fs/sda0 or /net/mmx/fs/sda0) -> nothing
 * is attached, the connector keeps stock MU0918 behaviour.  Log: /tmp/sq5_aa.log via AaLog.
 * Safety: every entry point catches Throwable; file I/O and decoding never run on the DSI or
 * HMI threads.
 *
 * Robustness (car log drive 36: every file failed with a bare IllegalStateException from the
 * J9 Inflater, see PngCover): the file signature is sniffed and logged (PNG decoded, JPEG passed
 * through when small and complete, WebP/GIF/BMP/HEIF rejected by name); a file that is missing,
 * short, truncated or changes size while being read is retried with backoff (100/200/400/800 ms,
 * ~1.5 s in total, abandoned at once when newer art arrives); every failure is logged with the
 * exception class and its first stack frames.  Only Java 1.4 / J9 Foundation APIs.
 */
package com.sq5.aa.coverart;

import com.sq5.aa.luka.AaLog;

import de.audi.app.terminalmode.IContext;
import de.audi.app.terminalmode.device.IActiveDeviceStateListener;
import de.audi.app.terminalmode.device.IDeviceManager;
import de.audi.app.terminalmode.device.TMDevice;
import de.audi.app.terminalmode.dsi.androidauto2.DSIAndroidAuto2DefaultListener;
import de.audi.app.terminalmode.smartphone.androidauto2.AndroidAuto2ListenerDistributor;
import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Vector;
import org.dsi.ifc.global.ResourceLocator;

public final class AaCoverArt {
    public static final String VERSION = "aa-coverart 1.0";
    public static final String KILL_FLAG = "sq5_coverart_off";
    static String[] sdRoots = {"/fs/sda0/", "/net/mmx/fs/sda0/"};
    /* Flat in /dev/shmem (= MMX /tmp): the VC picture server runs on the RCC and reads it as
     * /net/mmx/dev/shmem/... (AaCoverArtConnector.vcUrl). The old /tmp/sq5_coverart pseudo-dir path was
     * unreadable there: the VC showed a black placeholder (owner, 2026-09-29). */
    static final String OUT_DIR = "/dev/shmem";
    static final String PREFIX = "sq5_cover_";
    static final long CLEAR_GRACE_MS = 400L;
    /** Backoff before each retry of a retryable failure (sum ~1.5 s). */
    static final long[] RETRY_MS = {100L, 200L, 400L, 800L};
    /** Shortest file that can be a picture header we can classify. */
    static final int MIN_FILE_BYTES = 16;
    static final long JPEG_MAX_BYTES = 512L * 1024L;
    static final int JPEG_MAX_SIDE = 1024;
    static final int KEEP_FILES = 2;

    /** One published picture: BAP picture id + file handed to the VC picture server. */
    public static final class Ref {
        public final int id;
        public final String path;
        final long crc;

        Ref(int id, String path, long crc) {
            this.id = id;
            this.path = path;
            this.crc = crc;
        }

        public String toString() {
            return "id=" + id + " path=" + path;
        }
    }

    /** The rebuilt AppConnectorTerminalMode side; called on the terminal-mode dispatcher. */
    public interface Host {
        void coverArtChanged();
    }

    /** Thread hand-off to the terminal-mode dispatcher (tests run it inline). */
    interface Poster {
        void post(Runnable r);
    }

    /** Test hook: receives every log line. */
    interface LogSink {
        void line(String s);
    }

    private static volatile AaCoverArt instance;
    private static volatile Host host;
    static volatile LogSink logSink;

    /* ------------------------------------------------------------------ instance state */

    private final File dir;
    private final Poster poster;
    private final Object lock = new Object();
    private final Listener listener = new Listener();
    private final Vector files = new Vector();     /* our files, oldest first (worker only) */
    private boolean session;                        /* guarded by lock */
    private int sessionGen;                         /* guarded by lock */
    private boolean hasRequest;                     /* guarded by lock */
    private String requestUrl;                      /* guarded by lock; null = no art */
    private long requestSeq;                        /* guarded by lock */
    private volatile Ref current;
    private int lastId;                             /* worker only */
    private Thread worker;
    private int nArt, nClear, nFail, nDedup;

    AaCoverArt(File dir, Poster poster) {
        this.dir = dir;
        this.poster = poster;
    }

    /* ------------------------------------------------------------------ static facade */

    static void log(String msg) {
        String line = "coverart " + msg;
        try {
            AaLog.log(line);
        } catch (Throwable t) {
            /* ignore */
        }
        LogSink s = logSink;
        if (s != null) s.line(line);
    }

    /** SD kill switch; read on every call (SD may mount after the combi bundle starts). */
    public static boolean killed() {
        for (int i = 0; i < sdRoots.length; i++) {
            try {
                if (new File(sdRoots[i] + KILL_FLAG).exists()) return true;
            } catch (Throwable t) {
                /* ignore */
            }
        }
        return false;
    }

    /**
     * Called from AaLukaBridge.attach (rebuilt AndroidAuto2ListenerDistributor constructor).
     * Registers the cover-art DSI listener on the distributor and the Android Auto session
     * listener on the terminal-mode device manager. Idempotent, never throws.
     */
    public static synchronized void attach(AndroidAuto2ListenerDistributor distributor, final IContext context) {
        try {
            if (killed()) {
                log("kill switch " + KILL_FLAG + " present - Android Auto cover art disabled");
                return;
            }
            AaCoverArt c = instance;
            if (c != null) {
                distributor.addSingleListener(c.listener);
                log("attach: listener added to new distributor");
                return;
            }
            c = new AaCoverArt(new File(OUT_DIR), new Poster() {
                public void post(Runnable r) {
                    context.getDispatcher().execute(r);
                }
            });
            c.cleanDir();
            c.start();
            instance = c;
            distributor.addSingleListener(c.listener);
            IDeviceManager dm = context.getDeviceManager();
            dm.addActiveDeviceListener(c.listener);
            log("attached (" + VERSION + ", out " + OUT_DIR + ", " + PngCover.OUT + "x" + PngCover.OUT + " PNG)");
            TMDevice active = dm.getActiveDevice();
            if (active != null) c.listener.updateActiveDeviceState(active);
        } catch (Throwable t) {
            log("attach failed: " + t);
        }
    }

    /** Connector side: register the push target (may run before attach). */
    public static void setHost(Host h) {
        host = h;
    }

    public static boolean isSessionActive() {
        AaCoverArt c = instance;
        return c != null && c.sessionActive();
    }

    /** Current Android Auto picture, or null (no session / no art / disabled). */
    public static Ref current() {
        AaCoverArt c = instance;
        return c == null ? null : c.currentRef();
    }

    static void install(AaCoverArt c) {
        instance = c;
    }

    /* ------------------------------------------------------------------ events (any thread) */

    boolean sessionActive() {
        synchronized (lock) {
            return session;
        }
    }

    Ref currentRef() {
        synchronized (lock) {
            return session ? current : null;
        }
    }

    DSIAndroidAuto2DefaultListener listener() {
        return listener;
    }

    /** DSI thread: only records the request; the worker does the I/O. */
    void onCoverArt(ResourceLocator loc, int valid) {
        String url = null;
        if (valid == 1 && loc != null) {
            url = loc.getUrl();
            if (url != null && url.length() == 0) url = null;
        }
        boolean active;
        synchronized (lock) {
            requestUrl = url;
            requestSeq++;
            hasRequest = true;
            active = session;
            lock.notifyAll();
        }
        log("DSI coverArtUrl valid=" + valid + " url=" + url + (active ? "" : " (no Android Auto session yet: held)"));
    }

    void onSession(boolean on) {
        Ref dropped = null;
        synchronized (lock) {
            if (on == session) return;
            session = on;
            sessionGen++;
            if (!on) {
                dropped = current;
                current = null;
                hasRequest = false;
                requestUrl = null;
            }
            lock.notifyAll();
        }
        if (on) {
            log("Android Auto session start (gen " + sessionGen + ")");
        } else {
            log("Android Auto session end: art dropped" + (dropped != null ? " (" + dropped + ")" : ""));
            notifyHost();
        }
    }

    private void notifyHost() {
        final Host h = host;
        if (h == null) return;
        try {
            poster.post(new Runnable() {
                public void run() {
                    try {
                        h.coverArtChanged();
                    } catch (Throwable t) {
                        log("host update failed: " + t);
                    }
                }
            });
        } catch (Throwable t) {
            log("dispatcher post failed: " + t);
        }
    }

    /* ------------------------------------------------------------------ worker */

    void start() {
        Thread t = new Thread("sq5-aa-coverart") {
            public void run() {
                while (true) {
                    try {
                        step();
                    } catch (InterruptedException e) {
                        return;
                    } catch (Throwable e) {
                        log("worker: " + e + at(e));
                    }
                }
            }
        };
        t.setDaemon(true);
        worker = t;
        t.start();
    }

    void stop() {
        Thread t = worker;
        if (t != null) t.interrupt();
    }

    /** First stack frames of e (J9 exceptions from java.util.zip often carry no message). */
    static String at(Throwable e) {
        try {
            StackTraceElement[] st = e.getStackTrace();
            if (st == null || st.length == 0) return " (no stack trace)";
            StringBuffer b = new StringBuffer(" at ");
            for (int i = 0; i < st.length && i < 6; i++) {
                if (i > 0) b.append(" < ");
                b.append(st[i].getClassName()).append('.').append(st[i].getMethodName())
                    .append(':').append(st[i].getLineNumber());
            }
            Throwable cause = e.getCause();
            if (cause != null && cause != e) b.append(" caused by ").append(cause);
            return b.toString();
        } catch (Throwable t) {
            return "";
        }
    }

    /** One request: wait for it, process it, publish if still current. */
    void step() throws InterruptedException {
        String url;
        long seq;
        int gen;
        synchronized (lock) {
            while (!(hasRequest && session)) {
                if (!session && files.size() > 0) break;
                lock.wait();
            }
            if (!session) {
                gen = -1;
                url = null;
                seq = 0;
            } else {
                url = requestUrl;
                seq = requestSeq;
                gen = sessionGen;
                hasRequest = false;
            }
        }
        if (gen == -1) {           /* session ended: remove our files */
            deleteFiles(null);
            return;
        }
        if (url == null) {
            if (!waitQuiet(CLEAR_GRACE_MS, gen, seq)) return;   /* superseded by newer art */
            publish(null, gen, seq, "phone reports no art");
            return;
        }
        Ref r = null;
        String why = null;
        for (int attempt = 0; r == null; attempt++) {
            try {
                r = process(url);
            } catch (PngCover.Rejected e) {
                why = "rejected: " + e.getMessage();
                break;
            } catch (Throwable e) {
                why = "unreadable: " + e + at(e);
                if (attempt >= RETRY_MS.length) break;
                log(url + " " + why + " - retry " + (attempt + 1) + "/" + RETRY_MS.length + " in " + RETRY_MS[attempt]
                    + " ms");
                if (!waitQuiet(RETRY_MS[attempt], gen, seq)) return;
            }
        }
        if (r == null) nFail++;
        publish(r, gen, seq, r == null ? why : null);
    }

    /** false if a newer request or a session change arrived while waiting. */
    private boolean waitQuiet(long ms, int gen, long seq) throws InterruptedException {
        long end = System.currentTimeMillis() + ms;
        synchronized (lock) {
            while (true) {
                if (gen != sessionGen || !session || seq != requestSeq) return false;
                long left = end - System.currentTimeMillis();
                if (left <= 0) return true;
                lock.wait(left);
            }
        }
    }

    private void publish(Ref r, int gen, long seq, String why) {
        Ref old;
        boolean ok;
        synchronized (lock) {
            ok = gen == sessionGen && session && seq == requestSeq;
            old = current;
            if (ok) current = r;
        }
        if (!ok) {
            log("result dropped (superseded)" + (r != null ? ": " + r : ""));
            if (r != null && r != old) forgetFile(r);
            return;
        }
        if (r == null) {
            nClear++;
            log("art cleared" + (why != null ? " (" + why + ")" : "") + " art=" + nArt + " clears=" + nClear
                + " fails=" + nFail);
        } else if (r != old) {
            nArt++;
            log("art ready " + r);
        }
        if (r != old) {
            deleteFiles(r);
            notifyHost();
        }
    }

    /**
     * gal file -> our normalised file.  Throws Rejected (budget/format: final) or IOException /
     * RuntimeException (retryable: missing, short, truncated, still being written, decoder).
     */
    Ref process(String url) throws IOException {
        long t0 = System.currentTimeMillis();
        File f = new File(url);
        if (!f.exists()) throw new IOException("missing");
        if (!f.isFile() || !f.canRead()) throw new IOException("not a readable file");
        long len = f.length();
        long mtime = f.lastModified();
        if (len <= 0) throw new IOException("empty");
        if (len < MIN_FILE_BYTES) throw new IOException("only " + len + " bytes (still being written?)");
        if (len > PngCover.MAX_FILE_BYTES) throw new PngCover.Rejected("file too large: " + len + " bytes");
        byte[] head = new byte[MIN_FILE_BYTES];
        InputStream in = new FileInputStream(f);
        try {
            PngCover.readFully(in, head, 0, head.length);
        } finally {
            close(in);
        }
        String kind = PngCover.sniff(head, head.length);
        byte[] out;
        String ext;
        String what;
        if (PngCover.isPng(head)) {
            int[] info = new int[2];
            StringBuffer how = new StringBuffer();
            in = new BufferedInputStream(new FileInputStream(f), 8192);
            try {
                out = PngCover.toCover(in, info, how);
            } finally {
                close(in);
            }
            ext = ".png";
            what = "PNG " + info[0] + "x" + info[1] + " -> " + PngCover.OUT + "x" + PngCover.OUT + " " + how;
        } else if (PngCover.isJpeg(head)) {
            /* No JPEG decoder on J9: hand a small baseline-sized JPEG over unchanged (the stock
             * media path feeds the picture server JPEG covers), refuse anything larger. */
            if (len > JPEG_MAX_BYTES) throw new PngCover.Rejected("JPEG too large to pass through: " + len + " bytes");
            int[] wh;
            in = new BufferedInputStream(new FileInputStream(f), 4096);
            try {
                wh = PngCover.jpegSize(in, len);
            } finally {
                close(in);
            }
            if (wh == null) throw new IOException("JPEG without frame header (truncated?)");
            if (wh[0] <= 0 || wh[1] <= 0 || wh[0] > JPEG_MAX_SIDE || wh[1] > JPEG_MAX_SIDE) {
                throw new PngCover.Rejected("JPEG " + wh[0] + "x" + wh[1] + " exceeds " + JPEG_MAX_SIDE + " px pass-through limit");
            }
            out = new byte[(int) len];
            in = new FileInputStream(f);
            try {
                PngCover.readFully(in, out, 0, out.length);
            } finally {
                close(in);
            }
            if (!PngCover.jpegComplete(out, out.length)) throw new IOException("JPEG incomplete (no EOI marker)");
            ext = ".jpg";
            what = "JPEG " + wh[0] + "x" + wh[1] + " passed through";
        } else {
            boolean zeros = true;
            for (int i = 0; i < head.length; i++) zeros &= head[i] == 0;
            if (zeros) throw new IOException("header still zero (still being written?)");
            throw new PngCover.Rejected(kind.startsWith("unknown") ? "unknown image format " + kind.substring(8)
                : "unsupported image format " + kind + " (PNG is converted, JPEG passed through)");
        }
        long len2 = f.length();
        long mtime2 = f.lastModified();
        if (len2 != len || mtime2 != mtime) {
            throw new IOException("file changed while reading (" + len + " -> " + len2 + " bytes): still being written");
        }
        long c = PngCover.crc32(0, out, 0, out.length) & 0xFFFFFFFFL;
        Ref cur = current;
        if (cur != null && cur.crc == c) {
            nDedup++;
            log(url + ": same picture as current (" + cur + "), kept");
            return cur;
        }
        int id = (int) (c & 0x7FFFFFFFL);
        if (id == lastId) id = (id + 1) & 0x7FFFFFFF;   /* same art after a clear: new id -> VC refresh */
        if (id == 0) id = 1;
        lastId = id;
        if (!dir.exists()) dir.mkdirs();
        File dst = new File(dir, PREFIX + Integer.toHexString(id) + ext);
        /* Written in place, no temp + rename: MU0918 /tmp is /dev/shmem, where rename always fails
         * (car log 43).  Nothing reads dst before we return: the VC only learns the name from the
         * Ref below, and every new picture gets a new name. */
        if (dst.exists()) dst.delete();
        FileOutputStream fo = new FileOutputStream(dst);
        boolean written = false;
        try {
            fo.write(out);
            written = true;
        } finally {
            close(fo);
            if (!written) dst.delete();
        }
        for (int i = files.size() - 1; i >= 0; i--) {   /* A -> B -> A reuses A's name */
            if (((File) files.elementAt(i)).getPath().equals(dst.getPath())) files.removeElementAt(i);
        }
        files.addElement(dst);
        log(url + ": " + kind + " file " + len + " bytes; " + what + ", " + out.length + " bytes, "
            + (System.currentTimeMillis() - t0) + " ms");
        return new Ref(id, dst.getPath(), c);
    }

    /** Keep the current picture and the one before it (the VC may still be fetching it). */
    private void deleteFiles(Ref keep) {
        int limit = keep == null ? 0 : KEEP_FILES;
        int i = 0;
        while (files.size() > limit && i < files.size()) {
            File f = (File) files.elementAt(i);
            if (keep != null && f.getPath().equals(keep.path)) {
                i++;
                continue;
            }
            files.removeElementAt(i);
            try {
                f.delete();
            } catch (Throwable t) {
                /* ignore */
            }
        }
    }

    private void forgetFile(Ref r) {
        for (int i = files.size() - 1; i >= 0; i--) {
            File f = (File) files.elementAt(i);
            if (f.getPath().equals(r.path)) {
                files.removeElementAt(i);
                try {
                    f.delete();
                } catch (Throwable t) {
                    /* ignore */
                }
            }
        }
    }

    /** Boot: remove leftovers of an earlier HMI run. */
    private void cleanDir() {
        try {
            File[] l = dir.listFiles();
            for (int i = 0; l != null && i < l.length; i++) {
                if (l[i].getName().startsWith(PREFIX)) l[i].delete();
            }
        } catch (Throwable t) {
            /* ignore */
        }
    }

    private static void close(InputStream in) {
        try {
            in.close();
        } catch (Throwable t) {
            /* ignore */
        }
    }

    private static void close(FileOutputStream o) {
        try {
            o.close();
        } catch (Throwable t) {
            /* ignore */
        }
    }

    /* ------------------------------------------------------------------ session predicates */

    static boolean sessionState(TMDevice d) {
        if (d == null || !d.isAndroidAutoDevice()) return false;
        TMDevice.ConnectionState s = d.connectionState();
        return s != null && (s.is(TMDevice.ConnectionState.ACTIVATING) || s.is(TMDevice.ConnectionState.ACTIVE));
    }

    static boolean sessionGone(TMDevice d) {
        if (d == null || !d.isAndroidAutoDevice()) return true;
        TMDevice.ConnectionState s = d.connectionState();
        return s == null || s.is(TMDevice.ConnectionState.INVALID) || s.is(TMDevice.ConnectionState.NOT_ATTACHED)
            || s.is(TMDevice.ConnectionState.ATTACHED);
    }

    /** DSI cover-art listener + terminal-mode active-device listener. */
    private final class Listener extends DSIAndroidAuto2DefaultListener implements IActiveDeviceStateListener {
        public void updateCoverArtUrl(ResourceLocator loc, int valid) {
            try {
                onCoverArt(loc, valid);
            } catch (Throwable t) {
                log("coverArtUrl handler failed: " + t);
            }
        }

        public void updateActiveDeviceState(TMDevice d) {
            try {
                boolean was = sessionActive();
                if (!was && sessionState(d)) onSession(true);
                else if (was && sessionGone(d)) onSession(false);
            } catch (Throwable t) {
                log("device state handler failed: " + t);
            }
        }
    }
}
