/*
 * CarPlay Cover Art Hook - Transport layer interceptor
 *
 * Hooks read()/recv() to capture iAP2 transport packets (FF 5A framed),
 * assembles JPEG from chunks, decodes, resizes to 256x256, outputs as PNG file.
 */

#include "../framework/common.h"
#include "../framework/logging.h"
#include "../framework/bus.h"

/* stb_image for JPEG/PNG decoding */
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
#define OUTPUT_HEIGHT 256

/* Packet parser constants */
#define PACKET_HEADER_SIZE  4   /* FF 5A LL LL */
#define PACKET_WRAPPER_SIZE 7   /* 40 .. .. .. .. .. 80 */
#define PACKET_TRAILER_SIZE 1   /* Checksum byte */
#define TOTAL_STRIP_SIZE    (PACKET_HEADER_SIZE + PACKET_WRAPPER_SIZE)

/* Stream buffer limits */
#define STREAM_BUF_MAX      (1024 * 1024)
#define MAX_STREAMS         16

/* JPEG markers */
static const uint8_t jpeg_soi[] = {0xFF, 0xD8};
static const uint8_t jpeg_eoi[] = {0xFF, 0xD9};

/* Function pointer types */
typedef ssize_t (*ReadFunc)(int fd, void* buf, size_t count);
typedef ssize_t (*RecvFunc)(int sockfd, void* buf, size_t len, int flags);
typedef ssize_t (*WriteFunc)(int fd, const void* buf, size_t count);

static ReadFunc real_read = NULL;
static RecvFunc real_recv = NULL;
static WriteFunc real_write = NULL;

/* Stream state for packet assembly */
typedef struct {
    int fd;
    uint8_t* raw_buf;
    size_t raw_len;
    size_t raw_cap;
    uint8_t* jpeg_buf;
    size_t jpeg_len;
    size_t jpeg_cap;
    int target_session;
    int is_carplay_stream;
    int probe_calls;       /* incremented each handle_stream_data while !is_carplay_stream */
    int active;
} StreamState;

static StreamState streams[MAX_STREAMS];
static pthread_mutex_t stream_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Per-fd "skip" flag — set to 1 once we're confident a fd never carries
 * iAP2 (after FD_PROBE_LIMIT reads without a single FF 5A packet).
 * Lock-free single-byte read in the recv()/read() hooks for early exit;
 * dio_manager opens many fds for IPC / logs / audio — without this filter
 * every read on every fd does a mutex lock + linear FF 5A scan.
 *
 * Cleared on close() so a reused fd starts a fresh probe. */
#define FD_TABLE_SIZE     1024
#define FD_PROBE_LIMIT    64
static volatile uint8_t fd_skip[FD_TABLE_SIZE];

typedef int (*CloseFunc)(int);
static CloseFunc real_close = NULL;

/* Module state */
static struct {
    uint32_t last_crc;
    int read_count;
    int recv_count;
    int images_found;
    int slot;           /* ping-pong: 0 or 1 */
} g_coverart = {
    .last_crc = 0,
    .read_count = 0,
    .recv_count = 0,
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
static volatile int worker_started = 0;
static volatile int worker_shutdown = 0;
static volatile int worker_busy = 0;     /* 1 while inside maybe_handle_image */

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
            worker_busy = 1;
            (void)maybe_handle_image(data, len);
            free(data);
            worker_busy = 0;
        }
    }
    return NULL;
}

static void ensure_worker_started(void) {
    pthread_mutex_lock(&worker_mutex);
    if (!worker_started) {
        if (pthread_create(&worker_thread, NULL, coverart_worker_main, NULL) == 0) {
            pthread_detach(worker_thread);
            worker_started = 1;
            LOG_INFO(LOG_MODULE, "Async cover-art worker started");
        } else {
            LOG_ERROR(LOG_MODULE, "Failed to start cover-art worker (will fall back to sync decode)");
        }
    }
    pthread_mutex_unlock(&worker_mutex);
}

/* Hand off image bytes to the worker.  Takes ownership of `data` —
 * worker frees it.  If the worker isn't running, processes inline so
 * we never lose an image. */
