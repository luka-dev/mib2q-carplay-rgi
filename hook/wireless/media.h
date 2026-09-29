#ifndef CARPLAY_WIRELESS_MEDIA_H
#define CARPLAY_WIRELESS_MEDIA_H
#include "pairing.h"
#include "../framework/hook_framework.h"
int cp_media_enabled(void);
int cp_media_available(void);
int cp_media_normalize_setup(void *message);
int cp_media_prepare(void *owner, void *session, void *request, cp_pair_connection *pair);
void cp_media_setup_response(hook_setup_ctx_t *setup);
void cp_media_teardown(void *session, void *request, int reason);
void cp_media_forget(void *owner);
int cp_media_event_read(int fd, void *message, int *result);
int cp_media_event_write(int fd, struct iovec **vectors, int *count, int *result);
#endif
