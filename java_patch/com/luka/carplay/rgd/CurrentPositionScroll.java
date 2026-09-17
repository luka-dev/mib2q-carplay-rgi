package com.luka.carplay.rgd;

/** BAP FctID 19 fragment planner/timer. All calls are serialized by RouteGuidance.
 * Linear-size plan, no font/shaping work on ticks, no thread or frame backlog. */
final class CurrentPositionScroll {
    static final int WIDTH_64 = (359 - 6) * 64; // uncalibrated EAL rounding reserve
    static final int MAX_BYTES = 96;
    static final int HOLD_MS = 1800;
    static final int MIN_STEP_MS = 250;
    private static final int RETRY_MS = 500;
    private String raw = "", source = "", prefix = "", suffix = "";
    private String directionMark = "";
    private int[] starts = new int[0], ends = new int[0], dwell = new int[0];
    private int frameIndex;
    private String pendingText;
    private boolean pending, fallback;
    boolean missingGlyphs;
    private long deadline, lastClock;

    void clear() {
        raw = source = prefix = suffix = "";
        directionMark = "";
        starts = ends = dwell = new int[0];
        pendingText = null;
        pending = fallback = missingGlyphs = false;
        deadline = lastClock = 0;
        frameIndex = 0;
    }

    boolean configure(String text, String before, String after, boolean reset) {
        if (starts.length != 0 && raw.equals(text) && prefix.equals(before) && suffix.equals(after)) {
            if (reset) restart();
            return reset;
        }
        String normalized = VCUnicode.nfc(text);
        raw = text;
        if (starts.length != 0 && source.equals(normalized) && prefix.equals(before) && suffix.equals(after)) {
            if (reset) restart();
            return reset;
        }
        source = normalized;
        prefix = before;
        suffix = after;
        build();
        restart();
        return true;
    }

    void restart() {
        frameIndex = 0;
        pendingText = null;
        pending = true;
        deadline = lastClock = 0;
    }

    private static int width(String text) {
        VCTextData data = VCTextData.get();
        int result = 0;
        for (int i = 0; i < text.length();) {
            int cp = VCUnicode.codePoint(text, i);
            result += data.advance(cp);
            i += VCUnicode.chars(cp);
        }
        return result;
    }

    private static int bytes(String text) {
        int result = 0;
        for (int i = 0; i < text.length();) {
            int cp = VCUnicode.codePoint(text, i);
            result += VCUnicode.bytes(cp);
            i += VCUnicode.chars(cp);
        }
        return result;
    }

