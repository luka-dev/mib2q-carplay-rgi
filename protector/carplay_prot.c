/* carplay_prot.c - VIN gate wrapper. Defines the one PROT slot for this .so and
 * arms it once. Pure C, no constructor (slot is data), no STL/exceptions - safe
 * to link into the strictly-audited APAC .so. */
#include "carplay_prot.h"
#include "prot_rt.h"

PROT_DEFINE_SLOT();                 /* exactly one slot per .so */

int carplay_prot_active(const char *self_path)
{
    static int done = 0, active = 0;
    /* Keep the slot alive against -fdata-sections/--gc-sections: the runtime
     * finds the slot by scanning the file, so nothing references PROT_SLOT by
     * symbol and the linker would otherwise garbage-collect its section. This
     * volatile read forces retention (byte[0] is the 'P' magic, never 0xFF). */
    if (((volatile unsigned char *)PROT_SLOT)[0] == 0xFF) return 0;
    if (!done) {
        static const unsigned char none = 0;
        /* sentinel-only gate: n=0 constants, just the 0x5A sentinel check */
        prot_arm_path(self_path, 0x5A, &none, 0);
        active = prot_active();
        done = 1;
    }
    return active;
}
