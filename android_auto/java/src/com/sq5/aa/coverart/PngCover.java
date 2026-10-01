/*
 * Cover-art normaliser for the SQ5 Android Auto -> Virtual Cockpit cover art (luka-dev port).
 *
 * luka's CarPlay hook hands the VC picture manager a 256x256 8-bit RGB non-interlaced PNG
 * ("contain" fit on a black canvas).  Android Auto's gal writes whatever the phone sent to
 * /tmp/gal_albumArt_<n>.png, so this class turns that file into the same verified format.
 *
 * Pure Java for the unit's IBM J9 (no java.awt / javax.imageio there; java.util.zip only),
 * Java 1.4 source level, bounded memory: the PNG is decoded as a stream, one scan line at a
 * time, and box-filtered straight into the 256x256 output, so a 4096x4096 source costs two
 * row buffers (~32 KiB) plus the 256x256 canvas - never the full bitmap (J9 heap is -Xmx60m).
 *
 * Limits (checked on the IHDR, before any pixel is decoded): <= 4096 px per side,
 * <= 8 Mpx total, <= 4 MiB file, <= 5 s decode wall time (checked every 16 rows).
 * Supported: every non-interlaced PNG colour type / bit depth (gray 1-16, RGB 8/16, palette
 * 1-8 incl. tRNS alpha, gray+alpha 8/16, RGBA 8/16); alpha is composited over black.
 * Adam7-interlaced PNGs are rejected (Android's Bitmap.compress never interlaces).
 * JPEG is not decoded here; see jpegSize() for the pass-through decision.
 *
 * J9 class library (MU0918 lsd.jxe: jclFoundation11, com.ibm.oti.jcl.build 20070313) - the
 * java.util.zip classes there are IBM's pre-Harmony ones, NOT the Sun/OpenJDK ones the PC runs.
 * Differences that matter here (read from the ROM classes in lsd.jxe):
 *  - Inflater.needsDictionary() throws IllegalStateException until setInput() was called
 *    (the car's "unreadable: java.lang.IllegalStateException": the old loop asked it on the
 *    first, input-less iteration).  It is never called now; a preset dictionary is refused
 *    from the zlib header instead (PNG never uses one).
 *  - Inflater.inflate() has no "needsInput() -> return 0" guard: it goes straight to native
 *    zlib, which may report a buffer error without input.  inflate() is only called with input
 *    pending, except one final drain call after the last IDAT byte (errors there = truncated).
 *  - Inflater/Deflater methods other than finished()/needsInput()/getRemaining() throw
 *    IllegalStateException after end(); end() is called exactly once, in finally.
 *  - Deflater.setLevel/setStrategy throw IllegalStateException after setInput (not used).
 * Encoding never depends on Deflater: its output is self-checked (zlib header + Adler-32
 * trailer computed here) and any failure falls back to encodeStored() (uncompressed deflate
 * blocks, hand-written Adler-32 and CRC-32: no java.util.zip at all, ~197 KB per picture).
 */
package com.sq5.aa.coverart;

import java.io.ByteArrayOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.Deflater;
import java.util.zip.Inflater;

public final class PngCover {
    public static final int OUT = 256;
    public static final int MAX_SIDE = 4096;
    public static final long MAX_PIXELS = 8L * 1024L * 1024L;
    public static final long MAX_FILE_BYTES = 4L * 1024L * 1024L;
    public static final long MAX_DECODE_MS = 5000L;

    /** Refused before decoding (size/format budget): retrying cannot help. */
    public static final class Rejected extends IOException {
        public Rejected(String why) {
            super(why);
        }
    }

    private PngCover() {
    }

    public static boolean isPng(byte[] h) {
        return h != null && h.length >= 8 && (h[0] & 0xFF) == 0x89 && h[1] == 'P' && h[2] == 'N' && h[3] == 'G'
            && h[4] == 0x0D && h[5] == 0x0A && h[6] == 0x1A && h[7] == 0x0A;
    }

    public static boolean isJpeg(byte[] h) {
        return h != null && h.length >= 3 && (h[0] & 0xFF) == 0xFF && (h[1] & 0xFF) == 0xD8 && (h[2] & 0xFF) == 0xFF;
    }

