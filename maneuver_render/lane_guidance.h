#ifndef CR_LANE_GUIDANCE_H
#define CR_LANE_GUIDANCE_H
#include "protocol.h"
#include <string.h>
#define CR_LANE_CAPACITY 8
#define CR_LANE_ANGLE_CAPACITY 16
typedef struct {
    uint16_t position;
    uint8_t status, angle_count;
    int16_t primary, angles[CR_LANE_ANGLE_CAPACITY];
} cr_lane_record_t;
typedef struct {
    int32_t event_index;
    uint8_t count, showing, complete;
    cr_lane_record_t lanes[CR_LANE_CAPACITY];
} cr_lane_guidance_t;
typedef struct {
    cr_lane_guidance_t staged;
    uint32_t token;
    unsigned seen;
    int active, malformed;
} cr_lane_decoder_t;
static inline uint32_t cr_lane_u32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static inline uint16_t cr_lane_u16(const uint8_t *p) {return (uint16_t)((p[0]<<8)|p[1]);}
static inline void cr_lane_clear(cr_lane_decoder_t *d,cr_lane_guidance_t *out) {
    memset(d,0,sizeof(*d));memset(out,0,sizeof(*out));out->event_index=-1;
}
/* Atomic independent event; no scene, maneuver type, angle or animation input.
 * A malformed committed update clears stale guidance, without touching arrows. */
static inline int cr_lane_receive(cr_lane_decoder_t *d,const cr_cmd_t *cmd,cr_lane_guidance_t *out) {
    const uint8_t *p=cmd->payload;uint32_t token=cr_lane_u32(p);
    if (cmd->cmd==CMD_LANES_BEGIN) {
        memset(d,0,sizeof(*d));d->token=token;d->active=1;
        d->staged.count=p[4];d->staged.complete=p[5]==1;d->staged.showing=p[6]==1;
        d->staged.event_index=(int32_t)cr_lane_u32(p+8);
        d->malformed=p[4]>CR_LANE_CAPACITY || p[5]>1 || p[6]>1;
        return 0;
    }
    if (!d->active || token!=d->token) return 0;
    if (cmd->cmd==CMD_LANES_LANE) {
        unsigned i=p[4];
        if (i>=d->staged.count || i>=CR_LANE_CAPACITY || p[8]>CR_LANE_ANGLE_CAPACITY || (d->seen&(1u<<i))) {
            d->malformed=1;return 0;
        }
        cr_lane_record_t *lane=&d->staged.lanes[i];
        lane->position=cr_lane_u16(p+5);lane->status=p[7];lane->angle_count=p[8];
        lane->primary=(int16_t)cr_lane_u16(p+9);
        for (unsigned j=0;j<lane->angle_count;++j) lane->angles[j]=(int16_t)cr_lane_u16(p+11+j*2);
        d->seen|=1u<<i;return 0;
    }
    if (cmd->cmd!=CMD_LANES_COMMIT) return 0;
    if (d->malformed || d->seen!=((1u<<d->staged.count)-1)) {
        int32_t event=d->staged.event_index;
        memset(out,0,sizeof(*out));out->event_index=event;
    } else *out=d->staged;
    d->active=0;return 1;
}
#endif
