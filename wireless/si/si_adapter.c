/* si_adapter.so — LD_PRELOAD adapter inside smartphone_integrator (MHI2Q).
 *
 * Stage-1 contour proof (see docs/wireless-carplay/SI_MH2P_COMPARATIVE_RE.md,
 * section "Гибридная схема"):
 *   - no constructor; the worker starts from a service lifecycle interposer;
 *   - spawn() interposer: per-child envv injection for the CarPlay wrapper
 *     and defensive stripping of MHI2Q_SI_HOOK* from every other child's
 *     envv (QNX spawn takes an explicit envv, so nothing else leaks);
 *   - system()/open()/open64() interposers: USB-operation counters for the
 *     fixture (stopncm / media-con-ctrl / stack scripts must stay at zero
 *     during wireless attempts);
 *   - after the readiness gates the worker publishes the slot-table pointer
 *     into the graft's separate RW root (single 32-bit store after dmb) and
 *     serves a bounded AF_UNIX status/enqueue socket;
 *   - slot handlers are exception-free C, verdicts per si_protocol.h.
 *
 * Build rules mirrored from scripts/build_hook_common.sh: no __thread
 * (emutls), compiler-only .init_array, hidden visibility with an export
 * allowlist (si_adapter.map). */

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "si_envv.h"
#include "si_protocol.h"
#include "si_runtime_guard.h"
#ifdef SI_STAGE2
#include "si_controller.h"
#include "si_native.h"
static unsigned int g_tick_pending;
static int g_endpoint_lock = -1;
#endif

/* The version script keeps the allowlist; visibility("default") is required
 * to actually place the symbol in .dynsym under -fvisibility=hidden. */
#define SI_EXPORT __attribute__((visibility("default")))

#define ARENA_SZ 4096
#define MAX_INJECT 8
#define MAX_LOG_LINE 256
#define SOCK_BACKLOG 2
#define SOCK_BUF 256

typedef int (*spawn_fn)(const char *, int, const int *, const void *,
                        char *const *, char *const *);
typedef int (*system_fn)(const char *);
typedef int (*open_fn)(const char *, int, ...);

typedef void *(*job_alloc_fn)(void);
typedef int (*job_enq_fn)(void *job, int oob);

static void worker_kick(void);
static void *worker_main(void *arg);
static void status_socket_loop(void);
static int enqueue_adapter_job(uint32_t device_id, uint32_t opcode);
static int read_memory(void *ctx, uint32_t va, void *out, size_t size);

struct si_adapter_table {
    uint32_t magic;        /* 'SIAx' */
    uint32_t abi_version;
    uint32_t table_size;
#ifdef SI_STAGE2
    int32_t (*prepare)(void *, uint32_t, uint32_t);
    int32_t (*cleanup)(void *, uint32_t, uint32_t);
#else
    int32_t (*prepare)(int slot_id);
    int32_t (*cleanup)(int slot_id);
#endif
    int32_t (*job)(const void *job);
#ifdef SI_STAGE2
    int32_t (*connect)(void *, uint32_t, uint32_t);
    int32_t (*carplay_device)(void *, uint32_t, uint32_t);
#endif
};

static struct si_adapter_table g_table;
#ifdef SI_STAGE2
static int32_t cb_prepare(void *, uint32_t, uint32_t);
static int32_t cb_cleanup(void *, uint32_t, uint32_t);
#else
static int32_t cb_prepare(int slot_id);
static int32_t cb_cleanup(int slot_id);
#endif
static int32_t cb_job(const void *job);

static struct {
    int env_on;             /* MHI2Q_SI_HOOK=1 */
    unsigned long root_va;  /* cave root word VA */
    char child_path[128];   /* exact path or basename */
    char inject_str[512];   /* "K=V,K=V" for the matched child */
    char sock_path[104];    /* status socket */
    int verbose;
    int track_jobs;         /* stage-1 observer, adapter-created NOOP jobs only */
    int wireless;
} g_cfg;

