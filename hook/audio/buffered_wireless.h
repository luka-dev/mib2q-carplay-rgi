#ifndef SPA_BUFFERED_WIRELESS_H
#define SPA_BUFFERED_WIRELESS_H
/* Optional audio module owns the entire buffered103 contract and its shk.
 * Wireless supplies authenticated control admission, never a second decoder
 * or DataStream-key derivation for APAP. HOOK_AUDIO=0 leaves these absent. */
extern int spa_buffered_wireless_offer(void *session) __attribute__((weak));
extern int spa_buffered_wireless_validate(void *session,const void *descriptor) __attribute__((weak));
#endif
