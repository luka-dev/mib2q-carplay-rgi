/*
 * altscreen_render — receives the CarPlay cluster (111) H.264 (Annex-B) stream
 * from libairplay's alt forward (screenstream.c) over TCP :19831 and feeds it to a
 * video sink (file capture on Mac, OMX decode + cluster present on QNX).
 *
 * The socket thread drains libairplay continuously while a bounded worker queue
 * serializes decoder startup/teardown and frame delivery. On disconnect we
 * notify the sink and accept again.
 * POSIX sockets — builds on macOS and QNX 6.5.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include "log_stamp.h"
#include "protocol.h"
#include "frame_io.h"
#include "queue_policy.h"
#include "../common/alt_control_wire.h"
#include "../common/alt_timing_stats.h"
#include "sink.h"
#include "altctl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_running = 1;
static volatile sig_atomic_t g_listen_fd = -1;
static volatile sig_atomic_t g_client_fd = -1;
static volatile int g_startup_complete;
static volatile int g_teardown_complete;

/* av_sink_open creates Screen/EGL before :19831 starts listening. A Qualcomm
 * driver hang there otherwise leaves a live PID forever; the supervisor's
 * recurring check intentionally avoids netstat churn and would adopt it. */
static void *startup_watchdog_main(void *unused) {
    unsigned int waited = 0;
    (void)unused;
    while (!g_startup_complete && waited < 15000) {
        struct timespec pause = { 0, 100L * 1000000L };
        nanosleep(&pause, NULL);
        waited += 100;
    }
    if (!g_startup_complete) {
        static const char msg[] =
            "altscreen_render: Screen/EGL startup watchdog TIMEOUT; exiting isolated renderer\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(92);
    }
    return NULL;
}

static void *teardown_watchdog_main(void *unused) {
    unsigned int waited = 0;
    (void)unused;
    while (!g_teardown_complete && waited < 3500) {
        struct timespec pause = { 0, 100L * 1000000L };
        nanosleep(&pause, NULL);
        waited += 100;
    }
    if (!g_teardown_complete) {
        static const char msg[] =
            "altscreen_render: OMX/EGL teardown watchdog TIMEOUT; exiting isolated renderer\n";
        (void)write(STDERR_FILENO, msg, sizeof(msg) - 1);
        _exit(93);
    }
    return NULL;
}

/* Keep the loopback reader independent of Qualcomm OMX startup.  Loaded->Idle
 * can legitimately take up to ten seconds on APQ8064; doing that work inline
 * stopped draining :19831, filled the hook's socket, and made a dio-owned
 * receiver thread spend its complete send-stall budget in renderer I/O.
 *
 * The bounded queue preserves config+first IDR (they are the first messages of
 * each generation).  A newer connection atomically discards pending frames from
 * the superseded generation; a BEGIN marker resets OMX before its first frame. */
enum decode_item_type {
    DECODE_BEGIN = 1,
    DECODE_FRAME = 2,
    DECODE_END   = 3
};

typedef struct decode_item {
    struct decode_item *next;
    uint8_t *data;
    uint32_t len, cap;
    alt_frame_meta_t meta;
    uint32_t generation;
    uint64_t received_ms;
    int type;
} decode_item_t;

#define DECODE_QUEUE_MAX_ITEMS 1024u
#define DECODE_QUEUE_MAX_BYTES (24u * 1024u * 1024u)

static pthread_mutex_t g_decode_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_decode_cond = PTHREAD_COND_INITIALIZER;
static decode_item_t  *g_decode_head;
static decode_item_t  *g_decode_tail;
static decode_item_t *g_decode_pool;
static unsigned g_decode_pool_items;
static size_t g_decode_pool_bytes;
static int g_decode_steady;
static uint64_t g_decode_latest_input_ms;
static uint32_t g_health_owner;
static uint8_t g_health_packet[ALT_WIRE_STATUS_SIZE];
static unsigned g_health_used;
static int g_health_pending, g_health_last = -1;
static uint64_t g_health_last_us;
static alt_timing_stat_t g_decode_queue_timing;
#define DECODE_POOL_ITEMS 32u
#define DECODE_POOL_BYTES (8u * 1024u * 1024u)
#define DECODE_STEADY_BACKLOG_MS ((unsigned)ALT_QUEUE_BACKLOG_MS)

