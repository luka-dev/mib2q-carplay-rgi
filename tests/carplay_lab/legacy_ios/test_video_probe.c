/* Host sanitizer test for the diagnostic's AVCC -> Annex-B conversion. */
#include "video_probe.c"
extern FILE *tmpfile(void);
extern long ftell(FILE *);
extern void rewind(FILE *);
extern size_t fread(void *, size_t, size_t, FILE *);
extern int memcmp(const void *, const void *, size_t);

int main(void)
{
    FILE *file = tmpfile();
    if (!file) return 1;
    const unsigned char avcc[] = {0,0,0,2,0x65,0xaa, 0,0,0,1,0x41};
    const unsigned char annex[] = {0,0,0,1,0x65,0xaa, 0,0,0,1,0x41};
    struct output out = {.file = file};
    if (write_access_unit(&out, avcc, sizeof(avcc), 4) || out.bytes != sizeof(annex)) return 2;
    rewind(file);
    unsigned char bytes[sizeof(annex)] = {0};
    if (fread(bytes, 1, sizeof(bytes), file) != sizeof(bytes) || memcmp(bytes, annex, sizeof(bytes))) return 3;
    const unsigned char bad[][6] = {{0,0,0,0}, {0,0,0,9,0x65,0}, {0,0,0,1,0x65,0}};
    for (unsigned i = 0; i < 3; ++i) {
        long before = ftell(file);
        if (!write_access_unit(&out, bad[i], sizeof(bad[i]), 4) || ftell(file) != before) return 4;
    }
    /* Different length-prefix widths, truncation, zero sizes and random tails. */
    u32 rng = 0x4d484932;
    unsigned char input[64];
    for (unsigned trial = 0; trial < 20000; ++trial) {
        for (size_t i = 0; i < sizeof(input); ++i) {
            rng = rng * 1664525u + 1013904223u;
            input[i] = (unsigned char)(rng >> 24);
        }
        size_t size = trial % (sizeof(input) + 1);
        long before = ftell(file);
        int result = write_access_unit(&out, input, size, (int)(trial % 7));
        if (result && ftell(file) != before) return 5;
    }
    fclose(file);
    printf("video probe conversion: positive, negative, 20000 fuzz cases passed\n");
    return 0;
}
