/*
 * WazeCardManeuver: card class -> HUD (mainElement, direction).
 *
 * The expected pairs and the HUD picture set are the arrows the Audi B9 HUD
 * draws, written here as literal numbers so a change to a
 * ManeuverMapper constant cannot hide behind the class under test.  The class
 * ids are checked against the card_rec_class_t enum parsed from
 * altscreen_render/card_rec.h (argument 1) at test time.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

import com.luka.carplay.rgd.ManeuverMapper;
import com.luka.carplay.rgd.WazeCardManeuver;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class WazeCardManeuverTest {
    /* BAP values the HUD selects its arrow pictures by. */
    private static final int NO_SYMBOL = 0;
    private static final int ARRIVED = 3;
    private static final int TURN = 13;
    private static final int TURN_ON_MAINROAD = 14;
    private static final int UTURN = 25;

    private static final int[] NO = { NO_SYMBOL, 0 };
    private static final char[] SIDES = { 'R', 'L', 0 };

    /* class name (card_rec_class_t without CARD_REC_CLASS_), then the pair for
     * traffic side 'R', 'L' and 0 */
    private static final Object[][] EXPECTED = {
        { "NONE", NO, NO, NO },
        { "LEFT_TURN", p(TURN, 64), p(TURN, 64), p(TURN, 64) },
        { "RIGHT_TURN", p(TURN, 192), p(TURN, 192), p(TURN, 192) },
        { "STRAIGHT", p(TURN, 0), p(TURN, 0), p(TURN, 0) },
        { "KEEP_LEFT", p(TURN, 32), p(TURN, 32), p(TURN, 32) },
        { "KEEP_RIGHT", p(TURN, 224), p(TURN, 224), p(TURN, 224) },
        { "ARRIVE", p(ARRIVED, 0), p(ARRIVED, 0), p(ARRIVED, 0) },
        { "UTURN_LEFT", p(UTURN, 64), p(UTURN, 64), p(UTURN, 64) },
        { "UTURN_RIGHT", p(UTURN, 192), p(UTURN, 192), p(UTURN, 192) },
        { "ROUNDABOUT_NUMBERED", NO, NO, NO },
        { "ROUNDABOUT_LEFT", p(TURN, 64), p(TURN, 64), p(TURN, 64) },
        { "ROUNDABOUT_STRAIGHT", p(TURN, 0), p(TURN, 0), p(TURN, 0) },
        { "ROUNDABOUT_RIGHT", p(TURN, 192), p(TURN, 192), p(TURN, 192) },
        { "ROUNDABOUT_UTURN", p(UTURN, 64), p(UTURN, 192), NO },
    };

    private static int checks;

    private static int[] p(int mainElement, int direction) {
        return new int[] { mainElement, direction };
    }

    private static void check(boolean ok, String what) {
        checks++;
        if (!ok) throw new AssertionError(what);
    }

    private static void expectInt(String what, int expected, int actual) {
        check(expected == actual, what + ": expected " + expected + ", got " + actual);
    }

    private static String sideName(char side) {
        return side == 0 ? "0" : (side >= 0x20 && side < 0x7f) ? "'" + side + "'" : "\\u" + Integer.toHexString(side);
    }

    private static String show(int[] pair) {
        return pair == null ? "null" : pair.length != 2 ? "length " + pair.length : pair[0] + "/" + pair[1];
    }

    /* ---------------------------------------------------------- HUD facts */

    /* Pairs the HUD draws a picture for (sideStreets empty, zLevel 0).
     * ARRIVED with another direction draws a cap without a flag: not used. */
    private static boolean drawsPicture(int main, int dir) {
        if (main == TURN || main == TURN_ON_MAINROAD) {
            return dir == 0 || dir == 32 || dir == 64 || dir == 96 || dir == 160 || dir == 192 || dir == 224;
        }
        if (main == UTURN) return dir == 64 || dir == 192;
        if (main == ARRIVED) return dir == 0 || dir == 64 || dir == 192;
        return false;
    }

    /* Why a pair must never be sent on the Waze path, or null. */
    private static String forbidden(int main, int dir) {
        if ((main >= 21 && main <= 24) || main == 26 || main == 27 || main == 29) {
            return "roundabout mainElement " + main;
        }
        if (main == 11) return "FOLLOW_STREET";
        if (main == 1 || main == 32 || main == 33 || main == 36 || main == 37 || main >= 39) {
            return "mainElement " + main + " draws nothing";
        }
        if (dir < 0 || dir > 255) return "direction " + dir + " out of range";
        if (dir == 128) return "direction 128 draws nothing";
        if (dir % 32 == 16) return "direction " + dir + " is an odd multiple of 16";
        return null;
    }

    private static void checkHudFacts() {
        /* The literals above are the HUD facts; the shipped constants must agree. */
        expectInt("ManeuverMapper.NO_SYMBOL", NO_SYMBOL, ManeuverMapper.NO_SYMBOL);
        expectInt("ManeuverMapper.ARRIVED", ARRIVED, ManeuverMapper.ARRIVED);
        expectInt("ManeuverMapper.FOLLOW_STREET", 11, ManeuverMapper.FOLLOW_STREET);
        expectInt("ManeuverMapper.TURN", TURN, ManeuverMapper.TURN);
        expectInt("ManeuverMapper.TURN_ON_MAINROAD", TURN_ON_MAINROAD, ManeuverMapper.TURN_ON_MAINROAD);
        expectInt("ManeuverMapper.UTURN", UTURN, ManeuverMapper.UTURN);
        expectInt("ManeuverMapper.ROUNDABOUT_TRS_RIGHT", 21, ManeuverMapper.ROUNDABOUT_TRS_RIGHT);
        expectInt("ManeuverMapper.SQUARE_TRS_LEFT", 24, ManeuverMapper.SQUARE_TRS_LEFT);
        expectInt("ManeuverMapper.EXIT_ROUNDABOUT_TRS_LEFT", 27, ManeuverMapper.EXIT_ROUNDABOUT_TRS_LEFT);
        expectInt("ManeuverMapper.PREPARE_ROUNDABOUT", 29, ManeuverMapper.PREPARE_ROUNDABOUT);
        expectInt("ManeuverMapper.DIR_STRAIGHT", 0, ManeuverMapper.DIR_STRAIGHT);
        expectInt("ManeuverMapper.DIR_SLIGHT_LEFT", 32, ManeuverMapper.DIR_SLIGHT_LEFT);
        expectInt("ManeuverMapper.DIR_LEFT", 64, ManeuverMapper.DIR_LEFT);
        expectInt("ManeuverMapper.DIR_UTURN", 128, ManeuverMapper.DIR_UTURN);
        expectInt("ManeuverMapper.DIR_RIGHT", 192, ManeuverMapper.DIR_RIGHT);
        expectInt("ManeuverMapper.DIR_SLIGHT_RIGHT", 224, ManeuverMapper.DIR_SLIGHT_RIGHT);
        expectInt("WazeCardManeuver.SIDE_RIGHT", 'R', WazeCardManeuver.SIDE_RIGHT);
        expectInt("WazeCardManeuver.SIDE_LEFT", 'L', WazeCardManeuver.SIDE_LEFT);
        expectInt("WazeCardManeuver.SIDE_NONE", 0, WazeCardManeuver.SIDE_NONE);
        /* Every expected arrow is a picture, never a forbidden pair. */
        for (int i = 0; i < EXPECTED.length; i++) {
            for (int s = 1; s <= SIDES.length; s++) {
                int[] e = (int[]) EXPECTED[i][s];
                String what = "expectation " + EXPECTED[i][0] + " side " + sideName(SIDES[s - 1]) + " " + show(e);
                check(isNoSymbol(e) || (drawsPicture(e[0], e[1]) && forbidden(e[0], e[1]) == null), what);
            }
        }
    }

    private static boolean isNoSymbol(int[] pair) {
        return pair[0] == NO_SYMBOL && pair[1] == 0;
    }

    /* ------------------------------------------------ C enum parity */

    private static String readText(File file) throws IOException {
        InputStream in = new FileInputStream(file);
        try {
            byte[] buf = new byte[(int) file.length()];
            int off = 0;
            while (off < buf.length) {
                int n = in.read(buf, off, buf.length - off);
                if (n < 0) break;
                off += n;
            }
            return new String(buf, 0, off, "UTF-8");
        } finally {
            in.close();
        }
    }

    /* One left-to-right pass, as the C lexer does: whichever of a string or
     * char literal, '//' or slash-star starts first wins, so a slash-star inside
     * a '//' comment or a string opens nothing.  Comments become one space. */
    private static final Pattern C_TOKEN = Pattern.compile(
            "\"(?:\\\\.|[^\"\\\\\\n])*\"|'(?:\\\\.|[^'\\\\\\n])*'|//[^\\n]*|/\\*[\\s\\S]*?\\*/");

    static String stripComments(String text) {
        Matcher m = C_TOKEN.matcher(text);
        StringBuffer out = new StringBuffer(text.length());
        while (m.find()) {
            String tok = m.group();
            m.appendReplacement(out, tok.charAt(0) == '/' ? " " : Matcher.quoteReplacement(tok));
        }
        m.appendTail(out);
        return out.toString();
    }

    /* card_rec_class_t enumerators in declaration order -> value. */
    static Map<String, Integer> parseClassEnum(File header) throws IOException {
        String text = stripComments(readText(header));
        Matcher m = Pattern.compile("typedef\\s+enum\\s*\\{([^{}]*)\\}\\s*card_rec_class_t\\s*;").matcher(text);
        if (!m.find()) throw new AssertionError(header + ": no 'typedef enum { ... } card_rec_class_t;'");
        String body = m.group(1);
        if (m.find()) throw new AssertionError(header + ": card_rec_class_t declared twice");
        Pattern item = Pattern.compile("([A-Za-z_][A-Za-z0-9_]*)(?:\\s*=\\s*([0-9]+))?");
        Map<String, Integer> out = new LinkedHashMap<String, Integer>();
        String[] parts = body.split(",", -1);
        int next = 0;
        for (int i = 0; i < parts.length; i++) {
            String part = parts[i].trim();
            if (part.length() == 0) {
                if (i == parts.length - 1) break;           /* trailing comma */
                throw new AssertionError(header + ": empty card_rec_class_t enumerator");
            }
            Matcher im = item.matcher(part);
            if (!im.matches()) throw new AssertionError(header + ": cannot parse enumerator '" + part + "'");
            String name = im.group(1);
            int value = im.group(2) != null ? Integer.parseInt(im.group(2)) : next;
            if (!name.startsWith("CARD_REC_CLASS_")) {
                throw new AssertionError(header + ": unexpected enumerator " + name);
            }
            if (out.containsKey(name) || out.containsValue(Integer.valueOf(value))) {
                throw new AssertionError(header + ": duplicate enumerator " + name + " = " + value);
            }
            out.put(name, Integer.valueOf(value));
            next = value + 1;
        }
        return out;
    }

    private static Map<String, Integer> javaClassIds() throws IllegalAccessException {
        Map<String, Integer> out = new LinkedHashMap<String, Integer>();
        Field[] fields = WazeCardManeuver.class.getDeclaredFields();
        for (int i = 0; i < fields.length; i++) {
            Field f = fields[i];
            if (!f.getName().startsWith("CLASS_")) continue;
            int mod = f.getModifiers();
            check(Modifier.isPublic(mod) && Modifier.isStatic(mod) && Modifier.isFinal(mod)
                    && f.getType() == int.class, "WazeCardManeuver." + f.getName() + " is not public static final int");
            out.put(f.getName().substring("CLASS_".length()), Integer.valueOf(f.getInt(null)));
        }
        return out;
    }

    /* Returns the number of card classes (CLASS_COUNT). */
    private static int checkEnumParity(File header) throws Exception {
        Map<String, Integer> c = parseClassEnum(header);
        Map<String, Integer> java = javaClassIds();
        List<String> cNames = new ArrayList<String>();
        for (Map.Entry<String, Integer> e : c.entrySet()) {
            String name = e.getKey().substring("CARD_REC_CLASS_".length());
            cNames.add(name);
            Integer j = java.get(name);
            check(j != null, header + " declares CARD_REC_CLASS_" + name + " = " + e.getValue()
                    + " but WazeCardManeuver has no CLASS_" + name + ": map the new card class first");
            check(j.intValue() == e.getValue().intValue(), "CLASS_" + name + " = " + j + " but card_rec.h says "
                    + e.getValue() + ": the C enum changed order");
        }
        for (Map.Entry<String, Integer> e : java.entrySet()) {
            check(c.containsKey("CARD_REC_CLASS_" + e.getKey()), "WazeCardManeuver.CLASS_" + e.getKey()
                    + " has no CARD_REC_CLASS_" + e.getKey() + " in " + header);
        }
        check(cNames.size() >= 2 && "COUNT".equals(cNames.get(cNames.size() - 1)),
                header + ": card_rec_class_t must end with CARD_REC_CLASS_COUNT");
        int count = WazeCardManeuver.CLASS_COUNT;
        expectInt("CARD_REC_CLASS_NONE", 0, c.get("CARD_REC_CLASS_NONE").intValue());
        expectInt("classes before CARD_REC_CLASS_COUNT", count, cNames.size() - 1);
        /* The values are dense 0..COUNT-1 (ids index tables on the C side) and
         * the test has exactly one expectation per class, in enum order. */
        expectInt("expectation rows", count, EXPECTED.length);
        for (int i = 0; i < count; i++) {
            check(cNames.get(i).equals(EXPECTED[i][0]), "expectation row " + i + " is " + EXPECTED[i][0]
                    + ", card_rec.h has " + cNames.get(i) + " there");
            expectInt("CARD_REC_CLASS_" + cNames.get(i), i, c.get("CARD_REC_CLASS_" + cNames.get(i)).intValue());
        }
        return count;
    }

    /* ------------------------------------------------------ mapping */

    private static void checkExact(int count) {
        for (int cls = 0; cls < count; cls++) {
            for (int s = 0; s < SIDES.length; s++) {
                int[] got = WazeCardManeuver.hudArrow(cls, SIDES[s]);
                int[] want = (int[]) EXPECTED[cls][s + 1];
                String what = EXPECTED[cls][0] + " side " + sideName(SIDES[s]);
                check(got != null && got.length == 2, what + ": returned " + show(got));
                check(got[0] == want[0] && got[1] == want[1], what + ": expected " + show(want) + ", got " + show(got));
            }
        }
        /* A caller may keep or change the array: every call returns a new one. */
        int[] first = WazeCardManeuver.hudArrow(WazeCardManeuver.CLASS_LEFT_TURN, 'R');
        first[0] = 99;
        first[1] = 128;
        int[] again = WazeCardManeuver.hudArrow(WazeCardManeuver.CLASS_LEFT_TURN, 'R');
        check(again != first && again[0] == TURN && again[1] == 64, "hudArrow shares its result array");
    }

    /* Every class id near and far outside the range, with every char as side. */
    private static void checkSweep(int count) {
        int[] ids = new int[count + 10];
        for (int i = 0; i < count + 6; i++) ids[i] = i - 3;   /* -3 .. count+2 */
        ids[count + 6] = Integer.MIN_VALUE;
        ids[count + 7] = Integer.MAX_VALUE;
        ids[count + 8] = 100;
        ids[count + 9] = -100;
        int pictures = 0;
        for (int i = 0; i < ids.length; i++) {
            int cls = ids[i];
            boolean known = cls >= 0 && cls < count;
            for (int s = 0; s <= 0xFFFF; s++) {
                char side = (char) s;
                int[] got = WazeCardManeuver.hudArrow(cls, side);
                if (got == null || got.length != 2) {
                    check(false, "class " + cls + " side " + sideName(side) + ": returned " + show(got));
                }
                boolean sideKnown = side == 'R' || side == 'L' || side == 0;
                if (!known || !sideKnown) {
                    if (!isNoSymbol(got)) {
                        check(false, "class " + cls + " side " + sideName(side) + ": expected NO_SYMBOL 0/0, got "
                                + show(got));
                    }
                    continue;
                }
                if (isNoSymbol(got)) continue;
                if (got[0] == NO_SYMBOL) {
                    check(false, "class " + cls + " side " + sideName(side) + ": NO_SYMBOL with direction " + got[1]);
                }
                String why = forbidden(got[0], got[1]);
                if (why != null || !drawsPicture(got[0], got[1])) {
                    check(false, "class " + cls + " side " + sideName(side) + ": " + show(got) + " "
                            + (why != null ? why : "is not a HUD picture"));
                }
                pictures++;
            }
        }
        checks++;
        /* Pictures appear only where an expectation row has one. */
        int expected = 0;
        for (int i = 0; i < EXPECTED.length; i++) {
            for (int s = 1; s <= SIDES.length; s++) {
                if (!isNoSymbol((int[]) EXPECTED[i][s])) expected++;
            }
        }
        expectInt("picture outputs in the sweep", expected, pictures);
    }

    private static void checkUnknown() {
        char[] junk = { 'r', 'l', '-', ' ', 'X', '0', 'ÿ', '￿' };
        for (int cls = -1; cls <= WazeCardManeuver.CLASS_COUNT; cls++) {
            for (int j = 0; j < junk.length; j++) {
                int[] got = WazeCardManeuver.hudArrow(cls, junk[j]);
                check(isNoSymbol(got), "class " + cls + " side " + sideName(junk[j]) + ": " + show(got));
            }
        }
        for (int s = 0; s < SIDES.length; s++) {
            check(isNoSymbol(WazeCardManeuver.hudArrow(WazeCardManeuver.CLASS_COUNT, SIDES[s])), "CLASS_COUNT");
            check(isNoSymbol(WazeCardManeuver.hudArrow(-1, SIDES[s])), "class -1");
            check(isNoSymbol(WazeCardManeuver.hudArrow(WazeCardManeuver.CLASS_NONE, SIDES[s])), "NONE");
            check(isNoSymbol(WazeCardManeuver.hudArrow(WazeCardManeuver.CLASS_ROUNDABOUT_NUMBERED, SIDES[s])),
                    "ROUNDABOUT_NUMBERED");
        }
        check(isNoSymbol(WazeCardManeuver.hudArrow(WazeCardManeuver.CLASS_ROUNDABOUT_UTURN, (char) 0)),
                "ROUNDABOUT_UTURN without a traffic side");
    }

    public static void main(String[] args) throws Exception {
        File header = new File(args.length > 0 ? args[0] : "altscreen_render/card_rec.h");
        if (!header.isFile()) throw new AssertionError("card_rec.h not found: " + header);
        checkHudFacts();
        int count = checkEnumParity(header);
        checkExact(count);
        checkUnknown();
        checkSweep(count);
        System.out.println("WazeCardManeuverTest: " + checks + " checks PASS (" + count
                + " card classes match card_rec.h)");
    }
}