static unsigned int    g_decode_items;
static size_t          g_decode_bytes;
static uint32_t        g_decode_generation;
static int             g_decode_client_fd = -1; /* same lock as generation; reader owns close */
static int             g_decode_failed;
static uint32_t         g_decode_active_generation; /* g_decode_lock */
static int             g_decode_stop;
static pthread_t       g_decode_thread;
static int             g_decode_thread_started;
static int g_decode_thread_exited;
static unsigned long long g_decode_last_progress_ms;
static int             g_decode_worker_busy;
static uint64_t g_decode_busy_origin_ms;
static uint64_t decode_origin_ms(const decode_item_t *item, uint64_t now);

static unsigned long long decode_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL
         + (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static void decode_item_free(decode_item_t *item) {
    if (item) {
        free(item->data);
        free(item);
    }
}
static void decode_pool_clear_locked(void) {
    while (g_decode_pool) {
        decode_item_t *p = g_decode_pool;
        g_decode_pool = p->next;
        decode_item_free(p);
    }
    g_decode_pool_items = 0;
    g_decode_pool_bytes = 0;
}
static void decode_item_recycle_locked(decode_item_t *item) {
    if (g_decode_pool_items < DECODE_POOL_ITEMS &&
        item->cap <= DECODE_POOL_BYTES - g_decode_pool_bytes) {
        item->next = g_decode_pool;
        g_decode_pool = item;
        ++g_decode_pool_items;
        g_decode_pool_bytes += item->cap;
    } else
        decode_item_free(item);
}
static void decode_queue_clear_locked(void) {
    while (g_decode_head) {
        decode_item_t *p = g_decode_head;
        g_decode_head = p->next;
        decode_item_recycle_locked(p);
    }
    g_decode_tail = NULL;
    g_decode_items = 0;
    g_decode_bytes = 0;
}
static int decode_queue_push_ex_locked(int type, uint32_t generation, const uint8_t *data,
                                       uint32_t len, const alt_frame_meta_t *meta, uint8_t **owned,
                                       uint32_t *owned_cap) {
    decode_item_t *item;
    if (g_decode_stop || (type == DECODE_FRAME && (g_decode_items >= DECODE_QUEUE_MAX_ITEMS ||
                                                   len > DECODE_QUEUE_MAX_BYTES - g_decode_bytes)))
        return -1;
    item = g_decode_pool;
    if (item) {
        g_decode_pool = item->next;
        --g_decode_pool_items;
        g_decode_pool_bytes -= item->cap;
    } else
        item = (decode_item_t *)calloc(1, sizeof(*item));
    if (!item)
        return -1;
    {
        uint32_t required = owned && owned_cap ? *owned_cap : (item->cap > len ? item->cap : len);
        if (required > DECODE_QUEUE_MAX_BYTES - g_decode_bytes) {
            decode_item_recycle_locked(item);
            return -1;
        }
    }
    if (owned && owned_cap) {
        uint8_t *spare = item->data;
        uint32_t spare_cap = item->cap;
        item->data = *owned;
        item->cap = *owned_cap;
        *owned = spare;
        *owned_cap = spare_cap;
    } else if (len) {
        if (item->cap < len) {
            uint8_t *p = (uint8_t *)realloc(item->data, len);
            if (!p) {
                decode_item_recycle_locked(item);
                return -1;
            }
            item->data = p;
            item->cap = len;
        }
        memcpy(item->data, data, len);
    }
    item->type = type;
    item->generation = generation;
    item->len = len;
    item->next = NULL;
    memset(&item->meta, 0, sizeof(item->meta));
    if (meta)
        item->meta = *meta;
    item->received_ms = decode_now_ms();
    if (g_decode_tail)
        g_decode_tail->next = item;
    else
        g_decode_head = item;
    g_decode_tail = item;
    ++g_decode_items;
    g_decode_bytes += item->cap;
    pthread_cond_signal(&g_decode_cond);
    return 0;
}
static int decode_queue_push_locked(int type, uint32_t generation, const uint8_t *data,
                                    uint32_t len) {
    return decode_queue_push_ex_locked(type, generation, data, len, NULL, NULL, NULL);
}

static int decode_generation_is_current(uint32_t generation) {
    int current;
    pthread_mutex_lock(&g_decode_lock);
    current = !g_decode_stop && generation == g_decode_generation;
    pthread_mutex_unlock(&g_decode_lock);
    return current;
}

static void decode_queue_fail(uint32_t generation) {
    pthread_mutex_lock(&g_decode_lock);
    if (generation == g_decode_generation && !g_decode_failed) {
        g_decode_failed = 1;
        g_decode_active_generation = 0;
        decode_queue_clear_locked();
        /* Wake the socket reader without closing/reusing its descriptor.
         * The hook sees EOF/write failure and requests a new config + IDR. */
        if (g_decode_client_fd >= 0)
            shutdown(g_decode_client_fd, SHUT_RDWR);
        fprintf(stderr,
                "altscreen_render: decoder lost frame continuity; reconnect generation=%u\n",
                (unsigned)generation);
    }
    pthread_mutex_unlock(&g_decode_lock);
}

static void decode_set_active_generation(uint32_t generation) {
    pthread_mutex_lock(&g_decode_lock);
    g_decode_active_generation = generation;
    pthread_mutex_unlock(&g_decode_lock);
}

static void *decode_worker_main(void *unused) {
    uint32_t workerGeneration = 0;
    unsigned long frames = 0;
    uint64_t ageMax = 0;
    (void)unused;
    for (;;) {
        decode_item_t *item;
        pthread_mutex_lock(&g_decode_lock);
        while (!g_decode_head && !g_decode_stop)
            pthread_cond_wait(&g_decode_cond, &g_decode_lock);
        if (!g_decode_head && g_decode_stop) {
            pthread_mutex_unlock(&g_decode_lock);
            break;
        }
        item = g_decode_head;
        g_decode_head = item->next;
        if (!g_decode_head) g_decode_tail = NULL;
        if (g_decode_items) g_decode_items--;
        if (g_decode_bytes >= item->cap)
            g_decode_bytes -= item->cap;
        else g_decode_bytes = 0;
        g_decode_worker_busy = 1;
        g_decode_busy_origin_ms =
            item->type == DECODE_FRAME ? decode_origin_ms(item, decode_now_ms()) : 0;
        pthread_mutex_unlock(&g_decode_lock);

        if (item->type == DECODE_BEGIN) {
            if (decode_generation_is_current(item->generation)) {
                decode_set_active_generation(0);
                memset(&g_decode_queue_timing, 0, sizeof(g_decode_queue_timing));
                av_sink_session_end();
                workerGeneration = item->generation;
                decode_set_active_generation(workerGeneration);
                fprintf(stderr,
                        "altscreen_render: decoder generation %u begin\n",
                        (unsigned)workerGeneration);
            }
        } else if (item->type == DECODE_FRAME) {
            uint64_t now = decode_now_ms();
            uint64_t age = item->received_ms && now >= item->received_ms
                ? now - item->received_ms : 0;
            if (age > ageMax) ageMax = age;
            if (++frames <= 3 || (frames % 600 == 0 && log_verbose_marker())) {
                fprintf(stderr,
                        "altscreen_render: decode intake #%lu age=%llu ms max=%llu ms generation=%u\n",
                        frames, (unsigned long long)age, (unsigned long long)ageMax,
                        (unsigned)item->generation);
                ageMax = 0;
            }
            alt_timing_add(&g_decode_queue_timing, age * 1000u);
            if (g_decode_queue_timing.samples >= 300u) {
                if (log_verbose_marker())
                    alt_timing_report(stderr, "altscreen_render", "decode_queue",
                                      &g_decode_queue_timing);
                else
                    memset(&g_decode_queue_timing, 0, sizeof(g_decode_queue_timing));
            }
            if (item->generation == workerGeneration &&
                decode_generation_is_current(item->generation) &&
                av_sink_frame_ex(item->data, item->len, item->received_ms, &item->meta) < 0) {
                decode_queue_fail(item->generation);
                av_sink_session_end();
                workerGeneration = 0;
            }
        } else if (item->type == DECODE_END) {
            if (item->generation == workerGeneration &&
                decode_generation_is_current(item->generation)) {
                decode_set_active_generation(0);
                av_sink_session_end();
                workerGeneration = 0;
            }
        }
        pthread_mutex_lock(&g_decode_lock);
        g_decode_last_progress_ms = decode_now_ms();
        g_decode_worker_busy = 0;
        g_decode_busy_origin_ms = 0;
        decode_item_recycle_locked(item);
        pthread_mutex_unlock(&g_decode_lock);
    }
    decode_set_active_generation(0);
    av_sink_session_end();
    __atomic_store_n(&g_decode_thread_exited, 1, __ATOMIC_RELEASE);
    return NULL;
}

static int decode_worker_start(void) {
    int result;
    g_decode_stop = 0;
    __atomic_store_n(&g_decode_thread_exited, 0, __ATOMIC_RELAXED);
    result = pthread_create(&g_decode_thread, NULL, decode_worker_main, NULL);
    if (result == 0) g_decode_thread_started = 1;
    return result == 0 ? 0 : -1;
}

static uint32_t decode_queue_begin(int client_fd) {
    uint32_t generation;
    pthread_mutex_lock(&g_decode_lock);
    generation = ++g_decode_generation;
    if (generation == 0) generation = ++g_decode_generation;
    g_decode_failed = 0;
    g_decode_steady = 0;
    g_decode_latest_input_ms = 0;
    g_health_owner = 0;
    g_health_used = 0;
    g_health_pending = 0;
    g_health_last = -1;
    g_health_last_us = 0;
    g_decode_client_fd = -1;
    /* Nothing pending from the old TCP owner may reach OMX after this point. */
    decode_queue_clear_locked();
    if (!g_decode_worker_busy) g_decode_last_progress_ms = decode_now_ms();
    if (decode_queue_push_locked(DECODE_BEGIN, generation, NULL, 0) != 0)
        generation = 0;
    else
        g_decode_client_fd = client_fd;
    pthread_mutex_unlock(&g_decode_lock);
    return generation;
}

/* Only trust origin time from this host's metadata clock. Future/unavailable
 * origins fall back to local enqueue time; output timestamps are never filtered. */
static uint64_t decode_origin_ms(const decode_item_t *item, uint64_t now) {
    return alt_queue_origin_ms(&item->meta, item->received_ms, now);
}

static int decode_queue_frame_ex(uint32_t generation, const uint8_t *data, uint32_t len,
                                 const alt_frame_meta_t *meta, uint8_t **owned,
                                 uint32_t *owned_cap) {
    int result, backlogged = 0;
    int workerStuck = 0;
    unsigned long long nowMs = decode_now_ms();
    pthread_mutex_lock(&g_decode_lock);
    if (generation != g_decode_generation || g_decode_failed)
        result = -1;
    else {
        /* Startup keeps its large buffer budget. After a healthy, drained stream,
         * only an accumulating compressed queue may trigger continuity recovery.
         * Never filter decoded output age, a final frame, or an idle static scene. */
        backlogged = alt_queue_has_backlog(
            g_decode_steady, g_decode_items,
            g_decode_head ? decode_origin_ms(g_decode_head, nowMs) : 0, nowMs);
        result = backlogged ? -1
                            : decode_queue_push_ex_locked(DECODE_FRAME, generation, data, len, meta,
                                                          owned, owned_cap);
        if (result == 0 && len)
            g_decode_latest_input_ms = decode_origin_ms(g_decode_tail, nowMs);
    }
    nowMs = decode_now_ms();
    if (result == 0 && g_decode_items > 2 && nowMs != 0 &&
        g_decode_last_progress_ms != 0 &&
        nowMs - g_decode_last_progress_ms > 20000ULL)
        workerStuck = 1;
    pthread_mutex_unlock(&g_decode_lock);
    if (result < 0) {
        if (backlogged)
            fprintf(stderr,
                    "altscreen_render: steady compressed backlog >%u ms; config+IDR recovery\n",
                    DECODE_STEADY_BACKLOG_MS);
        decode_queue_fail(generation);
    }
    if (workerStuck) {
        fprintf(stderr,
                "altscreen_render: decoder worker made no progress for 20s; exiting isolated renderer\n");
        _exit(90);
    }
    return result;
}

static int __attribute__((unused)) decode_queue_frame(uint32_t generation, const uint8_t *data,
                                                      uint32_t len) {
    return decode_queue_frame_ex(generation, data, len, NULL, NULL, NULL);
}

static void decode_queue_end(uint32_t generation) {
    pthread_mutex_lock(&g_decode_lock);
    if (generation == g_decode_generation) {
        g_decode_client_fd = -1;
        decode_queue_push_locked(DECODE_END, generation, NULL, 0);
    }
    pthread_mutex_unlock(&g_decode_lock);
}

/* Same mutex owns descriptor lifetime and partial control records. Status is
 * opt-in via ACK, so old hooks never accumulate unread heartbeat bytes. */
static void decode_health_send_locked(int ready) {
    uint64_t now = alt_timing_now_us();
    if (g_decode_client_fd < 0 || !g_health_owner || g_decode_stop)
        return;
    if (!g_health_pending &&
        (g_health_last != ready || !g_health_last_us || now - g_health_last_us >= 500000u)) {
        alt_control_record_t record = {ALT_WIRE_STATUS, g_health_owner, ready ? 1u : 0u};
        if (!alt_control_encode(g_health_packet, sizeof(g_health_packet), &record))
            return;
        g_health_pending = 1;
        g_health_used = 0;
    }
    if (g_health_pending) {
        ssize_t n = send(g_decode_client_fd, g_health_packet + g_health_used,
                         sizeof(g_health_packet) - g_health_used, 0);
        if (n > 0) {
            g_health_used += (unsigned)n;
            if (g_health_used == sizeof(g_health_packet)) {
                g_health_pending = 0;
                g_health_last = alt_wire_u32(g_health_packet + 8) ? 1 : 0;
                g_health_last_us = now;
            }
        }
    }
}

/* Build a bounded policy snapshot while holding the queue mutex. */
static int decode_startup_drained_locked(int ready, uint64_t now) {
    alt_queue_view_t view = {0};
    decode_item_t *item;
    unsigned i = 0;
    view.ready = ready;
    view.busy = g_decode_worker_busy;
    view.pending_count = g_decode_items;
    view.latest_origin_ms = g_decode_latest_input_ms;
    view.busy_origin_ms = g_decode_busy_origin_ms;
    if (view.pending_count > ALT_QUEUE_STARTUP_ITEMS)
        return 0;
    for (item = g_decode_head; item && i < ALT_QUEUE_STARTUP_ITEMS; item = item->next)
        view.pending_origin_ms[i++] = item->type == DECODE_FRAME ? decode_origin_ms(item, now) : 0;
    if (item || i != view.pending_count)
        return 0;
    return alt_queue_can_enter_steady(&view, now);
}

static int decode_video_ready(void) {
    int ready, backlogged = 0;
    uint32_t generation;
    pthread_mutex_lock(&g_decode_lock);
    /* Keep the generation and sink snapshot together. The sink readiness lock
     * is a leaf here: no sink callback acquires g_decode_lock. An old callback
     * or queued END must never make a new/unconnected stream appear ready. */
    ready = !g_decode_stop && !g_decode_failed && g_decode_client_fd >= 0 &&
        g_decode_active_generation == g_decode_generation && av_sink_video_ready();
    {
        uint64_t now = decode_now_ms();
        if (!g_decode_steady && decode_startup_drained_locked(ready, now))
            g_decode_steady = 1;
        generation = g_decode_generation;
        backlogged =
            alt_queue_has_backlog(g_decode_steady, g_decode_items,
                                  g_decode_head ? decode_origin_ms(g_decode_head, now) : 0, now);
    }
    decode_health_send_locked(backlogged ? 0 : ready);
    pthread_mutex_unlock(&g_decode_lock);
    if (backlogged) {
        decode_queue_fail(generation);
        return 0;
    }
    return ready;
}

static void decode_worker_stop(void) {
    unsigned int waited = 0;
    if (!g_decode_thread_started) return;
    pthread_mutex_lock(&g_decode_lock);
    g_decode_stop = 1;
    decode_queue_clear_locked();
    pthread_cond_broadcast(&g_decode_cond);
    pthread_mutex_unlock(&g_decode_lock);
    while (!__atomic_load_n(&g_decode_thread_exited, __ATOMIC_ACQUIRE) && waited < 1500) {
        struct timespec pause;
        pause.tv_sec = 0;
        pause.tv_nsec = 10L * 1000000L;
        nanosleep(&pause, NULL);
        waited += 10;
    }
    if (!__atomic_load_n(&g_decode_thread_exited, __ATOMIC_ACQUIRE)) {
        fprintf(stderr,
                "altscreen_render: decoder worker stop TIMEOUT; exiting isolated renderer\n");
        _exit(88);
    }
    pthread_join(g_decode_thread, NULL);
    g_decode_thread_started = 0;
    pthread_mutex_lock(&g_decode_lock);
    decode_pool_clear_locked();
    pthread_mutex_unlock(&g_decode_lock);
}

/* close() is async-signal-safe. Closing both sockets guarantees SIGTERM breaks
 * an idle accept() and an active/partial frame read immediately, so the normal
 * OMX/Screen teardown runs before the supervisor's SIGKILL backstop. */
static void on_signal(int sig) {
    int fd;
    (void)sig;
    g_running = 0;
    fd = (int)g_client_fd;
    g_client_fd = -1;
    if (fd >= 0) close(fd);
    fd = (int)g_listen_fd;
    g_listen_fd = -1;
    if (fd >= 0) close(fd);
}

static int listen_socket(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ALTAV_PORT);
    addr.sin_addr.s_addr = inet_addr(ALTAV_HOST);   /* loopback only */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); close(fd); return -1; }
    if (listen(fd, 1) < 0) { perror("listen"); close(fd); return -1; }
    return fd;
}