static struct {
    unsigned long spawn_total;
    unsigned long spawn_injected;
    unsigned long usb_script_ops;   /* system(): stopncm/usblauncher/start_otg */
    unsigned long mediacon_opens;   /* /dev/media-con-ctrl opens */
    unsigned long pps_usb_ops;      /* PPS usb_ctrl1 opens */
    unsigned long job_noops;
    unsigned long job_deleted;
    unsigned long job_passthrough;
    unsigned long prepare_hits;
    unsigned long cleanup_hits;
    int published;
} g_stat;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_spawn_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_memfd = -1;
static int g_worker_started;
static spawn_fn real_spawn;
static system_fn real_system;
static open_fn real_open;
static open_fn real_open64;
/* Two ABI prefix words, then the five native FakeConnectJob virtual slots.
 * Used only by our freshly allocated NOOPs when explicitly enabled. */
static uint32_t g_noop_vtable[7];

static void log_line(const char *fmt, ...) {
    if (!g_cfg.verbose) return;
    char line[MAX_LOG_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[si-adapter] %s\n", line);
}

static void *dlsym_next(const char *name) {
    void *fn = dlsym(RTLD_NEXT, name);
    if (!fn) fprintf(stderr, "[si-adapter] dlsym(RTLD_NEXT, %s) failed\n", name);
    return fn;
}

static void cfg_load(void) {
    const char *v = getenv("MHI2Q_SI_HOOK");
    g_cfg.env_on = (v != NULL && strcmp(v, "1") == 0);
    if (!g_cfg.env_on) return;
    /* Being loaded into a non-SI process with these variables set is a
     * deploy mistake; the SI-only IPL_CONFIG_DIR gate plus the adapter's own
     * LD_PRELOAD reference make accidental co-loading practically impossible
     * and the cave signature check below fails safe. */
    if (!getenv("IPL_CONFIG_DIR_SMARTPHONE_INTEGRATOR")) {
        g_cfg.env_on = 0;
        return;
    }
    v = getenv("MHI2Q_SI_HOOK_ROOT");
    g_cfg.root_va = SI_HOOK_DEFAULT_ROOT_VA;
    if (v) {
        char *end;
        errno = 0;
        g_cfg.root_va = strtoul(v, &end, 0);
        if (errno || end == v || *end || g_cfg.root_va != SI_HOOK_DEFAULT_ROOT_VA) {
            g_cfg.env_on = 0;
            return;
        }
    }
    v = getenv("MHI2Q_SI_HOOK_CHILD");
    if (v && (!*v || strlen(v) >= sizeof(g_cfg.child_path))) { g_cfg.env_on = 0; return; }
    snprintf(g_cfg.child_path, sizeof(g_cfg.child_path), "%s",
             v ? v : "carplay_startup.sh");
    v = getenv("MHI2Q_SI_HOOK_CHILD_ENVS");
    if (v && strlen(v) >= sizeof(g_cfg.inject_str)) { g_cfg.env_on = 0; return; }
    snprintf(g_cfg.inject_str, sizeof(g_cfg.inject_str), "%s",
             v ? v : "");
    v = getenv("MHI2Q_SI_HOOK_STATUS");
    if (v && strlen(v) + 12 >= sizeof(g_cfg.sock_path)) { g_cfg.env_on = 0; return; }
    if (v && *v)
        snprintf(g_cfg.sock_path, sizeof(g_cfg.sock_path), "%s.%d", v, (int)getpid());
    else
#ifdef SI_STAGE2
        snprintf(g_cfg.sock_path, sizeof(g_cfg.sock_path), "/tmp/mhi2q_si_control.sock");
#else
        snprintf(g_cfg.sock_path, sizeof(g_cfg.sock_path), "/tmp/mhi2q_si_adapter.%d.sock", (int)getpid());
#endif
    v = getenv("MHI2Q_SI_HOOK_VERBOSE");
    g_cfg.verbose = (v != NULL && strcmp(v, "1") == 0);
    v = getenv("MHI2Q_SI_HOOK_TRACK_JOBS");
    g_cfg.track_jobs = (v != NULL && strcmp(v, "1") == 0);
    v = getenv("MHI2Q_SI_HOOK_WIRELESS");
    g_cfg.wireless = (v != NULL && strcmp(v, "1") == 0);
}

/* No .init_array constructors at all (build guard enforces compiler-only
 * 00000004): the first interposed call happens well after main, so lazy
 * config is safe and keeps the loader-lock landmine far away. */
static void cfg_ensure(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, cfg_load);
}