static void enqueue_image_async(uint8_t* data, size_t len) {
    if (!worker_started) {
        /* Worker not running — fall back to inline processing so cover
         * art still appears even in degraded mode. */
        (void)maybe_handle_image(data, len);
        free(data);
        return;
    }

    pthread_mutex_lock(&worker_mutex);
    if (pending_data) {
        /* Coalesce — drop the older pending image, latest wins. */
        free(pending_data);
    }
    pending_data = data;
    pending_len = len;
    pthread_cond_signal(&worker_cond);
    pthread_mutex_unlock(&worker_mutex);
}

/* Get or create stream state for fd */
static StreamState* get_stream(int fd) {
    StreamState* free_slot = NULL;

    for (int i = 0; i < MAX_STREAMS; i++) {
        if (streams[i].active && streams[i].fd == fd) {
            return &streams[i];
        }
        if (!streams[i].active && !free_slot) {
            free_slot = &streams[i];
        }
    }

    if (!free_slot) return NULL;

    free_slot->fd = fd;
    free_slot->raw_buf = NULL;
    free_slot->raw_len = 0;
    free_slot->raw_cap = 0;
    free_slot->jpeg_buf = NULL;
    free_slot->jpeg_len = 0;
    free_slot->jpeg_cap = 0;
    free_slot->target_session = -1;
    free_slot->is_carplay_stream = 0;
    free_slot->probe_calls = 0;
    free_slot->active = 1;
    return free_slot;
}