    /**
     * Image format from the first {@code n} bytes of {@code h}: "PNG", "JPEG", "WebP", "GIF",
     * "BMP", "HEIF/AVIF", or "unknown (xx xx ...)" with the leading bytes in hex.
     */
    public static String sniff(byte[] h, int n) {
        if (h == null) return "unknown (no data)";
        if (n > h.length) n = h.length;
        byte[] b = h;
        if (n < h.length) {
            b = new byte[n];
            System.arraycopy(h, 0, b, 0, n);
        }
        if (isPng(b)) return "PNG";
        if (isJpeg(b)) return "JPEG";
        if (n >= 12 && ascii(b, 0, "RIFF") && ascii(b, 8, "WEBP")) return "WebP";
        if (n >= 6 && (ascii(b, 0, "GIF87a") || ascii(b, 0, "GIF89a"))) return "GIF";
        if (n >= 2 && ascii(b, 0, "BM")) return "BMP";
        if (n >= 12 && ascii(b, 4, "ftyp")) return "HEIF/AVIF";
        StringBuffer s = new StringBuffer("unknown (");
        for (int i = 0; i < n && i < 12; i++) {
            if (i > 0) s.append(' ');
            int v = b[i] & 0xFF;
            s.append(Character.forDigit(v >> 4, 16)).append(Character.forDigit(v & 15, 16));
        }
        if (n == 0) s.append("empty");
        return s.append(')').toString();
    }

    private static boolean ascii(byte[] b, int off, String s) {
        if (off + s.length() > b.length) return false;
        for (int i = 0; i < s.length(); i++) {
            if (b[off + i] != (byte) s.charAt(i)) return false;
        }
        return true;
    }

    /** True if a complete JPEG ends here: EOI (FF D9) within the last 64 bytes (encoder padding). */
    public static boolean jpegComplete(byte[] j, int len) {
        int stop = len - 64 < 2 ? 2 : len - 64;
        for (int i = len - 2; i >= stop; i--) {
            if ((j[i] & 0xFF) == 0xFF && (j[i + 1] & 0xFF) == 0xD9) return true;
        }
        return false;
    }

    /* ------------------------------------------------------------------ PNG decode + scale */

    /**
     * Decode the PNG on {@code in} (positioned at the signature) and return a 256x256 RGB PNG.
     * {@code info[0..1]} (optional) receive the source width/height.
     */
    public static byte[] toCover(InputStream in, int[] info) throws IOException {
        return toCover(in, info, null);
    }

    /** As {@link #toCover(InputStream, int[])}; {@code note} (optional) receives how it was encoded. */
    public static byte[] toCover(InputStream in, int[] info, StringBuffer note) throws IOException {
        long deadline = System.currentTimeMillis() + MAX_DECODE_MS;
        byte[] sig = new byte[8];
        readFully(in, sig, 0, 8);
        if (!isPng(sig)) throw new Rejected("not a PNG");

        int w = -1, h = -1, depth = 0, ctype = 0, interlace = 0;
        byte[] palette = null;
        byte[] trns = null;
        byte[] hdr = new byte[8];
        while (true) {
            readFully(in, hdr, 0, 8);
            int len = be32(hdr, 0);
            String type = new String(new char[]{(char) hdr[4], (char) hdr[5], (char) hdr[6], (char) hdr[7]});
            if (len < 0 || len > MAX_FILE_BYTES) throw new Rejected("bad chunk length " + len);
            if (type.equals("IHDR")) {
                if (len != 13) throw new Rejected("bad IHDR");
                byte[] d = readChunk(in, len);
                w = be32(d, 0);
                h = be32(d, 4);
                depth = d[8] & 0xFF;
                ctype = d[9] & 0xFF;
                interlace = d[12] & 0xFF;
                if ((d[10] & 0xFF) != 0 || (d[11] & 0xFF) != 0) throw new Rejected("unknown compression/filter");
                checkBudget(w, h);
                if (!validDepth(ctype, depth)) throw new Rejected("bad colour type/depth " + ctype + "/" + depth);
                if (interlace != 0) throw new Rejected("interlaced PNG not supported");
                if (info != null && info.length >= 2) {
                    info[0] = w;
                    info[1] = h;
                }
            } else if (type.equals("PLTE")) {
                if (len % 3 != 0 || len > 768) throw new Rejected("bad PLTE");
                palette = readChunk(in, len);
            } else if (type.equals("tRNS")) {
                trns = len <= 256 ? readChunk(in, len) : null;
                if (trns == null) skip(in, len + 4);
            } else if (type.equals("IDAT")) {
                if (w < 0) throw new Rejected("IDAT before IHDR");
                if (ctype == 3 && palette == null) throw new Rejected("palette image without PLTE");
                IdatStream idat = new IdatStream(in, len);
                return decode(idat, w, h, depth, ctype, palette, ctype == 3 ? trns : null, deadline, note);
            } else if (type.equals("IEND")) {
                throw new Rejected("no IDAT");
            } else {
                skip(in, len + 4L);   /* data + CRC */
            }
        }
    }