/* ------------------------------------------------------------ spawn() */
SI_EXPORT int spawn(const char *path, int fd_count, const int *fd_map,
          const void *inherit, char *const *argv, char *const *envp) {
    cfg_ensure();
    if (!real_spawn) real_spawn = (spawn_fn)dlsym_next("spawn");
    if (!real_spawn) { errno = ENOSYS; return -1; }
    if (g_cfg.env_on && envp) {
        /* два child-конфига = два ProcessTNG = конкурентные spawn; общий
         * arena/entries требуют атомарности edit+spawn */
        pthread_mutex_lock(&g_spawn_lock);
        static char arena[ARENA_SZ];
        static const char *entries[SI_ENVV_MAX_ENTRIES + 1];
        const char *inject[MAX_INJECT];
        int inject_count = 0;
        int match = si_envv_path_matches(path, g_cfg.child_path);
#ifdef SI_STAGE2
        static const char *clean_entries[SI_ENVV_MAX_ENTRIES+1];
        int clean_count=0, clean_dropped=0, clean_added=0;
        if (si_envv_apply(envp,"MHI2Q_CP_SI",0,NULL,0,arena,sizeof(arena),
                          clean_entries,&clean_count,&clean_dropped,&clean_added)) {
            pthread_mutex_unlock(&g_spawn_lock); errno=E2BIG; return -1;
        }
        envp=(char *const *)clean_entries;
        uint32_t session=0, link=0;
        int transport=match && g_stat.published ? si_native_child_transport():0;
        int admitted=transport==1 && g_cfg.wireless && si_control_child(&session,&link);
        int mode=si_envv_child_mode(transport,g_cfg.wireless,admitted);
        int wireless_child=match && mode==1;
        if (match && mode<0) {
            /* A cancelled/stale WLAN spawn must fail, never be relabelled
             * wired just because it lost admission during an earlier job. */
            pthread_mutex_unlock(&g_spawn_lock); errno=ECANCELED; return -1;
        }
        char child_mode[64], child_epoch[64], child_session[64], child_link[64], child_parent[64], child_socket[160];
        if (match) {
            snprintf(child_mode,sizeof(child_mode),"MHI2Q_DIO_OWNER_MODE=%s",wireless_child?"wireless":"wired");
            inject[inject_count++]=child_mode;
            if (wireless_child) {
                snprintf(child_epoch,sizeof(child_epoch),"MHI2Q_CP_SI_EPOCH=%llx",(unsigned long long)si_control_epoch());
                snprintf(child_session,sizeof(child_session),"MHI2Q_CP_SI_SESSION=%u",session);
                snprintf(child_link,sizeof(child_link),"MHI2Q_CP_SI_LINK=%u",link);
                snprintf(child_parent,sizeof(child_parent),"MHI2Q_CP_SI_PARENT_PID=%ld",(long)getpid());
                inject[inject_count++]=child_epoch; inject[inject_count++]=child_session; inject[inject_count++]=child_link;
                inject[inject_count++]=child_parent;
                snprintf(child_socket,sizeof(child_socket),"MHI2Q_CP_SI_STATE_SOCKET=%s",g_cfg.sock_path);
                inject[inject_count++]=child_socket;
            }
        }
#else
        char tmp[512];
        if (match && g_cfg.inject_str[0]) {
            snprintf(tmp, sizeof(tmp), "%s", g_cfg.inject_str);
            char *p = tmp;
            while (p && inject_count < MAX_INJECT) {
                char *comma = strchr(p, ',');
                if (comma) *comma = '\0';
                inject[inject_count++] = p;
                p = comma ? comma + 1 : NULL;
            }
            if (p) {
                pthread_mutex_unlock(&g_spawn_lock);
                errno = E2BIG;
                return -1;
            }
        }
#endif
        int count = 0, dropped = 0, appended = 0;
        if (si_envv_apply(envp, "MHI2Q_SI_HOOK", match, inject, match ? inject_count : 0,
                          arena, sizeof(arena), entries, &count, &dropped, &appended) != 0) {
            pthread_mutex_unlock(&g_spawn_lock);
            errno = E2BIG;
            return -1; /* Never leak hook configuration or run the wrong mode. */
        } else {
            __sync_fetch_and_add(&g_stat.spawn_total, 1UL);
            if (appended > 0) __sync_fetch_and_add(&g_stat.spawn_injected, 1UL);

            log_line("spawn %s: dropped=%d injected=%d", path ? path : "?", dropped, appended);
            int rc = real_spawn(path, fd_count, fd_map, inherit, argv,
                                (char *const *)entries);
#ifdef SI_STAGE2
            if (wireless_child && rc>0) si_control_child_started(session,(uint32_t)rc);
#endif
            pthread_mutex_unlock(&g_spawn_lock);
            return rc;
        }
    }
    if (g_cfg.env_on) {
        __sync_fetch_and_add(&g_stat.spawn_total, 1UL);
    }
    return real_spawn(path, fd_count, fd_map, inherit, argv, envp);
}

