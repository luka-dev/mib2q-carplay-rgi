package com.luka.carplay.rgd;

import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * Exercises the shipping name-truncation helper (CurrentPositionScroll.fitWithEllipsis,
 * reused by BAPBridge.updateLatchedRouteText so the maneuver/status line never triggers
 * multi-page scrolling once a distance is pinned to it) and the exit/signpost turn-arrow
 * prefix fix (updateLatchedRouteText now calls getTurnArrowPrefix for the signpost branch
 * too, matching the turnTo branch, whenever a distance is present).
 */
public final class RouteTextTruncationAndExitArrowTest {
    private static int checks;
    private static void check(boolean ok, String label) {
        checks++;
        if (!ok) throw new AssertionError(label);
    }
    private static void equal(String label, Object expected, Object actual) {
        checks++;
        if (!expected.equals(actual)) throw new AssertionError(label + ": expected <" + expected + "> got <" + actual + ">");
    }
    private static String repeat(String s, int n) {
        StringBuffer b = new StringBuffer();
        for (int i = 0; i < n; i++) b.append(s);
        return b.toString();
    }

    private static Object get(Object target, String name) throws Exception {
        for (Class<?> c = target.getClass(); c != null; c = c.getSuperclass()) {
            try { Field f = c.getDeclaredField(name); f.setAccessible(true); return f.get(target); }
            catch (NoSuchFieldException e) { }
        }
        throw new NoSuchFieldException(name);
    }

    private static void truncationFitsOnePage() throws Exception {
        String prefix = BAPBridge.ARROW_STRAIGHT;
        String suffix = " | 0.1 mi";

        // A short name must survive unchanged (no ellipsis needed).
        String shortName = "Main St";
        String shortResult = CurrentPositionScroll.fitWithEllipsis(shortName, prefix, suffix);
        equal("short name left untouched", shortName, shortResult);

        // A very long street name must be shortened at a grapheme boundary with an ellipsis,
        // and the pinned " | 0.1 mi" suffix must survive intact.
        String longName = repeat("Northwest Pennsylvania Boulevard ", 8); // ~272 chars, far over budget
        String truncated = CurrentPositionScroll.fitWithEllipsis(longName, prefix, suffix);
        check(truncated.length() < longName.length(), "long name actually shortened");
        check(truncated.endsWith("…") || truncated.endsWith("..."), "ellipsis appended: " + truncated);
        check(longName.startsWith(truncated.substring(0, truncated.length()
                - (truncated.endsWith("…") ? 1 : 3))), "truncation is a genuine prefix of the source name");

        // The combined line (prefix + truncated name + pinned suffix) must fit CurrentPositionScroll's
        // own single-page width/byte budget -- the exact same measure build() uses for paging.
        int usedWidth = CurrentPositionScroll.width(prefix) + CurrentPositionScroll.width(truncated) + CurrentPositionScroll.width(suffix);
        int usedBytes = CurrentPositionScroll.bytes(prefix) + CurrentPositionScroll.bytes(truncated) + CurrentPositionScroll.bytes(suffix);
        check(usedWidth <= CurrentPositionScroll.WIDTH_64, "combined width within one-page budget: " + usedWidth);
        check(usedBytes <= CurrentPositionScroll.MAX_BYTES, "combined bytes within one-page budget: " + usedBytes);

        // End-to-end: feeding the combined text through the real CurrentPositionScroll must never
        // schedule multi-page scrolling (waitMillis == -1 once sent, exactly like a naturally-fitting
        // string in VCTextScrollTest's "fitting text has no timer" case).
        CurrentPositionScroll scroll = new CurrentPositionScroll();
        scroll.configure(truncated + suffix, prefix, "", true);
        long now = 100000L;
        String frame = scroll.next(now);
        check(frame != null, "single-page frame produced");
        check(frame.startsWith(prefix) && frame.endsWith(suffix), "pinned prefix/suffix preserved in rendered frame");
        scroll.sent(now);
        equal("truncated combined line never triggers scrolling", Long.valueOf(-1L), Long.valueOf(scroll.waitMillis(now)));

        // A name so long that even a single grapheme plus the pinned suffix cannot fit still
        // degrades to a single static page (the existing build() fallback path), never scrolling.
        String huge = repeat("x", 500);
        String hugeTruncated = CurrentPositionScroll.fitWithEllipsis(huge, prefix, suffix);
        check(hugeTruncated.length() < huge.length(), "even a huge name is shortened");
    }

