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
 *     HELLO, spawns reader, then waits for reader to exit (Java disconnect
 *     or error) and loops back to reconnect.
 *   - writer thread   : drains send queue, writes frames to the current
 *     client fd with TCP_NODELAY.  On write error it drops the fd and
 *     waits for the next connect.
 *   - reader thread   : parses inbound frames and dispatches to registered
 *     handlers.  Sets g_client_fd = -1 + signals g_cond on exit so the
 *     connector wakes and retries.
 *
 * Shared state is protected by a single mutex.  The send queue is a
 * simple ring buffer of heap-allocated frames.
 */

#include "bus.h"
#include "logging.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/select.h>

DEFINE_LOG_MODULE(BUS);

/* ============================================================
 * Configuration
 * ============================================================ */
#define SEND_QUEUE_CAPACITY   256        /* must be a power of two */
#define SEND_QUEUE_MASK       (SEND_QUEUE_CAPACITY - 1)
/* Directly-indexed per-type table.  Must cover the full protocol
 * range we allocate types in (see bus_protocol.h: currently through
 * 0x02FF with headroom for 0x0300+ ranges).  Type values >=MAX_TYPES
 * are rejected by slot_for(), so any future range additions must
 * grow this constant. */
#define MAX_TYPES             0x0400

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
 * Send queue — lock-free MPSC bounded ring (Vyukov algorithm).
 *
 * Multi-producer: any thread that calls bus_send() (iAP2 callbacks,
 * OMX callbacks, RGD module, heartbeat thread, ...).  Producers
 * compete on g_ring_tail via __sync_bool_compare_and_swap.
 *
 * Single-consumer: the writer thread is the only dequeuer.  Updates
 * g_ring_head with plain non-atomic stores.
 *
 * Each slot's `seq` field synchronizes producer and consumer:
 *   seq == pos        → empty, ready to produce at pos
 *   seq == pos + 1    → filled, ready to consume
 *   (after consume)   seq advanced by capacity so producer at
 *                     pos+capacity sees seq==(pos+capacity), ready.
 *
 * Frame ownership transfers move-style:
 *   - Producer mallocs payload, fills frame_t, calls ring_try_enqueue.
 *     On HOOK_OK the slot owns f.payload; producer must NOT free.
 *     On HOOK_ERR_BUSY the producer still owns f.payload.
 *   - Consumer's ring_try_dequeue copies frame_t out by value; the
 *     payload pointer transfers to the consumer's local copy and the
 *     consumer is responsible for frame_dispose()'ing it.
 *
 * On overflow ALL policies (RELIABLE/LOSSY/DROP_OLD) currently return
 * HOOK_ERR_BUSY.  Lossy semantics are still preserved for sticky-flagged
 * types via the per-type sticky cache (updated on every bus_send), so
 * snapshot replay always sees the latest value even if intermediate
 * frames were dropped under load. */
typedef struct {
    volatile uint32_t seq;
    frame_t           frame;
} ring_slot_t;

static ring_slot_t       g_ring[SEND_QUEUE_CAPACITY];
static volatile uint32_t g_ring_tail = 0;       /* multi-producer, CAS */
static          uint32_t g_ring_head = 0;       /* single-consumer, plain */

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
/* Serialises every on-wire frame (header + payload).  Held across both
 * send() calls inside send_frame() so two producers (writer thread and
 * reader thread during sync replay) cannot interleave bytes. */
static pthread_mutex_t  g_sock_write = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       g_connector_tid;
static pthread_t       g_writer_tid;
static pthread_t       g_reader_tid;
static pthread_t       g_heartbeat_tid;
/* Thread-state flags written from one thread (the thread itself on exit)
 * and read from others without lock — must be volatile so reads aren't
 * cached in registers. */
static volatile bool   g_connector_up = false;
static volatile bool   g_writer_up = false;
static volatile bool   g_reader_up = false;
static volatile bool   g_heartbeat_up = false;
static volatile bool   g_shutdown = false;

