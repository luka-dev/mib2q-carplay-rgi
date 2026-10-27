#!/usr/bin/env python3
"""Native lane-event cache: the real rgd_hook.c state, reset and lane-slot allocator.

Lane events are keyed by their own index.  Pre-sent events must survive any
maneuver list; only a 0x5201's current index drops events below it; a full
cache drops the oldest entry except the active one.  Prints PASS.
"""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/rgd-lane-cache'
BUILD.mkdir(parents=True, exist_ok=True)
HOOK = Path(os.environ.get('HOOK_SRC', ROOT / 'hook'))   # another hook tree, e.g. a mutant
hook = (HOOK / 'routeguidance/rgd_hook.c').read_text()

# the maneuver-index prune was the "lanes only at route start" bug
assert 'prune_stale_lane' not in hook, 'lane cache pruned by maneuver index again'

state = hook[hook.index('static struct {'):hook.index('/* Forward declarations */')]
reset_start = hook.index('static void rgd_maneuver_map_reset(void) {')
reset = hook[reset_start:hook.index('static const uint64_t RGD_UPD_WRITE_MASK', reset_start)]
alloc_start = hook.index('static int rgd_lane_slot_for_iap_index(uint16_t idx, bool create) {')
alloc = hook[alloc_start:hook.index('static void rgd_update_session_active(void) {', alloc_start)]

c = r'''
#include "routeguidance/rgd_tlv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint64_t now_monotonic_ms(void) { return 100; }
''' + state + '\n' + reset + '\n' + alloc + r'''
static int has(uint16_t idx) { return rgd_lane_slot_for_iap_index(idx, false) >= 0; }
int main(void) {
    rgd_maneuver_map_reset();
    /* route start: iOS sends lane events 0..5, maneuvers are already at 17..18 */
    for (uint16_t i = 0; i < 6; i++) assert(rgd_lane_slot_for_iap_index(i, true) >= 0);
    g_rgd.current_list_present = true; g_rgd.current_list_count = 2;
    g_rgd.current_list[0] = 17; g_rgd.current_list[1] = 18;
    for (uint16_t i = 0; i < 6; i++) assert(has(i));            /* no maneuver-index pruning */

    /* a 0x5201 making event 3 current drops only lane events below 3 */
    g_rgd.update_cache.present |= RGD_UPD_LANE_INDEX; g_rgd.update_cache.lane_guidance_index = 3;
    rgd_drop_passed_lane_events(3);
    assert(!has(0) && !has(1) && !has(2) && has(3) && has(4) && has(5));

    /* full cache: the OLDEST entry goes, never the active one, and a lookup
     * never refreshes anything.  Events 100..128 arrive after 3, 4, 5. */
    for (uint16_t i = 100; i < 129; i++) assert(rgd_lane_slot_for_iap_index(i, true) >= 0);
    for (int k = 0; k < 5; k++) assert(has(4));                 /* lookups of the oldest */
    assert(rgd_lane_slot_for_iap_index(200, true) >= 0);        /* evicts 4: 3 is active */
    assert(has(3) && !has(4) && has(5) && has(200));
    assert(rgd_lane_slot_for_iap_index(201, true) >= 0);        /* then 5 */
    assert(rgd_lane_slot_for_iap_index(202, true) >= 0);        /* then 100 */
    assert(has(3) && !has(5) && !has(100) && has(101) && has(202));

    /* a reroute renumbers the lane source without a reset: its new low events
     * all survive (dropping the lowest index would evict each one in turn) */
    for (uint16_t i = 0; i < 6; i++) assert(rgd_lane_slot_for_iap_index(i, true) >= 0);
    for (uint16_t i = 0; i < 6; i++) assert(has(i));
    assert(has(3));

    rgd_maneuver_map_reset();
    assert(!has(3) && !has(200));
    puts("lane cache: kept across maneuver lists, passed events dropped only by the current index, oldest-first eviction sparing the active event, renumbered events kept, lookups pure PASS");
    return 0;
}
'''
(BUILD / 'lane_cache.c').write_text(c)
cc = ['cc', '-std=c99', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
      '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DENABLE_LOGGING=0',
      '-I' + str(HOOK), str(BUILD / 'lane_cache.c'), str(HOOK / 'routeguidance/rgd_tlv.c'),
      '-o', str(BUILD / 'lane_cache')]
subprocess.run(cc, check=True)
sys.exit(subprocess.run([str(BUILD / 'lane_cache')]).returncode)