    private static void rtlTruncationSinglePage() throws Exception {
        String prefix = BAPBridge.ARROW_STRAIGHT;
        String suffix = " | 0.1 mi";
        long now = 100000L;

        // Long Hebrew street name (RTL script) -- build() prepends a U+200F/U+200E direction
        // mark for this, which fitWithEllipsis must budget for exactly as build() does
        // (Gate 2 remediation #2), or the combined line can still overflow into a second page.
        String longHebrew = repeat("רחוב הרצל הארוך מאוד ", 6);
        String truncatedHebrew = CurrentPositionScroll.fitWithEllipsis(longHebrew, prefix, suffix);
        check(truncatedHebrew.length() < longHebrew.length(), "long Hebrew name shortened");
        CurrentPositionScroll hebrewScroll = new CurrentPositionScroll();
        hebrewScroll.configure(truncatedHebrew + suffix, prefix, "", true);
        String hebrewFrame = hebrewScroll.next(now);
        check(hebrewFrame != null, "Hebrew single-page frame produced");
        hebrewScroll.sent(now);
        equal("Hebrew long name + pinned distance renders as exactly one page",
            Long.valueOf(-1L), Long.valueOf(hebrewScroll.waitMillis(now)));

        // Long Arabic street name, same guarantee.
        String longArabic = repeat("شارع الملك عبدالعزيز الطويل جدا ", 6);
        String truncatedArabic = CurrentPositionScroll.fitWithEllipsis(longArabic, prefix, suffix);
        check(truncatedArabic.length() < longArabic.length(), "long Arabic name shortened");
        CurrentPositionScroll arabicScroll = new CurrentPositionScroll();
        arabicScroll.configure(truncatedArabic + suffix, prefix, "", true);
        String arabicFrame = arabicScroll.next(now);
        check(arabicFrame != null, "Arabic single-page frame produced");
        arabicScroll.sent(now);
        equal("Arabic long name + pinned distance renders as exactly one page",
            Long.valueOf(-1L), Long.valueOf(arabicScroll.waitMillis(now)));
    }

    private static RouteGuidance.State exitState(int maneuverType) {
        RouteGuidance.State s = new RouteGuidance.State();
        s.maneuverOrder = new int[] {0};
        s.mType[0] = maneuverType;
        s.mJunctionType[0] = ManeuverMapper.JUNCTION_SINGLE_INTERSECTION;
        s.mExitInfo[0] = "Exit 101";
        s.distManeuverM = 800;
        return s;
    }

    private static void exitArrowPrefix() throws Exception {
        Method updateLatchedRouteText = BAPBridge.class.getDeclaredMethod("updateLatchedRouteText", RouteGuidance.State.class);
        updateLatchedRouteText.setAccessible(true);

        BAPBridge right = new BAPBridge();
        RouteGuidance.State rightState = exitState(ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT);
        updateLatchedRouteText.invoke(right, rightState);
        equal("off-ramp right gets the slight-right arrow", BAPBridge.ARROW_SLIGHT_RIGHT, get(right, "positionPrefix"));
        String rightText = (String) get(right, "latchedPositionText");
        check(rightText.startsWith("Exit 101") && rightText.indexOf(" | ") > 0, "signpost text + pinned distance: " + rightText);

        BAPBridge left = new BAPBridge();
        RouteGuidance.State leftState = exitState(ManeuverMapper.MT_HIGHWAY_OFF_RAMP_LEFT);
        updateLatchedRouteText.invoke(left, leftState);
        equal("off-ramp left gets the slight-left arrow", BAPBridge.ARROW_SLIGHT_LEFT, get(left, "positionPrefix"));

        // Historical behavior preserved: a signpost with NO distance keeps its old plain-text
        // form (no arrow) -- exactly what tests/RouteInfoPresentationTest.java's "signpost
        // priority, no road stripping" case already asserts for the shipping code.
        BAPBridge noDistance = new BAPBridge();
        RouteGuidance.State noDistanceState = exitState(ManeuverMapper.MT_HIGHWAY_OFF_RAMP_RIGHT);
        noDistanceState.distManeuverM = -1;
        updateLatchedRouteText.invoke(noDistance, noDistanceState);
        equal("no distance -> no arrow prefix (unchanged historical behavior)", "", get(noDistance, "positionPrefix"));
        equal("no distance -> plain signpost text", "Exit 101", get(noDistance, "latchedPositionText"));
    }

    public static void main(String[] args) throws Exception {
        truncationFitsOnePage();
        rtlTruncationSinglePage();
        exitArrowPrefix();
        System.out.println("RouteTextTruncationAndExitArrowTest: PASS (" + checks + " checks)");
    }
}
