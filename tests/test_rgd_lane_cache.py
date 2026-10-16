#!/usr/bin/env python3
"""Native lane-event cache: the real rgd_hook.c state, reset and lane-slot allocator.

Lane events are keyed by their own index.  Pre-sent events must survive any
maneuver list, and a full cache drops the lowest index except the active one
(the way iOS CarKitNavigation drops its own sent-cache).  Prints PASS.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build/rgd-lane-cache'
BUILD.mkdir(parents=True, exist_ok=True)
hook = (ROOT / 'hook/routeguidance/rgd_hook.c').read_text()

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
    /* route start: iOS sends lane events 0..5, maneuvers are already at 4..18 */
    for (uint16_t i = 0; i < 6; i++) assert(rgd_lane_slot_for_iap_index(i, true) >= 0);
    g_rgd.current_list_present = true; g_rgd.current_list_count = 2;
    g_rgd.current_list[0] = 17; g_rgd.current_list[1] = 18;
    for (uint16_t i = 0; i < 6; i++) assert(has(i));            /* no maneuver-index pruning */
    /* fill the cache, event 3 is active */
    g_rgd.update_cache.present |= RGD_UPD_LANE_INDEX; g_rgd.update_cache.lane_guidance_index = 3;
    for (uint16_t i = 6; i < MANEUVER_CACHE_SIZE; i++) assert(rgd_lane_slot_for_iap_index(i, true) >= 0);
    assert(rgd_lane_slot_for_iap_index(100, true) >= 0);        /* full: drops 0, the lowest */
    assert(!has(0) && has(1) && has(3) && has(100));
    assert(rgd_lane_slot_for_iap_index(101, true) >= 0);        /* then 1 */
    assert(rgd_lane_slot_for_iap_index(102, true) >= 0);        /* then 2 */
    assert(rgd_lane_slot_for_iap_index(103, true) >= 0);        /* 3 is active: then 4 */
    assert(!has(1) && !has(2) && has(3) && !has(4) && has(5));
    /* a lookup changes nothing, a new route empties it */
    int s = rgd_lane_slot_for_iap_index(5, false);
    assert(s == rgd_lane_slot_for_iap_index(5, false));
    rgd_maneuver_map_reset();
    assert(!has(3) && !has(100));
    puts("lane cache: lanes kept across maneuver lists, full cache drops the lowest non-active index PASS");
    return 0;
}
'''
(BUILD / 'lane_cache.c').write_text(c)
cc = ['cc', '-std=c99', '-O1', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-Wno-unused-variable',
      '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-DENABLE_LOGGING=0',
      '-I' + str(ROOT / 'hook'), str(BUILD / 'lane_cache.c'), str(ROOT / 'hook/routeguidance/rgd_tlv.c'),
      '-o', str(BUILD / 'lane_cache')]
subprocess.run(cc, check=True)
sys.exit(subprocess.run([str(BUILD / 'lane_cache')]).returncode)
