/*
 * CarPlay Cover Art Hook
 *
 * Taps the Cinemo transport at NmeTransport::Recv (via the framework's
 * hook_set_transport_recv_sink), reassembles the FF 5A framed iAP2 link
 * stream, extracts the NowPlaying artwork JPEG, decodes + resizes to
 * 256x256, and writes it as a PNG file (+ bus notify).  There is NO libc
 * read()/recv()/open()/close() interposition — cover art no longer touches
 * the process-wide I/O path.
 */

#include "../framework/common.h"
#include "../framework/logging.h"
#include "../framework/bus.h"
#include "coverart_hook.h"

/* stb_image for JPEG/PNG decoding.
 * STBI_NO_THREAD_LOCALS: stb defaults its globals (g_failure_reason, load flags)
 * to `__thread` on GCC — which on QNX 6.5 ARMv7 pulls in emutls (__emutls_*) and
 * crashes in an LD_PRELOAD .so. Decoding runs on the single coverart worker, so
 * plain globals are fine. MUST stay defined (verified: build was carrying emutls). */
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_GIF
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_TGA
#define STBI_NO_BMP
#include "stb_image.h"

#include <dlfcn.h>
#include <zlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

DEFINE_LOG_MODULE(COVERART);

/* Output paths */
#ifndef COVERART_DIR
#define COVERART_DIR "/var/app/icab/tmp/37"
#endif

#ifndef COVERART_FILE
#define COVERART_FILE COVERART_DIR "/coverart.png"
#endif

/* Size filter */
#ifndef COVERART_MIN_BYTES
#define COVERART_MIN_BYTES 500
#endif

#ifndef COVERART_MAX_BYTES
#define COVERART_MAX_BYTES 800000
#endif

/* Debug: dump raw JPEG for analysis (set to path to enable, NULL to disable) */
#ifndef COVERART_DUMP_DIR
#define COVERART_DUMP_DIR NULL
#endif

/* Output size */
#define OUTPUT_WIDTH  256
#define COVERART_CALLBACK_DRAIN_MS 500
#define COVERART_WORKER_DRAIN_MS 1000
#define OUTPUT_HEIGHT 256

/* Packet parser constants */
#define PACKET_HEADER_SIZE  4   /* FF 5A LL LL */
#define PACKET_WRAPPER_SIZE 7   /* 40 .. .. .. .. .. 80 */
#define PACKET_TRAILER_SIZE 1   /* Checksum byte */
#define TOTAL_STRIP_SIZE    (PACKET_HEADER_SIZE + PACKET_WRAPPER_SIZE)

/* Stream buffer limit */
#define STREAM_BUF_MAX      (1024 * 1024)

/* JPEG markers */
static const uint8_t jpeg_soi[] = {0xFF, 0xD8};
static const uint8_t jpeg_eoi[] = {0xFF, 0xD9};

/* Function pointer types — only write() is still resolved (for the PNG output
 * file).  read()/recv()/open()/close() are NO LONGER interposed: cover art
 * taps the Cinemo transport at NmeTransport::Recv instead of globally
 * interposing libc, which removes our symbols from the path of every I/O call
 * in dio_manager (the fragility that could perturb the CarPlay handshake). */
typedef ssize_t (*WriteFunc)(int fd, const void* buf, size_t count);

static WriteFunc real_write = NULL;

/* Stream state for FF 5A packet assembly. */
typedef struct {
    uint8_t* raw_buf;
    size_t raw_len;
    size_t raw_cap;
    uint8_t* jpeg_buf;
    size_t jpeg_len;
    size_t jpeg_cap;
    int target_session;
    int is_carplay_stream;
    pthread_mutex_t lock;
} StreamState;

/* Hard cap on jpeg accumulation per stream — defends against a malformed
 * iAP2 stream that opens a session and never emits a JPEG EOI marker.
 * Without this cap, jpeg_buf doubles unbounded (15 MB CarPlay covers exist;
 * 32 MB is well above any legitimate single-image size). */
#define COVERART_MAX_JPEG  (32u * 1024u * 1024u)

/* The single reassembler stream, fed by the NmeTransport::Recv seam (via
 * hook_set_transport_recv_sink → coverart_on_transport_recv).  There is
 * exactly one Cinemo transport carrying the iAP2 link, so one stream suffices
 * — no fd-keyed pool, no global mutex, no libc interposition. */
