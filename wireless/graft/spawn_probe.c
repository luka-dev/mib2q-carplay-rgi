/* QEMU-only passive trace of the factory launcher/servicemgr spawn boundary. */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t btstack_child;
static int btstack_child_reported;
static unsigned btstack_samples;
static uint32_t btstack_last_once;
static uint32_t btstack_last_status;
static uint32_t btstack_last_reply_flags;
static pid_t bluetooth_child;
static int bluetooth_child_reported;
static unsigned bluetooth_state_samples;
static off_t bluetooth_reg_trace_offset;
static off_t btstack_reg_trace_offset;
static off_t persistence_open_trace_offset;

typedef pid_t (*real_spawn_t)(const char *, int, const int *,
                              const struct inheritance *, char *const *,
                              char *const *);

pid_t spawn(const char *path, int fd_count, const int fd_map[],
            const struct inheritance *inherit, char *const argv[],
            char *const envp[])
{
    static real_spawn_t real_spawn;
    char line[560];
    int saved = errno;
    if (btstack_child > 0 && !btstack_child_reported) {
        char child_path[48];
        (void)snprintf(child_path, sizeof(child_path), "/proc/%d/as", (int)btstack_child);
        int child_fd = open(child_path, O_RDONLY);
        int child_errno = errno;
        uint32_t graft_state[2] = {0};
        unsigned char graft_code[16] = {0};
        ssize_t graft_read = child_fd >= 0 ?
            pread(child_fd, graft_state, sizeof(graft_state), (off_t)0x510000) : -1;
        ssize_t code_read = child_fd >= 0 ?
            pread(child_fd, graft_code, sizeof(graft_code), (off_t)0x508000) : -1;
        if (child_fd >= 0) (void)close(child_fd);
        int child_count = snprintf(line, sizeof(line),
                                   "[SP:%d] prior btstack child=%d as_fd=%d errno=%d graft_read=%ld once=%u status=0x%08x\n",
                                   (int)getpid(), (int)btstack_child, child_fd,
                                   child_fd < 0 ? child_errno : 0,
                                   (long)graft_read, graft_state[0], graft_state[1]);
        if (child_count > 0)
            (void)write(2, line, (size_t)child_count < sizeof(line) ? (size_t)child_count : sizeof(line));
        if (code_read == (ssize_t)sizeof(graft_code)) {
            int position = snprintf(line, sizeof(line), "[SP:%d] graft-code:", (int)getpid());
            for (unsigned index = 0; index < sizeof(graft_code) && position > 0 &&
                                    (size_t)position + 3 < sizeof(line); ++index)
                position += snprintf(line + position, sizeof(line) - (size_t)position,
                                     "%02x", graft_code[index]);
            if (position > 0 && (size_t)position + 1 < sizeof(line)) {
                line[position++] = '\n';
                (void)write(2, line, (size_t)position);
            }
        }
        int env_fd = open("/dev/shmem/btstack-wrapper-env", O_RDONLY);
        if (env_fd >= 0) {
            char environment[256] = {0};
            ssize_t env_count = read(env_fd, environment, sizeof(environment) - 1);
            (void)close(env_fd);
            if (env_count > 0) {
                child_count = snprintf(line, sizeof(line),
                                       "[SP:%d] wrapper env %s\n",
                                       (int)getpid(), environment);
                if (child_count > 0)
                    (void)write(2, line, (size_t)child_count < sizeof(line) ? (size_t)child_count : sizeof(line));
            }
        }
        btstack_child_reported = 1;
        int stage_fd = open("/dev/shmem/btstack-wrapper-stage", O_RDONLY);
        int stage_errno = errno;
        char stage = '?';
        int stage_read = stage_fd >= 0 ? (int)read(stage_fd, &stage, 1) : -1;
        if (stage_fd >= 0) (void)close(stage_fd);
        child_count = snprintf(line, sizeof(line),
                               "[SP:%d] wrapper stage=%c read=%d errno=%d\n",
                               (int)getpid(), stage, stage_read,
                               stage_fd < 0 ? stage_errno : 0);
        if (child_count > 0)
            (void)write(2, line, (size_t)child_count < sizeof(line) ? (size_t)child_count : sizeof(line));
        /* Keep the PID to correlate a later waitid() notification. */
    }
    if (btstack_child > 0) {
        char child_path[48];
        (void)snprintf(child_path, sizeof(child_path), "/proc/%d/as", (int)btstack_child);
        int child_fd = open(child_path, O_RDONLY);
        if (child_fd >= 0) {
            uint32_t once = 0;
            uint32_t status = 0;
            unsigned char granted_channel = 0;
            uint32_t reply_flags = 0;
            ssize_t size = pread(child_fd, &once, sizeof(once), (off_t)0x510000);
            ssize_t status_size = pread(child_fd, &status, sizeof(status), (off_t)0x510004);
            ssize_t channel_size = pread(child_fd, &granted_channel,
                                         sizeof(granted_channel), (off_t)0x510030);
            ssize_t reply_size = pread(child_fd, &reply_flags,
                                       sizeof(reply_flags), (off_t)0x52fffc);
            (void)close(child_fd);
            if (size == (ssize_t)sizeof(once) &&
                status_size == (ssize_t)sizeof(status) &&
                channel_size == (ssize_t)sizeof(granted_channel)) {
                ++btstack_samples;
                if (reply_size == (ssize_t)sizeof(reply_flags) &&
                    reply_flags != btstack_last_reply_flags) {
                    int count = snprintf(line, sizeof(line),
                                         "[SP:%d] graft reply flags=0x%08x\n",
                                         (int)getpid(), reply_flags);
                    if (count > 0)
                        (void)write(2, line,
                                    (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
                    btstack_last_reply_flags = reply_flags;
                }
                if (once != btstack_last_once || status != btstack_last_status) {
                    int count = snprintf(line, sizeof(line),
                                         "[SP:%d] btstack state sample=%u once=%u->%u status=0x%08x->0x%08x channel=%u\n",
                                         (int)getpid(), btstack_samples,
                                         btstack_last_once, once, btstack_last_status,
                                         status, granted_channel);
                    if (count > 0)
                        (void)write(2, line,
                                    (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
                    btstack_last_once = once;
                    btstack_last_status = status;
                }
            }
        }
    }
    if (bluetooth_child > 0 && !bluetooth_child_reported) {
        char child_path[48];
        (void)snprintf(child_path, sizeof(child_path), "/proc/%d/as", (int)bluetooth_child);
        int child_fd = open(child_path, O_RDONLY);
        int child_errno = errno;
        if (child_fd >= 0) (void)close(child_fd);
        int stage_fd = open("/dev/shmem/bluetooth-wrapper-stage", O_RDONLY);
        char stage = '?';
        int stage_read = stage_fd >= 0 ? (int)read(stage_fd, &stage, 1) : -1;
        if (stage_fd >= 0) (void)close(stage_fd);
        int count = snprintf(line, sizeof(line),
                             "[SP:%d] prior bluetooth child=%d as_fd=%d errno=%d stage=%c read=%d\n",
                             (int)getpid(), (int)bluetooth_child, child_fd,
                             child_fd < 0 ? child_errno : 0, stage, stage_read);
        if (count > 0)
            (void)write(2, line, (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
        bluetooth_child_reported = 1;
    }
    if (bluetooth_child > 0) {
        char child_path[48];
        (void)snprintf(child_path, sizeof(child_path), "/proc/%d/as", (int)bluetooth_child);
        int child_fd = open(child_path, O_RDONLY);
        if (child_fd >= 0) {
            unsigned char loaded = 0;
            uint32_t proxy = 0;
            ssize_t a = pread(child_fd, &loaded, sizeof(loaded), (off_t)0x25945c);
            ssize_t b = pread(child_fd, &proxy, sizeof(proxy), (off_t)0x259440);
            (void)close(child_fd);
            if (a == (ssize_t)sizeof(loaded) && b == (ssize_t)sizeof(proxy)) {
                ++bluetooth_state_samples;
                int count = snprintf(line, sizeof(line),
                                     "[SP:%d] bluetooth state sample=%u loaded=%u proxy=0x%08x\n",
                                     (int)getpid(), bluetooth_state_samples,
                                     loaded, proxy);
                if (count > 0)
                    (void)write(2, line,
                                (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
            }
        }
    }
    if (bluetooth_child > 0) {
        int trace_fd = open("/dev/shmem/bluetooth-reg-trace.log", O_RDONLY);
        if (trace_fd >= 0) {
            char trace[512];
            ssize_t size = pread(trace_fd, trace, sizeof(trace), bluetooth_reg_trace_offset);
            (void)close(trace_fd);
            if (size > 0) {
                (void)write(2, trace, (size_t)size);
                bluetooth_reg_trace_offset += size;
            }
        }
    }
    if (btstack_child > 0) {
        int trace_fd = open("/dev/shmem/btstack-reg-trace.log", O_RDONLY);
        if (trace_fd >= 0) {
            char trace[512];
            ssize_t size = pread(trace_fd, trace, sizeof(trace), btstack_reg_trace_offset);
            (void)close(trace_fd);
            if (size > 0) {
                (void)write(2, trace, (size_t)size);
                btstack_reg_trace_offset += size;
            }
        }
    }
    {
        int trace_fd = open("/dev/shmem/persistence-open-trace.log", O_RDONLY);
        if (trace_fd >= 0) {
            char trace[512];
            ssize_t size = pread(trace_fd, trace, sizeof(trace), persistence_open_trace_offset);
            (void)close(trace_fd);
            if (size > 0) {
                (void)write(2, trace, (size_t)size);
                persistence_open_trace_offset += size;
            }
        }
    }
    const char *preload = "(absent)";
    const char *library_path = "(absent)";
    const char *config_dir = "(absent)";
    if (envp) {
        for (char *const *entry = envp; *entry; ++entry) {
            if (strncmp(*entry, "LD_PRELOAD=", 11) == 0)
                preload = *entry + 11;
            else if (strncmp(*entry, "LD_LIBRARY_PATH=", 16) == 0)
                library_path = *entry + 16;
            else if (strncmp(*entry, "IPL_CONFIG_DIR=", 15) == 0)
                config_dir = *entry + 15;
        }
    }
    if (!real_spawn)
        real_spawn = (real_spawn_t)dlsym(RTLD_NEXT, "spawn");
    int count = snprintf(line, sizeof(line),
                         "[SP:%d] spawn path=%s argc0=%s argc1=%s argc2=%s fds=%d map=%d,%d,%d flags=0x%lx envp=%s preload=%s libs=%s config=%s\n",
                         (int)getpid(), path ? path : "(null)",
                         argv && argv[0] ? argv[0] : "(null)",
                         argv && argv[1] ? argv[1] : "(null)",
                         argv && argv[2] ? argv[2] : "(null)", fd_count,
                         fd_map && fd_count > 0 ? fd_map[0] : -1,
                         fd_map && fd_count > 1 ? fd_map[1] : -1,
                         fd_map && fd_count > 2 ? fd_map[2] : -1,
                         inherit ? inherit->flags : 0UL,
                         envp ? "provided" : "null",
                         preload, library_path, config_dir);
    if (count > 0) (void)write(2, line, (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
    errno = saved;
    pid_t result = real_spawn(path, fd_count, fd_map, inherit, argv, envp);
    int after = errno;
    if (result > 0 && path && strcmp(path, "/eso/bin/apps/btstack") == 0) {
        btstack_child = result;
        btstack_child_reported = 0;
        btstack_samples = 0;
        btstack_last_once = 0;
    }
    if (result > 0 && path && strcmp(path, "/eso/bin/apps/bluetooth") == 0) {
        bluetooth_child = result;
        bluetooth_child_reported = 0;
        bluetooth_state_samples = 0;
    }
    if (result < 0)
        count = snprintf(line, sizeof(line), "[SP:%d] spawn result=%d errno=%d path=%s\n",
                         (int)getpid(), (int)result, after, path ? path : "(null)");
    else
        count = snprintf(line, sizeof(line), "[SP:%d] spawn result=%d path=%s\n",
                         (int)getpid(), (int)result, path ? path : "(null)");
    if (count > 0) (void)write(2, line, (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
    errno = after;
    return result;
}

int waitid(idtype_t idtype, id_t id, siginfo_t *info, int options)
{
    typedef int (*real_waitid_t)(idtype_t, id_t, siginfo_t *, int);
    static real_waitid_t real_waitid;
    char line[180];
    if (!real_waitid)
        real_waitid = (real_waitid_t)dlsym(RTLD_NEXT, "waitid");
    int result = real_waitid(idtype, id, info, options);
    int after = errno;
    if (info && info->si_pid == btstack_child && btstack_child > 0) {
        int count = snprintf(line, sizeof(line),
                             "[SP:%d] waitid btstack=%d result=%d code=%d status=%d errno=%d samples=%u last_once=%u\n",
                             (int)getpid(), (int)btstack_child, result,
                             info->si_code, info->si_status, result < 0 ? after : 0,
                             btstack_samples, btstack_last_once);
        if (count > 0)
            (void)write(2, line, (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
    }
    if (info && info->si_pid == bluetooth_child && bluetooth_child > 0) {
        int count = snprintf(line, sizeof(line),
                             "[SP:%d] waitid bluetooth=%d result=%d code=%d status=%d errno=%d\n",
                             (int)getpid(), (int)bluetooth_child, result,
                             info->si_code, info->si_status, result < 0 ? after : 0);
        if (count > 0)
            (void)write(2, line, (size_t)count < sizeof(line) ? (size_t)count : sizeof(line));
    }
    errno = after;
    return result;
}
