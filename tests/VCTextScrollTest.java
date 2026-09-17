package com.luka.carplay.rgd;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.InputStream;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;

/** Deterministic font-free runtime tests; optional official Unicode fixtures. */
public final class VCTextScrollTest {
    private static int checks;
    private static void check(boolean value, String label) {
        checks++;
        if (!value) throw new AssertionError(label);
    }
    private static void equal(Object a, Object b, String label) {
        check(a.equals(b), label + ": expected <" + a + "> got <" + b + ">");
    }
    private static String repeat(String s, int n) {
        StringBuilder b = new StringBuilder();
        while (n-- > 0) b.append(s);
        return b.toString();
    }
    private static String hex(String s) {
        StringBuilder out = new StringBuilder();
        for (String cp : s.trim().split("\\s+"))
            if (!cp.isEmpty()) out.appendCodePoint(Integer.parseInt(cp, 16));
        return out.toString();
    }
    private static void packedData() throws Exception {
        check("jar".equals(VCTextData.class.getResource("vc-text.bin").getProtocol()),
            "text data must come from the shipping JAR");
        VCTextData d = VCTextData.get();
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        DataOutputStream out = new DataOutputStream(bytes);
        out.writeInt(0x56435431);
        out.writeInt(d.unknownAdvance);
        long[][] tables = {d.advances, d.properties, d.decompositions, d.compositions};
        int records = 0;
        for (long[] table : tables) {
            out.writeInt(table.length);
            records += table.length;
            for (long row : table) {
                out.writeInt(VCTextData.first(row));
                out.writeInt(VCTextData.second(row));
                out.writeInt(VCTextData.third(row));
            }
        }
        byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes.toByteArray());
        StringBuilder hash = new StringBuilder();
        for (byte b : digest) hash.append(String.format("%02x", b & 255));
        // Golden unpacked Unicode17/font data, captured before format v2.
        equal("7f11283d92a1ec869641553789e78acf8e55f08dea778c009513719fedbee1b4", hash.toString(),
            "every metric/Unicode record survives packing exactly");
        equal(58224, records * 8, "retained table payload");

