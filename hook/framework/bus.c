/*
 * CarPlay Hook Bus - implementation
 *
 * See bus.h / bus_protocol.h for the public API and wire format.
 *
 * Topology: hook is the TCP CLIENT, Java HMI is the long-lived TCP SERVER
 * listening on 127.0.0.1:19810.  This matches our process lifetime
 * hierarchy (Java alive from boot, dio_manager spawned per phone connect)
 * and removes the retry-loop / sleep hacks that the inverse direction
 * needed.
 *
 * Threads:
 *   - connector thread: connect() to Java with retry.  On success, sends
 *     HELLO + sticky snapshot and then waits for writer-side disconnect.
 *   - writer thread   : drains send queue, writes frames to the current
 *     client fd with TCP_NODELAY.  On write error it drops the fd and
 *     waits for the next connect.
 *   - timer thread    : optional 1 Hz local tick for lightweight module
 *     debounce.  It does not send heartbeat traffic to Java.
 *
 * Shared state is protected by a single mutex.  The send queue is a
 * simple ring buffer of heap-allocated frames.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */

#include "bus.h"
#include "signal_guard.h"
#include "logging.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>

DEFINE_LOG_MODULE(BUS);

/* ============================================================
 * Configuration
 * ============================================================ */
#define SEND_QUEUE_CAPACITY   256        /* must be a power of two */
#define BUS_SHUTDOWN_WAIT_MS 2500
/* Directly indexed events and commands. The highest current command is
 * CMD_ALT_RGI (0x0116); keep Java CarplayBus.MAX_TYPES in sync. */
#define MAX_TYPES             0x0120
#if CMD_ALT_RGI >= MAX_TYPES
#error AltScreen command is outside the bus dispatch table
#endif

/* ============================================================
 * Internal frame
 * ============================================================ */
typedef struct frame_s {
    uint16_t  type;
    uint8_t   flags;
    uint32_t  seq;
    uint32_t  len;
    uint8_t*  payload;
} frame_t;

/* ============================================================
 * Per-type policy + sticky cache
 * ============================================================ */
typedef struct {
    bool          configured;
    bool          sticky;
    bus_policy_t  policy;
    frame_t       last;          /* cached sticky frame (payload heap) */
    bool          has_last;
    bus_handler_t handler;
    void*         handler_ctx;
} type_slot_t;

/* Indexed directly by type.  slot_for() returns NULL for out-of-range
 * types; callers must tolerate that (they log + skip).  Direct indexing
 * avoids silent collisions that would corrupt sticky cache / handler
 * dispatch for different event/command types. */
static type_slot_t  g_types[MAX_TYPES];

/* ============================================================
 * Send queue (fixed ring, protected by g_lock)
 *
 * This intentionally uses the simple mutex-backed queue from the first
 * TCP implementation.  The later lock-free MPSC queue is faster on paper
 * but the MU1316/QNX logs show the hook disappearing at the first 1 Hz
 * heartbeat publish, exactly when that queue is first exercised.
 * ============================================================ */
static frame_t     g_ring[SEND_QUEUE_CAPACITY];
static int         g_ring_head = 0;      /* next read  */
static int         g_ring_tail = 0;      /* next write */
static int         g_ring_count = 0;

/* ============================================================
 * Global state
 *
 *   g_lock      : general shared state (queue, client fd, tx seq,
 *                 sticky cache, per-type policy/sticky flags).
 *   g_htable_rw : dedicated rwlock for the handler table.  Readers
 *                 (dispatch) hold it for the duration of the handler
 *                 callback; writers (bus_on/bus_off) block until no
 *                 dispatches are in flight.  This gives callers the
 *                 guarantee that bus_off() returns only when no
 *                 more invocations of that handler are running, so
 *                 they can safely free the ctx they passed to bus_on.
 * ============================================================ */
static pthread_mutex_t  g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_cond = PTHREAD_COND_INITIALIZER;
static pthread_rwlock_t g_htable_rw = PTHREAD_RWLOCK_INITIALIZER;
/* Serialises frames AND closing/replacing their socket.  When both locks
 * are needed, acquire g_sock_write before g_lock.  A writer rechecks the
 * connection generation under these locks before using a captured fd. */
static pthread_mutex_t  g_sock_write = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       g_connector_tid;
static pthread_t       g_writer_tid;
static pthread_t       g_timer_tid;
/* Thread-state flags written from one thread (the thread itself on exit)
 * and read from others without lock — must be volatile so reads aren't
 * cached in registers. */
static volatile bool   g_connector_up = false;
static volatile bool   g_writer_up = false;
static volatile bool   g_timer_up = false;
static volatile bool   g_shutdown = false;
/* Threads are JOINABLE (not detached) so bus_shutdown() can wait for them to
 * actually leave hook code before we free shared state / the .so is unloaded.
 * These record which were successfully created (join only those). */
static bool            g_connector_created = false;
static bool            g_writer_created = false;
static bool            g_timer_created = false;

/* g_listen_fd removed - hook is now TCP client, no listening socket. */
static int             g_client_fd = -1;    /* protected by g_lock */
/* PID that ran bus_init().  dio_manager fork()s helper processes: a fork INHERITS g_client_fd
 * + the thread-state flags but NOT the threads.  When such a fork exits, its __destructor runs
 * bus_shutdown() → shutdown(g_client_fd, SHUT_RDWR) which, unlike close(), tears down the SHARED
 * socket and kills the REAL dio_manager's bus (→ RGI/cover-art churn we chased).  bus_shutdown()
 * is a no-op unless getpid()==g_bus_owner_pid, so only the owning process ever tears the bus down. */
