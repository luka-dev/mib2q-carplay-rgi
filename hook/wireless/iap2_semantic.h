#ifndef CP_WIRELESS_IAP2_SEMANTIC_H
#define CP_WIRELESS_IAP2_SEMANTIC_H
#include <stddef.h>
#include <stdint.h>
/* The daemon owns link framing/checksums/ACKs. These are complete semantic
 * CSM frames or negotiated file-transfer payloads, never synthetic FF5A. */
enum { CP_IAP2_BEGIN=0,CP_IAP2_CONTROL=1,CP_IAP2_FILE=2 };
int mhi2q_wireless_iap2_receive(unsigned kind,const uint8_t *data,size_t size);
int mhi2q_wireless_iap2_outgoing(uint8_t *data,size_t capacity);
int mhi2q_wireless_iap2_nmea(const char *,size_t);
#endif