/* ----------------------------------------------------------- system() */
SI_EXPORT int system(const char *command) {
    cfg_ensure();
    if (!real_system) real_system = (system_fn)dlsym_next("system");
    if (g_cfg.env_on && command &&
        (strstr(command, "stopncm") || strstr(command, "usblauncher") ||
         strstr(command, "start_otg"))) {
        __sync_fetch_and_add(&g_stat.usb_script_ops, 1UL);
        log_line("usb script op: %s", command);
    }
    if (!real_system) { errno = ENOSYS; return -1; }
    return real_system(command);
}

/* The guest BSP may need this shim; it is never in the production adapter. */
#ifdef SI_ADAPTER_DIAGNOSTIC
SI_EXPORT int slogf(int major, int minor, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[slogf %d.%d] ", major, minor);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    return 0;
}
#endif

/* -------------------------------------------------------- open/open64 */
static int open_common(const char *path, int flags, va_list ap) {
    cfg_ensure();
    int mode = (flags & O_CREAT) ? va_arg(ap, int) : 0;
    if (!real_open64) real_open64 = (open_fn)dlsym_next("open64");
    if (!real_open64) { errno = ENOSYS; return -1; }
    int fd = real_open64(path, flags, mode);
    if (g_cfg.env_on && fd >= 0 && path) {
        if (strcmp(path, "/dev/media-con-ctrl") == 0) {
            __sync_fetch_and_add(&g_stat.mediacon_opens, 1UL);
            log_line("media-con-ctrl open, fd=%d", fd);
        } else if (strstr(path, "usb_ctrl1") != NULL) {
            /* OTG stack commands (start_stack::*) leave through PPS */
            __sync_fetch_and_add(&g_stat.pps_usb_ops, 1UL);
            log_line("pps usb_ctrl1 open, fd=%d", fd);
        }
    }
    return fd;
}

SI_EXPORT int open(const char *path, int flags, ...) {
    if (!real_open) real_open = (open_fn)dlsym_next("open");
    va_list ap;
    va_start(ap, flags);
    int r = open_common(path, flags, ap);
    va_end(ap);
    return r;
}

SI_EXPORT int open64(const char *path, int flags, ...) {
    if (!real_open64) real_open64 = (open_fn)dlsym_next("open64");
    va_list ap;
    va_start(ap, flags);
    int r = open_common(path, flags, ap);
    va_end(ap);
    return r;
}

/* ------------------------------------------------------ slot handlers */
#ifdef SI_STAGE2
static int32_t cb_prepare(void *self, uint32_t retry, uint32_t unused) {
    __sync_fetch_and_add(&g_stat.prepare_hits,1UL);
    return si_native_prepare(self,retry,unused);
}
static int32_t cb_cleanup(void *self, uint32_t error, uint32_t reset) {
    __sync_fetch_and_add(&g_stat.cleanup_hits,1UL);
    return si_native_cleanup(self,error,reset);
}
#else
static int32_t cb_prepare(int slot_id) {
    __sync_fetch_and_add(&g_stat.prepare_hits, 1UL);
    log_line("slot prepare hit (slot=%d) — stage-1 pass-through", slot_id);
    return 0; /* stage 1: original SI path */
}
static int32_t cb_cleanup(int slot_id) {
    __sync_fetch_and_add(&g_stat.cleanup_hits, 1UL);
    log_line("slot cleanup hit (slot=%d) — stage-1 pass-through", slot_id);
    return 0; /* stage 1: original SI path */
}
#endif

