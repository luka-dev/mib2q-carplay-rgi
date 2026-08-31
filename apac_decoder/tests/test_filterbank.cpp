// Differential test: apac::SynthesisFilterBank against
// APACSynthesisFilterBank in the real library.
//
// FIXTURE: tests/fixtures/filterbank.fixture, produced by building the
// library's own 184-byte object by hand (it has no exported constructor),
// calling Initialize (0xae980), Reset (0xae530) and then
// FrequencyToTimeInPlace (0xae5b0) once per step.
//
//   regenerate:  apac_decoder/tools/gen_fixtures_transform.sh   (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `fb`
//   inputs:      apac_decoder/reference/dsp_cases.h, shared with the oracle
//
// This is a STATEFUL test and that is the point.  The filter bank carries two
// things across calls, per channel: the overlap-add tail, and the window SHAPE
// the previous frame used - the rising half of frame k is windowed with frame
// k-1's shape, not its own.  The 30-step plan in dsp_cases.h interleaves three
// channels, walks all four window sequences, flips the shape mid-stream, and
// includes a step that changes shape while its channel is mid-transition.  If
// the port kept only one shape, or kept it globally instead of per channel, or
// applied the current shape to the rising half, the sequence would diverge and
// never recover.
//
// The alternate short-block layout is covered by the eight-short steps: eight
// 128-point transforms hopped by 128 with the falling half read backwards, and
// the 448-sample flat shoulders on either side.
//
// Tolerance and why there is one: tests/dsp_fixture.h.

#include "apac/filterbank.h"

#include "../reference/dsp_cases.h"
#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const uint32_t kFrame = 1024;

float g_buf[kFrame];
float g_want[kFrame];

