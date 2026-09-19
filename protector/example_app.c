/* example_app.c - the HOOK model: our mod stays INERT on a wrong/absent VIN.
 *
 * Our hooks are additive. We arm once at load; each hook checks prot_active()
 * (a cached bool, no crypto) and, when inactive, calls straight through to the
 * original - the head runs its stock path untouched, as if the mod weren't
 * installed. Wrong VIN, absent VIN (bench/bus down), patched image, or an on-unit
 * hiccup -> inactive -> stock. Nothing bricks; forcing "active" gains nothing,
 * because the premium effect uses VIN-blinded constants that are wrong without
 * the right VIN.
 *
 * Stamped constants (protect.py --mode blind): [ SENTINEL, gain, offset ]. */
#include "prot_rt.h"
#include <stdio.h>

PROT_DEFINE_SLOT();

#define SENTINEL 0x5A

/* The ORIGINAL stock function we are hooking. This is the head's own behavior;
 * our mod must not disturb it when we're not licensed for this VIN. */
static long stock_process(const short *in, int n)
{
    long acc = 0;
    for (int i = 0; i < n; i++) acc += in[i];   /* stock: plain sum */
    return acc;
}

/* Our HOOK/detour over stock_process. */
PROT_HOT static long hooked_process(const short *in, int n)
{
    if (!prot_active())                         /* wrong/absent VIN -> inert */
        return stock_process(in, n);            /* stock path, mod invisible */

    int gain = prot_gate(0), offset = prot_gate(1);   /* VIN-blinded: wrong VIN,
                                                         wrong numbers, never the
                                                         real feature */
    long acc = 0;
    for (int i = 0; i < n; i++) acc += (long)in[i] * gain + offset;
    return acc;
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    /* Arm once at load. STOCK defaults are only a placeholder for the cached
     * constants; when inactive the hook ignores them and calls the original. */
    static const uint8_t STOCK[2] = { 1, 0 };

#ifdef PROT_DEMO_ARGV
    /* test build: VIN + self path from argv, so selftest can drive right/wrong */
    prot_arm(argv[0], argc > 1 ? argv[1] : "", SENTINEL, STOCK, 2);
#else
    /* production: live VIN from the FEC quorum + our own image, no arguments */
    prot_arm_auto(SENTINEL, STOCK, 2);
#endif

    short samples[8] = { 100,100,100,100,100,100,100,100 };
    long out = hooked_process(samples, 8);
    printf("hook=%s process=%ld\n", prot_active() ? "ACTIVE" : "inert", out);
    return 0;
}
