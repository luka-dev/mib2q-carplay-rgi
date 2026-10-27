#ifndef CARPLAY_WIRELESS_HOOK_H
#define CARPLAY_WIRELESS_HOOK_H
#include "../framework/hook_framework.h"
extern const hook_module_def_t wireless_module_def;
int carplay_wireless_copy_pairing_identity(char *id, size_t capacity, uint8_t pk[32]);
int carplay_wireless_receiver_ready(void);
void cp_wireless_advertisement_failed(void);
void cp_wireless_http_created(void *server);
void cp_wireless_http_queue(void *server,void *queue);
#endif
