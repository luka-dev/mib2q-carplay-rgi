#include "framework/signal_guard.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

static spa_signal_guard_t guard;
static volatile sig_atomic_t hookCalls;
static volatile sig_atomic_t ownerCalls;

static void owner_handler(int signalNumber)
{
    (void)signalNumber;
    ownerCalls++;
}

static void hook_handler(int signalNumber)
{
    hookCalls++;
    spa_signal_guard_chain(&guard, signalNumber);
}

static int set_handler(void (*handler)(int))
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    return sigaction(SIGUSR1, &action, NULL);
}

int main(void)
{
    struct sigaction action;

    if (set_handler(owner_handler) != 0) return 1;
    memset(&action, 0, sizeof(action));
    action.sa_handler = hook_handler;
    sigemptyset(&action.sa_mask);
    if (spa_signal_guard_install(&guard, SIGUSR1, &action) != 0) return 2;
    if (raise(SIGUSR1) != 0) return 3;
    if (hookCalls != 1 || ownerCalls != 1) return 4;

    if (spa_signal_guard_install(&guard, SIGUSR1, &action) != 0) return 5;
    spa_signal_guard_restore(&guard);
    if (raise(SIGUSR1) != 0) return 6;
    if (hookCalls != 1 || ownerCalls != 2) return 7;

    puts("signal_guard_test: all tests passed");
    return 0;
}