/* g_listen_fd removed - hook is now TCP client, no listening socket. */
static int             g_client_fd = -1;    /* protected by g_lock */
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
    dst->type  = src->type;
    dst->flags = src->flags;
    dst->seq   = src->seq;
    dst->len   = src->len;
    if (src->len > 0) {
        dst->payload = (uint8_t*)malloc(src->len);
        if (!dst->payload) return HOOK_ERR_MEMORY;
        memcpy(dst->payload, src->payload, src->len);
    } else {
        dst->payload = NULL;
    }
    return HOOK_OK;
}

/* ============================================================
 * Queue ops — lock-free MPSC ring (no mutex held).
 * ============================================================ */
static void ring_init(void) {
    int i;
    for (i = 0; i < SEND_QUEUE_CAPACITY; i++) {
        g_ring[i].seq = (uint32_t)i;
        g_ring[i].frame.payload = NULL;
    }
    g_ring_tail = 0;
    g_ring_head = 0;
}

/* Try to enqueue a frame.  On HOOK_OK ownership of f->payload transfers
 * to the slot (caller must not free).  On any error the caller still
 * owns f->payload.  Lock-free: producers race via CAS on g_ring_tail. */
static hook_result_t ring_try_enqueue(const frame_t* f) {
    uint32_t pos;
    ring_slot_t *s;

    for (;;) {
        pos = g_ring_tail;                    /* volatile load */
        s   = &g_ring[pos & SEND_QUEUE_MASK];
        uint32_t seq = s->seq;                /* volatile load */
        int32_t  diff = (int32_t)(seq - pos);
        if (diff == 0) {
            if (__sync_bool_compare_and_swap(&g_ring_tail, pos, pos + 1))
                break;                        /* claimed slot at `pos` */
            /* CAS failed → another producer claimed; retry */
        } else if (diff < 0) {
            return HOOK_ERR_BUSY;             /* queue full */
        }
        /* diff > 0 → another producer claimed but hasn't published yet;
         * spin briefly. */
    }

    /* We exclusively own slot `pos & MASK`.  Write the frame, then
     * publish via seq update with a release fence. */
    s->frame = *f;                            /* memcpy struct, payload ptr moves in */
    __sync_synchronize();
    s->seq = pos + 1;                         /* publish */
    return HOOK_OK;
}

/* Try to dequeue.  Returns true if a frame was taken; *out then owns
 * its payload (caller must frame_dispose).  Single-consumer; no CAS
 * on the head side.
 *
 * Memory ordering: producer writes `s->frame`, then `__sync_synchronize`,
 * then publishes `s->seq = pos + 1` (release).  Consumer reads `s->seq`
 * first, sees publish, then **must execute an acquire barrier** before
 * reading `s->frame`.  Without this, on weakly-ordered architectures
 * (ARM/QNX) the consumer can observe the new seq value but stale frame
 * fields, leading to garbage data or use-after-free of payload pointers. */
static bool ring_try_dequeue(frame_t* out) {
    uint32_t pos = g_ring_head;
    ring_slot_t *s = &g_ring[pos & SEND_QUEUE_MASK];
    uint32_t seq = s->seq;
    int32_t  diff = (int32_t)(seq - (pos + 1));
    if (diff < 0) return false;               /* empty / not yet published */

    /* Acquire barrier: pair with producer's release fence before publish.
     * Now safe to read `s->frame` knowing all producer writes are visible. */
    __sync_synchronize();

    *out = s->frame;                          /* take ownership of payload */
    s->frame.payload = NULL;
    __sync_synchronize();                     /* release: ensure copy-out
                                                 completes before slot reuse */
    s->seq = pos + SEND_QUEUE_MASK + 1;       /* mark slot reusable */
    g_ring_head = pos + 1;
    return true;
}