    private static void checkBudget(int w, int h) throws Rejected {
        if (w <= 0 || h <= 0) throw new Rejected("bad size " + w + "x" + h);
        if (w > MAX_SIDE || h > MAX_SIDE) throw new Rejected("too large " + w + "x" + h + " (max " + MAX_SIDE + "/side)");
        if ((long) w * (long) h > MAX_PIXELS) throw new Rejected("too many pixels " + w + "x" + h);
    }

    private static boolean validDepth(int ct, int d) {
        switch (ct) {
            case 0: return d == 1 || d == 2 || d == 4 || d == 8 || d == 16;
            case 3: return d == 1 || d == 2 || d == 4 || d == 8;
            case 2: case 4: case 6: return d == 8 || d == 16;
            default: return false;
        }
    }

    private static int channels(int ct) {
        switch (ct) {
            case 0: return 1;
            case 2: return 3;
            case 3: return 1;
            case 4: return 2;
            default: return 4;
        }
    }

    private static byte[] decode(IdatStream idat, int w, int h, int depth, int ctype, byte[] palette, byte[] trns,
                                 long deadline, StringBuffer note) throws IOException {
        int ch = channels(ctype);
        int bitsPerPixel = ch * depth;
        int bpp = Math.max(1, bitsPerPixel / 8);
        int rowBytes = (int) (((long) w * bitsPerPixel + 7) / 8);

        /* contain fit */
        int nw, nh;
        if (w >= h) {
            nw = OUT;
            nh = (int) (((long) h * OUT + w / 2) / w);
        } else {
            nh = OUT;
            nw = (int) (((long) w * OUT + h / 2) / h);
        }
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        boolean down = nw <= w && nh <= h;
        int x0 = (OUT - nw) / 2;
        int y0 = (OUT - nh) / 2;
        byte[] canvas = new byte[OUT * OUT * 3];   /* black */

        int[] rgb = new int[w];                   /* one decoded row, 0x00RRGGBB */
        int[] full = down ? null : new int[w * h];  /* upscale path only: w,h < 256 */
        int[] sr = down ? new int[nw] : null, sg = null, sb = null, cnt = null;
        int[] colMap = null;
        if (down) {
            sg = new int[nw];
            sb = new int[nw];
            cnt = new int[nw];
            colMap = new int[w];
            for (int x = 0; x < w; x++) colMap[x] = (int) ((long) x * nw / w);
        }
        int band = 0;

        Inflater inf = newInflater();
        try {
            byte[] prev = new byte[rowBytes];
            byte[] cur = new byte[rowBytes];
            byte[] ftype = new byte[1];
            byte[] inBuf = new byte[4096];
            /* zlib header first (J9: never ask the Inflater about dictionaries, see header) */
            int got = 0;
            while (got < 2) {
                int r = idat.read(inBuf, got, inBuf.length - got);
                if (r <= 0) throw new EOFException("image data truncated (no zlib header)");
                got += r;
            }
            int cmf = inBuf[0] & 0xFF, flg = inBuf[1] & 0xFF;
            if ((cmf & 0x0F) != 8 || (cmf >> 4) > 7 || ((cmf << 8) | flg) % 31 != 0) {
                throw new IOException("bad zlib header " + Integer.toHexString(cmf) + " " + Integer.toHexString(flg));
            }
            if ((flg & 0x20) != 0) throw new Rejected("zlib preset dictionary not supported");
            inf.setInput(inBuf, 0, got);
            for (int y = 0; y < h; y++) {
                if ((y & 15) == 0 && System.currentTimeMillis() > deadline) {
                    throw new IOException("decode time budget exceeded at row " + y + "/" + h);
                }
                inflateFully(inf, idat, inBuf, ftype, 1);
                inflateFully(inf, idat, inBuf, cur, rowBytes);
                unfilter(ftype[0] & 0xFF, cur, prev, bpp);
                toRgb(cur, rgb, w, depth, ctype, palette, trns);
                if (down) {
                    int dy = (int) ((long) y * nh / h);
                    if (dy != band) {
                        flush(canvas, band + y0, x0, sr, sg, sb, cnt);
                        band = dy;
                    }
                    for (int x = 0; x < w; x++) {
                        int c = rgb[x];
                        int dx = colMap[x];
                        sr[dx] += (c >> 16) & 0xFF;
                        sg[dx] += (c >> 8) & 0xFF;
                        sb[dx] += c & 0xFF;
                        cnt[dx]++;
                    }
                } else {
                    System.arraycopy(rgb, 0, full, y * w, w);
                }
                byte[] t = prev;
                prev = cur;
                cur = t;
            }
        } finally {
            try {
                inf.end();   /* native zlib memory: release now, not at GC; exactly once */
            } catch (Throwable t) {
                /* never mask the decode result */
            }
        }
        if (down) {
            flush(canvas, band + y0, x0, sr, sg, sb, cnt);
        } else {
            upscale(full, w, h, canvas, x0, y0, nw, nh);
        }
        return encode(canvas, OUT, OUT, note);
    }

