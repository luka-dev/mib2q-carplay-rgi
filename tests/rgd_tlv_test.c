/* Production RGD parser: reject damaged deltas before any prefix reaches caches. */
#include "routeguidance/rgd_tlv.h"
#include <assert.h>

static void header(uint8_t *p, size_t n, uint16_t msg) {
    p[0] = p[1] = 0x40; write_be16(p + 2, (uint16_t)n); write_be16(p + 4, msg);
}
static uint32_t rng = 1;
static uint32_t random_u32(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
int main(void) {
    uint8_t buf[256] = {0};
    rgd_update_t u;
    rgd_maneuver_t m;
    rgd_lane_guidance_t l;
    size_t i, n;
    /* Valid active-state prefix followed by an incomplete TLV must not activate a route. */
    header(buf, 15, 0x5201);
    write_be16(buf + 6, 5); write_be16(buf + 8, RGD_TLV_ROUTE_GUIDANCE_STATE); buf[10] = 1;
    write_be16(buf + 11, 9); write_be16(buf + 13, RGD_TLV_SOURCE_SUPPORTS_RG);
    rgd_parse_update(buf, 15, &u); assert(u.present == 0);
    /* A complete update with unknown extension is still a valid delta. */
    header(buf, 15, 0x5201); write_be16(buf + 11, 4); write_be16(buf + 13, 0xFFFE);
    rgd_parse_update(buf, 15, &u); assert(u.present == RGD_UPD_ROUTE_STATE && u.route_state == 1);
    /* Explicit empty maneuver list is present, unlike an absent list. */
    write_be16(buf + 13, RGD_TLV_MANEUVER_LIST);
    rgd_parse_update(buf, 15, &u); assert((u.present & RGD_UPD_MANEUVER_LIST) && !u.maneuver_list_count);
    header(buf, 16, 0x5201); buf[15] = 0xAA;
    rgd_parse_update(buf, 16, &u); assert(u.present == 0);
    /* A valid maneuver index plus truncated following TLV must not replace that slot. */
    header(buf, 16, 0x5202); write_be16(buf + 6, 6); write_be16(buf + 8, MAN_TLV_INDEX); write_be16(buf + 10, 17);
    write_be16(buf + 12, 5); write_be16(buf + 14, MAN_TLV_TYPE);
    rgd_parse_maneuver(buf, 16, &m); assert(m.present == 0 && m.linked_lane_guidance_index == 0xFFFF);
    header(buf, 12, 0x5202); rgd_parse_maneuver(buf, 12, &m);
    assert(m.index == 17 && m.present == (RGD_MAN_INDEX | RGD_MAN_EXIT_ANGLE) && m.exit_angle == 1000);
    /* iOS's signed-16 serializer preserves supplied values. Only an absent
     * field gets our +1000 default; zero, -1 and both sentinels survive parsing. */
    {
        const int16_t angles[] = {-32768, -1001, -1000, -999, -181, -180,
                                 -1, 0, 1, 180, 181, 999, 1000, 1001, 32767};
        header(buf, 24, 0x5202);
        write_be16(buf + 12, 6); write_be16(buf + 14, MAN_TLV_EXIT_ANGLE);
        write_be16(buf + 18, 6); write_be16(buf + 20, MAN_TLV_JUNCTION_ANGLES);
        for (size_t j = 0; j < sizeof(angles) / sizeof(angles[0]); ++j) {
            write_be16(buf + 16, (uint16_t)angles[j]);
            write_be16(buf + 22, (uint16_t)angles[j]);
            assert(rgd_parse_maneuver(buf, 24, &m));
            assert(m.exit_angle == angles[j] && (m.present & RGD_MAN_EXIT_ANGLE));
            assert(m.junction_angle_count == 1 && m.junction_angles[0] == angles[j]);
        }
    }
    /* Nested lane list: valid index/status followed by a malformed child length. */
    header(buf, 25, 0x5204); write_be16(buf + 6, 6); write_be16(buf + 8, LANE_MSG_TLV_LANE_GUIDANCE_INDEX); write_be16(buf + 10, 2);
    write_be16(buf + 12, 13); write_be16(buf + 14, LANE_MSG_TLV_LANE_INFORMATIONS);
    write_be16(buf + 16, 5); write_be16(buf + 18, LANE_INFO_TLV_STATUS); buf[20] = 1;
    write_be16(buf + 21, 6); write_be16(buf + 23, LANE_INFO_TLV_ANGLES);
    rgd_parse_lane_guidance(buf, 25, &l); assert(l.present == 0 && !l.lane_count);
    header(buf, 21, 0x5204); write_be16(buf + 12, 9);
    rgd_parse_lane_guidance(buf, 21, &l); assert(l.lane_count == 1 && l.lanes[0].status == 1);
    assert(!l.lane_complete); // Implicit index cannot prove a physical road edge.
    {
        uint8_t full[512]={0};size_t off=12;
        write_be16(full+6,6);write_be16(full+8,LANE_MSG_TLV_LANE_GUIDANCE_INDEX);write_be16(full+10,7);
        for(int lane=0;lane<9;++lane) {
            write_be16(full+off,21);write_be16(full+off+2,LANE_MSG_TLV_LANE_INFORMATIONS);
            write_be16(full+off+4,6);write_be16(full+off+6,LANE_INFO_TLV_INDEX);write_be16(full+off+8,lane);
            write_be16(full+off+10,5);write_be16(full+off+12,LANE_INFO_TLV_STATUS);full[off+14]=2;
            write_be16(full+off+15,6);write_be16(full+off+17,LANE_INFO_TLV_ANGLES);write_be16(full+off+19,(uint16_t)-1000);
            off+=21;header(full,off,0x5204);assert(rgd_parse_lane_guidance(full,off,&l));
            assert(l.lane_complete==(lane<8));assert(l.lane_count==(lane<8?lane+1:8));
            assert(l.lanes[0].direction==-1000);
        }
        /* An overflowing angle vector is incomplete even though its stored prefix is valid. */
        off=12;write_be16(full+off,53);write_be16(full+off+19,0);
        write_be16(full+off+15,38);
        for(int a=0;a<17;++a)write_be16(full+off+19+a*2,45);
        header(full,65,0x5204);assert(rgd_parse_lane_guidance(full,65,&l));
        assert(l.lane_count==1 && l.lanes[0].angle_count==16 && !l.lane_complete);
    }
    /* Header length/type, every short read, and deterministic malformed/random payloads. */
    for (i = 0; i < 120000; ++i) {
        n = random_u32() % sizeof(buf);
        for (size_t j = 0; j < n; ++j) buf[j] = (uint8_t)random_u32();
        if (n >= 6 && (i & 1)) {
            header(buf, n, (uint16_t[]){0x5201, 0x5202, 0x5204}[i % 3]);
            if (i & 2) {
                for (size_t off = 6; n - off >= 4;) {
                    size_t chunk = 4 + random_u32() % (n - off - 3);
                    write_be16(buf + off, (uint16_t)chunk);
                    write_be16(buf + off + 2, (uint16_t)(random_u32() % 24));
                    off += chunk;
                }
            }
        }
        rgd_parse_update(buf, n, &u); rgd_parse_maneuver(buf, n, &m); rgd_parse_lane_guidance(buf, n, &l);
        assert(u.component_count <= MAX_COMPONENT_LIST && m.component_count <= MAX_COMPONENT_LIST);
        assert(l.lane_count <= MAX_LANE_GUIDANCE);
        for (size_t j = 0; j < l.lane_count; ++j) assert(l.lanes[j].angle_count <= MAX_LANE_ANGLES);
    }
    rgd_parse_update(NULL, 100, &u); assert(!u.present);
    rgd_parse_maneuver(NULL, 100, &m); assert(!m.present);
    rgd_parse_lane_guidance(NULL, 100, &l); assert(!l.present);
    puts("rgd_tlv_test: atomic deltas, nested lanes, unknown/empty fields and 120000 malformed inputs PASS");
    return 0;
}