/* Approximate emptiness check — used only for cond-wait predicates.
 * Non-atomic snapshot; OK because spurious wakeups self-recover. */
static bool ring_is_empty_approx(void) {
    return g_ring_tail == g_ring_head;
}

/* ============================================================
 * IO: write full frame to fd, big-endian header
 * ============================================================ */
static int write_all(int fd, const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    while (len > 0) {
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

static int read_all(int fd, void* buf, size_t len) {
    uint8_t* p = (uint8_t*)buf;
    while (len > 0) {
        ssize_t n = recv(fd, p, len, 0);
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

static int send_frame(int fd, const frame_t* f) {
    uint8_t hdr[BUS_HEADER_SIZE];
    write_be32(hdr + 0, BUS_MAGIC);
    write_be32(hdr + 4, f->seq);
    write_be16(hdr + 8, f->type);
    hdr[10] = f->flags;
    hdr[11] = 0;
    write_be32(hdr + 12, f->len);

    /* Atomic on-wire: header and payload together, no interleaving. */
    pthread_mutex_lock(&g_sock_write);
    int rc = 0;
    if (write_all(fd, hdr, sizeof(hdr)) != 0) {
        rc = -1;
    } else if (f->len > 0 && f->payload) {
        if (write_all(fd, f->payload, f->len) != 0) rc = -1;
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
 * bus_send — sticky cache update + lock-free ring enqueue.
 *
 * Sticky cache + tx_seq update happens under g_lock (rare-ish path,
 * needs frame_dup heap allocation).  Ring enqueue is lock-free:
 * producers compete only via CAS on g_ring_tail, no mutex contention
 * between producer threads on the hot path.
 * ============================================================ */
hook_result_t bus_send(uint16_t type, uint8_t flags,
                       const uint8_t* payload, uint32_t len) {
    if (len > BUS_MAX_PAYLOAD) return HOOK_ERR_PARAM;
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
    bool any_publish = false;
    uint32_t my_seq;

    pthread_mutex_lock(&g_lock);
    my_seq = g_tx_seq++;
    f.seq = my_seq;

    if ((flags & BUS_FLAG_STICKY) || s->sticky) {
        if (s->has_last) frame_dispose(&s->last);
        if (frame_dup(&s->last, &f) == HOOK_OK) {
            s->has_last = true;
            any_publish = true;
        }
    }
    pthread_mutex_unlock(&g_lock);

    /* Lock-free ring enqueue.  On success ownership of f.payload moves
     * to the slot; on failure we still own and must free. */
    hook_result_t enq = ring_try_enqueue(&f);
    if (enq == HOOK_OK) {
        any_publish = true;
        f.payload = NULL;                     /* moved into ring */

        /* Wake the writer.  Briefly take g_lock just to signal cond;
         * ring access stays lock-free.  cond_signal is safe here. */
        pthread_mutex_lock(&g_lock);
        pthread_cond_signal(&g_cond);
        pthread_mutex_unlock(&g_lock);
    }

    /* Rollback the unused seq if neither sticky cache nor ring took the
     * frame.  Done under g_lock; only safe if nothing else observed
     * my_seq (i.e., we're still the most recent allocator). */
    if (!any_publish) {
        pthread_mutex_lock(&g_lock);
        if (g_tx_seq == my_seq + 1) g_tx_seq--;
        pthread_mutex_unlock(&g_lock);
    }

    /* Free local payload if ring didn't take it.  When sticky cache
     * took a copy via frame_dup, our local payload is still ours. */
    if (f.payload) free(f.payload);

    if (enq != HOOK_OK) {
        LOG_WARN(LOG_MODULE, "send queue full, dropped type=0x%04x", type);
    }
    return enq;
}

/* ============================================================
 * Dispatch inbound - holds handler-table read lock across the callback
 * so bus_off() (writer lock) will wait until no dispatch is in flight.
 * This lets callers free their ctx immediately after bus_off returns.
 * ============================================================ */
static void dispatch_inbound(uint16_t type, uint8_t flags,
                             const uint8_t* payload, uint32_t len) {
    type_slot_t* s = slot_for(type);
    if (!s) {
        LOG_WARN(LOG_MODULE, "dispatch: type 0x%04x out of range, dropping", type);
        return;
    }
    pthread_rwlock_rdlock(&g_htable_rw);
    bus_handler_t h = s->handler;
    void* c = s->handler_ctx;
    if (h) h(type, flags, payload, len, c);
    pthread_rwlock_unlock(&g_htable_rw);
}

/* ============================================================
 * Sync replay - send all sticky caches between SYNC_BEGIN/END.
 *
 * Deep-copies sticky cache under the lock, then sends frames
 * without holding it - so concurrent bus_send() from iAP2 or OMX
 * callbacks is not blocked by slow network I/O here.
 * ============================================================ */
static void send_sync_snapshot(int fd) {
    frame_t  begin = { EVT_SYNC_BEGIN, 0, 0, 0, NULL };
    frame_t  end   = { EVT_SYNC_END,   0, 0, 0, NULL };
    frame_t* snapshot = NULL;
    int      snapshot_count = 0;
    int      i;

    pthread_mutex_lock(&g_lock);
    begin.seq = g_tx_seq++;
    end.seq   = g_tx_seq++;

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
                    k++;
                }
            }
            snapshot_count = k;
        } else {
            snapshot_count = 0;
        }
    }
    pthread_mutex_unlock(&g_lock);

    /* Send outside the lock.  Network failure here is non-fatal:
     * reader_main will close the fd and exit on the next IOException. */
    send_frame(fd, &begin);
    for (i = 0; i < snapshot_count; i++) {
        send_frame(fd, &snapshot[i]);
        frame_dispose(&snapshot[i]);
    }
    send_frame(fd, &end);
    free(snapshot);
}

/* ============================================================
 * Reader thread - parses inbound frames from g_client_fd.
 * Lives per-connection; exits when client closes.
 * ============================================================ */
static void* reader_main(void* arg) {
    int fd = (int)(intptr_t)arg;
    LOG_INFO(LOG_MODULE, "reader thread started fd=%d", fd);

    for (;;) {
        uint8_t hdr[BUS_HEADER_SIZE];
        if (read_all(fd, hdr, sizeof(hdr)) != 0) break;

        uint32_t magic = read_be32(hdr + 0);
        if (magic != BUS_MAGIC) {
            LOG_WARN(LOG_MODULE, "bad magic 0x%08x, closing", magic);
            break;
        }
        uint32_t seq = read_be32(hdr + 4);
        uint16_t type = read_be16(hdr + 8);
        uint8_t  flags = hdr[10];
        uint32_t len = read_be32(hdr + 12);

        if (len > BUS_MAX_PAYLOAD) {
            LOG_WARN(LOG_MODULE, "oversize frame type=0x%04x len=%u", type, len);
            break;
        }

        uint8_t* payload = NULL;
        if (len > 0) {
            payload = (uint8_t*)malloc(len);
            if (!payload || read_all(fd, payload, len) != 0) {
                free(payload);
                break;
            }
        }

        (void)seq;

        /* Internal commands handled here; everything else is dispatched. */
        if (type == CMD_SYNC_REQ) {
            send_sync_snapshot(fd);
        } else if (type == CMD_PING) {
            /* Echo PING payload back as PONG for round-trip test. */
            bus_send(EVT_PONG, 0, payload, len);
        } else {
            dispatch_inbound(type, flags, payload, len);
        }

        free(payload);
    }

    LOG_INFO(LOG_MODULE, "reader thread exiting fd=%d", fd);

    pthread_mutex_lock(&g_lock);
    if (g_client_fd == fd) {
        close(g_client_fd);
        g_client_fd = -1;
    }
    g_reader_up = false;
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);
    return NULL;
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

        /* Wait until something might be available.  We can't atomically
         * check "ring has data AND fd connected" without g_lock, so we
         * use cond_wait under g_lock with the approx ring-empty check.
         * Spurious wakeups are fine — we always re-check ring_try_dequeue
         * outside the lock. */
        pthread_mutex_lock(&g_lock);
        while (!g_shutdown && (ring_is_empty_approx() || g_client_fd < 0)) {
            pthread_cond_wait(&g_cond, &g_lock);
        }
        if (g_shutdown) { pthread_mutex_unlock(&g_lock); break; }
        fd = g_client_fd;
        pthread_mutex_unlock(&g_lock);

        /* Lock-free dequeue outside the mutex.  May return false if the
         * ring became empty between the empty-check above and now (e.g.,
         * spurious wakeup); that's fine — the loop just goes back to wait. */
        if (!ring_try_dequeue(&f)) continue;

        if (fd >= 0 && send_frame(fd, &f) != 0) {
            LOG_WARN(LOG_MODULE, "send failed fd=%d type=0x%04x err=%s", fd, f.type, strerror(errno));
            pthread_mutex_lock(&g_lock);
            if (g_client_fd == fd) {
                close(g_client_fd);
                g_client_fd = -1;
            }
            pthread_mutex_unlock(&g_lock);
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

static void* connector_main(void* arg) {
    (void)arg;
    LOG_INFO(LOG_MODULE, "connector thread started -> %s:%d", BUS_TCP_HOST, BUS_TCP_PORT);

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

        LOG_INFO(LOG_MODULE, "connected to Java server fd=%d", fd);

        /* Replace any stale fd (shouldn't exist but defensive). */
        pthread_mutex_lock(&g_lock);
        if (g_client_fd >= 0) {
            close(g_client_fd);
        }
        g_client_fd = fd;
        pthread_cond_broadcast(&g_cond);
        pthread_mutex_unlock(&g_lock);

        /* Send HELLO so Java side knows we're up. */
        const char hello[] = "ver:n:1\nproto:s:carplay_bus\n";
        bus_send(EVT_HELLO, 0, (const uint8_t*)hello, sizeof(hello) - 1);

        /* Spawn reader. */
        pthread_t t;
        g_reader_up = true;
        if (pthread_create(&t, NULL, reader_main, (void*)(intptr_t)fd) != 0) {
            LOG_ERROR(LOG_MODULE, "reader pthread_create failed");
            pthread_mutex_lock(&g_lock);
            close(g_client_fd);
            g_client_fd = -1;
            pthread_mutex_unlock(&g_lock);
            g_reader_up = false;
            continue;
        }
        g_reader_tid = t;
        pthread_detach(t);

        /* Wait for reader to exit (Java disconnected or read error).
         * Reader sets g_reader_up=false + broadcasts g_cond on exit. */
        pthread_mutex_lock(&g_lock);
        while (g_reader_up && !g_shutdown) {
            pthread_cond_wait(&g_cond, &g_lock);
        }
        pthread_mutex_unlock(&g_lock);

        if (!g_shutdown) {
            LOG_INFO(LOG_MODULE, "reader exited; will reconnect");
        }
    }

    g_connector_up = false;
    LOG_INFO(LOG_MODULE, "connector thread exiting");
    return NULL;
}

/* ============================================================
 * Heartbeat thread - sends EVT_PONG every 1 s while connected.
 *
 * Java side has SO_TIMEOUT = 5 s on the accepted socket; if no
 * inbound for 5 s the Java reader exits and the bus reconnects.
 * That's how Java detects force-killed dio_manager whose TCP
 * state may not get cleaned up promptly by the kernel.
 *
 * Also drives `bus_set_periodic_tick()` callback at 1 Hz — lets other
 * modules (e.g., RGD debounce flush) piggyback without spawning their
 * own timer threads.
 * ============================================================ */
static volatile bus_tick_cb_t g_tick_cb = NULL;

void bus_set_periodic_tick(bus_tick_cb_t cb) {
    g_tick_cb = cb;
}

static void* heartbeat_main(void* arg) {
    (void)arg;
    LOG_INFO(LOG_MODULE, "heartbeat thread started (1 s interval)");
    while (!g_shutdown) {
        usleep(1000 * 1000);   /* 1 second */
        if (g_shutdown) break;

        /* Fire registered periodic tick.  Callback runs on heartbeat
         * thread; expected to be cheap (no blocking I/O). */
        bus_tick_cb_t cb = g_tick_cb;
        if (cb) cb();

        /* Only send when actually connected — otherwise queue grows
         * unbounded while waiting for first connect. */
        bool connected;
        pthread_mutex_lock(&g_lock);
        connected = (g_client_fd >= 0);
        pthread_mutex_unlock(&g_lock);
        if (connected) {
            bus_send(EVT_PONG, 0, NULL, 0);   /* empty payload */
        }
    }
    g_heartbeat_up = false;
    LOG_INFO(LOG_MODULE, "heartbeat thread exiting");
    return NULL;
}

/* ============================================================
 * Lifecycle
 * ============================================================ */
hook_result_t bus_init(void) {
    if (g_connector_up || g_writer_up) return HOOK_ERR_BUSY;

    /* Do NOT memset g_types here: modules may have called bus_on()
     * from their own __constructor before bus_init ran (constructor
     * order is unspecified across object files).  BSS zero-init
     * already guarantees clean initial state. */
    ring_init();
    g_tx_seq = 1;
    g_shutdown = false;

    g_connector_up = true;
    if (pthread_create(&g_connector_tid, NULL, connector_main, NULL) != 0) {
        LOG_ERROR(LOG_MODULE, "connector pthread_create failed");
        g_connector_up = false;
        return HOOK_ERR_INIT;
    }
    pthread_detach(g_connector_tid);

    g_writer_up = true;
    if (pthread_create(&g_writer_tid, NULL, writer_main, NULL) != 0) {
        LOG_ERROR(LOG_MODULE, "writer pthread_create failed");
        g_writer_up = false;
        g_shutdown = true;
        return HOOK_ERR_INIT;
    }
    pthread_detach(g_writer_tid);

    g_heartbeat_up = true;
    if (pthread_create(&g_heartbeat_tid, NULL, heartbeat_main, NULL) != 0) {
        LOG_ERROR(LOG_MODULE, "heartbeat pthread_create failed");
        g_heartbeat_up = false;
        /* Non-fatal: bus still works without heartbeat (Java falls back to
         * natural-traffic timeout, but force-kill detection is degraded). */
    } else {
        pthread_detach(g_heartbeat_tid);
    }

    LOG_INFO(LOG_MODULE, "bus initialised on %s:%d", BUS_TCP_HOST, BUS_TCP_PORT);
    return HOOK_OK;
}

void bus_shutdown(void) {
    g_shutdown = true;

    pthread_mutex_lock(&g_lock);
    if (g_client_fd >= 0) {
        shutdown(g_client_fd, SHUT_RDWR);
        close(g_client_fd);
        g_client_fd = -1;
    }
    pthread_cond_broadcast(&g_cond);
    pthread_mutex_unlock(&g_lock);

    /* Drain queue (lock-free dequeue; we're past the point where
     * other producers are still firing). */
    {
        frame_t f;
        while (ring_try_dequeue(&f)) {
            frame_dispose(&f);
        }
    }

    pthread_mutex_lock(&g_lock);
    int i;
    for (i = 0; i < MAX_TYPES; i++) {
        if (g_types[i].has_last) frame_dispose(&g_types[i].last);
        g_types[i].has_last = false;
    }
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(LOG_MODULE, "bus shutdown complete");
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
 * replace any byte < 0x20 with a space, matching the old PPS writer.
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