    private static void flush(byte[] canvas, int row, int x0, int[] sr, int[] sg, int[] sb, int[] cnt) {
        int base = (row * OUT + x0) * 3;
        for (int i = 0; i < cnt.length; i++) {
            int n = cnt[i];
            if (n > 0) {
                canvas[base + i * 3] = (byte) ((sr[i] + n / 2) / n);
                canvas[base + i * 3 + 1] = (byte) ((sg[i] + n / 2) / n);
                canvas[base + i * 3 + 2] = (byte) ((sb[i] + n / 2) / n);
            }
            sr[i] = 0;
            sg[i] = 0;
            sb[i] = 0;
            cnt[i] = 0;
        }
    }

    /** Bilinear enlarge of a small source (both sides < 256) into the canvas. */
    private static void upscale(int[] src, int w, int h, byte[] canvas, int x0, int y0, int nw, int nh) {
        for (int dy = 0; dy < nh; dy++) {
            /* 16.16 fixed point, pixel centres aligned */
            long fy = (((long) dy * 2 + 1) * h * 65536L) / (2L * nh) - 32768L;
            if (fy < 0) fy = 0;
            int yA = (int) (fy >> 16);
            int wy = (int) (fy & 0xFFFF);
            int yB = yA + 1 < h ? yA + 1 : h - 1;
            for (int dx = 0; dx < nw; dx++) {
                long fx = (((long) dx * 2 + 1) * w * 65536L) / (2L * nw) - 32768L;
                if (fx < 0) fx = 0;
                int xA = (int) (fx >> 16);
                int wx = (int) (fx & 0xFFFF);
                int xB = xA + 1 < w ? xA + 1 : w - 1;
                int p00 = src[yA * w + xA], p01 = src[yA * w + xB], p10 = src[yB * w + xA], p11 = src[yB * w + xB];
                int o = ((y0 + dy) * OUT + x0 + dx) * 3;
                for (int s = 16, k = 0; s >= 0; s -= 8, k++) {
                    long a = (p00 >> s) & 0xFF, b = (p01 >> s) & 0xFF, c = (p10 >> s) & 0xFF, d = (p11 >> s) & 0xFF;
                    long top = a * (65536L - wx) + b * wx;
                    long bot = c * (65536L - wx) + d * wx;
                    long v = (top * (65536L - wy) + bot * wy + (1L << 31)) >> 32;
                    canvas[o + k] = (byte) (v > 255 ? 255 : v);
                }
            }
        }
    }

