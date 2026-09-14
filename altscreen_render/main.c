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
#include "protocol.h"
#include "frame_io.h"
#include "sink.h"
#include "altctl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
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
    uint32_t len;
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
static unsigned int    g_decode_items;
static size_t          g_decode_bytes;
static uint32_t        g_decode_generation;
static int             g_decode_client_fd = -1; /* same lock as generation; reader owns close */
static int             g_decode_failed;
static uint32_t         g_decode_active_generation; /* g_decode_lock */
static int             g_decode_stop;
static pthread_t       g_decode_thread;
static int             g_decode_thread_started;
static volatile int    g_decode_thread_exited;
static unsigned long   g_decode_dropped;
static unsigned long long g_decode_last_progress_ms;
static int             g_decode_worker_busy;

static unsigned long long decode_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000ULL
         + (unsigned long long)ts.tv_nsec / 1000000ULL;
}

static void decode_item_free(decode_item_t *item) {
    if (!item) return;
    free(item->data);
    free(item);
}

static void decode_queue_clear_locked(void) {
    decode_item_t *item = g_decode_head;
    while (item) {
        decode_item_t *next = item->next;
        decode_item_free(item);
        item = next;
    }
    g_decode_head = g_decode_tail = NULL;
    g_decode_items = 0;
    g_decode_bytes = 0;
}

static int decode_queue_push_locked(int type, uint32_t generation,
                                    const uint8_t *data, uint32_t len) {
    decode_item_t *item;
    if (g_decode_stop) return -1;
    if (type == DECODE_FRAME &&
        (g_decode_items >= DECODE_QUEUE_MAX_ITEMS ||
         g_decode_bytes + (size_t)len > DECODE_QUEUE_MAX_BYTES)) {
        g_decode_dropped++;
        if (g_decode_dropped <= 3 || (g_decode_dropped % 120) == 0)
            fprintf(stderr,
                    "altscreen_render: decode queue full; dropped frame #%lu len=%u\n",
                    g_decode_dropped, (unsigned)len);
        return -1;
    }
    item = (decode_item_t *)calloc(1, sizeof(*item));
    if (!item) return -1;
    if (len) {
        item->data = (uint8_t *)malloc(len);
        if (!item->data) { free(item); return -1; }
        memcpy(item->data, data, len);
    }
    item->type = type;
    item->generation = generation;
    item->len = len;
    item->received_ms = decode_now_ms();
    if (g_decode_tail) g_decode_tail->next = item;
    else g_decode_head = item;
    g_decode_tail = item;
    g_decode_items++;
    g_decode_bytes += len;
    pthread_cond_signal(&g_decode_cond);
    return 0;
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
        if (g_decode_bytes >= item->len) g_decode_bytes -= item->len;
        else g_decode_bytes = 0;
        g_decode_worker_busy = 1;
        pthread_mutex_unlock(&g_decode_lock);

        if (item->type == DECODE_BEGIN) {
            if (decode_generation_is_current(item->generation)) {
                decode_set_active_generation(0);
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
            if (++frames <= 3 || frames % 600 == 0) {
                fprintf(stderr,
                        "altscreen_render: decode intake #%lu age=%llu ms max=%llu ms generation=%u\n",
                        frames, (unsigned long long)age, (unsigned long long)ageMax,
                        (unsigned)item->generation);
                ageMax = 0;
            }
            if (item->generation == workerGeneration &&
                decode_generation_is_current(item->generation) &&
                av_sink_frame(item->data, item->len, item->received_ms) < 0) {
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
        pthread_mutex_unlock(&g_decode_lock);
        decode_item_free(item);
    }
    decode_set_active_generation(0);
    av_sink_session_end();
    g_decode_thread_exited = 1;
    __sync_synchronize();
    return NULL;
}

static int decode_worker_start(void) {
    int result;
    g_decode_stop = 0;
    g_decode_thread_exited = 0;
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

static int decode_queue_frame(uint32_t generation,
                              const uint8_t *data, uint32_t len) {
    int result;
    int workerStuck = 0;
    unsigned long long nowMs;
    pthread_mutex_lock(&g_decode_lock);
    if (generation != g_decode_generation || g_decode_failed) result = -1;
    else result = decode_queue_push_locked(DECODE_FRAME, generation, data, len);
    nowMs = decode_now_ms();
    if (result == 0 && g_decode_items > 2 && nowMs != 0 &&
        g_decode_last_progress_ms != 0 &&
        nowMs - g_decode_last_progress_ms > 20000ULL)
        workerStuck = 1;
    pthread_mutex_unlock(&g_decode_lock);
    if (workerStuck) {
        fprintf(stderr,
                "altscreen_render: decoder worker made no progress for 20s; exiting isolated renderer\n");
        _exit(90);
    }
    return result;
}

static void decode_queue_end(uint32_t generation) {
    pthread_mutex_lock(&g_decode_lock);
    if (generation == g_decode_generation) {
        g_decode_client_fd = -1;
        decode_queue_push_locked(DECODE_END, generation, NULL, 0);
    }
    pthread_mutex_unlock(&g_decode_lock);
}

static int decode_video_ready(void) {
    int ready;
    pthread_mutex_lock(&g_decode_lock);
    /* Keep the generation and sink snapshot together. The sink readiness lock
     * is a leaf here: no sink callback acquires g_decode_lock. An old callback
     * or queued END must never make a new/unconnected stream appear ready. */
    ready = !g_decode_stop && !g_decode_failed && g_decode_client_fd >= 0 &&
        g_decode_active_generation == g_decode_generation && av_sink_video_ready();
    pthread_mutex_unlock(&g_decode_lock);
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
    while (!g_decode_thread_exited && waited < 1500) {
        struct timespec pause;
        pause.tv_sec = 0;
        pause.tv_nsec = 10L * 1000000L;
        nanosleep(&pause, NULL);
        waited += 10;
    }
    if (!g_decode_thread_exited) {
        fprintf(stderr,
                "altscreen_render: decoder worker stop TIMEOUT; exiting isolated renderer\n");
        _exit(88);
    }
    pthread_join(g_decode_thread, NULL);
    g_decode_thread_started = 0;
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
    if (++frames % 600 == 0) {
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
    int one = 1;
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
    uint32_t cap = 0, len = 0;
    for (;;) {
        int r = av_read_frame(cfd, &buf, &cap, &len);
        if (r == 0) {
            if (len > 0) socket_pace_sample();
            if (len > 0 && decode_queue_frame(generation, buf, len) != 0) {
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
    fprintf(stderr, "altscreen_render: listening on %s:%d\n", ALTAV_HOST, ALTAV_PORT);

    /* Control channel to Java/lsd — reports video on/off so Java flips setVideoAvailable(). */
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
                fprintf(stderr, "altscreen_render: hook connected generation=%u\n",
                        (unsigned)generation);
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
