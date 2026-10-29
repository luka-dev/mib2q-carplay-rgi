#ifndef MHI2Q_SI_CONTROLLER_H
#define MHI2Q_SI_CONTROLLER_H
#include "si_wire.h"
#include "si_media_watch.h"
#include "si_child_state.h"

#define SI_CONTROL_REQUESTS 32u
#define SI_CONTROL_PHONES 8u
struct si_native_status { int32_t app, state; uint32_t device; };
struct si_native_ops {
    int (*snapshot)(void *, struct si_native_status *);
    int (*upsert)(void *, uint32_t *, const char *, const char *);
    int (*remove)(void *, uint32_t);
    int (*activate)(void *, uint32_t, uint32_t);
    int (*cancel)(void *, uint32_t);
    int (*alive)(void *, uint32_t);
    int (*child_state)(void *, uint32_t, uint32_t, uint32_t);
};
void si_control_init(uint64_t epoch, const struct si_native_ops *ops);
/* submit returns 1 for a new queued request, 0 for an immediate reply.
 * A slot remains owned until execution/rejection; retries do not re-enqueue. */
int si_control_submit(const struct si_request *, unsigned *slot, struct si_reply *);
void si_control_reject(unsigned slot);
void si_control_execute(unsigned slot, void *native_context);
void si_control_result(unsigned slot, struct si_reply *);
void si_control_tick(void *native_context);
int si_control_media_target(struct si_media_target *);
void si_control_media_ended(const struct si_media_target *);
int si_control_wireless(uint32_t device);
int si_control_prepare(uint32_t device);
int si_control_before_connect(void *native_context, uint32_t device, uint32_t app);
int si_control_child(uint32_t *session, uint32_t *link);
void si_control_child_started(uint32_t session, uint32_t pid);
uint64_t si_control_epoch(void);
uint32_t si_control_owner_pid(void);
int si_control_submit_child_state(const struct si_child_state *,unsigned *,struct si_reply *);
#endif