    private static void unfilter(int f, byte[] cur, byte[] prev, int bpp) throws IOException {
        int n = cur.length;
        switch (f) {
            case 0:
                return;
            case 1:
                for (int i = bpp; i < n; i++) cur[i] = (byte) (cur[i] + cur[i - bpp]);
                return;
            case 2:
                for (int i = 0; i < n; i++) cur[i] = (byte) (cur[i] + prev[i]);
                return;
            case 3:
                for (int i = 0; i < n; i++) {
                    int left = i >= bpp ? cur[i - bpp] & 0xFF : 0;
                    cur[i] = (byte) (cur[i] + ((left + (prev[i] & 0xFF)) >> 1));
                }
                return;
            case 4:
                for (int i = 0; i < n; i++) {
                    int a = i >= bpp ? cur[i - bpp] & 0xFF : 0;
                    int b = prev[i] & 0xFF;
                    int c = i >= bpp ? prev[i - bpp] & 0xFF : 0;
                    int p = a + b - c;
                    int pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
                    int pr = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    cur[i] = (byte) (cur[i] + pr);
                }
                return;
            default:
                throw new IOException("bad PNG filter type " + f);
        }
    }

    /** One unfiltered scan line -> 0x00RRGGBB, alpha composited over black. */
    private static void toRgb(byte[] row, int[] out, int w, int depth, int ctype, byte[] pal, byte[] trns) {
        int step = depth == 16 ? 2 : 1;
        switch (ctype) {
            case 0: {
                if (depth >= 8) {
                    for (int x = 0; x < w; x++) {
                        int g = row[x * step] & 0xFF;
                        out[x] = (g << 16) | (g << 8) | g;
                    }
                } else {
                    int max = (1 << depth) - 1;
                    for (int x = 0; x < w; x++) {
                        int g = sample(row, x, depth) * 255 / max;
                        out[x] = (g << 16) | (g << 8) | g;
                    }
                }
                return;
            }
            case 2:
                for (int x = 0; x < w; x++) {
                    int o = x * 3 * step;
                    out[x] = ((row[o] & 0xFF) << 16) | ((row[o + step] & 0xFF) << 8) | (row[o + 2 * step] & 0xFF);
                }
                return;
            case 3: {
                int entries = pal.length / 3;
                for (int x = 0; x < w; x++) {
                    int i = depth == 8 ? row[x] & 0xFF : sample(row, x, depth);
                    if (i >= entries) {
                        out[x] = 0;
                        continue;
                    }
                    int r = pal[i * 3] & 0xFF, g = pal[i * 3 + 1] & 0xFF, b = pal[i * 3 + 2] & 0xFF;
                    if (trns != null && i < trns.length) {
                        int a = trns[i] & 0xFF;
                        r = r * a / 255;
                        g = g * a / 255;
                        b = b * a / 255;
                    }
                    out[x] = (r << 16) | (g << 8) | b;
                }
                return;
            }
            case 4:
                for (int x = 0; x < w; x++) {
                    int o = x * 2 * step;
                    int g = (row[o] & 0xFF) * (row[o + step] & 0xFF) / 255;
                    out[x] = (g << 16) | (g << 8) | g;
                }
                return;
            default:
                for (int x = 0; x < w; x++) {
                    int o = x * 4 * step;
                    int a = row[o + 3 * step] & 0xFF;
                    int r = (row[o] & 0xFF) * a / 255;
                    int g = (row[o + step] & 0xFF) * a / 255;
                    int b = (row[o + 2 * step] & 0xFF) * a / 255;
                    out[x] = (r << 16) | (g << 8) | b;
                }
        }
    }

    private static int sample(byte[] row, int x, int depth) {
        int bit = x * depth;
        int v = row[bit >> 3] & 0xFF;
        int shift = 8 - depth - (bit & 7);
        return (v >> shift) & ((1 << depth) - 1);
    }

    /** Test hook: the Inflater used by decode (host tests substitute one with J9 semantics). */
    interface InflaterFactory {
        Inflater create();
    }

    static volatile InflaterFactory inflaterFactory;

    private static Inflater newInflater() {
        InflaterFactory f = inflaterFactory;
        return f != null ? f.create() : new Inflater();
    }

