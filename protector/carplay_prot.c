/* carplay_prot.c - VIN gate wrapper. Defines the one PROT slot for this .so and
 * arms it once. Pure C, no constructor (slot is data), no STL/exceptions - safe
 * to link into the strictly-audited APAC .so. */
#include "carplay_prot.h"
#include "prot_rt.h"
#include "prot_vin.h"

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
        /* Token gate: the stamp decodes to the 4-byte TAG only for the stamped VIN
         * (a 1-byte sentinel let 1 in 256 foreign VINs through). */
        char vin[PROT_VIN_LEN + 1];
        unsigned char none[1];
        size_t n = 0;
        active = self_path && prot_read_vin(vin) == 0 &&
                 prot_unlock(self_path, vin, none, 0, &n) == 0;
        done = 1;
    }
    return active;
}
