/*
 * Self-check for the CoreUtils status names: the constants our own return
 * paths use (hook/framework/ap_status.h) and the suffix ap_status() appends
 * to a log line (hook/framework/logging.c) come from one list, and this is
 * the check that they stay in step.
 *
 *   cc -I hook/framework -o /tmp/test_ap_status tests/test_ap_status.c && /tmp/test_ap_status
 *
 * Compiles the decoder alone: ENABLE_LOGGING=0 drops the writer, and
 * ap_status() lives outside that guard on purpose.
 */

#define ENABLE_LOGGING 0
#include "../hook/framework/logging.c"

#include <assert.h>
#include <string.h>

/* Walk the shared list and check the invariant ap_status() depends on: the
 * entries descend from kGenericErrorBase one at a time, so that (-6700 -
 * status) really is the index of that status, and the string at that index
 * really is that entry's name.  An inserted, dropped or misplaced line fails
 * here.  What no test can check is the spelling of a name itself - the ground
 * truth for that is the record table in the unit's own libairplay.so, which
 * tools/dump_errtab.py reads. */
#define CHECK_ENTRY(v, n)                                      \
    assert((v) == kGenericErrorBase - index);                  \
    assert(strcmp(ap_status(v), " (" #n ")") == 0);             \
    ++index;

static void check_list(void)
{
    int index = 0;
    AP_STATUS_LIST(CHECK_ENTRY)
    assert(index == 76);                /* every name we decoded is still here */
}

int main(void)
{
    check_list();

    /* the codes we have actually met on a car */
    assert(strcmp(ap_status(-6714), " (kNotHandledErr)") == 0);
    assert(strcmp(ap_status(-6735), " (kUnsupportedErr)") == 0);
    assert(strcmp(ap_status(-6740), " (kBadReferenceErr)") == 0);
    assert(strcmp(ap_status(-6728), " (kNoMemoryErr)") == 0);

    /* both ends of the table */
    assert(strcmp(ap_status(-6700), " (kUnknownErr)") == 0);
    assert(strcmp(ap_status(-6775), " (kWriteWouldBlockErr)") == 0);

    /* anything else must add nothing to the log line */
    assert(ap_status(0)[0] == '\0');
    assert(ap_status(1)[0] == '\0');
    assert(ap_status(-1)[0] == '\0');
    assert(ap_status(-6699)[0] == '\0');
    assert(ap_status(-6776)[0] == '\0');
    assert(ap_status(-11)[0] == '\0');          /* errno, not an OSStatus */
    assert(ap_status(-2147483647 - 1)[0] == '\0');

    puts("ap_status: OK");
    return 0;
}