/* Handle incoming stream data - parse FF 5A packets */
static void handle_stream_data(int fd, const uint8_t* buf, size_t len) {
    if (fd < 0 || fd >= 1024 || !buf || len == 0) return;

    pthread_mutex_lock(&stream_mutex);

    StreamState* st = get_stream(fd);
    if (!st) {
        pthread_mutex_unlock(&stream_mutex);
        return;
    }

    /* Append to raw buffer */
    if (st->raw_len + len > st->raw_cap) {
        size_t new_cap = st->raw_cap ? st->raw_cap * 2 : 65536;
        while (new_cap < st->raw_len + len) new_cap *= 2;
        if (new_cap > STREAM_BUF_MAX) {
            st->raw_len = 0;
            pthread_mutex_unlock(&stream_mutex);
            return;
        }
        uint8_t* tmp = realloc(st->raw_buf, new_cap);
        if (!tmp) {
            st->raw_len = 0;
            pthread_mutex_unlock(&stream_mutex);
            return;
        }
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
                    size_t new_cap = st->jpeg_cap ? st->jpeg_cap * 2 : 65536;
                    while (new_cap < st->jpeg_len + payload_len) new_cap *= 2;
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
    uint8_t* process_buf = NULL;
    size_t process_len = 0;

    if (st->jpeg_len > 0) {
        uint8_t* soi = my_memmem(st->jpeg_buf, st->jpeg_len, jpeg_soi, 2);
        if (soi) {
            size_t soi_offset = (size_t)(soi - st->jpeg_buf);
            uint8_t* eoi = my_memmem(soi, st->jpeg_len - soi_offset, jpeg_eoi, 2);

            if (eoi) {
                size_t img_len = (size_t)((eoi - soi) + 2);
                LOG_DEBUG(LOG_MODULE, "Complete JPEG: %zu bytes", img_len);

                process_buf = malloc(img_len);
                if (process_buf) {
                    memcpy(process_buf, soi, img_len);
                    process_len = img_len;
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

    /* fd-filter: if this fd has been read N times without ever showing a
     * valid FF 5A iAP2 packet, mark it skip so future recv()/read() on
     * it bypass handle_stream_data entirely.  Confirmed iAP2 fds are
     * never marked. */
    if (!st->is_carplay_stream) {
        st->probe_calls++;
        if (st->probe_calls >= FD_PROBE_LIMIT
                && fd >= 0 && fd < FD_TABLE_SIZE) {
            fd_skip[fd] = 1;
            /* Free buffers — fd is permanently skipped, no point keeping them. */
            free(st->raw_buf);
            st->raw_buf = NULL;
            st->raw_len = 0;
            st->raw_cap = 0;
            st->active = 0;  /* release stream slot for actual iAP2 fd */
        }
    }

    pthread_mutex_unlock(&stream_mutex);

    /* Hand off to async worker so the recv()/read() thread doesn't
     * stall on JPEG decode + PNG encode (~50-100 ms).  Worker takes
     * ownership of process_buf and frees it after processing. */
    if (process_buf) {
        enqueue_image_async(process_buf, process_len);
    }
}

/* Hook: read() */
ssize_t read(int fd, void* buf, size_t count) {
    ssize_t result;

    if (!real_read) {
        real_read = (ReadFunc)dlsym(RTLD_NEXT, "read");
        if (!real_read) {
            return -1;
        }
        LOG_INFO(LOG_MODULE, "read() hooked");
    }

    result = real_read(fd, buf, count);

    /* Fast skip path: fds we've confirmed don't carry iAP2 — bypass
     * mutex + scan entirely.  Critical: dio_manager has many non-iAP2
     * fds (logs, IPC) that would otherwise burn cycles every read. */
    if (result > 0 && buf
            && (fd < 0 || fd >= FD_TABLE_SIZE || !fd_skip[fd])) {
        g_coverart.read_count++;
        handle_stream_data(fd, (const uint8_t*)buf, (size_t)result);
    }

    return result;
}

/* Hook: recv() */
ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
    ssize_t result;

    if (!real_recv) {
        real_recv = (RecvFunc)dlsym(RTLD_NEXT, "recv");
        if (!real_recv) {
            return -1;
        }
        LOG_INFO(LOG_MODULE, "recv() hooked");
    }

    result = real_recv(sockfd, buf, len, flags);

    if (result > 0 && buf
            && (sockfd < 0 || sockfd >= FD_TABLE_SIZE || !fd_skip[sockfd])) {
        g_coverart.recv_count++;
        handle_stream_data(sockfd, (const uint8_t*)buf, (size_t)result);
    }

    return result;
}

/* Hook: close() — clear fd_skip flag and free stream slot so a reused
 * fd starts fresh probing.  Without this, a reused fd would stay
 * permanently skipped and we'd miss iAP2 packets if iPhone reconnects
 * onto the same fd number. */
int close(int fd) {
    if (!real_close) {
        real_close = (CloseFunc)dlsym(RTLD_NEXT, "close");
        if (!real_close) {
            errno = EBADF;
            return -1;
        }
    }

    if (fd >= 0 && fd < FD_TABLE_SIZE) {
        fd_skip[fd] = 0;
        pthread_mutex_lock(&stream_mutex);
        for (int i = 0; i < MAX_STREAMS; i++) {
            if (streams[i].active && streams[i].fd == fd) {
                free(streams[i].raw_buf);
                free(streams[i].jpeg_buf);
                streams[i].raw_buf = NULL;
                streams[i].jpeg_buf = NULL;
                streams[i].raw_len = streams[i].raw_cap = 0;
                streams[i].jpeg_len = streams[i].jpeg_cap = 0;
                streams[i].active = 0;
                break;
            }
        }
        pthread_mutex_unlock(&stream_mutex);
    }

    return real_close(fd);
}

__attribute__((constructor))
static void coverart_init(void) {
    memset(streams, 0, sizeof(streams));
    /* Spin up the async decode worker eagerly — by the time the first
     * complete JPEG arrives, the worker is ready to take it.  Avoids
     * lazy-start overhead landing on the first cover-art event. */
    ensure_worker_started();
    LOG_INFO(LOG_MODULE, "Cover art hook initialized (read/recv)");
}

__attribute__((destructor))
static void coverart_fini(void) {
    /* Tell the worker to exit so it doesn't outlive the library. */
    pthread_mutex_lock(&worker_mutex);
    worker_shutdown = 1;
    if (pending_data) {
        free(pending_data);
        pending_data = NULL;
        pending_len = 0;
    }
    pthread_cond_signal(&worker_cond);
    pthread_mutex_unlock(&worker_mutex);

    /* Best-effort wait if worker is mid-decode — up to 200 ms.  Avoids
     * the rare SIGSEGV where dlclose pulls the library text out from
     * under stbi_load_from_memory.  Detached worker means we can't
     * pthread_join, so poll worker_busy with a short backoff. */
    for (int i = 0; i < 20 && worker_busy; i++) {
        struct timespec ts = { 0, 10L * 1000L * 1000L };  /* 10 ms */
        nanosleep(&ts, NULL);
    }

    /* Free all stream buffers (active or not) */
    for (int i = 0; i < MAX_STREAMS; i++) {
        free(streams[i].raw_buf);
        free(streams[i].jpeg_buf);
    }

    LOG_INFO(LOG_MODULE, "Cleanup: read=%d recv=%d images=%d",
             g_coverart.read_count, g_coverart.recv_count, g_coverart.images_found);
}
