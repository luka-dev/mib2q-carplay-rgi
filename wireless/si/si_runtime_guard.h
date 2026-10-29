#ifndef SI_RUNTIME_GUARD_H
#define SI_RUNTIME_GUARD_H
#include <stddef.h>
#include <stdint.h>

/* A failed read is an ordinary error, never a process-wide signal handler. */
typedef int (*si_read_memory)(void *, uint32_t, void *, size_t);
struct si_guard_range { uint32_t va; const unsigned char *bytes; size_t size; };
struct si_ready_snapshot { uint32_t controller, impl, listener; };

#define SI_CONTROLLER_PTR 0x1c5928u
#define SI_CONTROLLER_IMPL_OFF 0xcc4u
#define SI_IMPL_STATE_OFF 0x90u
/* Job lookup 0x128994 resolves CControllerInternalJobListener::s_listenerArray,
 * not CMediaActiveObjectMediator::s_registrationArray (0x1c5118). */
#define SI_REGISTRATION_ARRAY 0x1c3c90u

int si_guard_candidate(si_read_memory read_memory, void *ctx);
int si_guard_ready(si_read_memory read_memory, void *ctx,
                   struct si_ready_snapshot *snapshot);
#endif
