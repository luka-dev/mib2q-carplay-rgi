package com.luka.carplay.rgd;

/** NFC and extended grapheme boundaries with pinned Unicode 17 tables.
 * Deliberately independent of the HU's old Character/BreakIterator tables. */
final class VCUnicode {
    static int codePoint(String s, int i) {
        int c = s.charAt(i);
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.length()) {
            int low = s.charAt(i + 1);
            if (low >= 0xDC00 && low <= 0xDFFF)
                return 0x10000 + ((c - 0xD800) << 10) + low - 0xDC00;
        }
        return c >= 0xD800 && c <= 0xDFFF ? 0xFFFD : c;
    }

    static int chars(int cp) { return cp >= 0x10000 ? 2 : 1; }
    static int bytes(int cp) { return cp < 128 ? 1 : cp < 2048 ? 2 : cp < 65536 ? 3 : 4; }

    private static void append(StringBuffer b, int cp) {
        if (cp < 65536) b.append((char) cp);
        else {
            cp -= 65536;
            b.append((char) (0xD800 + (cp >> 10)));
            b.append((char) (0xDC00 + (cp & 1023)));
        }
    }

    private static final class Points {
        int[] values;
        int size;
        Points(int capacity) { values = new int[capacity + 8]; }
        void add(int cp) {
            if (size == values.length) {
                int[] bigger = new int[values.length * 2];
                System.arraycopy(values, 0, bigger, 0, size);
                values = bigger;
            }
            values[size++] = cp;
        }
    }

    private static int index(long[] table, int cp) {
        int lo = 0, hi = table.length - 1;
        while (lo <= hi) {
            int m = (lo + hi) / 2;
            int key = VCTextData.first(table[m]);
            if (cp < key) hi = m - 1;
            else if (cp > key) lo = m + 1;
            else return m;
        }
        return -1;
    }

    private static void decompose(VCTextData d, Points out, int cp) {
        int h = cp - 0xAC00;
        if (h >= 0 && h < 11172) {
            out.add(0x1100 + h / 588);
            out.add(0x1161 + (h % 588) / 28);
            if (h % 28 != 0) out.add(0x11A7 + h % 28);
            return;
        }
        int i = index(d.decompositions, cp);
        if (i < 0) out.add(cp);
        else {
            long row = d.decompositions[i];
            decompose(d, out, VCTextData.second(row));
            int second = VCTextData.third(row);
            if (second != 0) decompose(d, out, second);
        }
    }

    private static int ccc(VCTextData d, int cp) { return (d.props(cp) >> 5) & 255; }

    private static int compose(VCTextData d, int a, int b) {
        if (a >= 0x1100 && a < 0x1113 && b >= 0x1161 && b < 0x1176)
            return 0xAC00 + ((a - 0x1100) * 21 + b - 0x1161) * 28;
        if (a >= 0xAC00 && a < 0xD7A4 && (a - 0xAC00) % 28 == 0 && b > 0x11A7 && b < 0x11C3)
            return a + b - 0x11A7;
        long[] t = d.compositions;
        int lo = 0, hi = t.length - 1;
        while (lo <= hi) {
            int m = (lo + hi) / 2;
            long row = t[m];
            int first = VCTextData.first(row), second = VCTextData.second(row);
            if (a < first || (a == first && b < second)) hi = m - 1;
            else if (a > first || (a == first && b > second)) lo = m + 1;
            else return VCTextData.third(row);
        }
        return -1;
    }

    static String nfc(String source) {
        VCTextData d = VCTextData.get();
        Points out = new Points(source.length());
        for (int i = 0; i < source.length();) {
            int cp = codePoint(source, i);
            decompose(d, out, cp);
            i += chars(cp);
        }
        // Stable counting sort only for non-canonical combining runs. This
        // avoids quadratic insertion sorting on pathological long clusters.
        for (int i = 0; i < out.size;) {
            if (ccc(d, out.values[i]) == 0) { i++; continue; }
            int start = i, prev = 0;
            boolean ordered = true;
            while (i < out.size && ccc(d, out.values[i]) != 0) {
                int c = ccc(d, out.values[i++]);
                if (c < prev) ordered = false;
                prev = c;
            }
            if (!ordered) {
                int[] counts = new int[256], sorted = new int[i - start];
                for (int j = start; j < i; j++) counts[ccc(d, out.values[j])]++;
                int offset = 0;
                for (int j = 0; j < 256; j++) { int n = counts[j]; counts[j] = offset; offset += n; }
                for (int j = start; j < i; j++) sorted[counts[ccc(d, out.values[j])]++] = out.values[j];
                System.arraycopy(sorted, 0, out.values, start, sorted.length);
            }
        }
        int size = 0, starter = -1, lastClass = 0;
        for (int i = 0; i < out.size; i++) {
            int cp = out.values[i], cls = ccc(d, cp);
            int joined = starter >= 0 && (lastClass == 0 || lastClass < cls)
                ? compose(d, out.values[starter], cp) : -1;
            if (joined >= 0) out.values[starter] = joined;
            else {
                if (cls == 0) starter = size;
                out.values[size++] = cp;
                lastClass = cls;
            }
        }
        StringBuffer result = new StringBuffer(source.length());
        for (int i = 0; i < size; i++) append(result, out.values[i]);
        String normalized = result.toString();
        return source.equals(normalized) ? source : normalized;
    }

    static int[] boundaries(String text) {
        VCTextData data = VCTextData.get();
        Points bounds = new Points(text.length());
        bounds.add(0);
        int previous = 0, ri = 0, indic = 0;
        boolean epRun = false, zwjAfterEp = false;
        for (int i = 0; i < text.length();) {
            int cp = codePoint(text, i), props = data.props(cp), g = props & 31;
            int incb = (props >> 13) & 3;
            boolean ep = (props & 32768) != 0;
            boolean split = i != 0;
            if (previous == 1 && g == 2) split = false; // CR LF
            else if (previous == 1 || previous == 2 || previous == 3 || g == 1 || g == 2 || g == 3) { }
            else if (previous == 9 && (g == 9 || g == 10 || g == 12 || g == 13)) split = false;
            else if ((previous == 12 || previous == 10) && (g == 10 || g == 11)) split = false;
            else if ((previous == 13 || previous == 11) && g == 11) split = false;
            else if (g == 4 || g == 5 || g == 8 || previous == 7) split = false;
            else if (incb == 1 && indic == 2) split = false;
            else if (ep && previous == 5 && zwjAfterEp) split = false;
            else if (previous == 6 && g == 6 && ri % 2 == 1) split = false;
            if (split) bounds.add(i);
            zwjAfterEp = g == 5 && epRun;
            if (ep) epRun = true;
            else if (g != 4) epRun = false;
            if (incb == 1) indic = 1;
            else if (incb == 3 && indic != 0) indic = 2;
            else if (incb != 2) indic = 0;
            ri = g == 6 ? ri + 1 : 0;
            previous = g;
            i += chars(cp);
        }
        if (text.length() > 0) bounds.add(text.length());
        int[] result = new int[bounds.size];
        System.arraycopy(bounds.values, 0, result, 0, result.length);
        return result;
    }
}