        InputStream resource = VCTextData.class.getResourceAsStream("vc-text.bin");
        bytes.reset();
        byte[] buffer = new byte[4096];
        for (int n; (n = resource.read(buffer)) >= 0;) bytes.write(buffer, 0, n);
        resource.close();
        byte[] data = bytes.toByteArray();
        for (int length : new int[] {0, 3, 6, data.length / 2, data.length - 1})
            rejected(Arrays.copyOf(data, length), "truncated embedded resource " + length);
        byte[] oldVersion = data.clone(); oldVersion[3] = '1';
        rejected(oldVersion, "old version fails explicitly");
        rejected(Arrays.copyOf(data, data.length + 1), "trailing resource data rejected");
        rejected(new byte[] {'V', 'C', 'T', '2', (byte) 255, (byte) 255, (byte) 255, (byte) 255},
            "unbounded varint rejected");
        System.out.println("VC packed data: all 7,278 records identical, 58,224 table bytes; JAR-only loading PASS");
    }
    private static void rejected(byte[] data, String label) {
        try {
            new VCTextData(new ByteArrayInputStream(data));
        } catch (IllegalStateException expected) {
            check(true, label);
            return;
        }
        check(false, label);
    }
    private static void unicodeConformance(File dir) throws Exception {
        BufferedReader in = new BufferedReader(new InputStreamReader(new FileInputStream(
            new File(dir, "auxiliary/GraphemeBreakTest.txt")), "UTF-8"));
        String line;
        int cases = 0;
        while ((line = in.readLine()) != null) {
            int comment = line.indexOf('#');
            line = (comment < 0 ? line : line.substring(0, comment)).trim();
            if (line.isEmpty()) continue;
            StringBuilder text = new StringBuilder();
            ArrayList<Integer> bounds = new ArrayList<Integer>();
            for (String part : line.split("\\s+")) {
                if (part.equals("÷")) bounds.add(text.length());
                else if (!part.equals("×")) text.appendCodePoint(Integer.parseInt(part, 16));
            }
            int[] got = VCUnicode.boundaries(text.toString());
            check(got.length == bounds.size(), "GCB length case " + line + " got " + Arrays.toString(got));
            for (int i = 0; i < got.length; i++) equal(bounds.get(i), got[i], "GCB " + line);
            cases++;
        }
        in.close();
        System.out.println("Unicode 17 grapheme conformance: " + cases + " cases");
        in = new BufferedReader(new InputStreamReader(new FileInputStream(new File(dir, "NormalizationTest.txt")), "UTF-8"));
        cases = 0;
        while ((line = in.readLine()) != null) {
            int comment = line.indexOf('#');
            line = (comment < 0 ? line : line.substring(0, comment)).trim();
            if (line.isEmpty() || line.startsWith("@")) continue;
            String[] f = line.split(";");
            String c1 = hex(f[0]), c2 = hex(f[1]), c3 = hex(f[2]), c4 = hex(f[3]), c5 = hex(f[4]);
            equal(c2, VCUnicode.nfc(c1), "NFC c1 " + line);
            equal(c2, VCUnicode.nfc(c2), "NFC c2 " + line);
            equal(c2, VCUnicode.nfc(c3), "NFC c3 " + line);
            equal(c4, VCUnicode.nfc(c4), "NFC c4 " + line);
            equal(c4, VCUnicode.nfc(c5), "NFC c5 " + line);
            cases++;
        }
        in.close();
        System.out.println("Unicode 17 NFC conformance: " + cases + " cases");
    }
    private static void fit(String frame) throws Exception {
        check(frame.getBytes("UTF-8").length <= 96, "wire budget: " + frame);
        VCTextData d = VCTextData.get();
        int width = 0;
        for (int i = 0; i < frame.length();) {
            int cp = frame.codePointAt(i);
            check(cp < 0xD800 || cp > 0xDFFF, "surrogate intact");
            width += d.advance(cp);
            i += Character.charCount(cp);
        }
        check(width <= CurrentPositionScroll.WIDTH_64, "width budget: " + frame);
    }
    private static void traverse(String source, String prefix, String suffix) throws Exception {
        CurrentPositionScroll scroll = new CurrentPositionScroll();
        scroll.configure(source, prefix, suffix, false);
        String normalized = VCUnicode.nfc(source);
        int[] boundaries = VCUnicode.boundaries(normalized);
        long now = 100000L;
        int lastStart = -1, lastEnd = 0, count = 0;
        String first = scroll.next(now);
        check(first != null, "first frame ready");
        if (first.charAt(0) == '\u200E' || first.charAt(0) == '\u200F') prefix = first.substring(0, 1) + prefix;
        String frame = first;
        while (true) {
            fit(frame);
            check(frame.startsWith(prefix) && frame.endsWith(suffix), "decorations survive");
            String payload = frame.substring(prefix.length(), frame.length() - suffix.length());
            int start = normalized.indexOf(payload, lastStart + 1);
            check(start >= 0, "source fragment exists: " + payload);
            int end = start + payload.length();
            check(Arrays.binarySearch(boundaries, start) >= 0 && Arrays.binarySearch(boundaries, end) >= 0,
                "never split extended grapheme");
            check(start <= lastEnd, "no skipped source text");
            lastStart = start; lastEnd = end;
            scroll.sent(now);
            if (end == normalized.length()) break;
            long wait = scroll.waitMillis(now);
            check(wait >= 250, "transport rate bound");
            now += wait;
            frame = scroll.next(now);
            check(frame != null, "next fragment ready");
            check(++count < normalized.length(), "finite traversal");
        }
        if (count > 0) {
            long wait = scroll.waitMillis(now);
            check(wait >= 1800, "end hold");
            equal(first, scroll.next(now + wait), "loop to first frame without blank");
        } else equal(-1L, scroll.waitMillis(now), "fitting text has no timer");
    }
    private static void timing() throws Exception {
        CurrentPositionScroll s = new CurrentPositionScroll();
        String text = "North Pennsylvania Avenue via Washington Boulevard and Main Street";
        s.configure(text, "‹", "›", false);
        long t = 100000;
        String first = s.next(t);
        s.sent(t);
        equal(1800L, s.waitMillis(t), "first hold");
        check(!s.configure(text, "‹", "›", false), "duplicates reuse plan");
        s.sent(t + 1000); // a cached BAP replay must not restart a hold
        equal(800L, s.waitMillis(t + 1000), "replay preserves deadline");
        check(s.next(t + 1799) == null, "not early");
        String second = s.next(t + 1800);
        check(!first.equals(second), "advance");
        s.failed(t + 1800);
        check(s.next(t + 2299) == null, "bounded retry");
        equal(second, s.next(t + 2300), "same pending fragment retried");
        s.sent(t + 2300);
        s.configure(text, "‹", "›", true);
        equal(first, s.next(t + 2400), "new maneuver/view restarts identical text");
        s.sent(t + 2400);
        check(s.waitMillis(t - 999999) == 1800, "backwards clock rebases");
        check(s.waitMillis(t + 999999) == 1800, "forward clock rebases without burst");
        s.clear();
        equal(-1L, s.waitMillis(t), "stop cancels deadline");
        check(s.next(t) == null, "no stale frame after clear");
        s.configure("Café", "", "", false);
        s.next(t); s.sent(t);
        check(!s.configure("Cafe\u0301", "", "", false), "canonical equivalence preserves plan");
        equal(-1L, s.waitMillis(t), "NFD name normalized and static");
        s.configure(repeat("W", 12), "", "", false);
        s.next(t); s.sent(t);
        equal(-1L, s.waitMillis(t), "near-width-limit string fits");
        s.configure(repeat("W", 13), "", "", false);
        s.next(t); s.sent(t);
        check(s.waitMillis(t) > 0, "one more wide glyph triggers scrolling");
        s.configure("a" + repeat("\u0301", 100), "‹", "›", false);
        check(s.isFallback(), "oversized grapheme flagged");
        equal("‹…›", s.next(t), "oversized grapheme explicit fallback");
        s.sent(t);
        equal(-1L, s.waitMillis(t), "oversized grapheme does not spin");
        s.configure("\uD800x\uDC00", "", "", false);
        equal("\uFFFDx\uFFFD", s.next(t), "malformed UTF-16 repaired");
        check(s.missingGlyphs, "missing glyph coverage explicit");
    }
    public static void main(String[] args) throws Exception {
        packedData();
        traverse("Main Street", "● ", "");
        traverse("North Pennsylvania Avenue via Washington Boulevard and Main Street", "‹", "›");
        traverse("Москва — улица Большая Дмитровка, поворот на Большой Каретный переулок", "● ", "");
        traverse("Avenida de la Constitución — dirección Plaza de España, salida número 12", "‹", "›");
        traverse("Avenue des Champs-E\u0301lyse\u0301es — direction Cafe\u0301 de la République", "", "");
        traverse("北京市朝阳区建国门外大街向东直到下一条路右转然后进入停车场", "‹", "›");
        traverse("東京都千代田区丸の内一丁目中央通りから次の交差点で右折", "", "");
        traverse("서울특별시 종로구 세종대로 광화문광장에서 우회전하세요", "", "");
        traverse("شارع الملك عبدالعزيز الطريق 42 باتجاه المطار الدولي المنعطف التالي", "‹", "›");
        traverse("Main Street شارع الملك عبدالعزيز الطريق 42 باتجاه المطار الدولي", "‹", "›");
        traverse("Start 👨‍👩‍👧‍👦 family 🇫🇷 flag क्‍ष junction Cafe\u0301 end", "", "");
        traverse(repeat("i", 120) + "final suffix", "", "");
        traverse(repeat("i\uFE0F", 40) + "wire-bound suffix", "‹", "›");
        timing();
        if (args.length > 0) unicodeConformance(new File(args[0]));
        System.out.println("VCTextScrollTest: PASS (" + checks + " checks)");
    }
}
