#ifndef MHI2Q_SI_WIRE_H
#define MHI2Q_SI_WIRE_H
#include <stddef.h>
#include <stdint.h>

/* Fixed-size, explicitly little-endian frames; no wire-to-struct casts. */
#define SI_WIRE_SIZE 192u
#define SI_PHONE_KEY_SIZE 32u
#define SI_PHONE_NAME_SIZE 96u
#define SI_WIRE_VERSION 2u
enum si_operation { SI_HELLO=1, SI_SNAPSHOT, SI_UPSERT_PHONE, SI_ACTIVATE,
                    SI_CANCEL, SI_REMOVE_PHONE, SI_DETACH_PEER };
enum si_result { SI_OK=0, SI_BAD_REQUEST=1, SI_STALE=2, SI_BUSY=3,
                 SI_NOT_FOUND=4, SI_UNAVAILABLE=5, SI_PENDING=6, SI_DENIED=7 };
struct si_request {
    uint8_t operation;
    uint32_t request_id, daemon_epoch, link_generation, session_generation;
    uint64_t si_epoch;
    char phone_key[SI_PHONE_KEY_SIZE];
    char name[SI_PHONE_NAME_SIZE];
    /* Internal only: never decoded from SIW2 daemon requests. */
    uint32_t child_pid, native_state, state_sequence;
    uint32_t event_kind,event_cookie;
};
struct si_reply {
    uint8_t operation;
    uint32_t request_id, result, device_id, session_generation;
    uint64_t si_epoch;
    int32_t app, app_state;
    uint32_t active_device, child_pid, present;
};
int si_wire_decode(const unsigned char frame[SI_WIRE_SIZE], struct si_request *out);
void si_wire_reply(const struct si_reply *reply, unsigned char frame[SI_WIRE_SIZE]);
#endif
