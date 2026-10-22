/* /mnt/app/carplay_layout.cfg parser and Maps URL builder (hook/altscreen/cluster_layout.h). */
#include "../hook/altscreen/cluster_layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const cl_rect_t k_def[CL_AREA_COUNT] = {
    { 270, 90, 900, 290 }, { 90, 90, 360, 300 }, { 540, 90, 360, 300 }
};

static int rect_eq(cl_rect_t r, int x, int y, int w, int h)
{
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

static void parse(cl_layout_t *l, const char *text)
{
    cl_layout_defaults(l, k_def);
    cl_layout_parse(l, text, strlen(text));
}

int main(void)
{
    cl_layout_t l;
    char off[192], on[192];

    /* No file or an empty one: luka's fixed URLs, byte for byte, and the built-in areas. */
    parse(&l, "");
    assert(cl_url_rgi_off(&l, off, sizeof(off)) == 0 && cl_url_rgi_on(&l, on, sizeof(on)) == 0);
    assert(!strcmp(off, "maps:/car/instrumentcluster?showETA=yes&maneuverLayout=rightAligned&showSpeedLimit=no"));
    assert(!strcmp(on, "maps:/car/instrumentcluster/map?showETA=no&maneuverLayout=no&showSpeedLimit=no"));
    assert(rect_eq(l.safe[0], 270, 90, 900, 290) && rect_eq(l.safe[1], 90, 90, 360, 300) &&
           rect_eq(l.safe[2], 540, 90, 360, 300));
    assert(!l.safe_from_file && !l.options_from_file && !l.rejected);
    /* Every built-in area passes its own validation. */
    assert(cl_rect_valid(&k_def[0]) && cl_rect_valid(&k_def[1]) && cl_rect_valid(&k_def[2]));

    /* Q7 4M values from the aberbic issue #5 review (1440x540 canvas), comments,
     * CRLF, tabs, mixed case and Java-only keys. */
    parse(&l, "# Q7\r\nFULL 360 87 720 297\r\n\tclassic 520 87 400 297 # centred\r\n"
              "sport 114 88 350 296\nview_source vc\ncompact classic\n");
    assert(rect_eq(l.safe[0], 360, 87, 720, 297) && rect_eq(l.safe[1], 114, 88, 350, 296) &&
           rect_eq(l.safe[2], 520, 87, 400, 297));
    assert(l.safe_from_file == 7u && l.rejected == 0);

    /* Bad lines are dropped one by one; the rest of the file still applies. */
    parse(&l, "full 0 0 1440 543\n"          /* below the panel */
              "sport 90 90 360 250\n"        /* narrow and too short for the card layout */
              "classic 1200 90 360 300\n"    /* off the right edge */
              "full 10 10 164 300\n"         /* narrower than the card platter */
              "full 270 90 900\n"            /* missing height */
              "full -1 90 900 290\n"         /* negative */
              "full 270 90 900 290 1\n"      /* extra field */
              "cards middle\nspeed_limit maybe\nzoom 3\n"
              "full 0 0 1440 542\n"          /* whole panel is legal */
              "sport 0 100 500 210\n");      /* wide enough that 210 px is fine */
    assert(l.rejected == 10 && l.first_bad_line == 1);
    assert(rect_eq(l.safe[0], 0, 0, 1440, 542) && rect_eq(l.safe[1], 0, 100, 500, 210) &&
           rect_eq(l.safe[2], 540, 90, 360, 300));
    assert(l.safe_from_file == 3u && !l.options_from_file);

    /* Card options shape only the RGI-off scene; RGI on stays map-only. */
    parse(&l, "cards left\neta no\nspeed_limit yes\n");
    assert(cl_url_rgi_off(&l, off, sizeof(off)) == 0 && cl_url_rgi_on(&l, on, sizeof(on)) == 0);
    assert(!strcmp(off, "maps:/car/instrumentcluster?showETA=no&maneuverLayout=leftAligned&showSpeedLimit=yes"));
    assert(!strcmp(on, "maps:/car/instrumentcluster/map?showETA=no&maneuverLayout=no&showSpeedLimit=yes"));
    parse(&l, "cards Center\n");
    assert(cl_url_rgi_off(&l, off, sizeof(off)) == 0);
    assert(strstr(off, "maneuverLayout=topAligned") && l.options_from_file);
    parse(&l, "cards top\neta off\n");
    assert(l.cards == CL_CARDS_CENTER && l.eta == 0);

    /* Later lines win; a too-small buffer is reported, never truncated silently. */
    parse(&l, "cards left\ncards right\n");
    assert(l.cards == CL_CARDS_RIGHT);
    assert(cl_url_rgi_off(&l, off, 20) == -1);

    /* A line with more than six tokens is rejected without overrunning the token array. */
    parse(&l, "full 1 2 3 4 5 6 7 8\n");
    assert(l.rejected == 1 && !l.safe_from_file);
    /* No trailing newline, and a file that is only a comment. */
    parse(&l, "eta no");
    assert(l.eta == 0);
    parse(&l, "#full 0 0 1440 542");
    assert(!l.safe_from_file && !l.rejected);

    /* Car profile (/tmp/carplay_layout.vehicle) between built-in and the user file. */
    {
        const char *q7 = "# layout de.esolutions...LayoutMIB2HighQ7\nfull    360 87 720 297\n"
                         "sport   114 88 350 296\nclassic 520 87 400 297\n";
        cl_layout_build(&l, k_def, q7, strlen(q7), NULL, 0);
        assert(rect_eq(l.safe[0], 360, 87, 720, 297) && rect_eq(l.safe[1], 114, 88, 350, 296) &&
               rect_eq(l.safe[2], 520, 87, 400, 297));
        assert(l.safe_from_vehicle == 7u && l.safe_from_file == 0u && l.car_profile == 1);
        /* the user file wins per area and keeps the rest of the profile */
        cl_layout_build(&l, k_def, q7, strlen(q7), "classic 540 90 360 300\ncards left\n", 34);
        assert(rect_eq(l.safe[2], 540, 90, 360, 300) && rect_eq(l.safe[0], 360, 87, 720, 297));
        assert(l.safe_from_vehicle == 3u && l.safe_from_file == 4u && l.cards == CL_CARDS_LEFT);
        /* car_profile off: back to built-in, wherever the line sits in the file */
        cl_layout_build(&l, k_def, q7, strlen(q7), "eta no\ncar_profile off\n", 23);
        assert(rect_eq(l.safe[0], 270, 90, 900, 290) && rect_eq(l.safe[1], 90, 90, 360, 300) &&
               !l.safe_from_vehicle && l.car_profile == 0 && l.eta == 0);
        /* no profile file (A5/B9): built-in */
        cl_layout_build(&l, k_def, NULL, 0, NULL, 0);
        assert(rect_eq(l.safe[0], 270, 90, 900, 290) && !l.safe_from_vehicle);
        /* a broken profile line is ignored, the user file's rejections are the only ones reported */
        cl_layout_build(&l, k_def, "full 1 2 3\nsport 114 88 350 296\n", 32, "car_profile maybe\n", 18);
        assert(rect_eq(l.safe[0], 270, 90, 900, 290) && rect_eq(l.safe[1], 114, 88, 350, 296));
        assert(l.rejected == 1 && l.car_profile == 1);
    }

    puts("altscreen_cluster_layout_test: defaults = luka URLs/areas, Q7 areas, per-line rejection, "
         "card/ETA/speed-limit URLs, car profile PASS");
    return 0;
}
