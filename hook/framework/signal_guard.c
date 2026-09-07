#include "signal_guard.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

int spa_signal_guard_install(spa_signal_guard_t *guard, int signalNumber,
                             const struct sigaction *replacement)
{
    spa_signal_guard_entry_t *entry;
    struct sigaction previous;
    unsigned i;

    if (!guard || !replacement || signalNumber <= 0) {
        errno = EINVAL;
        return -1;
    }
    for (i = 0; i < guard->count; ++i) {
        if (guard->entries[i].installed &&
            guard->entries[i].signalNumber == signalNumber) {
            errno = EBUSY;
            return -1;
        }
    }
    if (guard->count >= SPA_SIGNAL_GUARD_CAPACITY) {
        errno = ENOSPC;
        return -1;
    }
    entry = &guard->entries[guard->count++];
    memset(entry, 0, sizeof(*entry));
    entry->signalNumber = signalNumber;
    if (sigaction(signalNumber, NULL, &previous) != 0) {
        guard->count--;
        return -1;
    }
    entry->previous = previous;
    /* Publish the predecessor before installing a handler which can call
     * spa_signal_guard_chain from another thread. */
    entry->installed = 1;
    if (sigaction(signalNumber, replacement, NULL) != 0) {
        entry->installed = 0;
        guard->count--;
        return -1;
    }
    return 0;
}

void spa_signal_guard_restore(spa_signal_guard_t *guard)
{
    unsigned i;

    if (!guard) return;
    i = guard->count;
    while (i != 0) {
        spa_signal_guard_entry_t *entry = &guard->entries[--i];

        if (entry->installed) {
            (void)sigaction(entry->signalNumber, &entry->previous, NULL);
            entry->installed = 0;
        }
    }
    guard->count = 0;
}

/* Called from the hook's fatal signal handler.  sigaction() and raise() are
 * async-signal-safe.  The current signal remains blocked until our handler
 * returns, so the raised instance is delivered to the restored dio_manager
 * disposition afterwards (including SA_SIGINFO/default/ignore semantics). */
void spa_signal_guard_chain(spa_signal_guard_t *guard, int signalNumber)
{
    size_t i;

    if (guard) {
        for (i = 0; i < guard->count; ++i) {
            spa_signal_guard_entry_t *entry = &guard->entries[i];

            if (entry->installed && entry->signalNumber == signalNumber) {
                (void)sigaction(signalNumber, &entry->previous, NULL);
                entry->installed = 0;
                (void)raise(signalNumber);
                return;
            }
        }
    }
    /* This is reachable only if the guard was corrupted or called for a
     * signal it never installed. Do not invoke non-async-safe recovery code
     * from a fatal handler. */
    _exit(128 + (signalNumber & 0x7f));
}