static int32_t cb_job(const void *job) {
    const uint32_t *fields = (const uint32_t *)job;
    uint32_t opcode = fields[8];   /* job+32: method dword */
    uint32_t device_id = fields[7]; /* job+28 */
#ifdef SI_STAGE2
    if (opcode==SI_JOB_OP_CONTROL || opcode==SI_JOB_OP_TICK) {
        struct si_ready_snapshot ready;
        if (si_guard_ready(read_memory,&g_memfd,&ready)) {
            struct si_native_context ctx={ready.controller,ready.impl};
            if (opcode==SI_JOB_OP_CONTROL) si_control_execute(device_id,&ctx);
            else si_control_tick(&ctx);
        } else if (opcode==SI_JOB_OP_CONTROL) si_control_reject(device_id);
        if (opcode==SI_JOB_OP_TICK) __sync_lock_test_and_set(&g_tick_pending,0);
        return 1;
    }
#endif
    if (opcode == SI_JOB_OP_NOOP) {
        __sync_fetch_and_add(&g_stat.job_noops, 1UL);
        log_line("adapter job consumed: NOOP device=%u", device_id);
        return 1; /* consumed: dispatcher returns 0 */
    }
    __sync_fetch_and_add(&g_stat.job_passthrough, 1UL);
    return 0;
}

static void *noop_deleting_destructor(void *job) {
    typedef void *(*destructor_fn)(void *);
    destructor_fn native = (destructor_fn)(uintptr_t)SI_VA_JOB_DELETE;
    /* Restore the native type before entering its original deleting
     * destructor. Never read the object after the native deallocator returns. */
    *(uint32_t *)job = SI_VA_JOB_VTABLE;
    void *result = native(job);
    __sync_fetch_and_add(&g_stat.job_deleted, 1UL);
    log_line("native NOOP deleting destructor returned");
    return result;
}

/* ------------------------------------------------------------- worker */
static void worker_kick(void) {
    if (!g_cfg.env_on) return;
    pthread_mutex_lock(&g_lock);
    if (g_worker_started) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    g_worker_started = 1;
    pthread_mutex_unlock(&g_lock);
    pthread_t tid;
    if (pthread_create(&tid, NULL, worker_main, NULL) != 0) {
        fprintf(stderr, "[si-adapter] worker start failed\n");
        pthread_mutex_lock(&g_lock);
        g_worker_started = 0;
        pthread_mutex_unlock(&g_lock);
        return;
    }
    pthread_detach(tid);
    log_line("worker started");
}

/* Start only from imported service-management lifecycle methods, not generic
 * open/getenv hooks which can be reached under the runtime loader lock. */
SI_EXPORT void si_report_app(void *self, unsigned mode)
    __asm__("_ZN8servmngt11Application16reportAppRunModeEj");
SI_EXPORT void si_report_system(void *self, int mode, _Bool force)
    __asm__("_ZN8servmngt11Application19reportSystemRunModeENS_13SystemRunModeEb");
SI_EXPORT void si_watchdog_period(void *self, unsigned period)
    __asm__("_ZN8servmngt11Application17setWatchdogPeriodEj");