    private void build() {
        VCTextData data = VCTextData.get();
        int[] boundaries = VCUnicode.boundaries(source);
        int n = boundaries.length - 1;
        long[] widths = new long[n + 1];
        int[] wire = new int[n + 1];
        boolean pages = false, hasRtl = false;
        int direction = 0;
        missingGlyphs = fallback = false;
        for (int c = 0; c < n; c++) {
            widths[c + 1] = widths[c];
            wire[c + 1] = wire[c];
            for (int i = boundaries[c]; i < boundaries[c + 1];) {
                int cp = VCUnicode.codePoint(source, i), props = data.props(cp);
                int g = props & 31;
                if (i == boundaries[c] && (g == 4 || g == 8) && (props & 65536) == 0)
                    widths[c + 1] += data.advance(0x25CC); // shaper's dotted-circle base for an orphan mark
                int advance = (props & 65536) != 0 ? 0 : VCTextData.range(data.advances, cp, -1);
                if (advance < 0) { missingGlyphs = true; advance = data.unknownAdvance; }
                widths[c + 1] += advance;
                wire[c + 1] += VCUnicode.bytes(cp);
                if ((props & 131072) != 0) hasRtl = true;
                if (direction == 0) {
                    if ((props & 131072) != 0) direction = 2;
                    else if ((props & 262144) != 0) direction = 1;
                }
                // Contextual/RTL text uses readable overlapping pages in logical
                // order. The native VC still performs bidi and shaping itself.
                if ((props & 131072) != 0 || (cp >= 0x900 && cp <= 0x1CFF)
                        || (cp >= 0xA800 && cp <= 0xABFF) || (cp >= 0x11000 && cp <= 0x11FFF)) pages = true;
                i += VCUnicode.chars(cp);
            }
        }
        // Keep the source paragraph's first-strong direction when a later page
        // starts with digits or the opposite script. Never reverse source text.
        directionMark = hasRtl ? (direction == 2 ? "\u200F" : "\u200E") : "";
        int availableWidth = WIDTH_64 - width(prefix) - width(suffix);
        int availableBytes = MAX_BYTES - bytes(prefix) - bytes(suffix) - bytes(directionMark);
        for (int c = 0; c < n; c++) {
            if (widths[c + 1] - widths[c] > availableWidth || wire[c + 1] - wire[c] > availableBytes) {
                // A single extended grapheme cannot be divided safely. Keep the
                // complete source in memory, show one explicit static fallback.
                fallback = true;
                break;
            }
        }
        if (fallback || n == 0) {
            starts = new int[] {0}; ends = new int[] {0}; dwell = new int[] {HOLD_MS};
            return;
        }
        int[] first = new int[n], last = new int[n], times = new int[n];
        int count = 0, end = 0;
        for (int start = 0; start < n;) {
            if (end < start) end = start;
            while (end < n && widths[end + 1] - widths[start] <= availableWidth
                    && wire[end + 1] - wire[start] <= availableBytes) end++;
            int visibleEnd = end;
            if (pages && end < n) {
                // Prefer a complete word, but retain at least 2/3 of the row.
                for (int c = end; c > start + (end - start) * 2 / 3; c--) {
                    if (source.charAt(boundaries[c] - 1) == ' ') { visibleEnd = c; break; }
                }
            }
            first[count] = boundaries[start];
            last[count] = boundaries[visibleEnd];
            int next = pages ? start + Math.max(1, (visibleEnd - start) * 3 / 4) : start + 1;
            if (pages) {
                for (int c = next; c < visibleEnd; c++) {
                    if (source.charAt(boundaries[c] - 1) == ' ') { next = c; break; }
                }
            }
            long step = (widths[next] - widths[start]) * 1000L / (24 * 64);
            times[count] = pages ? Math.max(HOLD_MS, (int) step) : Math.max(MIN_STEP_MS, (int) step);
            count++;
            if (visibleEnd == n) break;
            start = next;
        }
        starts = new int[count]; ends = new int[count]; dwell = new int[count];
        System.arraycopy(first, 0, starts, 0, count);
        System.arraycopy(last, 0, ends, 0, count);
        System.arraycopy(times, 0, dwell, 0, count);
        dwell[0] = Math.max(dwell[0], HOLD_MS);
        dwell[count - 1] = Math.max(dwell[count - 1], HOLD_MS);
    }

    boolean isFallback() { return fallback; }

    /** Negative means no scheduled work. Clock corrections rebase the hold;
     * a delayed wake advances at most one frame, never bursts a backlog. */
    long waitMillis(long now) {
        if (starts.length == 0 || (!pending && starts.length == 1)) return -1L;
        if (lastClock != 0 && (now < lastClock || now - lastClock > 60000L)) {
            deadline = now + (pending ? RETRY_MS : dwell[frameIndex]);
        }
        lastClock = now;
        return deadline <= now ? 0 : deadline - now;
    }

    String next(long now) {
        if (waitMillis(now) != 0) return null;
        if (!pending) {
            frameIndex = (frameIndex + 1) % starts.length;
            pending = true;
            pendingText = null;
        }
        if (pendingText == null) {
            pendingText = directionMark + prefix + (fallback ? "\u2026" : source.substring(starts[frameIndex], ends[frameIndex])) + suffix;
            if (pendingText.length() == 0) pendingText = "\u2026";
        }
        return pendingText;
    }

    /** Used by initial/replay publication, preserving the current fragment. */
    String current() {
        if (pendingText == null) {
            pendingText = directionMark + prefix + (fallback ? "\u2026" : source.substring(starts[frameIndex], ends[frameIndex])) + suffix;
            if (pendingText.length() == 0) pendingText = "\u2026";
        }
        return pendingText;
    }

    void sent(long now) {
        if (pending) deadline = now + dwell[frameIndex];
        pending = false;
        lastClock = now;
    }

    void failed(long now) {
        pending = true;
        deadline = now + RETRY_MS;
        lastClock = now;
    }
}