    /**
     * Inflate exactly {@code len} bytes into {@code dst}.  J9-safe call order: setInput before
     * inflate whenever the Inflater has consumed its input; inflate without pending input only
     * as the drain after the last IDAT byte (zlib may still hold output of a long match).
     * A zlib error with input pending = corrupt data; without = truncated data.
     */
    private static void inflateFully(Inflater inf, IdatStream idat, byte[] inBuf, byte[] dst, int len)
            throws IOException {
        int off = 0;
        int idle = 0;
        while (off < len) {
            boolean draining = false;
            if (inf.needsInput()) {
                int r = idat.read(inBuf, 0, inBuf.length);
                if (r > 0) inf.setInput(inBuf, 0, r);
                else draining = true;
            }
            int n;
            try {
                n = inf.inflate(dst, off, len - off);
            } catch (Exception e) {
                /* DataFormatException (declared on HotSpot; the J9 ROM class carries no throws
                 * clause) or anything else the native side raises: data problem, never a crash */
                if (draining) throw new EOFException("image data truncated (" + e + ")");
                throw new IOException("corrupt image data: " + e);
            }
            if (n > 0) {
                off += n;
                idle = 0;
                continue;
            }
            if (inf.finished()) throw new EOFException("image data ends early");
            if (draining) throw new EOFException("image data truncated");
            /* no output with input pending: zlib is consuming headers; a real stall is corrupt data */
            if (++idle > 64) throw new IOException("inflater stalled (no output, " + inf.getRemaining() + " bytes pending)");
        }
    }

    /** IDAT payload across consecutive IDAT chunks (chunk CRCs skipped). */
    private static final class IdatStream {
        private final InputStream in;
        private int left;
        private boolean done;

        IdatStream(InputStream in, int firstLen) {
            this.in = in;
            this.left = firstLen;
        }

        int read(byte[] b, int off, int len) throws IOException {
            while (left == 0) {
                if (done) return -1;
                skip(in, 4);   /* CRC of the finished chunk */
                byte[] hdr = new byte[8];
                readFully(in, hdr, 0, 8);
                int l = be32(hdr, 0);
                if (hdr[4] == 'I' && hdr[5] == 'D' && hdr[6] == 'A' && hdr[7] == 'T' && l >= 0) {
                    left = l;
                } else {
                    done = true;
                    return -1;
                }
            }
            int n = in.read(b, off, Math.min(len, left));
            if (n <= 0) throw new EOFException("PNG truncated inside IDAT");
            left -= n;
            return n;
        }
    }

    /* ------------------------------------------------------------------ PNG encode */

    /** Stored-deflate IDAT chunk size (libpng's default layout: many small IDATs). */
    static final int STORED_IDAT_CHUNK = 8192;

    /** Test hook: thrown inside the Deflater path (after setInput) to exercise the fallback. */
    static volatile RuntimeException deflateFault;

    /** 8-bit RGB, non-interlaced, filter 0 rows. */
    public static byte[] encode(byte[] rgb, int w, int h) throws IOException {
        return encode(rgb, w, h, null);
    }

    /**
     * Deflater first (small file, the format luka's hook produces); any Deflater failure or
     * a zlib stream that fails its self-check -> encodeStored(), which cannot fail on J9.
     * {@code note} (optional) receives "deflate" or "stored (reason)".
     */
    public static byte[] encode(byte[] rgb, int w, int h, StringBuffer note) throws IOException {
        byte[] raw = rawRows(rgb, w, h);
        byte[] z;
        try {
            z = zlibDeflate(raw);
        } catch (Throwable t) {
            if (t instanceof OutOfMemoryError) throw (OutOfMemoryError) t;
            if (note != null) note.append("stored (Deflater failed: ").append(describe(t)).append(')');
            return encodeStored(rgb, w, h);
        }
        if (note != null) note.append("deflate");
        ByteArrayOutputStream out = new ByteArrayOutputStream(z.length + 64);
        header(out, w, h);
        chunk(out, "IDAT", z, 0, z.length);
        chunk(out, "IEND", new byte[0], 0, 0);
        return out.toByteArray();
    }