static volatile int    g_bus_owner_pid = 0;
/* Bumped for every new connection; checked before a queued writer uses its fd.
 * The connector owns close(). Other threads only shutdown/retire the socket,
 * leaving its descriptor allocated until the connector has finished reading. */
static uint32_t        g_client_gen = 0;
static uint32_t        g_tx_seq = 1;

/* ============================================================
 * Small helpers
 * ============================================================ */
static type_slot_t* slot_for(uint16_t type) {
    if ((unsigned)type >= MAX_TYPES) return NULL;
    return &g_types[type];
}

static void frame_dispose(frame_t* f) {
    if (f && f->payload) {
        free(f->payload);
        f->payload = NULL;
    }
    if (f) {
        f->type = 0;
        f->flags = 0;
        f->len = 0;
        f->seq = 0;
    }
}

static hook_result_t frame_dup(frame_t* dst, const frame_t* src) {
    uint8_t* payload = NULL;
    if (src->len > 0) {
        payload = (uint8_t*)malloc(src->len);
        if (!payload) return HOOK_ERR_MEMORY;
        memcpy(payload, src->payload, src->len);
    }
    *dst = *src;
    dst->payload = payload;
    return HOOK_OK;
}

/* ============================================================
 * Queue ops — must hold g_lock
 * ============================================================ */
static bool q_is_full(void) { return g_ring_count == SEND_QUEUE_CAPACITY; }
static bool q_is_empty(void) { return g_ring_count == 0; }

static hook_result_t q_enqueue_nolock(const frame_t* f) {
    if (q_is_full()) return HOOK_ERR_BUSY;
    hook_result_t r = frame_dup(&g_ring[g_ring_tail], f);
    if (r != HOOK_OK) return r;
    g_ring_tail = (g_ring_tail + 1) % SEND_QUEUE_CAPACITY;
    g_ring_count++;
    return HOOK_OK;
}

static bool q_dequeue_nolock(frame_t* out) {
    if (q_is_empty()) return false;
    *out = g_ring[g_ring_head];              /* moves payload ownership */
    g_ring[g_ring_head].payload = NULL;
    g_ring_head = (g_ring_head + 1) % SEND_QUEUE_CAPACITY;
    g_ring_count--;
    return true;
}

/* ============================================================
 * IO: write full frame to fd, big-endian header
 * ============================================================ */
