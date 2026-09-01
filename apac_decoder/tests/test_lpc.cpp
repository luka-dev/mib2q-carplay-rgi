/*
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

// Differential test: apac::LPCBase and the stateless half of apac::LSFTools
// against the real library.
//
// FIXTURE: tests/fixtures/lpc.fixture.
//
//   regenerate:  apac_decoder/tools/gen_fixtures_transform.sh   (needs Docker)
//   oracle:      apac_decoder/reference/apac_fn_ref.cpp, command `lpc`
//   inputs:      apac_decoder/reference/dsp_cases.h, shared with the oracle
//
// Ten entry points over 16 cases, orders 2 to 16:
//
//   LPC_Weighting (0xb4190), LevinsonDurbin (0xb4620), ComputeLPCs (0xb4930)
//   with and without the lag window; LSP_To_LSF (0xb5010), LSP_To_LPC
//   (0xb4460), both LSF_To_LSP overloads (0xb50f0, 0xb51c0),
//   get_LSFWeightingFactors (0xb4220), Condition_LSFs (0xb4390) and
//   ConditionLSFs (0x9c6e0).
//
// Two things this test SETTLED that the decompilation could not:
//
//   * LSP_To_LPC builds two half-order polynomials and Hex-Rays gave both stack
//     arrays the same base pointer, dropping one side of every merged
//     expression - so which polynomial takes the difference recursion and which
//     the sum was not readable.  It is decided here, against the library.
//   * The trig is DOUBLE precision and divides by the literal 3.14159265, not
//     by M_PI.  Using M_PI, or doing it in float, misses by far more than the
//     tolerance.

#include "apac/lpc.h"

#include "../reference/dsp_cases.h"
#include "dsp_fixture.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const float kFs = 48000.0f;

float g_want[64];
float g_have[64];
float g_in[64];
float g_out[64];
float g_win[apac_cases::kLPCSignal + 64];
float g_sig[apac_cases::kLPCSignal + 64];

int g_checks = 0;
double g_worst = 0.0;
char g_worstWhere[128] = { 0 };

void expect(const char* what, unsigned c, uint32_t n, float scale)
{
    char label[128];
    snprintf(label, sizeof label, "%s case %u (order %u)", what, c,
             apac_cases::lpc_plan()[c].order);
    double w = dspfx::compare(g_have, g_want, n, label, scale);
    if (w > g_worst) {
        g_worst = w;
        snprintf(g_worstWhere, sizeof g_worstWhere, "%s", label);
    }
    ++g_checks;
}

} // namespace

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "tests/fixtures/lpc.fixture";
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }

    char line[65536];
    unsigned caseIdx = 0;
    uint32_t order = 0;
    char section[32] = { 0 };
    uint32_t wantN = 0, got = 0;

    apac::LPCBase lpc;
    apac::Autocorr ac;
    apac::LSFTools lsf;
    memset(&ac, 0, sizeof ac);
    lsf.order = 0;
    lsf.sampleRate = kFs;

    // Runs the port's version of whichever section just finished reading.
    struct Run {
        static void go(const char* section, unsigned c, uint32_t order,
                       apac::LPCBase* lpc, apac::Autocorr* ac, apac::LSFTools* lsf)
        {
            if (!strcmp(section, "WEIGHT")) {
                apac_cases::lpc_fill_autocorr(c, g_in, order);
                memset(g_have, 0, sizeof g_have);
                apac::LPCBase::LPC_Weighting(g_in, g_have, 0.92f, (uint16_t)order);
                expect("LPC_Weighting", c, order + 1, dspfx::peak(g_in, order + 1));
                return;
            }
            if (!strcmp(section, "LEVINSON")) {
                apac_cases::lpc_fill_autocorr(c, g_in, order);
                memset(g_out, 0, sizeof g_out);
                lpc->Configure(order, g_out, ac);
                lpc->LevinsonDurbin(g_in);
                memcpy(g_have, g_out, sizeof(float) * (order + 1));
                expect("LevinsonDurbin", c, order + 1, 1.0f);
                return;
            }
            if (!strncmp(section, "COMPUTE", 7)) {
                const bool lagWindow = (section[8] == '1');
                ac->order = order;
                apac_cases::lpc_fill_window(c, g_win, apac_cases::kLPCSignal);
                ac->window = g_win;
                ac->windowLength = apac_cases::kLPCSignal;
                apac_cases::lpc_fill_signal(c, g_sig, apac_cases::kLPCSignal);
                memset(g_out, 0, sizeof g_out);
                lpc->Configure(order, g_out, ac);
                lpc->ComputeLPCs(g_sig, lagWindow);
                memcpy(g_have, g_out, sizeof(float) * (order + 1));
                expect(lagWindow ? "ComputeLPCs(lag)" : "ComputeLPCs", c,
                       order + 1, 1.0f);
                return;
            }

            lsf->order = order;
            if (!strcmp(section, "LSP2LSF")) {
                apac_cases::lpc_fill_lsp(c, g_in, order);
                memset(g_have, 0, sizeof g_have);
                apac::LSFTools::LSP_To_LSF(g_in, g_have, (uint16_t)order, kFs);
                expect("LSP_To_LSF", c, order, kFs * 0.5f);
                return;
            }
            if (!strcmp(section, "LSP2LPC")) {
                apac_cases::lpc_fill_lsp(c, g_in, order);
                memset(g_have, 0, sizeof g_have);
                apac::LSFTools::LSP_To_LPC(g_in, g_have, (uint16_t)order);
                expect("LSP_To_LPC", c, order + 1, dspfx::peak(g_want, order + 1));
                return;
            }
            if (!strcmp(section, "LSF2LSP")) {
                apac_cases::lpc_fill_lsf(c, g_in, order, kFs);
                memset(g_have, 0, sizeof g_have);
                lsf->LSF_To_LSP(g_in, g_have, kFs);
                expect("LSF_To_LSP", c, order, 1.0f);
                return;
            }
            if (!strcmp(section, "LSF2LSPIP")) {
                apac_cases::lpc_fill_lsf(c, g_in, order, kFs);
                memcpy(g_have, g_in, sizeof(float) * order);
                lsf->LSF_To_LSP(g_have, kFs);
                expect("LSF_To_LSP in place", c, order, 1.0f);
                return;
            }
            if (!strcmp(section, "LSFWEIGHT")) {
                apac_cases::lpc_fill_lsf(c, g_in, order, kFs);
                memset(g_have, 0, sizeof g_have);
                apac::LSFTools::get_LSFWeightingFactors(g_in, g_have,
                                                        (uint16_t)order, kFs);
                expect("get_LSFWeightingFactors", c, order,
                       dspfx::peak(g_want, order));
                return;
            }
            if (!strcmp(section, "COND3")) {
                apac_cases::lpc_fill_lsf(c, g_in, order, kFs);
                memcpy(g_have, g_in, sizeof(float) * order);
                lsf->Condition_LSFs(g_have, 50.0f, kFs);
                expect("Condition_LSFs", c, order, kFs * 0.5f);
                return;
            }
            if (!strcmp(section, "COND1")) {
                apac_cases::lpc_fill_lsf(c, g_in, order, kFs);
                memcpy(g_have, g_in, sizeof(float) * order);
                lsf->ConditionLSFs(g_have);
                expect("ConditionLSFs", c, order, kFs * 0.5f);
                return;
            }
            fprintf(stderr, "unknown section %s\n", section);
            exit(1);
        }
    };

    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        if (!strncmp(line, "LPC ", 4)) {
            unsigned c, o;
            if (sscanf(line, "LPC %u order=%u", &c, &o) != 2) {
                fprintf(stderr, "bad LPC line: %s", line);
                return 1;
            }
            assert(apac_cases::lpc_plan()[c].order == o);
            caseIdx = c;
            order = o;
            section[0] = 0;
            continue;
        }

        if (line[0] != ' ') {
            // A new section header.  Sizes: everything is order + 1 floats
            // except the LSF-domain vectors, which are order.
            size_t n = strlen(line);
            while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
            snprintf(section, sizeof section, "%s", line);
            const bool lsfDomain =
                !strcmp(section, "LSP2LSF") || !strcmp(section, "LSF2LSP") ||
                !strcmp(section, "LSF2LSPIP") || !strcmp(section, "LSFWEIGHT") ||
                !strcmp(section, "COND3") || !strcmp(section, "COND1");
            wantN = lsfDomain ? order : order + 1;
            got = 0;
            continue;
        }

        got += dspfx::parse_hex_floats(line, g_want, 64, got);
        if (got == wantN && section[0]) {
            Run::go(section, caseIdx, order, &lpc, &ac, &lsf);
            section[0] = 0;
        }
    }
    fclose(f);

    // 11 sections per case: WEIGHT, LEVINSON, COMPUTE 0/1, and seven LSF ones.
    assert(g_checks == 11 * apac_cases::kLPCCases);

    printf("OK  %d checks over %d orders, worst %.2g of full scale (%s)\n",
           g_checks, (int)apac_cases::kLPCCases, g_worst, g_worstWhere);
    return 0;
}