SI_EXPORT void si_report_app(void *self, unsigned mode) {
    typedef void (*fn)(void *, unsigned);
    fn next = (fn)dlsym_next("_ZN8servmngt11Application16reportAppRunModeEj");
    if (!next) return;
    next(self, mode);
    cfg_ensure();
    worker_kick();
}
SI_EXPORT void si_report_system(void *self, int mode, _Bool force) {
    typedef void (*fn)(void *, int, _Bool);
    fn next = (fn)dlsym_next("_ZN8servmngt11Application19reportSystemRunModeENS_13SystemRunModeEb");
    if (!next) return;
    next(self, mode, force);
    cfg_ensure();
    worker_kick();
}
SI_EXPORT void si_watchdog_period(void *self, unsigned period) {
    typedef void (*fn)(void *, unsigned);
    fn next = (fn)dlsym_next("_ZN8servmngt11Application17setWatchdogPeriodEj");
    if (!next) return;
    next(self, period);
    cfg_ensure();
    worker_kick();
}

static int read_memory(void *ctx, uint32_t va, void *out, size_t size) {
    int fd = *(int *)ctx;
    unsigned char *p = out;
    size_t done = 0;
    while (done < size) {
        ssize_t n = pread(fd, p + done, size - done, (off_t)va + (off_t)done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        done += (size_t)n;
    }
    return 1;
}

static int controller_ready(void) {
    struct si_ready_snapshot snapshot;
    return g_memfd >= 0 && si_guard_ready(read_memory, &g_memfd, &snapshot);
}

static void *worker_main(void *arg) {
    char path[48];
    const char *disabled_stage="signal mask";
    sigset_t blocked;
    (void)arg;
    /* A disconnected status client must yield EPIPE, not terminate SI.
     * Block only this worker's SIGPIPE; no process signal handler changes. */
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blocked, NULL)) goto disabled;
    disabled_stage="root configuration";
    if (g_cfg.root_va != SI_HOOK_DEFAULT_ROOT_VA) goto disabled;
    if (!real_open) real_open = (open_fn)dlsym_next("open");
    if (!real_open) goto disabled;
    snprintf(path, sizeof(path), "/proc/%d/as", (int)getpid());
    disabled_stage="immutable image";
    g_memfd = real_open(path, O_RDONLY);
    if (g_memfd < 0 || fcntl(g_memfd, F_SETFD, FD_CLOEXEC) < 0 ||
        !si_guard_candidate(read_memory, &g_memfd)) goto disabled;
    /* A controller pointer alone is insufficient: require DSI startup plus
     * the registered native CarPlay listener. Timeout leaves root unpublished. */
    disabled_stage="controller readiness";
    for (int attempt = 0; !controller_ready(); ++attempt) {
        if (attempt == 300) goto disabled;
        usleep(100000);
    }
    g_table.magic = 0x78414953u;
    g_table.abi_version = SI_HOOK_ABI_VER;
    g_table.table_size = (uint32_t)sizeof(g_table);
    g_table.prepare = cb_prepare;
    g_table.cleanup = cb_cleanup;
    g_table.job = cb_job;
#ifdef SI_STAGE2
    /* One native SI control endpoint. Kernel ownership, never a PID file,
     * decides whether an old pathname may be removed after a restart. */
    disabled_stage="control endpoint lock";
    g_endpoint_lock=real_open("/mnt/persist/.mhi2q_si_adapter.lock",O_RDWR|O_CREAT,0600);
    struct stat lock_stat; struct flock endpoint;
    memset(&endpoint,0,sizeof(endpoint)); endpoint.l_type=F_WRLCK; endpoint.l_whence=SEEK_SET;
    if (g_endpoint_lock<0 || fstat(g_endpoint_lock,&lock_stat) ||
        !S_ISREG(lock_stat.st_mode) || lock_stat.st_uid!=geteuid() ||
        fchmod(g_endpoint_lock,0600) || fcntl(g_endpoint_lock,F_SETFD,FD_CLOEXEC) ||
        fcntl(g_endpoint_lock,F_SETLK,&endpoint)) goto disabled;
    g_table.connect=si_native_connect;
    g_table.carplay_device=si_native_carplay_device;
    uint64_t epoch=0;
    disabled_stage="system entropy";
    int random=real_open("/dev/urandom",O_RDONLY);
    if (random<0) goto disabled;
    ssize_t bytes=read(random,&epoch,sizeof(epoch)); close(random);
    if (bytes!=sizeof(epoch) || !epoch) goto disabled;
    si_control_init(epoch,&si_native_operations);