/* Socket-side arrival pacing.  screen_egl_present.c already reports the interval
 * between DECODED frames; that measurement cannot tell a jittery producer from a
 * jittery decoder.  Sampling the same statistic here, the instant a frame leaves
 * the loopback socket, splits the two: matching numbers mean the hook/phone
 * delivers unevenly, a smooth socket with a jittery present means we add it. */
static void socket_pace_sample(void) {
    static unsigned long long last_ms;
    static unsigned long long sum_ms, max_ms;
    static unsigned long samples, frames;
    unsigned long long now = decode_now_ms();
    if (now == 0) return;
    if (last_ms != 0 && now > last_ms) {
        unsigned long long d = now - last_ms;
        sum_ms += d;
        if (d > max_ms) max_ms = d;
        samples++;
    }
    last_ms = now;
    if (++frames % 600 == 0 && log_verbose_marker()) {
        fprintf(stderr,
                "altscreen_render: %lu frames off the socket; arrival avg=%lu max=%lu ms\n",
                frames,
                samples ? (unsigned long)(sum_ms / samples) : 0UL,
                (unsigned long)max_ms);
        sum_ms = 0; max_ms = 0; samples = 0;
    }
}

/* Serve one connected client until it disconnects or a protocol error. */
static void serve_client(int cfd, uint32_t generation) {
    int one = 1, descriptor_flags = fcntl(cfd, F_GETFL, 0);
    if (descriptor_flags < 0 || fcntl(cfd, F_SETFL, descriptor_flags | O_NONBLOCK) != 0)
        return;
    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    /* A static/occluded instrument-cluster scene is allowed to stop producing
     * encoded frames without closing the stream.  Do NOT put SO_RCVTIMEO on this
     * loopback connection: the old 5 s timeout treated that legal idle period as
     * EOF, tore down a healthy decoder and exposed the loader forever while the
     * hook was still waiting on the live iOS socket.  Process shutdown remains
     * bounded because on_signal() closes g_client_fd, and a hook/dio exit closes
     * the peer socket in the kernel.  KEEPALIVE covers a genuinely half-open TCP
     * peer without assigning protocol meaning to ordinary frame silence. */
    setsockopt(cfd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));

    uint8_t *buf = NULL;
    uint32_t cap = 0, len = 0, status_owner = 0;
    alt_wire_state_t wire_state = {0};
    uint64_t last_arrival = 0, last_pts = 0;
    unsigned timing_frames = 0;
    alt_timing_stat_t arrivals = {0}, sender_gaps = {0}, link_delays = {0};
    uint8_t hello[ALT_WIRE_HELLO_SIZE];
    alt_control_record_t greeting = {ALT_WIRE_HELLO, 0, ALT_HELLO_STATUS_CAPABILITY};
    if (!alt_control_encode(hello, sizeof(hello), &greeting))
        return;
    /* Best effort only: old send-only hooks never read this tiny greeting. */
    (void)send(cfd, hello, sizeof(hello), 0);
    for (;;) {
        alt_frame_meta_t meta;
        int r = av_read_frame_mode(cfd, &buf, &cap, &len, &meta, wire_state.version);
        if (r == 0 && meta.flags == ALT_WIRE_INTERNAL_ACK) {
            pthread_mutex_lock(&g_decode_lock);
            if (wire_state.version || g_health_owner || generation != g_decode_generation)
                r = -2;
            else {
                g_health_owner = meta.generation;
                status_owner = meta.generation;
            }
            pthread_mutex_unlock(&g_decode_lock);
            if (r == 0)
                continue;
        }
        if (r == 0 && len && status_owner &&
            (meta.version != ALT_WIRE_VERSION || meta.generation != status_owner))
            r = -2;
        if (r == 0 && alt_wire_accept(&wire_state, len, &meta) != 0)
            r = -2;
        if (r == 0) {
            if (len && !(meta.flags & ALT_WIRE_CONFIG)) {
                uint64_t now = alt_timing_now_us(), pts_gap;
                alt_timing_elapsed(&arrivals, last_arrival, now);
                last_arrival = now;
                if (meta.version) {
                    if (!alt_wire_pts_delta(last_pts, meta.sender_pts, &pts_gap))
                        alt_timing_add(&sender_gaps, pts_gap);
                    last_pts = meta.sender_pts;
                    alt_timing_elapsed(&link_delays, meta.received_us, now);
                }
                if (++timing_frames == 300u) {
                    if (log_verbose_marker()) {
                        alt_timing_report(stderr, "altscreen_render", "socket_arrivals", &arrivals);
                        alt_timing_report(stderr, "altscreen_render", "sender_gaps", &sender_gaps);
                        alt_timing_report(stderr, "altscreen_render", "hook_to_socket",
                                          &link_delays);
                    } else {
                        memset(&arrivals, 0, sizeof(arrivals));
                        memset(&sender_gaps, 0, sizeof(sender_gaps));
                        memset(&link_delays, 0, sizeof(link_delays));
                    }
                    timing_frames = 0;
                }
            }
            if (len > 0) socket_pace_sample();
            if (len > 0 && decode_queue_frame_ex(generation, buf, len, &meta, &buf, &cap) != 0) {
                fprintf(stderr,
                        "altscreen_render: decode queue rejected generation %u frame\n",
                        (unsigned)generation);
                break;
            }
            continue;
        }
        if (r == -2)
            fprintf(stderr, "altscreen_render: protocol error, dropping hook link\n");
        else
            fprintf(stderr,
                    "altscreen_render: hook link ended (EOF/error errno=%d); waiting for reconnect\n",
                    errno);
        break;                                   /* -1 disconnect / -2 proto error */
    }
    free(buf);
}

