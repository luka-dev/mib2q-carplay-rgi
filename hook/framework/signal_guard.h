/* Preserve process-owned signal dispositions around an injected hook policy. */
#ifndef CARPLAY_SIGNAL_GUARD_H
#define CARPLAY_SIGNAL_GUARD_H

#include <signal.h>

#define SPA_SIGNAL_GUARD_CAPACITY 8u

typedef struct {
    int signalNumber;
    struct sigaction previous;
    volatile sig_atomic_t installed;
} spa_signal_guard_entry_t;

typedef struct {
    spa_signal_guard_entry_t entries[SPA_SIGNAL_GUARD_CAPACITY];
    unsigned count;
} spa_signal_guard_t;

int spa_signal_guard_install(spa_signal_guard_t *guard, int signalNumber,
                             const struct sigaction *replacement);
void spa_signal_guard_restore(spa_signal_guard_t *guard);
void spa_signal_guard_chain(spa_signal_guard_t *guard, int signalNumber);

#endif /* CARPLAY_SIGNAL_GUARD_H */
