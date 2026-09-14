#ifndef CURSOR_SESSION_H
#define CURSOR_SESSION_H
#include "engine.h"
#define CURSOR_PORT 19840
/* One connection, one owner thread. Socket replacement always resets the plane. */
typedef struct {
    uint32_t token;
    uint64_t token_until, next_draw;
    cursor_engine engine;
    cursor_frame displayed;
    int open;
    int awaiting;
    cursor_frame pending;
} cursor_session;
void cursor_session_init(cursor_session *s, uint32_t seed);
void cursor_session_reset(cursor_session *s);
/* -1 malformed input; 0 no reply; 1 exactly CURSOR_ACK_SIZE bytes in reply. */
int cursor_session_receive(cursor_session *s, const uint8_t *p, size_t len,
                           uint64_t now, uint8_t *reply);
int cursor_session_tick(cursor_session *s, uint64_t now, uint8_t *reply);
#endif
