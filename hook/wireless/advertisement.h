#ifndef CARPLAY_WIRELESS_ADVERTISEMENT_H
#define CARPLAY_WIRELESS_ADVERTISEMENT_H
#include <stdint.h>
#define CP_WIRELESS_FEATURES ((UINT64_C(1) << 37) | (UINT64_C(1) << 38))
#define CP_WIRELESS_PCM16 (((UINT64_C(1) << 12) - (UINT64_C(1) << 2)) | (UINT64_C(1) << 14) | (UINT64_C(1) << 15))
int cp_advertisement_available(void);
void cp_advertisement_info(void *session, void *info);
#endif
