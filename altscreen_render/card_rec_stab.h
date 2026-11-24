/*
 * CarPlay instruction-card recogniser: the time-driven feed of the temporal
 * rule.  Included by
 * card_rec.h - include that, not this.
 *
 * card_rec_stab_push() counts FRAMES; its constants were tuned on a steady
 * 30 fps stream.  iOS sends the cluster card stream as a ~0.5 s burst at
 * 60 fps (~32 frames) when the card changes, then nothing while it stays: a
 * burst counts double, the silence not at all, so a card that needs
 * SPLIT_K * n reads never shows and ages / the V_MAX bound stand still.  This
 * feed drives the unchanged frame rule by wall time instead:
 *
 *   ticks    t_ms (one monotonic millisecond clock, read when the call is made;
 *            the calls serialised) is rounded to the nearest 30 fps tick
 *            (100 ms = 3 ticks, exact integers) of a grid anchored at the first
 *            frame; the rule is pushed once per tick, so frames exactly
 *            33.333 ms apart give its decisions bit for bit.
 *   in-tick  several frames in one tick: a repeat of the tick's previous frame
 *            (an input the rule cannot tell from it) is not pushed again, so a
 *            60 fps burst of one card counts once per tick; any other frame is
 *            pushed on top.  No change is ever hidden: the rule sees every real
 *            frame but such repeats, so every run reset, withdrawal or new
 *            candidate it would see (a REJECT, another key, an upward jump,
 *            another text) still happens, whichever side of a tick it fell.
 *   silence  a tick without a frame replays the newest real result (the screen
 *            still shows it: the evidence a steady stream gives), but only once
 *            that result was read in CONF distinct ticks with no other result
 *            since.  A burst ends on the settled card (the simulator's: 11-34
 *            frames, all read alike), so CONF = 3 ticks (3 frames at 30 fps,
 *            4-5 at 60 fps) costs nothing there; a stall on 1-2 alike frames
 *            never publishes them.  Unconfirmed, a tick is time only: ages
 *            advance (V_MAX, HIST_AGE, FRESH_MAX stay wall time), the run,
 *            candidate and shown maneuver do not move.  advance() completes
 *            ticks only (a frame may still come in the current one).
 *   bound    after UNIT_K * n + 1 replays of one result a push moves counters
 *            only (every need is met); the rest of a gap is added at once,
 *            saturating, equal to pushing it: a gap of any length costs at
 *            most UNIT_K * n + 1 pushes.
 *   late     a frame stamped at or up to LATE ticks (1 s) before the newest
 *            tick (latency, a stale stamp) counts now: in that tick if it holds
 *            a real frame, else in the next - its evidence is never dropped.
 *            advance() at or before the newest tick does nothing (no evidence).
 *   reset    push_at(NULL) is card_rec_stab_push(NULL) (withdraw, run reset,
 *            memory / history kept) and drops the replay source and the grid:
 *            the next frame anchors a new one, in phase with its stream, and
 *            first ages the state by the ticks since (prev: wall time across
 *            a reset, too).  A frame more than LATE ticks back (the clock went
 *            back: a new stream) does the same, but the gap is unknown, so not
 *            aged.  advance() never resets, and does nothing without a grid.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#ifndef ALTSCREEN_CARD_REC_STAB_H
#define ALTSCREEN_CARD_REC_STAB_H

#ifndef ALTSCREEN_CARD_REC_H
#error "include card_rec.h"
#endif

#define CARD_REC_STAB_CONF      3       /* distinct ticks a read needs before silence replays it */
#define CARD_REC_STAB_LATE      30      /* ticks (1 s) a frame may lag the newest tick and count now */

typedef struct {
    card_rec_stab_t core;               /* the frame rule: once per tick, again per change in one */
    card_rec_result_t last;             /* newest real result: what the screen shows */
    int64_t t0, tick, last_tick;        /* grid origin (ms); newest tick done; tick of `last` */
    int started, open, prev;            /* grid set; `tick` holds a real frame (`last`); */
                                        /* !started: t0 / tick are the old grid's (age the gap) */
    int have_last, conf;                /* `last` valid; ticks it was read in a row (<= CONF) */
    uint32_t steps;                     /* core pushes made (diagnostic, wraps) */
} card_rec_stab_clock_t;

void card_rec_stab_clock_init(card_rec_stab_clock_t *c, int n);   /* n as card_rec_stab_init */
/* One decoded frame's result r at t_ms (r NULL: stream reset).  Returns 1 and
 * fills *shown (may be NULL) when the HUD may show a maneuver now, else 0. */
int card_rec_stab_push_at(card_rec_stab_clock_t *c, const card_rec_result_t *r, int64_t t_ms,
                          card_rec_shown_t *shown);
/* No frame: completes the ticks before t_ms.  Call it periodically (e.g. every
 * 50 ms) while frames are absent - a card can publish here.  Returns as push_at. */
int card_rec_stab_advance(card_rec_stab_clock_t *c, int64_t t_ms, card_rec_shown_t *shown);

#endif /* ALTSCREEN_CARD_REC_STAB_H */
