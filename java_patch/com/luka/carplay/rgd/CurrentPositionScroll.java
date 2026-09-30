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
    private int scrollDirection = 1;
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
        scrollDirection = 1;
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
        scrollDirection = 1;
        pendingText = null;
        pending = true;
        deadline = lastClock = 0;
    }

    static int width(String text) {
        VCTextData data = VCTextData.get();
        int result = 0;
        for (int i = 0; i < text.length();) {
            int cp = VCUnicode.codePoint(text, i);
            result += data.advance(cp);
            i += VCUnicode.chars(cp);
        }
        return result;
    }

    static int bytes(String text) {
        int result = 0;
        for (int i = 0; i < text.length();) {
            int cp = VCUnicode.codePoint(text, i);
            result += VCUnicode.bytes(cp);
            i += VCUnicode.chars(cp);
        }
        return result;
    }

    /**
     * Truncates `name` at a grapheme boundary and appends an ellipsis so that
     * prefix + result + suffix fits in the SAME single-page width (WIDTH_64)
     * and byte (MAX_BYTES) budget that build() uses for paging -- including
     * the RTL paragraph-direction mark build() prepends (see directionMarkFor,
     * the exact same detection build() runs on `source`), so the combined
     * text this produces never triggers multi-page scrolling.  Returns `name`
     * unchanged (no ellipsis) if it already fits.  `prefix` and `suffix` are
     * measured verbatim and never truncated themselves; only `name` is
     * shortened.  As a final guarantee, the candidate is re-checked against
     * build()'s own authoritative page-count logic (via a scratch instance)
     * and trimmed one more grapheme at a time until it genuinely fits.
     */
    static String fitWithEllipsis(String name, String prefix, String suffix) {
        if (name == null) name = "";
        if (prefix == null) prefix = "";
        if (suffix == null) suffix = "";
        String normalizedName = VCUnicode.nfc(name);
        String normalizedSuffix = VCUnicode.nfc(suffix);
        if (fitsOnePage(normalizedName + normalizedSuffix, prefix)) {
            return name;
        }
        VCTextData data = VCTextData.get();
        String ellipsis = data.covered(0x2026) ? "…" : "...";
        // Same RTL paragraph-direction mark, and same bytes-only reservation for it,
        // that build() computes and subtracts (build()'s availableWidth does NOT
        // subtract directionMark width either -- matched here on purpose).
        String directionMark = directionMarkFor(normalizedName + normalizedSuffix);
        int availableWidth = WIDTH_64 - width(prefix) - width(normalizedSuffix);
        int availableBytes = MAX_BYTES - bytes(prefix) - bytes(normalizedSuffix) - bytes(directionMark);
        int fitWidth = availableWidth - width(ellipsis);
        int fitBytes = availableBytes - bytes(ellipsis);
        int[] boundaries = VCUnicode.boundaries(normalizedName);
        int n = boundaries.length - 1;
        int cutIndex = 0;
        if (fitWidth > 0 && fitBytes > 0) {
            int w = 0, b = 0;
            for (int c = 0; c < n; c++) {
                int segW = 0, segB = 0;
                for (int i = boundaries[c]; i < boundaries[c + 1];) {
                    int cp = VCUnicode.codePoint(normalizedName, i);
                    segW += data.advance(cp);
                    segB += VCUnicode.bytes(cp);
                    i += VCUnicode.chars(cp);
                }
                if (w + segW > fitWidth || b + segB > fitBytes) break;
                w += segW;
                b += segB;
                cutIndex = c + 1;
            }
        }
        /* Safety net: the budget estimate above can still disagree with build()'s own page-count
         * decision at the margin (e.g. this exact candidate's directionMark differs once the cut
         * point changes the text's first-strong character, or build()'s word-boundary page
         * splitting lands differently near the limit).  Re-run build() itself -- via a scratch
         * instance, so it is the same code path production uses, not a re-implementation -- and
         * keep trimming one more grapheme at a time until it genuinely reports a single page. */
        while (cutIndex > 0 && !fitsOnePage(
                normalizedName.substring(0, boundaries[cutIndex]) + ellipsis + normalizedSuffix, prefix)) {
            cutIndex--;
        }
        if (cutIndex <= 0) return ellipsis;
        return normalizedName.substring(0, boundaries[cutIndex]) + ellipsis;
    }

    /** RTL paragraph-direction mark build() prepends for this text: U+200F when the text's
     * first-strong character is RTL, U+200E when it is LTR-but-contains-some-RTL, or "" when
     * the text has no RTL codepoints at all.  Byte-for-byte the same bit tests and priority
     * order build() uses on `source`, factored out so fitWithEllipsis can reserve an identical
     * budget to what build() will actually render for the same text. */
    private static String directionMarkFor(String text) {
        VCTextData data = VCTextData.get();
        boolean hasRtl = false;
        int direction = 0;
        for (int i = 0; i < text.length();) {
            int cp = VCUnicode.codePoint(text, i);
            int props = data.props(cp);
            if ((props & 131072) != 0) hasRtl = true;
            if (direction == 0) {
                if ((props & 131072) != 0) direction = 2;
                else if ((props & 262144) != 0) direction = 1;
            }
            i += VCUnicode.chars(cp);
        }
        return hasRtl ? (direction == 2 ? "‏" : "‎") : "";
    }

    /** Authoritative single-page check: actually runs build() (via a disposable scratch
     * CurrentPositionScroll) on this exact (text, prefix) pair -- `after` is always "" for
     * every real caller, since the pinned suffix is embedded in `text` itself -- and reports
     * whether it would render as one page (no scrolling) with no fallback collapse. */
    private static boolean fitsOnePage(String text, String prefix) {
        CurrentPositionScroll probe = new CurrentPositionScroll();
        probe.configure(text, prefix, "", true);
        return !probe.fallback && probe.starts.length <= 1;
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
            if (starts.length > 1) {
                int next = frameIndex + scrollDirection;
                if (next >= starts.length) {
                    scrollDirection = -1;
                    next = frameIndex - 1;
                } else if (next < 0) {
                    scrollDirection = 1;
                    next = frameIndex + 1;
                }
                frameIndex = next;
            }
            pending = true;
            pendingText = null;
        }
        if (pendingText == null) {
            pendingText = formatFrame(frameIndex);
        }
        return pendingText;
    }

    private String formatFrame(int index) {
        String body = fallback ? "\u2026" : source.substring(starts[index], ends[index]);
        String text = directionMark + prefix + body + suffix;
        return text.length() == 0 ? "\u2026" : text;
    }

    /** Used by initial/replay publication, preserving the current fragment. */
    String current() {
        if (pendingText == null) {
            pendingText = formatFrame(frameIndex);
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