#endif
    if (g_cfg.track_jobs) {
        memcpy(g_noop_vtable, (const void *)(uintptr_t)(SI_VA_JOB_VTABLE - 8), sizeof(g_noop_vtable));
        g_noop_vtable[3] = (uint32_t)(uintptr_t)noop_deleting_destructor;
    }
    /* Recheck the immutable image and unclaimed root after the wait. */
    disabled_stage="final image/controller checks";
    if (!si_guard_candidate(read_memory, &g_memfd) || !controller_ready()) goto disabled;
    __asm__ volatile("dmb ishst" ::: "memory");
    *(volatile uint32_t *)g_cfg.root_va = (uint32_t)(uintptr_t)&g_table;
    __asm__ volatile("dmb ish" ::: "memory");
    __sync_lock_test_and_set(&g_stat.published, 1);
    log_line("slot table published after image and controller checks");
    status_socket_loop();
    return NULL;
disabled:
    { int failure_errno=errno;
#ifdef SI_STAGE2
    if (g_endpoint_lock>=0) close(g_endpoint_lock);
    g_endpoint_lock=-1;
#endif
    if (g_memfd >= 0) close(g_memfd);
    g_memfd = -1;
    fprintf(stderr, "[si-adapter] disabled at %s (errno=%d)\n",disabled_stage,failure_errno);
    }
    return NULL;
}

/* ------------------------------------------------------- status socket */
#ifdef SI_STAGE2
static uint64_t deadline_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) return 0;
    return (uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;
}
static void handle_wire(int fd, const char *prefix, size_t received) {
    unsigned char frame[SI_WIRE_SIZE];
    if (received>sizeof(frame)) return;
    memcpy(frame,prefix,received);
    uint64_t deadline=deadline_ms()+1500;
    while (received<sizeof(frame)) {
        if (deadline_ms()>=deadline) return;
        ssize_t n=recv(fd,frame+received,sizeof(frame)-received,0);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return;
        received+=(size_t)n;
    }
    struct si_request request; struct si_reply reply;
    memset(&reply,0,sizeof(reply)); reply.result=SI_BAD_REQUEST;
    struct si_child_state child_event;
    int child=g_cfg.wireless&&si_child_state_decode(frame,sizeof(frame),&child_event);
    if (g_cfg.wireless && (child || si_wire_decode(frame,&request))) {
        unsigned slot=0;
        if (child ? si_control_submit_child_state(&child_event,&slot,&reply) : si_control_submit(&request,&slot,&reply)) {
            if (enqueue_adapter_job(slot,SI_JOB_OP_CONTROL)) si_control_reject(slot);
            deadline=deadline_ms()+1500;
            do {
                si_control_result(slot,&reply);
                if (reply.result!=SI_PENDING) break;
                usleep(10000);
            } while (deadline_ms()<deadline);
        }
    } else if (!g_cfg.wireless) reply.result=SI_UNAVAILABLE;
    si_wire_reply(&reply,frame);
    size_t sent=0; deadline=deadline_ms()+1000;
    while (sent<sizeof(frame) && deadline_ms()<deadline) {
        ssize_t n=send(fd,frame+sent,sizeof(frame)-sent,0);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) break;
        sent+=(size_t)n;
    }
}
#endif
static void handle_client(int fd) {
    char buf[SOCK_BUF];
    ssize_t n = recv(fd, buf,
#ifdef SI_STAGE2
                     SI_WIRE_SIZE,
#else
                     sizeof(buf) - 1,
#endif
                     0);
    if (n <= 0) return;
#ifdef SI_STAGE2
    if (!memcmp(buf,"SIW2",(size_t)n<4?(size_t)n:4) ||
        !memcmp(buf,"SIA3",(size_t)n<4?(size_t)n:4)) {
        handle_wire(fd,buf,(size_t)n); return;
    }
#endif
    buf[n] = '\0';
    if (strncmp(buf, "STATUS", 6) == 0) {
        char reply[SOCK_BUF];
        snprintf(reply, sizeof(reply),
                 "published=%d spawn=%lu injected=%lu usb_scripts=%lu mediacon=%lu "
                 "pps_usb=%lu job_noop=%lu job_pass=%lu prepare=%lu cleanup=%lu job_deleted=%lu\n",
                 g_stat.published, g_stat.spawn_total, g_stat.spawn_injected,
                 g_stat.usb_script_ops, g_stat.mediacon_opens, g_stat.pps_usb_ops,
                 g_stat.job_noops, g_stat.job_passthrough,
                 g_stat.prepare_hits, g_stat.cleanup_hits, g_stat.job_deleted);
        send(fd, reply, strlen(reply), 0);
    } else if (strncmp(buf, "ENQ ", 4) == 0) {
        unsigned long devid = 0, op = 0;
        if (sscanf(buf + 4, "%lx %lx", &devid, &op) == 2) {
            int rc = (devid <= UINT32_MAX && op <= UINT32_MAX) ?
                enqueue_adapter_job((uint32_t)devid, (uint32_t)op) : -1;
            const char *reply = rc == 0 ? "ENQOK\n" : "ENQFAIL\n";
            send(fd, reply, strlen(reply), 0);
        } else {
            send(fd, "BADARGS\n", 8, 0);
        }
    } else {
        send(fd, "UNKNOWN\n", 8, 0);
    }
}

