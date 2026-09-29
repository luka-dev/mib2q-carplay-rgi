/* Pure, bounded CarPlay UUID insertion into a 240-byte Bluetooth EIR buffer.
 * The input length is the exact number of AD bytes returned by the target
 * Blue SDK builder, excluding its zero-filled spare capacity. */
typedef unsigned char u8;
typedef unsigned int u32;

__attribute__((used)) static const u8 carplay_uuid_lsb[16] = {
    0xd3,0x1f,0xbf,0x50,0x5d,0x57,0x27,0x97,
    0xa2,0x40,0x41,0xcd,0x48,0x43,0x88,0xec
};

static int same_bytes(const u8 *a, const u8 *b, u32 length) {
    for (u32 i = 0; i < length; ++i) if (a[i] != b[i]) return 0;
    return 1;
}

u32 eir_append_carplay_uuid(u8 *eir, u32 length) {
    if (!eir || length > 240u) return length;
    u32 pos = 0;
    while (pos < length) {
        u32 field = eir[pos];
        if (!field || field > length - pos - 1u) return length;
        if (eir[pos + 1u] == 7u) {
            if ((field - 1u) % 16u) return length;
            for (u32 i = pos + 2u; i + 16u <= pos + 1u + field; i += 16u)
                if (same_bytes(eir + i, carplay_uuid_lsb, 16u)) return length;
            if (field > 239u || length + 16u >= 240u) return length;
            u32 tail = pos + 1u + field;
            for (u32 i = length; i > tail; --i) eir[i + 15u] = eir[i - 1u];
            for (u32 i = 0; i < 16u; ++i) eir[tail + i] = carplay_uuid_lsb[i];
            eir[pos] = (u8)(field + 16u);
            eir[length + 16u] = 0;
            return length + 16u;
        }
        pos += field + 1u;
    }
    if (length + 18u >= 240u) return length;
    eir[length] = 17u;
    eir[length + 1u] = 7u;
    for (u32 i = 0; i < 16u; ++i) eir[length + 2u + i] = carplay_uuid_lsb[i];
    eir[length + 18u] = 0;
    return length + 18u;
}
