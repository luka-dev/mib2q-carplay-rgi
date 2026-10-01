package com.luka.carplay.rgd;

import java.io.DataInputStream;
import java.io.BufferedInputStream;
import java.io.IOException;
import java.io.InputStream;

/** Generated font bounds and Unicode 17 data, loaded once from inside this JAR.
 * No fonts, AWT, ICU, native shaper or modern JRE APIs on the HU. */
final class VCTextData {
    final int unknownAdvance;
    // Three 21-bit fields per positive long. No expanded int[3*n] copy remains.
    final long[] advances, properties, decompositions, compositions;

    private static final class Holder {
        static final VCTextData DATA = new VCTextData();
    }

    static VCTextData get() { return Holder.DATA; }

    private VCTextData() {
        this(VCTextData.class.getResourceAsStream("vc-text.bin"));
    }

    VCTextData(InputStream raw) {
        if (raw == null) throw new IllegalStateException("Missing embedded VC text data");
        DataInputStream in = new DataInputStream(new BufferedInputStream(raw, 8192));
        try {
            if (in.readInt() != 0x56435432) throw new IOException("VC text data version");
            unknownAdvance = field(readVar(in));
            advances = readTable(in, 0);
            properties = readTable(in, 1);
            decompositions = readTable(in, 2);
            compositions = readTable(in, 3);
            if (in.read() != -1) throw new IOException("Trailing VC text data");
        } catch (IOException e) {
            throw new IllegalStateException("VC text data: " + e);
        } finally {
            try { in.close(); } catch (IOException ignored) { }
        }
    }

    private static int readVar(DataInputStream in) throws IOException {
        int value = 0;
        for (int shift = 0; shift <= 21; shift += 7) {
            int b = in.readUnsignedByte();
            value |= (b & 127) << shift;
            if ((b & 128) == 0) {
                if (value > 0x3fffff) break;
                return value;
            }
        }
        throw new IOException("VC varint overflow");
    }

    private static int unzigzag(int value) { return (value >>> 1) ^ -(value & 1); }

    private static int field(int value) throws IOException {
        if (value < 0 || value > 0x1fffff) throw new IOException("VC field overflow");
        return value;
    }

    private static long[] readTable(DataInputStream in, int kind) throws IOException {
        int count = readVar(in);
        if (count > 10000) throw new IOException("VC table length");
        long[] table = new long[count];
        int a = 0;
        for (int i = 0; i < count; i++) {
            int delta = readVar(in);
            // Range gaps need the lengths from column 2; temporarily store the
            // gap in this same array. Point keys can be accumulated immediately.
            a = kind < 2 ? delta : a + delta;
            table[i] = (long) field(a) << 42;
        }
        int previousFirst = -1, previousSecond = -1;
        for (int i = 0; i < count; i++) {
            a = first(table[i]);
            int value = readVar(in), b;
            if (kind < 2) {
                a += previousSecond + 1;
                b = a + value;
            } else if (kind == 2) b = a + unzigzag(value);
            else b = value + (a == previousFirst ? previousSecond : 0);
            table[i] = ((long) field(a) << 42) | ((long) field(b) << 21);
            previousFirst = a;
            previousSecond = b;
        }
        for (int i = 0; i < count; i++) {
            int value = readVar(in);
            if (kind == 3) value = first(table[i]) + unzigzag(value);
            table[i] |= field(value);
        }
        return table;
    }

    static int first(long record) { return (int) (record >>> 42); }
    static int second(long record) { return (int) (record >>> 21) & 0x1fffff; }
    static int third(long record) { return (int) record & 0x1fffff; }

    static int range(long[] table, int cp, int missing) {
        int lo = 0, hi = table.length - 1;
        while (lo <= hi) {
            int m = (lo + hi) / 2;
            long row = table[m];
            if (cp < first(row)) hi = m - 1;
            else if (cp > second(row)) lo = m + 1;
            else return third(row);
        }
        return missing;
    }

    int props(int cp) { return range(properties, cp, 0); }

    /** In 1/64 px. Unknown glyphs have an explicit .notdef bound, not zero. */
    int advance(int cp) {
        if ((props(cp) & 65536) != 0) return 0; // default-ignorable, including ZWJ/VS
        return range(advances, cp, unknownAdvance);
    }

    boolean covered(int cp) {
        return (props(cp) & 65536) != 0 || range(advances, cp, -1) >= 0;
    }
}