    /**
     * 8-bit RGB PNG with zlib STORED blocks (no compression): pure Java, no java.util.zip.
     * Size for 256x256: 196,864 raw bytes + 5 per 64 KiB block + zlib/PNG framing.
     */
    public static byte[] encodeStored(byte[] rgb, int w, int h) {
        byte[] raw = rawRows(rgb, w, h);
        int blocks = (raw.length + 65534) / 65535;
        if (blocks == 0) blocks = 1;
        byte[] z = new byte[2 + blocks * 5 + raw.length + 4];
        z[0] = 0x78;   /* CM 8, 32K window */
        z[1] = 0x01;   /* FLEVEL 0, no dict, (0x7801 % 31 == 0) */
        int o = 2, p = 0;
        for (int b = 0; b < blocks; b++) {
            int n = Math.min(65535, raw.length - p);
            z[o++] = (byte) (b == blocks - 1 ? 1 : 0);   /* BFINAL, BTYPE 00 */
            z[o++] = (byte) n;
            z[o++] = (byte) (n >>> 8);
            z[o++] = (byte) ~n;
            z[o++] = (byte) ((~n) >>> 8);
            System.arraycopy(raw, p, z, o, n);
            o += n;
            p += n;
        }
        put32(z, o, adler32(raw, 0, raw.length));
        ByteArrayOutputStream out = new ByteArrayOutputStream(z.length + (z.length / STORED_IDAT_CHUNK + 4) * 12 + 64);
        header(out, w, h);
        for (int i = 0; i < z.length; i += STORED_IDAT_CHUNK) {
            chunk(out, "IDAT", z, i, Math.min(STORED_IDAT_CHUNK, z.length - i));
        }
        chunk(out, "IEND", new byte[0], 0, 0);
        return out.toByteArray();
    }

    private static byte[] rawRows(byte[] rgb, int w, int h) {
        byte[] raw = new byte[h * (w * 3 + 1)];   /* filter byte 0 per row */
        for (int y = 0; y < h; y++) {
            System.arraycopy(rgb, y * w * 3, raw, y * (w * 3 + 1) + 1, w * 3);
        }
        return raw;
    }

    /**
     * java.util.zip.Deflater, used the way both class libraries accept it: fresh instance,
     * one setInput with everything, finish, deflate until finished (bounded), end once.
     * The result is checked here (zlib header, Adler-32 trailer) instead of trusting it.
     */
    static byte[] zlibDeflate(byte[] raw) throws IOException {
        Deflater def = new Deflater(6);
        ByteArrayOutputStream z = new ByteArrayOutputStream(64 * 1024);
        try {
            def.setInput(raw, 0, raw.length);
            RuntimeException fault = deflateFault;
            if (fault != null) throw fault;
            def.finish();
            byte[] buf = new byte[16384];
            int idle = 0;
            while (!def.finished()) {
                int n = def.deflate(buf, 0, buf.length);
                if (n > 0) {
                    z.write(buf, 0, n);
                    idle = 0;
                } else if (++idle > 16) {
                    throw new IOException("Deflater stalled before finishing");
                }
                if (z.size() > raw.length + raw.length / 8 + 1024) throw new IOException("Deflater output runaway");
            }
        } finally {
            try {
                def.end();
            } catch (Throwable t) {
                /* ignore */
            }
        }
        byte[] b = z.toByteArray();
        int n = b.length;
        if (n < 6 || (b[0] & 0x0F) != 8 || (((b[0] & 0xFF) << 8) | (b[1] & 0xFF)) % 31 != 0 || (b[1] & 0x20) != 0) {
            throw new IOException("Deflater output: bad zlib header");
        }
        if (be32(b, n - 4) != adler32(raw, 0, raw.length)) throw new IOException("Deflater output: Adler-32 mismatch");
        return b;
    }

    /** Exception class, message and first frames (J9 exceptions often carry no message). */
    static String describe(Throwable t) {
        StringBuffer s = new StringBuffer(String.valueOf(t));
        try {
            StackTraceElement[] st = t.getStackTrace();
            for (int i = 0; st != null && i < st.length && i < 4; i++) {
                s.append(i == 0 ? " at " : " < ").append(st[i].getClassName()).append('.')
                    .append(st[i].getMethodName()).append(':').append(st[i].getLineNumber());
            }
        } catch (Throwable e) {
            /* ignore */
        }
        return s.toString();
    }