static StreamState g_recv_stream = {
    .target_session = -1,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

/* Module state */
static struct {
    uint32_t last_crc;
    int images_found;
    int slot;           /* ping-pong: 0 or 1 */
} g_coverart = {
    .last_crc = 0,
    .images_found = 0,
    .slot = 0
};

/* CRC32 for deduplication */
static uint32_t crc32_bytes(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* Check for PNG signature */
static int is_png(const uint8_t* data, size_t size) {
    static const uint8_t sig[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    if (size < sizeof(sig)) return 0;
    return memcmp(data, sig, sizeof(sig)) == 0;
}

/* Check for JPEG signature */
static int is_jpeg(const uint8_t* data, size_t size) {
    if (size < 3) return 0;
    return data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF;
}

/* memmem implementation */
static void* my_memmem(const void* haystack, size_t haystacklen,
                       const void* needle, size_t needlelen) {
    const uint8_t* h = (const uint8_t*)haystack;
    const uint8_t* n = (const uint8_t*)needle;

    if (needlelen == 0) return (void*)haystack;
    if (haystacklen < needlelen) return NULL;

    for (size_t i = 0; i <= haystacklen - needlelen; i++) {
        if (h[i] == n[0] && memcmp(h + i, n, needlelen) == 0) {
            return (void*)(h + i);
        }
    }
    return NULL;
}

/*
 * Parse EXIF orientation from JPEG data.
 * Returns: 1=normal, 2=flip-h, 3=rotate-180, 4=flip-v,
 *          5=transpose, 6=rotate-90-cw, 7=transverse, 8=rotate-90-ccw
 *          0=not found or error
 */
static int get_jpeg_orientation(const uint8_t* data, size_t len) {
    if (!data || len < 12) return 0;
    if (data[0] != 0xFF || data[1] != 0xD8) return 0;  /* Not JPEG */

    size_t pos = 2;
    while (pos + 4 < len) {
        if (data[pos] != 0xFF) {
            pos++;
            continue;
        }

        uint8_t marker = data[pos + 1];

        /* Skip padding FF bytes */
        if (marker == 0xFF) {
            pos++;
            continue;
        }

        /* End markers */
        if (marker == 0xD9 || marker == 0xDA) break;

        /* Markers without length (RST, SOI, EOI) */
        if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0xD8 || marker == 0x01) {
            pos += 2;
            continue;
        }

        /* Get segment length */
        if (pos + 4 > len) break;
        uint16_t seg_len = (data[pos + 2] << 8) | data[pos + 3];
        if (seg_len < 2 || pos + 2 + seg_len > len) break;

        /* APP1 marker (EXIF) */
        if (marker == 0xE1 && seg_len >= 8) {
            const uint8_t* seg = data + pos + 4;
            size_t seg_size = seg_len - 2;

            /* Check for "Exif\0\0" header */
            if (seg_size >= 6 && memcmp(seg, "Exif\0\0", 6) == 0) {
                const uint8_t* tiff = seg + 6;
                size_t tiff_size = seg_size - 6;

                if (tiff_size < 8) break;

                /* Check byte order (II=little, MM=big) */
                int little_endian = (tiff[0] == 'I' && tiff[1] == 'I');
                int big_endian = (tiff[0] == 'M' && tiff[1] == 'M');
                if (!little_endian && !big_endian) break;

                /* Get IFD0 offset */
                uint32_t ifd_offset;
                if (little_endian) {
                    ifd_offset = tiff[4] | (tiff[5] << 8) | (tiff[6] << 16) | (tiff[7] << 24);
                } else {
                    ifd_offset = (tiff[4] << 24) | (tiff[5] << 16) | (tiff[6] << 8) | tiff[7];
                }

                if (ifd_offset + 2 > tiff_size) break;

                /* Get number of IFD entries */
                uint16_t num_entries;
                if (little_endian) {
                    num_entries = tiff[ifd_offset] | (tiff[ifd_offset + 1] << 8);
                } else {
                    num_entries = (tiff[ifd_offset] << 8) | tiff[ifd_offset + 1];
                }

                /* Search for orientation tag (0x0112) */
                size_t entry_offset = ifd_offset + 2;
                for (int i = 0; i < num_entries && entry_offset + 12 <= tiff_size; i++) {
                    const uint8_t* entry = tiff + entry_offset;
                    uint16_t tag;
                    if (little_endian) {
                        tag = entry[0] | (entry[1] << 8);
                    } else {
                        tag = (entry[0] << 8) | entry[1];
                    }

                    if (tag == 0x0112) {  /* Orientation tag */
                        uint16_t value;
                        if (little_endian) {
                            value = entry[8] | (entry[9] << 8);
                        } else {
                            value = (entry[8] << 8) | entry[9];
                        }
                        LOG_DEBUG(LOG_MODULE, "EXIF orientation: %d", value);
                        return (value >= 1 && value <= 8) ? value : 0;
                    }

                    entry_offset += 12;
                }
            }
        }

        pos += 2 + seg_len;
    }

    return 0;  /* Not found */
}

/* Nearest-neighbor resize RGB with optional vertical flip */
static void resize_rgb(const uint8_t* src, int sw, int sh,
                       uint8_t* dst, int dw, int dh, int flip) {
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        int src_y = flip ? (sh - 1 - sy) : sy;
        const uint8_t* src_row = src + src_y * sw * 3;
        uint8_t* dst_row = dst + y * dw * 3;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            const uint8_t* p = src_row + sx * 3;
            dst_row[x * 3 + 0] = p[0];
            dst_row[x * 3 + 1] = p[1];
            dst_row[x * 3 + 2] = p[2];
        }
    }
}

/* Contain resize into dst canvas with black background */
static void resize_rgb_contain(const uint8_t* src, int sw, int sh,
                               uint8_t* dst, int dw, int dh, int flip) {
    if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

    memset(dst, 0, (size_t)dw * dh * 3);

    double sx = (double)dw / (double)sw;
    double sy = (double)dh / (double)sh;
    double scale = (sx < sy) ? sx : sy;

    int nw = (int)(sw * scale + 0.5);
    int nh = (int)(sh * scale + 0.5);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    if (nw > dw) nw = dw;
    if (nh > dh) nh = dh;

    uint8_t* tmp = (uint8_t*)malloc((size_t)nw * nh * 3);
    if (!tmp) return;

    resize_rgb(src, sw, sh, tmp, nw, nh, flip);

    int x0 = (dw - nw) / 2;
    int y0 = (dh - nh) / 2;

    for (int y = 0; y < nh; y++) {
        uint8_t* dst_row = dst + ((y0 + y) * dw + x0) * 3;
        const uint8_t* src_row = tmp + y * nw * 3;
        memcpy(dst_row, src_row, (size_t)nw * 3);
    }

    free(tmp);
}