static int write_all(int fd, const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    while (len > 0) {
        /* flags MUST be 0 on QNX 6.5: the io-pkt stack does NOT implement MSG_NOSIGNAL (the macro
         * is defined in the headers but send() with it returns ENOSYS = "Function not implemented"),
         * which silently killed the WHOLE bus (RGD + coverart never reached Java).  SIGPIPE is
         * suppressed process-wide instead (SIG_IGN at bus init). */
        ssize_t n = send(fd, p, len, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Caller holds g_sock_write and g_lock. */
static void disconnect_current_nolock(int fd, uint32_t gen) {
    if (g_client_fd != fd || g_client_gen != gen) return;
    /* close alone need not wake a recv already blocked in another thread. */
    shutdown(fd, SHUT_RDWR);
    g_client_fd = -1;
    pthread_cond_broadcast(&g_cond);
}

static int send_frame(int fd, uint32_t gen, bool published, const frame_t* f) {
    uint8_t hdr[BUS_HEADER_SIZE];
    write_be32(hdr + 0, BUS_MAGIC);
    write_be32(hdr + 4, f->seq);
    write_be16(hdr + 8, f->type);
    hdr[10] = f->flags;
    hdr[11] = 0;
    write_be32(hdr + 12, f->len);

    /* Atomic on-wire: header and payload together, no interleaving. */
    pthread_mutex_lock(&g_sock_write);
    if (published) {
        pthread_mutex_lock(&g_lock);
        bool current = !g_shutdown && g_client_fd == fd && g_client_gen == gen;
        pthread_mutex_unlock(&g_lock);
        if (!current) {
            pthread_mutex_unlock(&g_sock_write);
            errno = ENOTCONN;
            return -1;
        }
    }
    int rc = 0;
    if (write_all(fd, hdr, sizeof(hdr)) != 0) {
        LOG_WARN(LOG_MODULE, "send header failed fd=%d type=0x%04x len=%u err=%s",
                 fd, f->type, f->len, strerror(errno));
        rc = -1;
    } else if (f->len > 0 && f->payload) {
        if (write_all(fd, f->payload, f->len) != 0) {
            LOG_WARN(LOG_MODULE, "send payload failed fd=%d type=0x%04x len=%u err=%s",
                     fd, f->type, f->len, strerror(errno));
            rc = -1;
        }
    }
    if (rc != 0 && published) {
        /* Retire before another writer can append bytes to a partial frame. */
        int saved_errno = errno;
        pthread_mutex_lock(&g_lock);
        disconnect_current_nolock(fd, gen);
        pthread_mutex_unlock(&g_lock);
        errno = saved_errno;
    }
    pthread_mutex_unlock(&g_sock_write);
    return rc;
}

/* ============================================================
 * Per-type configuration
 * ============================================================ */
void bus_set_sticky(uint16_t type, bool sticky) {
    pthread_mutex_lock(&g_lock);
    type_slot_t* s = slot_for(type);
    if (s) {
        s->configured = true;
        s->sticky = sticky;
    }
    pthread_mutex_unlock(&g_lock);
}

void bus_set_policy(uint16_t type, bus_policy_t policy) {
    pthread_mutex_lock(&g_lock);
    type_slot_t* s = slot_for(type);
    if (s) {
        s->configured = true;
        s->policy = policy;
    }
    pthread_mutex_unlock(&g_lock);
}

hook_result_t bus_on(uint16_t type, bus_handler_t handler, void* ctx) {
    if (!slot_for(type)) {
        LOG_WARN(LOG_MODULE, "bus_on: type 0x%04x out of range (max=0x%04x)", type, MAX_TYPES);
        return HOOK_ERR_PARAM;
    }
    pthread_rwlock_wrlock(&g_htable_rw);  /* waits for in-flight dispatches */
    type_slot_t* s = slot_for(type);
    s->handler = handler;
    s->handler_ctx = ctx;
    pthread_rwlock_unlock(&g_htable_rw);
    return HOOK_OK;
}

void bus_off(uint16_t type) {
    if (!slot_for(type)) return;
    pthread_rwlock_wrlock(&g_htable_rw);  /* waits for in-flight dispatches */
    type_slot_t* s = slot_for(type);
    s->handler = NULL;
    s->handler_ctx = NULL;
    pthread_rwlock_unlock(&g_htable_rw);
}

bool bus_is_connected(void) {
    pthread_mutex_lock(&g_lock);
    bool connected = (g_client_fd >= 0);
    pthread_mutex_unlock(&g_lock);
    return connected;
}

/* ============================================================
 * bus_send — sticky cache update + mutex-backed ring enqueue.
 *
 * Sticky cache + tx_seq update happens under g_lock (rare-ish path,
 * needs frame_dup heap allocation).  The ring enqueue is under g_lock
 * by design for MU1316/QNX reliability.
 * ============================================================ */
hook_result_t bus_send(uint16_t type, uint8_t flags,
                       const uint8_t* payload, uint32_t len) {
    if (len > BUS_MAX_PAYLOAD) return HOOK_ERR_PARAM;
    if (len && !payload) return HOOK_ERR_PARAM;
    if (!slot_for(type)) {
        LOG_WARN(LOG_MODULE, "bus_send: type 0x%04x out of range", type);
        return HOOK_ERR_PARAM;
    }

    frame_t f;
    f.type = type;
    f.flags = flags;
    f.len = len;
    f.seq = 0;
    f.payload = NULL;
    if (len > 0) {
        f.payload = (uint8_t*)malloc(len);
        if (!f.payload) return HOOK_ERR_MEMORY;
        memcpy(f.payload, payload, len);
    }

    /* Sticky cache + seq under g_lock.  This is short and bounded,
     * uncontended on the producer hot path (only sticky-flagged sends
     * reach the cache copy). */
    type_slot_t* s = slot_for(type);
    uint32_t my_seq;

    pthread_mutex_lock(&g_lock);
    if (g_shutdown) {                       /* no enqueue after drain — would leak / touch freed state */
        pthread_mutex_unlock(&g_lock);
        if (f.payload) free(f.payload);
        return HOOK_ERR_BUSY;
    }
    my_seq = g_tx_seq++;
    f.seq = my_seq;

    if ((flags & BUS_FLAG_STICKY) || s->sticky) {
        frame_t cached;
        if (frame_dup(&cached, &f) != HOOK_OK) {
            /* Keep the previous valid cache intact; never publish a cache
             * entry with len > 0 and a NULL payload after allocation fails. */
            g_tx_seq--;
            pthread_mutex_unlock(&g_lock);
            frame_dispose(&f);
            return HOOK_ERR_MEMORY;
        }
        if (s->has_last) frame_dispose(&s->last);
        s->last = cached;
        s->has_last = true;
    }

    hook_result_t enq = q_enqueue_nolock(&f);
    if (enq == HOOK_OK) {
        pthread_cond_signal(&g_cond);
    } else {
        if (g_tx_seq == my_seq + 1) g_tx_seq--;
    }
    pthread_mutex_unlock(&g_lock);

    if (f.payload) free(f.payload);

    if (enq != HOOK_OK) {
        LOG_WARN(LOG_MODULE, "send queue full, dropped type=0x%04x", type);
    }
    return enq;
}

/* ============================================================
 * Sync replay - send all sticky caches between SYNC_BEGIN/END.
 *
 * Deep-copies sticky cache under the lock, then sends frames
 * without holding it - so concurrent bus_send() from iAP2 or OMX
 * callbacks is not blocked by slow network I/O here.
 * ============================================================ */
static int send_sync_snapshot(int fd, uint32_t gen, bool published) {
    frame_t  begin = { EVT_SYNC_BEGIN, 0, 0, 0, NULL };
    frame_t  end   = { EVT_SYNC_END,   0, 0, 0, NULL };
    frame_t* snapshot = NULL;
    int      snapshot_count = 0;
    int      i;

    pthread_mutex_lock(&g_lock);
    begin.seq = g_tx_seq++;

    /* Count first, then allocate & copy. */
    for (i = 0; i < MAX_TYPES; i++) if (g_types[i].has_last) snapshot_count++;
    if (snapshot_count > 0) {
        snapshot = (frame_t*)calloc((size_t)snapshot_count, sizeof(frame_t));
        if (snapshot) {
            int k = 0;
            for (i = 0; i < MAX_TYPES; i++) {
                if (!g_types[i].has_last) continue;
                if (frame_dup(&snapshot[k], &g_types[i].last) == HOOK_OK) {
                    snapshot[k].flags |= BUS_FLAG_REPLAY;
                    snapshot[k].seq = g_tx_seq++;   /* fresh seq in tx order (was the stale cached seq) */
                    k++;
                }
            }
            snapshot_count = k;
        } else {
            snapshot_count = 0;
        }
    }
    end.seq = g_tx_seq++;                            /* after the replay frames → begin < replay < end */
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(LOG_MODULE, "sending snapshot fd=%d count=%d", fd, snapshot_count);
    int rc = send_frame(fd, gen, published, &begin);
    for (i = 0; i < snapshot_count; i++) {
        if (rc == 0) rc = send_frame(fd, gen, published, &snapshot[i]);
        frame_dispose(&snapshot[i]);
    }
    if (rc == 0) rc = send_frame(fd, gen, published, &end);
    LOG_INFO(LOG_MODULE, "snapshot completed fd=%d rc=%d", fd, rc);
    free(snapshot);
    return rc;
}

/* ============================================================
 * Writer thread - drains send queue to g_client_fd.
 * ============================================================ */
static void* writer_main(void* arg) {
    (void)arg;
    LOG_INFO(LOG_MODULE, "writer thread started");

    while (!g_shutdown) {
        frame_t f;
        int fd;
        uint32_t gen;

        pthread_mutex_lock(&g_lock);
        while (!g_shutdown && (q_is_empty() || g_client_fd < 0)) {
            pthread_cond_wait(&g_cond, &g_lock);
        }
        if (g_shutdown) { pthread_mutex_unlock(&g_lock); break; }
        if (!q_dequeue_nolock(&f)) { pthread_mutex_unlock(&g_lock); continue; }
        /* Send on the ORIGINAL socket fd — do NOT dup() it.  On QNX 6.5 io-pkt, send() on a
         * dup'd socket fd returns ENOSYS ("Function not implemented"), which silently kills the
         * whole bus (RGD + coverart never reach Java).  The reference impl sends on g_client_fd
         * directly. send_frame rechecks (fd,gen) while excluding close/replacement
         * so a delayed writer cannot send into a reused descriptor. */
        fd  = g_client_fd;
        gen = g_client_gen;
        pthread_mutex_unlock(&g_lock);

        if (fd >= 0) {
            int rc = send_frame(fd, gen, true, &f);
            if (rc != 0) {
                LOG_WARN(LOG_MODULE, "send failed fd=%d type=0x%04x err=%s", fd, f.type, strerror(errno));
            }
        }

        frame_dispose(&f);
    }

    LOG_INFO(LOG_MODULE, "writer thread exiting");
    g_writer_up = false;
    return NULL;
}

/* ============================================================
 * Connector thread - connect() to Java server with retry +
 * automatic reconnect on disconnect.
 * ============================================================ */
static int try_connect_once(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

    /* 500ms send timeout so a stalled renderer (TCP buffer full, blocked
     * select() in server) cannot pin the iAP2 dispatch thread inside
     * send().  send() returns -1/EWOULDBLOCK after the timeout; the
     * writer thread drops the message and we close+reconnect. */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500 * 1000 };
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BUS_TCP_PORT);
    addr.sin_addr.s_addr = inet_addr(BUS_TCP_HOST);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Read exactly len bytes (blocking).  0 = ok, -1 = EOF/error. */
static int read_all(int fd, void* buf, size_t len) {
    uint8_t* p = (uint8_t*)buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
        if (n == 0) return -1;                 /* peer closed */
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Read one inbound frame from the peer (Java) and dispatch it to the registered
 * handler.  Blocks until a frame arrives, EOF, or error.  Makes the bus
 * BIDIRECTIONAL: the hook now both sends events AND receives commands
 * (CMD_ALT_ZOOM, CMD_ALT_ZONE, …) that modules register for via bus_on().
 * Returns 0 = ok, -1 = disconnect (framing lost / EOF → reconnect). */
static int read_frame(int fd, uint32_t gen) {
    uint8_t hdr[BUS_HEADER_SIZE];
    if (read_all(fd, hdr, sizeof(hdr)) != 0) return -1;

    if (read_be32(hdr + 0) != BUS_MAGIC) {
        LOG_WARN(LOG_MODULE, "inbound bad magic - desync, dropping link");
        return -1;                             /* can't resync a stream → reconnect */
    }
    uint16_t type  = read_be16(hdr + 8);
    uint8_t  flags = hdr[10];
    uint32_t len   = read_be32(hdr + 12);
    if (len > BUS_MAX_PAYLOAD) {
        LOG_WARN(LOG_MODULE, "inbound len=%u > max - dropping link", len);
        return -1;
    }

    uint8_t* payload = NULL;
    if (len > 0) {
        payload = (uint8_t*)malloc(len);
        if (!payload) return -1;               /* can't stay framed → reconnect */
        if (read_all(fd, payload, len) != 0) { free(payload); return -1; }
    }

    /* Listener catch-up.  Java starts before dio_manager, but on a connected-
     * phone cold boot the hook can finish its automatic sticky replay before
     * RouteGuidance has registered EVT_RGD_UPDATE.  Java then sends this after
     * listener registration.  Replay directly on the current socket; the
     * socket-write lock keeps it frame-atomic with the async writer. */
    if (type == CMD_SYNC_REQ) {
        LOG_INFO(LOG_MODULE, "sync snapshot requested fd=%d", fd);
        if (payload) free(payload);
        return send_sync_snapshot(fd, gen, true);
    }

    /* Dispatch with the handler rwlock held ACROSS the callback so bus_off()
     * can guarantee no in-flight dispatch (bus.h contract).  Handlers must be
     * quick or hand off to another thread. */
    type_slot_t* s = slot_for(type);
    if (s) {
        pthread_rwlock_rdlock(&g_htable_rw);
        if (s->handler) s->handler(type, flags, payload, len, s->handler_ctx);
        pthread_rwlock_unlock(&g_htable_rw);
    } else {
        LOG_WARN(LOG_MODULE, "inbound type=0x%04x out of range", type);
    }

    if (payload) free(payload);
    return 0;
}

static void disconnect_current_fd_if_matches(int fd, uint32_t gen, const char* reason) {
    (void)reason;
    pthread_mutex_lock(&g_sock_write);
    pthread_mutex_lock(&g_lock);
    if (g_client_fd == fd && g_client_gen == gen) {
        LOG_INFO(LOG_MODULE, "disconnecting fd=%d (%s)", fd, reason ? reason : "disconnect");
        disconnect_current_nolock(fd, gen);
    }
    pthread_mutex_unlock(&g_lock);
    pthread_mutex_unlock(&g_sock_write);
}

/* Connector only, after its last read/dispatch. A blocked recv must finish
 * before close makes the descriptor available to unrelated open/socket calls. */
static void release_connection(int fd, uint32_t gen) {
    pthread_mutex_lock(&g_sock_write);
    pthread_mutex_lock(&g_lock);
    disconnect_current_nolock(fd, gen);
    pthread_mutex_unlock(&g_lock);
    close(fd);
    pthread_mutex_unlock(&g_sock_write);
}

static void* connector_main(void* arg) {
    (void)arg;
    LOG_INFO(LOG_MODULE, "connector thread started pid=%d -> %s:%d", (int)getpid(), BUS_TCP_HOST, BUS_TCP_PORT);

    /* Outer loop: keep the link alive across Java HMI restarts. */
    while (!g_shutdown) {
        int fd = -1;
        int backoff_ms = 100;

        /* Connect attempt loop with exponential-ish backoff capped at 2 s.
         * Java should already be listening (alive from boot) but on a
         * fresh-flash boot race we may beat it by a few ms. */
        while (!g_shutdown && fd < 0) {
            fd = try_connect_once();
            if (fd < 0) {
                LOG_DEBUG(LOG_MODULE, "connect failed (%s), retrying in %d ms",
                          strerror(errno), backoff_ms);
                usleep(backoff_ms * 1000);
                if (backoff_ms < 2000) backoff_ms *= 2;
            }
        }
        if (g_shutdown) {
            if (fd >= 0) close(fd);
            break;
        }

        LOG_INFO(LOG_MODULE, "connected to Java server pid=%d fd=%d", (int)getpid(), fd);

        /* Send HELLO directly before exposing the fd to the async writer.
         * The previous path used bus_send(), which made the first bytes on
         * a fresh socket depend on the ring queue + writer thread.  These
         * logs showed the process disappearing before the reader marker, so
         * keep the handshake synchronous and diagnosable. */
        const char hello[] = "ver:n:1\nproto:s:carplay_bus\n";
        frame_t hello_frame;
        memset(&hello_frame, 0, sizeof(hello_frame));
        hello_frame.type = EVT_HELLO;
        hello_frame.flags = 0;
        hello_frame.len = sizeof(hello) - 1;
        hello_frame.payload = (uint8_t*)hello;

        pthread_mutex_lock(&g_lock);
        hello_frame.seq = g_tx_seq++;
        pthread_mutex_unlock(&g_lock);

        LOG_INFO(LOG_MODULE, "sending direct HELLO fd=%d seq=%u", fd, hello_frame.seq);
        if (send_frame(fd, 0, false, &hello_frame) != 0) {
            LOG_WARN(LOG_MODULE, "direct HELLO failed fd=%d err=%s", fd, strerror(errno));
            close(fd);
            continue;
        }
        LOG_INFO(LOG_MODULE, "direct HELLO sent fd=%d", fd);

        if (send_sync_snapshot(fd, 0, false) != 0) {
            close(fd);
            continue;
        }

        /* Replace any stale fd (shouldn't exist but defensive). */
        uint32_t gen;
        pthread_mutex_lock(&g_sock_write);
        pthread_mutex_lock(&g_lock);
        if (g_shutdown) {
            pthread_mutex_unlock(&g_lock);
            pthread_mutex_unlock(&g_sock_write);
            close(fd);
            break;
        }
        if (g_client_fd >= 0) {
            int stale_fd = g_client_fd;
            disconnect_current_nolock(stale_fd, g_client_gen);
            close(stale_fd);
        }
        g_client_fd = fd;
        gen = ++g_client_gen;
        pthread_cond_broadcast(&g_cond);
        pthread_mutex_unlock(&g_lock);
        pthread_mutex_unlock(&g_sock_write);

        while (!g_shutdown) {
            bool still_current;
            pthread_mutex_lock(&g_lock);
            still_current = (g_client_fd == fd && g_client_gen == gen);
            pthread_mutex_unlock(&g_lock);
            if (!still_current) break;

            /* Blocking read+dispatch of inbound frames (bidirectional bus).
             * Wakes on a frame, on EOF (Java gone), or on error (writer thread
             * dropped this fd) → reconnect. */
            if (read_frame(fd, gen) != 0) {
                break;
            }
        }
        release_connection(fd, gen);

        if (!g_shutdown) {
            LOG_INFO(LOG_MODULE, "disconnect detected; will reconnect");
        }
    }

    g_connector_up = false;
    LOG_INFO(LOG_MODULE, "connector thread exiting");
    return NULL;
}

/* ============================================================
 * Timer thread - runs optional local 1 Hz tick.
 *
 * Also drives `bus_set_periodic_tick()` callback at 1 Hz — lets other
 * modules (e.g., RGD debounce flush) piggyback without spawning their
 * own timer threads.
 * ============================================================ */
static volatile bus_tick_cb_t g_tick_cb = NULL;

void bus_set_periodic_tick(bus_tick_cb_t cb) {
    g_tick_cb = cb;
}

static void* timer_main(void* arg) {
    (void)arg;
    LOG_INFO(LOG_MODULE, "timer thread started (1 s interval)");
    while (!g_shutdown) {
        usleep(1000 * 1000);   /* 1 second */
        if (g_shutdown) break;

        /* Fire registered periodic tick.  Callback runs on timer
         * thread; expected to be cheap (no blocking I/O). */
        bus_tick_cb_t cb = g_tick_cb;
        if (cb) cb();

    }
    g_timer_up = false;
    LOG_INFO(LOG_MODULE, "timer thread exiting");
    return NULL;
}

/* ============================================================
 * Lifecycle
 * ============================================================ */
/* Crash diagnostic — mark which FATAL signal hit before the process dies.
 *
 * ASYNC-SIGNAL-SAFE ONLY.  The previous version called LOG_ERROR, which
 * grabs a pthread mutex + snprintf + open/write + strerror inside the
 * handler.  If the fault landed while any thread held g_log.lock (we log
 * constantly), the handler DEADLOCKED — the process hung instead of
 * dumping a core, which an external watchdog then SIGKILLs, masking the
 * very crash we were chasing.  Now: one write(2) of a fixed string, then
 * restore and re-raise into dio_manager's exact previous disposition. */
static spa_signal_guard_t g_signal_guard;

static void bus_crash_handler(int sig) {
    const char* msg;
    switch (sig) {
        case SIGSEGV: msg = "[hook] FATAL SIGSEGV in dio_manager\n"; break;
        case SIGBUS:  msg = "[hook] FATAL SIGBUS in dio_manager\n";  break;
        case SIGILL:  msg = "[hook] FATAL SIGILL in dio_manager\n";  break;
        case SIGFPE:  msg = "[hook] FATAL SIGFPE in dio_manager\n";  break;
        case SIGABRT: msg = "[hook] FATAL SIGABRT in dio_manager\n"; break;
        default:      msg = "[hook] FATAL signal in dio_manager\n";  break;
    }
    size_t n = 0;                       /* inline (strlen is not async-signal-safe) */
    while (msg[n]) n++;
    (void)write(STDERR_FILENO, msg, n);
    spa_signal_guard_chain(&g_signal_guard, sig);
}

static int bus_install_signal_policy(void)
{
    static const int fatalSignals[] = {
        SIGSEGV, SIGBUS, SIGABRT, SIGILL, SIGFPE
    };
    struct sigaction action;
    unsigned i;

    memset(&action, 0, sizeof(action));
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    if (spa_signal_guard_install(&g_signal_guard, SIGPIPE, &action) != 0)
        return -1;

    memset(&action, 0, sizeof(action));
    action.sa_handler = bus_crash_handler;
    sigemptyset(&action.sa_mask);
    for (i = 0; i < sizeof(fatalSignals) / sizeof(fatalSignals[0]); ++i) {
        if (spa_signal_guard_install(&g_signal_guard, fatalSignals[i],
                                     &action) != 0) {
            spa_signal_guard_restore(&g_signal_guard);
            return -1;
        }
    }
    return 0;
}

static void disconnect_for_shutdown(void) {
    int fd;
    uint32_t gen;
    /* Wake a blocked sender first, without freeing its descriptor. Retirement
     * waits for the frame lock; the connector closes after its last read. */
    pthread_mutex_lock(&g_lock);
    fd = g_client_fd;
    gen = g_client_gen;
    if (fd >= 0) shutdown(fd, SHUT_RDWR);
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
    if (fd >= 0) disconnect_current_fd_if_matches(fd, gen, "shutdown");
}

hook_result_t bus_init(void) {
    if (g_connector_up || g_writer_up || g_timer_up) return HOOK_ERR_BUSY;

    /* A peer reset during initial sync must not terminate dio_manager. Every
     * hook socket uses flags=0 because QNX io-pkt rejects MSG_NOSIGNAL, so the
     * reversible policy ignores SIGPIPE while the bus is active. Diagnostic
     * fault handlers chain to dio_manager's
     * exact previous disposition.  ONLY fault signals — we must NOT hijack the
     * host process's control signals (SIGTERM/INT/QUIT/HUP/USR1/USR2): those
     * belong to dio_manager's own supervisor/shutdown logic, not to an injected
     * library.  SIGKILL cannot be caught. */
    if (bus_install_signal_policy() != 0) {
        LOG_ERROR(LOG_MODULE, "failed to install reversible signal policy errno=%d", errno);
        return HOOK_ERR_INIT;
    }

    /* Do NOT memset g_types here: lazy module registration calls bus_on()
     * before bus_init runs.  BSS zero-init already guarantees clean state. */
    pthread_mutex_lock(&g_lock);
    g_ring_head = 0;
    g_ring_tail = 0;
    g_ring_count = 0;
    pthread_mutex_unlock(&g_lock);
    g_tx_seq = 1;
    g_shutdown = false;
    g_bus_owner_pid = (int)getpid();     /* fork children must NOT tear this bus down (see bus_shutdown) */

    g_connector_up = true;
    if (pthread_create(&g_connector_tid, NULL, connector_main, NULL) != 0) {
        LOG_ERROR(LOG_MODULE, "connector pthread_create failed");
        g_connector_up = false;
        spa_signal_guard_restore(&g_signal_guard);
        return HOOK_ERR_INIT;
    }
    g_connector_created = true;

    g_writer_up = true;
    if (pthread_create(&g_writer_tid, NULL, writer_main, NULL) != 0) {
        LOG_ERROR(LOG_MODULE, "writer pthread_create failed");
        g_writer_up = false;
        /* unwind the connector we already started (joinable → must join) */
        g_shutdown = true;
        disconnect_for_shutdown();
        pthread_join(g_connector_tid, NULL);
        g_connector_created = false;
        spa_signal_guard_restore(&g_signal_guard);
        return HOOK_ERR_INIT;
    }
    g_writer_created = true;

    g_timer_up = true;
    if (pthread_create(&g_timer_tid, NULL, timer_main, NULL) != 0) {
        LOG_ERROR(LOG_MODULE, "timer pthread_create failed");
        g_timer_up = false;
        /* Non-fatal: bus still works; only route debounce tick is degraded. */
    } else {
        g_timer_created = true;
    }

    LOG_INFO(LOG_MODULE, "bus initialised FORKSAFE owner-pid=%d on %s:%d", (int)getpid(), BUS_TCP_HOST, BUS_TCP_PORT);
    return HOOK_OK;
}

void bus_shutdown(void) {
    int waited_ms = 0;
    bool threads_stuck;
    /* Fork-safety: a dio_manager fork() inherits g_client_fd + the flags but not the threads.
     * Its __destructor calls us on exit — and shutdown(g_client_fd) would kill the OWNER's shared
     * socket.  Skip entirely unless we are the process that ran bus_init(). */
    if (g_bus_owner_pid != 0 && (int)getpid() != g_bus_owner_pid) {
        return;
    }

    g_shutdown = true;

    /* 1. Shutdown the socket to wake recv/send, then retire it. The connector
     *    owns close(), and the queue condition wakes an idle writer. */
    disconnect_for_shutdown();

    /* 2. QNX 6.5 has no portable timed pthread_join.  Wait for each thread's
     * terminal flag first, exactly like the stream-111 reaper.  Never enter an
     * unbounded join from an ELF destructor: a wedged socket/stock callback
     * must not keep dio_manager alive until SI's 60 s watchdog fires. */
    while (waited_ms < BUS_SHUTDOWN_WAIT_MS &&
           ((g_connector_created && g_connector_up) ||
            (g_writer_created && g_writer_up) ||
            (g_timer_created && g_timer_up))) {
        usleep(10 * 1000);
        waited_ms += 10;
    }

    if (g_connector_created && !g_connector_up) {
        pthread_join(g_connector_tid, NULL);
        g_connector_created = false;
    }
    if (g_writer_created && !g_writer_up) {
        pthread_join(g_writer_tid, NULL);
        g_writer_created = false;
    }
    if (g_timer_created && !g_timer_up) {
        pthread_join(g_timer_tid, NULL);
        g_timer_created = false;
    }

    threads_stuck = (g_connector_created || g_writer_created || g_timer_created);
    if (threads_stuck) {
        /* Process-exit path: keep all queue/cache storage intact for any thread
         * still returning from a system/stock call.  The kernel will reclaim
         * it moments later; freeing it here would trade a watchdog hang for UAF. */
        LOG_WARN(LOG_MODULE,
                 "bus shutdown deferred after %d ms connector=%d writer=%d timer=%d",
                 BUS_SHUTDOWN_WAIT_MS,
                 g_connector_created ? 1 : 0,
                 g_writer_created ? 1 : 0,
                 g_timer_created ? 1 : 0);
        return;
    }

    /* 3. Now single-threaded — safe to free the queue + sticky cache.  (bus_send
     *    also early-returns once g_shutdown is set, so nothing re-enqueues.) */
    pthread_mutex_lock(&g_lock);
    while (!q_is_empty()) {
        frame_t f;
        q_dequeue_nolock(&f);
        frame_dispose(&f);
    }
    int i;
    for (i = 0; i < MAX_TYPES; i++) {
        if (g_types[i].has_last) frame_dispose(&g_types[i].last);
        g_types[i].has_last = false;
    }
    pthread_mutex_unlock(&g_lock);

    spa_signal_guard_restore(&g_signal_guard);

    LOG_INFO(LOG_MODULE, "bus shutdown complete pid=%d owner=%d", (int)getpid(), g_bus_owner_pid);
}

/* ============================================================
 * Text builder
 * ============================================================ */
void bus_text_begin_with(bus_text_builder_t* b, const char* object_name,
                         uint8_t* buf, uint32_t cap) {
    b->buf = buf;
    b->cap = cap;
    b->len = 0;
    b->own_buf = false;
    b->overflow = false;
    if (object_name && object_name[0]) {
        bus_text_raw(b, "@");
        bus_text_raw(b, object_name);
        bus_text_raw(b, "\n");
    }
}

hook_result_t bus_text_begin_heap(bus_text_builder_t* b, const char* object_name, uint32_t cap) {
    uint8_t* buf = (uint8_t*)malloc(cap);
    if (!buf) return HOOK_ERR_MEMORY;
    bus_text_begin_with(b, object_name, buf, cap);
    b->own_buf = true;
    return HOOK_OK;
}

void bus_text_free(bus_text_builder_t* b) {
    if (b && b->own_buf && b->buf) {
        free(b->buf);
        b->buf = NULL;
        b->own_buf = false;
    }
}

static void bt_append(bus_text_builder_t* b, const char* s, size_t n) {
    if (b->overflow) return;
    if (b->len + n > b->cap) { b->overflow = true; return; }
    memcpy(b->buf + b->len, s, n);
    b->len += (uint32_t)n;
}

/* Append a value string with sanitization.  The wire format is newline-
 * separated key:type:value records, so any \n or \r in the value would
 * end the record early and forge a new key on the parser side.  iOS can
 * hand us road names / descriptions with arbitrary characters, so we
 * replace any byte < 0x20 with a space, matching the old text writer.
 * UTF-8 multibyte sequences and ordinary punctuation pass through. */
static void bt_append_sanitized(bus_text_builder_t* b, const char* s) {
    if (!s) return;
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c < 0x20) {
            bt_append(b, " ", 1);
        } else {
            char ch = (char)c;
            bt_append(b, &ch, 1);
        }
    }
}

static void bt_kv_begin(bus_text_builder_t* b, const char* key, char type_ch) {
    bt_append(b, key, strlen(key));
    char sep[3] = { ':', type_ch, ':' };
    bt_append(b, sep, 3);
}

static void bt_nl(bus_text_builder_t* b) { bt_append(b, "\n", 1); }

void bus_text_str(bus_text_builder_t* b, const char* key, const char* value) {
    if (!b || !key) return;
    if (!value) value = "";
    bt_kv_begin(b, key, 's');
    bt_append_sanitized(b, value);
    bt_nl(b);
}

void bus_text_int(bus_text_builder_t* b, const char* key, int64_t value) {
    if (!b || !key) return;
    char num[32];
    int n = snprintf(num, sizeof(num), "%lld", (long long)value);
    if (n <= 0) return;
    bt_kv_begin(b, key, 'n');
    bt_append(b, num, (size_t)n);
    bt_nl(b);
}

void bus_text_uint(bus_text_builder_t* b, const char* key, uint64_t value) {
    if (!b || !key) return;
    char num[32];
    int n = snprintf(num, sizeof(num), "%llu", (unsigned long long)value);
    if (n <= 0) return;
    bt_kv_begin(b, key, 'n');
    bt_append(b, num, (size_t)n);
    bt_nl(b);
}

void bus_text_bool(bus_text_builder_t* b, const char* key, bool value) {
    if (!b || !key) return;
    bt_kv_begin(b, key, 'b');
    bt_append(b, value ? "true" : "false", value ? 4 : 5);
    bt_nl(b);
}

void bus_text_fmt(bus_text_builder_t* b, const char* key, char type,
                  const char* fmt, ...) {
    if (!b || !key || !fmt) return;
    char scratch[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(scratch, sizeof(scratch), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof(scratch)) n = (int)sizeof(scratch) - 1;
    scratch[n] = '\0';
    bt_kv_begin(b, key, type);
    /* Same sanitization as bus_text_str - format may embed user strings. */
    bt_append_sanitized(b, scratch);
    bt_nl(b);
}

void bus_text_raw(bus_text_builder_t* b, const char* line) {
    if (!b || !line) return;
    bt_append(b, line, strlen(line));
}

hook_result_t bus_send_text(uint16_t type, uint8_t flags, bus_text_builder_t* b) {
    hook_result_t r;
    if (!b || !b->buf) return HOOK_ERR_PARAM;
    if (b->overflow) {
        LOG_WARN(LOG_MODULE, "text builder overflow (type=0x%04x, cap=%u)", type, b->cap);
        bus_text_free(b);
        return HOOK_ERR_MEMORY;
    }
    /* Strip BUS_FLAG_BINARY if set by accident. */
    flags &= (uint8_t)~BUS_FLAG_BINARY;
    r = bus_send(type, flags, b->buf, b->len);
    bus_text_free(b);
    return r;
}