    private static void header(ByteArrayOutputStream out, int w, int h) {
        byte[] sig = new byte[]{(byte) 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        out.write(sig, 0, 8);
        byte[] ihdr = new byte[13];
        put32(ihdr, 0, w);
        put32(ihdr, 4, h);
        ihdr[8] = 8;   /* depth */
        ihdr[9] = 2;   /* RGB; compression, filter, interlace 0 */
        chunk(out, "IHDR", ihdr, 0, 13);
    }

    private static void chunk(ByteArrayOutputStream out, String type, byte[] data, int off, int len) {
        byte[] b = new byte[4];
        put32(b, 0, len);
        out.write(b, 0, 4);
        byte[] t = new byte[]{(byte) type.charAt(0), (byte) type.charAt(1), (byte) type.charAt(2), (byte) type.charAt(3)};
        out.write(t, 0, 4);
        out.write(data, off, len);
        int c = crc32(0, t, 0, 4);
        c = crc32(c, data, off, len);
        put32(b, 0, c);
        out.write(b, 0, 4);
    }

    /* ------------------------------------------------------------------ checksums (pure Java) */

    private static final int[] CRC_TABLE = new int[256];

    static {
        for (int n = 0; n < 256; n++) {
            int c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
            CRC_TABLE[n] = c;
        }
    }

    /** CRC-32 (ISO-HDLC, as PNG/zip) continuing from {@code crc} (0 to start). */
    public static int crc32(int crc, byte[] b, int off, int len) {
        int c = ~crc;
        for (int i = off, end = off + len; i < end; i++) c = CRC_TABLE[(c ^ b[i]) & 0xFF] ^ (c >>> 8);
        return ~c;
    }

    /** Adler-32 as in the zlib trailer. */
    public static int adler32(byte[] b, int off, int len) {
        long a = 1, s = 0;   /* long: zlib's NMAX bound assumes unsigned 32-bit sums */
        while (len > 0) {
            int n = len < 5552 ? len : 5552;
            len -= n;
            while (n-- > 0) {
                a += b[off++] & 0xFF;
                s += a;
            }
            a %= 65521;
            s %= 65521;
        }
        return (int) ((s << 16) | a);
    }

    /* ------------------------------------------------------------------ JPEG header */

    /** {width, height} from the first SOFn marker, or null (not JPEG / no SOF / malformed). */
    public static int[] jpegSize(InputStream in, long maxScan) throws IOException {
        int a = in.read(), b = in.read();
        if (a != 0xFF || b != 0xD8) return null;
        long pos = 2;
        while (pos < maxScan) {
            int m = in.read();
            pos++;
            if (m < 0) return null;
            if (m != 0xFF) continue;
            int t = in.read();
            pos++;
            while (t == 0xFF) {
                t = in.read();
                pos++;
            }
            if (t < 0) return null;
            if (t == 0xD8 || (t >= 0xD0 && t <= 0xD7) || t == 0x01 || t == 0x00) continue;
            if (t == 0xD9 || t == 0xDA) return null;   /* EOI / SOS before any SOF */
            int hi = in.read(), lo = in.read();
            if (hi < 0 || lo < 0) return null;
            int len = (hi << 8) | lo;
            if (len < 2) return null;
            if (t >= 0xC0 && t <= 0xCF && t != 0xC4 && t != 0xC8 && t != 0xCC) {
                byte[] d = new byte[5];
                readFully(in, d, 0, 5);
                int h = ((d[1] & 0xFF) << 8) | (d[2] & 0xFF);
                int w = ((d[3] & 0xFF) << 8) | (d[4] & 0xFF);
                return new int[]{w, h};
            }
            skip(in, len - 2);
            pos += len;
        }
        return null;
    }

    /* ------------------------------------------------------------------ io helpers */

    static int be32(byte[] b, int o) {
        return ((b[o] & 0xFF) << 24) | ((b[o + 1] & 0xFF) << 16) | ((b[o + 2] & 0xFF) << 8) | (b[o + 3] & 0xFF);
    }

    private static void put32(byte[] b, int o, int v) {
        b[o] = (byte) (v >>> 24);
        b[o + 1] = (byte) (v >>> 16);
        b[o + 2] = (byte) (v >>> 8);
        b[o + 3] = (byte) v;
    }

    private static byte[] readChunk(InputStream in, int len) throws IOException {
        byte[] d = new byte[len];
        readFully(in, d, 0, len);
        skip(in, 4);
        return d;
    }

    static void readFully(InputStream in, byte[] b, int off, int len) throws IOException {
        while (len > 0) {
            int n = in.read(b, off, len);
            if (n < 0) throw new EOFException("file truncated");
            off += n;
            len -= n;
        }
    }

    static void skip(InputStream in, long n) throws IOException {
        while (n > 0) {
            long s = in.skip(n);
            if (s <= 0) {
                if (in.read() < 0) throw new EOFException("file truncated");
                s = 1;
            }
            n -= s;
        }
    }
}