static void open_runtime_log(const char *path, const char *previous) {
    /* Keep RAM-backed diagnostics bounded. The hook log already has size-based
     * rotation; retain one previous renderer generation and truncate the current
     * file on process start. Reopen in append mode so the supervisor's in-place
     * truncation also resets where subsequent writes land. */
    unlink(previous);
    rename(path, previous);
    freopen(path, "a", stderr);
    setvbuf(stderr, NULL, _IONBF, 0);
}

int main(void) {
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);

    /* Qualcomm's QNX EGL loader dereferences its graphics-module root during
     * eglGetDisplay.  maneuver_render has always supplied this explicitly;
     * altscreen_render did not need EGL before the native-buffer GPU path.
     * Keep the renderer self-contained so a supervisor/environment change
     * cannot turn a missing variable into an egl14.so SIGSEGV. */
    if (!getenv("GRAPHICS_ROOT"))
        putenv("GRAPHICS_ROOT=/proc/boot");
    if (!getenv("IPL_CONFIG_DIR"))
        putenv("IPL_CONFIG_DIR=/etc/eso/production");

    open_runtime_log("/tmp/altscreen_render.log", "/tmp/altscreen_render.log.1");
    fprintf(stderr, "%s altscreen_render: process start\n", log_stamp());

    {
        pthread_t watchdog;
        if (pthread_create(&watchdog, NULL, startup_watchdog_main, NULL) == 0)
            pthread_detach(watchdog);
        else
            fprintf(stderr, "altscreen_render: startup watchdog create FAILED\n");
    }

    if (av_sink_open() != 0) return 1;
    if (decode_worker_start() != 0) {
        fprintf(stderr, "altscreen_render: decoder worker create FAILED\n");
        av_sink_close();
        return 1;
    }

    int lfd = listen_socket();
    if (lfd < 0) { decode_worker_stop(); av_sink_close(); return 1; }
    g_listen_fd = lfd;
    __sync_synchronize();
    g_startup_complete = 1;
    __sync_synchronize();
    fprintf(stderr, "%s altscreen_render: listening on %s:%d\n", log_stamp(), ALTAV_HOST, ALTAV_PORT);

    /* Start only after av_sink_open has posted PRISM. Java treats the first
     * VIDEO_ON/OFF as surface readiness before selecting cluster plane 99. */
    altctl_start(decode_video_ready);

    while (g_running) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            if (errno == EINTR) continue;        /* signal → re-check g_running */
            perror("accept");
            break;
        }
        g_client_fd = cfd;
        {
            uint32_t generation = decode_queue_begin(cfd);
            if (generation == 0) {
                fprintf(stderr, "altscreen_render: decoder BEGIN enqueue FAILED\n");
            } else {
                fprintf(stderr, "%s altscreen_render: hook connected generation=%u\n",
                        log_stamp(), (unsigned)generation);
                serve_client(cfd, generation);
                decode_queue_end(generation);
            }
        }
        if ((int)g_client_fd == cfd) {
            g_client_fd = -1;
            close(cfd);
        }
        fprintf(stderr, "altscreen_render: hook disconnected\n");
    }

    {
        pthread_t watchdog;
        if (pthread_create(&watchdog, NULL, teardown_watchdog_main, NULL) == 0)
            pthread_detach(watchdog);
        else
            fprintf(stderr, "altscreen_render: teardown watchdog create FAILED\n");
    }
    altctl_stop();
    if ((int)g_listen_fd == lfd) {
        g_listen_fd = -1;
        close(lfd);
    }
    decode_worker_stop();
    av_sink_close();
    __sync_synchronize();
    g_teardown_complete = 1;
    __sync_synchronize();
    return 0;
}