/* Build PNG from RGB data */
static int build_png(const uint8_t* rgb, int w, int h, uint8_t** out_data, size_t* out_len) {
    if (!rgb || w <= 0 || h <= 0 || !out_data || !out_len) return -1;

    size_t raw_len = (size_t)h * (w * 3 + 1);
    size_t max_png = 8 + 25 + raw_len + 1024;

    uint8_t* buf = (uint8_t*)malloc(max_png);
    if (!buf) return -1;

    size_t pos = 0;

    /* PNG signature */
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    memcpy(buf + pos, sig, 8);
    pos += 8;

    /* IHDR chunk */
    uint8_t ihdr[13] = {
        (uint8_t)((w >> 24) & 0xFF), (uint8_t)((w >> 16) & 0xFF),
        (uint8_t)((w >> 8) & 0xFF), (uint8_t)(w & 0xFF),
        (uint8_t)((h >> 24) & 0xFF), (uint8_t)((h >> 16) & 0xFF),
        (uint8_t)((h >> 8) & 0xFF), (uint8_t)(h & 0xFF),
        8, 2, 0, 0, 0
    };

    buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 13;
    memcpy(buf + pos, "IHDR", 4); pos += 4;
    memcpy(buf + pos, ihdr, 13); pos += 13;
    uLong png_crc = crc32(0L, Z_NULL, 0);
    png_crc = crc32(png_crc, (const Bytef*)"IHDR", 4);
    png_crc = crc32(png_crc, ihdr, 13);
    buf[pos++] = (uint8_t)((png_crc >> 24) & 0xFF);
    buf[pos++] = (uint8_t)((png_crc >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((png_crc >> 8) & 0xFF);
    buf[pos++] = (uint8_t)(png_crc & 0xFF);

    uint8_t* raw = (uint8_t*)malloc(raw_len);
    if (!raw) { free(buf); return -1; }

    for (int y = 0; y < h; y++) {
        raw[y * (w * 3 + 1)] = 0;
        memcpy(raw + y * (w * 3 + 1) + 1, rgb + y * w * 3, (size_t)(w * 3));
    }

    uLongf comp_len = compressBound((uLong)raw_len);
    uint8_t* comp = (uint8_t*)malloc(comp_len);
    if (!comp) { free(raw); free(buf); return -1; }

    if (compress2(comp, &comp_len, raw, (uLong)raw_len, Z_BEST_SPEED) != Z_OK) {
        free(comp); free(raw); free(buf);
        return -1;
    }
    free(raw);

    buf[pos++] = (uint8_t)((comp_len >> 24) & 0xFF);
    buf[pos++] = (uint8_t)((comp_len >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((comp_len >> 8) & 0xFF);
    buf[pos++] = (uint8_t)(comp_len & 0xFF);
    memcpy(buf + pos, "IDAT", 4); pos += 4;
    memcpy(buf + pos, comp, comp_len); pos += comp_len;
    png_crc = crc32(0L, Z_NULL, 0);
    png_crc = crc32(png_crc, (const Bytef*)"IDAT", 4);
    png_crc = crc32(png_crc, comp, comp_len);
    buf[pos++] = (uint8_t)((png_crc >> 24) & 0xFF);
    buf[pos++] = (uint8_t)((png_crc >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((png_crc >> 8) & 0xFF);
    buf[pos++] = (uint8_t)(png_crc & 0xFF);
    free(comp);

    buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0; buf[pos++] = 0;
    memcpy(buf + pos, "IEND", 4); pos += 4;
    png_crc = crc32(0L, Z_NULL, 0);
    png_crc = crc32(png_crc, (const Bytef*)"IEND", 4);
    buf[pos++] = (uint8_t)((png_crc >> 24) & 0xFF);
    buf[pos++] = (uint8_t)((png_crc >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((png_crc >> 8) & 0xFF);
    buf[pos++] = (uint8_t)(png_crc & 0xFF);

    *out_data = buf;
    *out_len = pos;
    return 0;
}

/* Create directory and parents */
static int ensure_dir(const char* dir) {
    struct stat st;
    if (stat(dir, &st) == 0) return 0;  /* Already exists */

    char tmp[256];
    size_t len = strlen(dir);
    if (len >= sizeof(tmp)) return -1;

    strcpy(tmp, dir);

    for (char* p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                LOG_ERROR(LOG_MODULE, "mkdir(%s) failed: %d", tmp, errno);
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        LOG_ERROR(LOG_MODULE, "mkdir(%s) failed: %d", tmp, errno);
        return -1;
    }

    /* Verify it was created */
    if (stat(dir, &st) != 0) {
        LOG_ERROR(LOG_MODULE, "Directory %s still doesn't exist after mkdir", dir);
        return -1;
    }

    LOG_INFO(LOG_MODULE, "Created directory: %s", dir);
    return 0;
}

/*
 * Write PNG via atomic symlink swap.
 *
 * Ping-pongs between coverart_0.png and coverart_1.png.
 * COVERART_FILE ("coverart.png") is a symlink that always points
 * to a fully-written file - readers never see a partial write.
 *
 *   1. Write data to coverart_<new_slot>.png
 *   2. Create temp symlink coverart.png.tmp -> coverart_<new_slot>.png
 *   3. rename() temp symlink over coverart.png  (atomic on POSIX)
 *   4. Unlink old coverart_<old_slot>.png
 */
static int write_png_file(const uint8_t* data, size_t len) {
    if (!real_write) {
        real_write = (WriteFunc)dlsym(RTLD_NEXT, "write");
    }

    ensure_dir(COVERART_DIR);

    int new_slot = 1 - g_coverart.slot;
    int old_slot = g_coverart.slot;

    char new_path[256], old_path[256], tmp_link[256], new_name[64];

    snprintf(new_path, sizeof(new_path), COVERART_DIR "/coverart_%d.png", new_slot);
    snprintf(old_path, sizeof(old_path), COVERART_DIR "/coverart_%d.png", old_slot);
    snprintf(tmp_link, sizeof(tmp_link), COVERART_FILE ".tmp");
    snprintf(new_name, sizeof(new_name), "coverart_%d.png", new_slot);

    /* 1. Write PNG to new slot */
    int fd = open(new_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOG_ERROR(LOG_MODULE, "Failed to open %s: %d", new_path, errno);
        return -1;
    }

    ssize_t written;
    if (real_write) {
        written = real_write(fd, data, len);
    } else {
        written = write(fd, data, len);
    }
    close(fd);

    if (written != (ssize_t)len) {
        LOG_ERROR(LOG_MODULE, "Write failed: %zd/%zu", written, len);
        unlink(new_path);
        return -1;
    }

    /* 2. Create temp symlink (relative target within same dir) */
    unlink(tmp_link);
    if (symlink(new_name, tmp_link) != 0) {
        LOG_ERROR(LOG_MODULE, "symlink(%s, %s) failed: %d", new_name, tmp_link, errno);
        unlink(new_path);
        return -1;
    }

    /* 3. Atomic swap: rename temp symlink over canonical path */
    if (rename(tmp_link, COVERART_FILE) != 0) {
        LOG_ERROR(LOG_MODULE, "rename(%s, %s) failed: %d", tmp_link, COVERART_FILE, errno);
        unlink(tmp_link);
        return -1;
    }

    /* 4. Remove old slot file (ignore errors - may not exist on first run) */
    unlink(old_path);

    g_coverart.slot = new_slot;
    return 0;
}

/* Publish cover-art-ready event on the TCP bus.
 * Sticky so a late-connecting Java client still learns the current CRC.
 * The on-disk PNG path (COVERART_FILE) is implicit and never changes. */
static void write_coverart_notify(uint32_t crc) {
    bus_text_builder_t b;
    uint8_t scratch[128];
    bus_text_begin_with(&b, "coverart", scratch, sizeof(scratch));
    bus_text_uint(&b, "crc",  (uint64_t)crc);
    bus_text_str (&b, "path", COVERART_FILE);
    bus_send_text(EVT_COVERART, BUS_FLAG_STICKY, &b);
    LOG_DEBUG(LOG_MODULE, "bus EVT_COVERART crc=%08x", crc);
}

/* Dump raw JPEG for debugging */
static void dump_raw_jpeg(const uint8_t* data, size_t len, int index) {
    static const char* dump_dir = COVERART_DUMP_DIR;
    if (!dump_dir) return;

    if (!real_write) {
        real_write = (WriteFunc)dlsym(RTLD_NEXT, "write");
    }

    ensure_dir(dump_dir);

    char path[256];
    snprintf(path, sizeof(path), "%s/raw_%03d.jpg", dump_dir, index);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOG_ERROR(LOG_MODULE, "Dump failed to open %s: %d", path, errno);
        return;
    }

    ssize_t written;
    if (real_write) {
        written = real_write(fd, data, len);
    } else {
        written = write(fd, data, len);
    }
    close(fd);

    if (written == (ssize_t)len) {
        LOG_INFO(LOG_MODULE, "Dumped JPEG #%d: %s (%zu bytes)", index, path, len);
    } else {
        LOG_ERROR(LOG_MODULE, "Dump write failed: %zd/%zu", written, len);
    }
}

/* Process and save artwork */
static int save_artwork(const uint8_t* data, size_t len) {
    if (!data || len < COVERART_MIN_BYTES || len > COVERART_MAX_BYTES) {
        return 0;
    }

    /* CRC check for deduplication */
    uint32_t crc = crc32_bytes(data, len);
    if (crc == g_coverart.last_crc) {
        return 1;  /* Same image, already saved */
    }

    /* Dump raw JPEG if enabled */
    dump_raw_jpeg(data, len, g_coverart.images_found + 1);

    /* Decode image to RGB using stb_image */
    /* Reset global flip state in case other code modified it */
    stbi_set_flip_vertically_on_load(0);
#ifdef STBI_THREAD_LOCAL
    /* If thread-locals are enabled, a previous call in this thread to
     * stbi_set_flip_vertically_on_load_thread(1) would override the global.
     * Force thread-local flip off so our own flip logic is deterministic. */
    stbi_set_flip_vertically_on_load_thread(0);
#endif

    int src_w = 0, src_h = 0, channels = 0;
    uint8_t* rgb = stbi_load_from_memory(data, (int)len, &src_w, &src_h, &channels, 3);

    if (!rgb || src_w <= 0 || src_h <= 0) {
        LOG_DEBUG(LOG_MODULE, "Image decode failed (len=%zu)", len);
        if (rgb) stbi_image_free(rgb);
        return 0;
    }

    /* Check EXIF orientation */
    int orientation = get_jpeg_orientation(data, len);

    /*
     * IMPORTANT: Do not apply a default vertical flip.
     *
     * stb_image returns scanlines in the conventional top-to-bottom order,
     * which is what our downstream consumer expects. A previous implementation
     * flipped by default, which produced upside-down cover art once stb_image's
     * own flip state was made deterministic.
     *
     * We keep a very narrow EXIF handling here:
     * - Most incoming CarPlay cover art has orientation=1 (or no EXIF).
     * - If we ever see EXIF "flip vertical" (4), we correct it via a flip.
     * - Other orientations (rotations, transpose) are currently not handled.
     */
    int flip = (orientation == 4) ? 1 : 0;

    LOG_INFO(LOG_MODULE, "Decoded: %dx%d, %d ch, %zu bytes, orient=%d flip=%d",
             src_w, src_h, channels, len, orientation, flip);

    /* Resize to output size */
    uint8_t* resized = (uint8_t*)malloc(OUTPUT_WIDTH * OUTPUT_HEIGHT * 3);
    if (!resized) {
        stbi_image_free(rgb);
        return 0;
    }

    resize_rgb_contain(rgb, src_w, src_h, resized, OUTPUT_WIDTH, OUTPUT_HEIGHT, flip);
    stbi_image_free(rgb);

    /* Encode as PNG */
    uint8_t* png = NULL;
    size_t png_len = 0;

    if (build_png(resized, OUTPUT_WIDTH, OUTPUT_HEIGHT, &png, &png_len) != 0) {
        free(resized);
        LOG_ERROR(LOG_MODULE, "PNG encode failed");
        return 0;
    }
    free(resized);

    /* Write to file */
    if (write_png_file(png, png_len) == 0) {
        g_coverart.last_crc = crc;
        g_coverart.images_found++;
        LOG_INFO(LOG_MODULE, "Saved PNG #%d: %s (%zu bytes)",
                 g_coverart.images_found, COVERART_FILE, png_len);

        /* Write bus notification for Java watcher */
        write_coverart_notify(crc);
    } else {
        LOG_ERROR(LOG_MODULE, "Failed to write artwork");
    }

    free(png);
    return 1;
}

/* Check buffer for complete image */
static int maybe_handle_image(const uint8_t* data, size_t size) {
    if (!data || size < COVERART_MIN_BYTES || size > COVERART_MAX_BYTES) {
        return 0;
    }

    if (is_jpeg(data, size) || is_png(data, size)) {
        LOG_DEBUG(LOG_MODULE, "Found image: %zu bytes", size);
        return save_artwork(data, size);
    }
    return 0;
}

/* ============================================================
 * Async cover-art worker
 *
 * Decoding a 150 KB JPEG + resize + PNG encode + file write takes
 * ~50-100 ms.  When this runs synchronously on the recv()/read() hook
 * thread, every concurrent iAP2 packet on the same connection waits.
 * During CarPlay handshake that's enough latency to cause iOS to retry
 * frames (and sometimes give up on the first connect attempt).
 *
 * Strategy: detect a complete JPEG on the recv thread (cheap), copy
 * the bytes into a 1-slot pending queue, signal a worker.  Recv thread
 * returns immediately.  Worker runs save_artwork off-line.
 *
 * Coalescing: if a new image arrives while the previous is still being
 * processed, we drop the older pending one — only the most recent
 * artwork matters anyway (rapid track skips).
 * ============================================================ */

static pthread_t worker_thread;
static pthread_mutex_t worker_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t worker_cond = PTHREAD_COND_INITIALIZER;
static uint8_t* pending_data = NULL;     /* owned by queue */
static size_t pending_len = 0;
static int worker_started = 0;           /* guarded by worker_mutex */
static int worker_shutdown = 0;          /* guarded by worker_mutex */
static int worker_exited = 0;            /* guarded by worker_mutex */

/* Receive-callback quiescence.  Unregistering a function pointer alone is not
 * enough: a concurrent NmeTransport::Recv may already have copied the old
 * pointer.  The enter/leave count lets shutdown wait before freeing stream
 * buffers. */
static pthread_once_t coverart_init_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t runtime_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t runtime_cond = PTHREAD_COND_INITIALIZER;
static int runtime_initialized = 0;      /* guarded by runtime_mutex */
static int runtime_shutting_down = 0;    /* guarded by runtime_mutex */
static int runtime_callbacks = 0;        /* guarded by runtime_mutex */

static void coverart_deadline_after_ms(struct timespec* deadline, long ms) {
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_sec += ms / 1000L;
    deadline->tv_nsec += (ms % 1000L) * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

static void* coverart_worker_main(void* arg) {
    (void)arg;
    while (1) {
        uint8_t* data;
        size_t len;

        pthread_mutex_lock(&worker_mutex);
        while (!pending_data && !worker_shutdown) {
            pthread_cond_wait(&worker_cond, &worker_mutex);
        }
        if (worker_shutdown && !pending_data) {
            pthread_mutex_unlock(&worker_mutex);
            break;
        }
        data = pending_data;
        len = pending_len;
        pending_data = NULL;
        pending_len = 0;
        pthread_mutex_unlock(&worker_mutex);

        if (data) {
            (void)maybe_handle_image(data, len);
            free(data);
        }
    }
    pthread_mutex_lock(&worker_mutex);
    worker_exited = 1;
    pthread_cond_broadcast(&worker_cond);
    pthread_mutex_unlock(&worker_mutex);
    return NULL;
}

/* Hand off image bytes to the worker.  Takes ownership of `data` —
 * worker frees it.  The worker is created lazily for the first complete
 * image.  Never fall back to inline decode: this function is called from the
 * stock NmeIAP2Link receive thread and blocking it can wedge the session. */
static void enqueue_image_async(uint8_t* data, size_t len) {
    int create_rc = 0;
    int started_now = 0;

    if (!data) return;

    pthread_mutex_lock(&worker_mutex);
    if (worker_shutdown) {
        pthread_mutex_unlock(&worker_mutex);
        free(data);
        return;
    }

    if (!worker_started) {
        worker_exited = 0;
        create_rc = pthread_create(&worker_thread, NULL, coverart_worker_main, NULL);
        if (create_rc != 0) {
            pthread_mutex_unlock(&worker_mutex);
            LOG_ERROR(LOG_MODULE,
                      "Failed to start lazy cover-art worker rc=%d; dropping image to protect iAP2 receive thread",
                      create_rc);
            free(data);
            return;
        }
        worker_started = 1;
        started_now = 1;
    }

    if (pending_data) {
        /* Coalesce — drop the older pending image, latest wins. */
        free(pending_data);
    }
    pending_data = data;
    pending_len = len;
    pthread_cond_signal(&worker_cond);
    pthread_mutex_unlock(&worker_mutex);

    if (started_now)
        LOG_INFO(LOG_MODULE, "Lazy joinable cover-art worker started");
}

/* FF 5A reassembly core — MUST be called with st->lock held.  Appends `buf`
 * to the stream, extracts iAP2 link packets, accumulates the target JPEG.  On
 * a complete JPEG, returns it (malloc'd; caller enqueues + frees) via the
 * out_buf / out_len pointers. */
static void stream_reassemble_locked(StreamState* st,
                                     const uint8_t* buf, size_t len,
                                     uint8_t** out_buf, size_t* out_len) {
    *out_buf = NULL;
    *out_len = 0;

    /* Append to raw buffer */
    if (st->raw_len + len > st->raw_cap) {
        size_t new_cap = st->raw_cap ? st->raw_cap * 2 : 65536;
        while (new_cap < st->raw_len + len) new_cap *= 2;
        if (new_cap > STREAM_BUF_MAX) { st->raw_len = 0; return; }
        uint8_t* tmp = realloc(st->raw_buf, new_cap);
        if (!tmp) { st->raw_len = 0; return; }
        st->raw_buf = tmp;
        st->raw_cap = new_cap;
    }
    memcpy(st->raw_buf + st->raw_len, buf, len);
    st->raw_len += len;

    /* Process packets */
    size_t processed = 0;
    while (st->raw_len - processed >= 4) {
        uint8_t* p = st->raw_buf + processed;

        /* Check for FF 5A sync */
        if (p[0] != 0xFF || p[1] != 0x5A) {
            processed++;
            continue;
        }

        /* Packet length (big endian) */
        uint16_t packet_len = (uint16_t)((p[2] << 8) | p[3]);
        if (packet_len < 4) {
            processed++;
            continue;
        }

        /* Wait for complete packet */
        if (st->raw_len - processed < packet_len) {
            break;
        }

        /* Validate with lookahead */
        size_t next_offset = processed + packet_len;
        int has_lookahead = (st->raw_len >= next_offset + 2);

        if (has_lookahead) {
            if (st->raw_buf[next_offset] != 0xFF || st->raw_buf[next_offset + 1] != 0x5A) {
                processed++;
                continue;
            }
        } else if (processed > 0) {
            break;
        }

        /* Valid packet */
        st->is_carplay_stream = 1;

        /* Extract payload (skip header + wrapper, exclude trailer) */
        if (packet_len > TOTAL_STRIP_SIZE + PACKET_TRAILER_SIZE) {
            uint8_t session_id = p[7];
            uint8_t* payload_ptr = p + TOTAL_STRIP_SIZE;
            size_t payload_len = packet_len - TOTAL_STRIP_SIZE - PACKET_TRAILER_SIZE;

            /* Look for JPEG SOI to lock session */
            if (st->target_session == -1) {
                if (my_memmem(payload_ptr, payload_len, jpeg_soi, 2)) {
                    st->target_session = (int)session_id;
                    LOG_DEBUG(LOG_MODULE, "Locked session %d for JPEG", session_id);
                }
            }

            /* Append if target session */
            if (st->target_session == (int)session_id) {
                if (st->jpeg_len + payload_len > st->jpeg_cap) {
                    size_t need = st->jpeg_len + payload_len;
                    /* Hard cap: malformed stream w/o EOI marker would otherwise
                     * double jpeg_cap forever.  Reset and skip this payload. */
                    if (need > COVERART_MAX_JPEG) {
                        free(st->jpeg_buf);
                        st->jpeg_buf = NULL;
                        st->jpeg_len = 0;
                        st->jpeg_cap = 0;
                        st->target_session = -1;
                        break;
                    }
                    size_t new_cap = st->jpeg_cap ? st->jpeg_cap * 2 : 65536;
                    while (new_cap < need) new_cap *= 2;
                    if (new_cap > COVERART_MAX_JPEG) new_cap = COVERART_MAX_JPEG;
                    uint8_t* tmp = realloc(st->jpeg_buf, new_cap);
                    if (!tmp) break;
                    st->jpeg_buf = tmp;
                    st->jpeg_cap = new_cap;
                }
                memcpy(st->jpeg_buf + st->jpeg_len, payload_ptr, payload_len);
                st->jpeg_len += payload_len;
            }
        }

        processed += packet_len;
    }

    /* Remove processed bytes */
    if (processed > 0) {
        size_t remaining = st->raw_len - processed;
        if (remaining > 0) {
            memmove(st->raw_buf, st->raw_buf + processed, remaining);
        }
        st->raw_len = remaining;
    }

    /* Check for complete JPEG */
    if (st->jpeg_len > 0) {
        uint8_t* soi = my_memmem(st->jpeg_buf, st->jpeg_len, jpeg_soi, 2);
        if (soi) {
            size_t soi_offset = (size_t)(soi - st->jpeg_buf);
            uint8_t* eoi = my_memmem(soi, st->jpeg_len - soi_offset, jpeg_eoi, 2);

            if (eoi) {
                size_t img_len = (size_t)((eoi - soi) + 2);
                LOG_DEBUG(LOG_MODULE, "Complete JPEG: %zu bytes", img_len);

                uint8_t* pbuf = malloc(img_len);
                if (pbuf) {
                    memcpy(pbuf, soi, img_len);
                    *out_buf = pbuf;
                    *out_len = img_len;
                }

                /* Reset for next image */
                st->jpeg_len = 0;
                st->target_session = -1;
            }
        }

        /* Safety: free if too large */
        if (st->jpeg_len > 10 * 1024 * 1024) {
            free(st->jpeg_buf);
            st->jpeg_buf = NULL;
            st->jpeg_len = 0;
            st->jpeg_cap = 0;
            st->target_session = -1;
        }
    }
}

/* NmeTransport::Recv seam — the sole cover-art input.  Fed by the framework's
 * Recv interposition (coverart_on_transport_recv).  Logs the first confirmed
 * FF 5A stream once so an on-unit run can verify the seam carries link frames. */
static void handle_recv_stream(const uint8_t* buf, size_t len) {
    if (!buf || len == 0) return;

    pthread_mutex_lock(&g_recv_stream.lock);
    uint8_t* process_buf = NULL;
    size_t process_len = 0;
    stream_reassemble_locked(&g_recv_stream, buf, len, &process_buf, &process_len);
    int confirmed = g_recv_stream.is_carplay_stream;
    pthread_mutex_unlock(&g_recv_stream.lock);

    static volatile int seam_confirmed = 0;
    if (confirmed && !seam_confirmed) {
        seam_confirmed = 1;
        LOG_INFO(LOG_MODULE, "NmeTransport::Recv seam confirmed (FF 5A) — cover art live");
    }

    if (process_buf) {
        enqueue_image_async(process_buf, process_len);
    }
}

static int coverart_callback_enter(void) {
    int allowed;

    pthread_mutex_lock(&runtime_mutex);
    allowed = runtime_initialized && !runtime_shutting_down;
    if (allowed) runtime_callbacks++;
    pthread_mutex_unlock(&runtime_mutex);
    return allowed;
}

static void coverart_callback_leave(void) {
    pthread_mutex_lock(&runtime_mutex);
    if (runtime_callbacks > 0) runtime_callbacks--;
    if (runtime_shutting_down && runtime_callbacks == 0)
        pthread_cond_broadcast(&runtime_cond);
    pthread_mutex_unlock(&runtime_mutex);
}

/* Sink registered with the framework (hook_set_transport_recv_sink).  Called
 * for every NmeTransport::Recv delivery.  (The framework logs the first
 * delivery's ret + leading bytes for on-unit seam validation.) */
static void coverart_on_transport_recv(const uint8_t* data, unsigned int len) {
    if (!coverart_callback_enter()) return;
    handle_recv_stream(data, (size_t)len);
    coverart_callback_leave();
}

/* Session-boundary reset (hook_set_transport_recv_reset).  Framework calls
 * this on a new Identify so a JPEG left half-reassembled by a prior session's
 * disconnect can't concatenate into the next.  dio_manager is spawned per
 * phone connect so the stream is normally fresh per process anyway; this is
 * belt-and-suspenders for any future single-process multi-session transport.
 * Buffers are kept (lengths reset) to avoid realloc churn. */
static void coverart_reset_recv_stream(void) {
    if (!coverart_callback_enter()) return;
    pthread_mutex_lock(&g_recv_stream.lock);
    g_recv_stream.raw_len = 0;
    g_recv_stream.jpeg_len = 0;
    g_recv_stream.target_session = -1;
    g_recv_stream.is_carplay_stream = 0;
    pthread_mutex_unlock(&g_recv_stream.lock);
    coverart_callback_leave();
}

/* Resolve write() once from the lazy runtime init (used for PNG output). */
static void coverart_resolve_syms(void) {
    if (!real_write) real_write = (WriteFunc)dlsym(RTLD_NEXT, "write");
}

static void coverart_runtime_init_once(void) {
    /* Only dio_manager owns the Cinemo CarPlay transport.  Inherited helper
     * processes remain completely inert: no sink, no symbol resolution and,
     * most importantly, no pthread. */
    if (!hook_process_is_dio_manager()) return;

    coverart_resolve_syms();

    pthread_mutex_lock(&runtime_mutex);
    runtime_shutting_down = 0;
    runtime_initialized = 1;
    pthread_mutex_unlock(&runtime_mutex);

    /* Register the narrow transport-receive tap — cover art's ONLY input.
     * No libc read/recv/open/close interposition.  The reset fires on each
     * new Identify to drop stale partial-JPEG state at a session boundary. */
    hook_set_transport_recv_sink(coverart_on_transport_recv);
    hook_set_transport_recv_reset(coverart_reset_recv_stream);

    LOG_INFO(LOG_MODULE,
             "Cover art runtime initialized lazily (NmeTransport::Recv seam; worker deferred)");
}

void coverart_runtime_init(void) {
    pthread_once(&coverart_init_once, coverart_runtime_init_once);
}

void coverart_runtime_shutdown(void) {
    pthread_t thread_to_join;
    int join_worker = 0;
    int callbacks_quiesced = 0;
    int worker_quiesced = 1;
    int wait_rc = 0;
    struct timespec deadline;
    uint8_t* raw_buf;
    uint8_t* jpeg_buf;

    /* Unregister the framework taps FIRST so no new Recv delivery enters
     * cover art.  A caller that already copied the old function pointer is
     * covered by runtime_callbacks below. */
    hook_set_transport_recv_sink(NULL);
    hook_set_transport_recv_reset(NULL);

    pthread_mutex_lock(&runtime_mutex);
    if (!runtime_initialized) {
        pthread_mutex_unlock(&runtime_mutex);
        return;
    }
    if (runtime_shutting_down) {
        pthread_mutex_unlock(&runtime_mutex);
        return;
    }
    runtime_shutting_down = 1;
    coverart_deadline_after_ms(&deadline, COVERART_CALLBACK_DRAIN_MS);
    while (runtime_callbacks != 0 && wait_rc != ETIMEDOUT)
        wait_rc = pthread_cond_timedwait(&runtime_cond, &runtime_mutex,
                                         &deadline);
    callbacks_quiesced = (runtime_callbacks == 0);
    pthread_mutex_unlock(&runtime_mutex);

    if (!callbacks_quiesced)
        LOG_WARN(LOG_MODULE,
                 "cover-art callback drain exceeded %d ms; preserving buffers for process-exit reclaim",
                 COVERART_CALLBACK_DRAIN_MS);

    /* Tell the worker to exit and drop an unprocessed coalesced image.  A
     * decode already in progress is allowed to finish and publish while the
     * framework bus is still alive; pthread_join then gives a hard lifetime
     * boundary before this shared object's text can be unloaded. */
    pthread_mutex_lock(&worker_mutex);
    worker_shutdown = 1;
    if (pending_data) {
        free(pending_data);
        pending_data = NULL;
        pending_len = 0;
    }
    pthread_cond_signal(&worker_cond);
    if (worker_started) {
        thread_to_join = worker_thread;
        worker_quiesced = worker_exited;
        wait_rc = 0;
        coverart_deadline_after_ms(&deadline, COVERART_WORKER_DRAIN_MS);
        while (!worker_exited && wait_rc != ETIMEDOUT)
            wait_rc = pthread_cond_timedwait(&worker_cond, &worker_mutex,
                                             &deadline);
        worker_quiesced = worker_exited;
        join_worker = worker_quiesced;
    }
    pthread_mutex_unlock(&worker_mutex);

    if (join_worker) {
        int join_rc = pthread_join(thread_to_join, NULL);
        if (join_rc != 0)
            LOG_ERROR(LOG_MODULE, "cover-art worker pthread_join failed rc=%d", join_rc);
    } else if (!worker_quiesced) {
        LOG_WARN(LOG_MODULE,
                 "cover-art worker drain exceeded %d ms; process-exit cleanup will reclaim it",
                 COVERART_WORKER_DRAIN_MS);
    }

    pthread_mutex_lock(&worker_mutex);
    if (worker_quiesced) worker_started = 0;
    pthread_mutex_unlock(&worker_mutex);

    /* No callback or worker can touch the reassembler now.  Detach its
     * buffers under the stream lock, then free outside the critical section. */
    if (!callbacks_quiesced) goto finish;
    pthread_mutex_lock(&g_recv_stream.lock);
    raw_buf = g_recv_stream.raw_buf;
    jpeg_buf = g_recv_stream.jpeg_buf;
    g_recv_stream.raw_buf = NULL;
    g_recv_stream.raw_len = 0;
    g_recv_stream.raw_cap = 0;
    g_recv_stream.jpeg_buf = NULL;
    g_recv_stream.jpeg_len = 0;
    g_recv_stream.jpeg_cap = 0;
    g_recv_stream.target_session = -1;
    g_recv_stream.is_carplay_stream = 0;
    pthread_mutex_unlock(&g_recv_stream.lock);

    free(raw_buf);
    free(jpeg_buf);

    LOG_INFO(LOG_MODULE, "Cleanup: recv_bytes seam, images=%d",
             g_coverart.images_found);

finish:
    pthread_mutex_lock(&runtime_mutex);
    runtime_initialized = 0;
    pthread_cond_broadcast(&runtime_cond);
    pthread_mutex_unlock(&runtime_mutex);
}