int g_steps = 0;
int g_floats = 0;
double g_worst = 0.0;
char g_worstWhere[128] = { 0 };
int g_seen[4] = { 0, 0, 0, 0 };     // per window sequence
int g_shape[2] = { 0, 0 };
// Running peak input magnitude per channel: the overlap tail of every earlier
// frame on that channel is still contributing, so the error budget has to be
// measured against the largest thing that has entered the channel so far.
float g_inPeak[apac_cases::kFBChannels] = { 0.0f };

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/filterbank.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    apac::SynthesisFilterBank fb;
    char line[65536];
    int step = -1;
    uint32_t got = 0;
    bool collecting = false;
    bool initialised = false;
    int rejectChecks = 0;

    struct Check {
        static void run(int step, apac::SynthesisFilterBank* fb)
        {
            const apac_cases::FBStep& st = apac_cases::fb_plan()[step];
            apac_cases::case_fill(st.caseIdx, g_buf, kFrame);
            const float inScale = dspfx::peak(g_buf, kFrame);
            if (inScale > g_inPeak[st.channel]) g_inPeak[st.channel] = inScale;
            int32_t rc = fb->FrequencyToTimeInPlace(st.channel, g_buf,
                                                    st.shape, st.sequence);
            assert(rc == 0);
            char what[128];
            snprintf(what, sizeof what, "step %d (ch %u, shape %u, seq %u, case %s)",
                     step, st.channel, st.shape, st.sequence,
                     apac_cases::case_name(st.caseIdx));
            double w = dspfx::compare(g_buf, g_want, kFrame, what,
                                      g_inPeak[st.channel]);
            if (w > g_worst) {
                g_worst = w;
                snprintf(g_worstWhere, sizeof g_worstWhere, "%s", what);
            }
            g_seen[st.sequence & 3]++;
            g_shape[st.shape & 1]++;
            g_floats += (int)kFrame;
            ++g_steps;
        }
    };

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "FB ", 3)) {
            int rc;
            unsigned ch, fr;
            if (sscanf(line, "FB init=%d channels=%u frame=%u", &rc, &ch, &fr) != 3) {
                fprintf(stderr, "bad FB line: %s", line);
                return 1;
            }
            assert(ch == apac_cases::kFBChannels && fr == kFrame);
            int32_t ours = fb.Initialize(ch, fr);
            if (ours != rc) {
                fprintf(stderr, "\nMISMATCH Initialize: port %d, library %d\n", ours, rc);
                return 1;
            }
            fb.Reset();
            initialised = true;
            continue;
        }

        if (!strncmp(line, "STEP ", 5)) {
            if (step >= 0 && got == kFrame) Check::run(step, &fb);
            unsigned ch, shape, seq, ci;
            int rc;
            if (sscanf(line, "STEP %d ch=%u shape=%u seq=%u case=%u rc=%d",
                       &step, &ch, &shape, &seq, &ci, &rc) != 6) {
                fprintf(stderr, "bad STEP line: %s", line);
                return 1;
            }
            assert(rc == 0);
            // The fixture's own idea of the plan must match dsp_cases.h, or the
            // inputs the two sides used were different.
            const apac_cases::FBStep& st = apac_cases::fb_plan()[step];
            assert(st.channel == ch && st.shape == shape &&
                   st.sequence == seq && st.caseIdx == ci);
            got = 0;
            collecting = false;
            continue;
        }

        if (!strncmp(line, "OUT", 3)) { collecting = true; continue; }

        if (!strncmp(line, "REJECT ", 7)) {
            if (step >= 0 && got == kFrame) Check::run(step, &fb);
            step = -1;
            collecting = false;
            char which[32];
            int rc;
            if (sscanf(line, "REJECT %31s rc=%d", which, &rc) != 2) {
                fprintf(stderr, "bad REJECT line: %s", line);
                return 1;
            }
            int32_t ours = !strcmp(which, "badchannel")
                ? fb.FrequencyToTimeInPlace(apac_cases::kFBChannels, g_buf, 0, 0)
                : fb.FrequencyToTimeInPlace(0, g_buf, 0, 4);
            if (ours != rc) {
                fprintf(stderr, "\nMISMATCH REJECT %s: port %d, library %d\n",
                        which, ours, rc);
                return 1;
            }
            ++rejectChecks;
            continue;
        }

        if (collecting)
            got += dspfx::parse_hex_floats(line, g_want, kFrame, got);
    }
    if (step >= 0 && got == kFrame) Check::run(step, &fb);
    fclose(f);

    assert(initialised);
    assert(g_steps == apac_cases::kFBSteps);
    assert(rejectChecks == 2);
    for (int i = 0; i < 4; ++i) assert(g_seen[i] > 0);   // every window sequence
    assert(g_shape[0] > 0 && g_shape[1] > 0);            // both window shapes

    // Configuration gates.  frameLength must be 1024: the short window is
    // looked up at frameLength >> 3, and only 128 and 1024 exist.
    apac::SynthesisFilterBank other;
    assert(other.Initialize(2, 128) != 0);
    assert(other.Initialize(2, 2048) != 0);
    assert(other.Initialize(0, 1024) != 0);
    assert(other.Initialize(apac::kFilterBankMaxChannels + 1, 1024) != 0);
    assert(other.Initialize(8, 1024) == 0);

    // Reset really clears: a long frame decoded straight after Reset must have
    // no tail from the previous one.
    static float a[kFrame], b[kFrame];
    apac_cases::case_fill(9, a, kFrame);
    assert(other.FrequencyToTimeInPlace(0, a, 0, 0) == 0);
    other.Reset();
    apac_cases::case_fill(0, b, kFrame);            // all zeros in
    assert(other.FrequencyToTimeInPlace(0, b, 0, 0) == 0);
    for (uint32_t i = 0; i < kFrame; ++i) assert(b[i] == 0.0f);

    printf("OK  %d steps, %d floats, seq %d/%d/%d/%d, worst %.2g of full scale\n",
           g_steps, g_floats, g_seen[0], g_seen[1], g_seen[2], g_seen[3], g_worst);
    return 0;
}