static void status_socket_loop(void) {
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) return;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", g_cfg.sock_path);
    unlink(addr.sun_path);
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        chmod(addr.sun_path,0600) || listen(listener, SOCK_BACKLOG) != 0) {
        fprintf(stderr, "[si-adapter] status socket %s failed\n", addr.sun_path);
        close(listener);
        return;
    }
    log_line("status socket %s", addr.sun_path);
    for (;;) {
#ifdef SI_STAGE2
        fd_set reads; FD_ZERO(&reads); FD_SET(listener,&reads);
        struct timeval tick={1,0};
        int ready=select(listener+1,&reads,NULL,NULL,&tick);
        if (ready==0) {
            if(g_cfg.wireless) {
                struct si_media_target target;
                if(si_control_media_target(&target) && si_media_watch(&target))
                    si_control_media_ended(&target);
            }
            if (g_cfg.wireless && !__sync_lock_test_and_set(&g_tick_pending,1) &&
                enqueue_adapter_job(0,SI_JOB_OP_TICK))
                __sync_lock_test_and_set(&g_tick_pending,0);
            continue;
        }
        if (ready<0) { if (errno==EINTR) continue; break; }
#endif
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) continue;
        struct timeval timeout = {0, 500000};
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0 &&
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0)
            handle_client(fd);
        close(fd);
    }
}

static int enqueue_adapter_job(uint32_t device_id, uint32_t opcode) {
    /* SI addresses from the manifest: fixed VAs, ET_EXEC base 0x100000.
     * Allocation and enqueue reuse SI's own job machinery, so ownership and
     * deletion stay on the SI side (see the comparative RE report, Q3). */
    job_alloc_fn alloc = (job_alloc_fn)(uintptr_t)SI_VA_JOB_ALLOC;
    job_enq_fn enq = (job_enq_fn)(uintptr_t)SI_VA_JOB_ENQUEUE;
    if ((opcode != SI_JOB_OP_NOOP
#ifdef SI_STAGE2
         && opcode != SI_JOB_OP_CONTROL && opcode != SI_JOB_OP_TICK
#endif
         ) ||
        !__sync_fetch_and_add(&g_stat.published, 0) || !controller_ready()) return -1;
    uint32_t *job = (uint32_t *)alloc();
    if (!job) return -1;
    job[2] = SI_VA_CTRL_AO_ID;  /* job+8:  controller AO id (verify live) */
    job[7] = device_id;         /* job+28 */
    job[8] = opcode;            /* job+32: adapter opcode */
    if (g_cfg.track_jobs && opcode==SI_JOB_OP_NOOP) job[0] = (uint32_t)(uintptr_t)&g_noop_vtable[2];
    /* Native mediator returns a boolean: 0x13f2c8 sets accepted=1,
     * 0x13f32c returns it. It also owns disposal on rejection. */
    return enq(job, 0) ? 0 : -1;
}
