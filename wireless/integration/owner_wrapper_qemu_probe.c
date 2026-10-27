/* Disposable QNX probe: execute the exact generated shell wrapper into stock dio. */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/procfs.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int copy_file(const char *source, const char *name, mode_t mode)
{
    char destination[160];
    snprintf(destination, sizeof(destination), "/mnt/owner/%s", name);
    int from = open(source, O_RDONLY);
    int to = open(destination, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (from < 0 || to < 0)
        return -1;
    char buffer[4096];
    ssize_t count;
    while ((count = read(from, buffer, sizeof(buffer))) > 0) {
        ssize_t done = 0;
        while (done < count) {
            ssize_t n = write(to, buffer + done, (size_t)(count - done));
            if (n <= 0)
                return -1;
            done += n;
        }
    }
    close(from);
    close(to);
    return count < 0 ? -1 : chmod(destination, mode);
}

static int receiver_from_installed_path(pid_t pid)
{
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%ld/as", (long)pid);
    int fd = open(proc_path, O_RDONLY);
    if (fd < 0) {
        printf("[owner-wrapper-qemu] map open errno=%d\n", errno);
        return 0;
    }
    int count = 0;
    int error = devctl(fd, DCMD_PROC_MAPINFO, NULL, 0, &count);
    if (error != EOK || count < 1 || count > 4096) {
        printf("[owner-wrapper-qemu] map count error=%d count=%d\n", error, count);
        close(fd);
        return 0;
    }
    procfs_mapinfo *maps = calloc((size_t)count, sizeof(*maps));
    if (!maps) {
        close(fd);
        return 0;
    }
    int allocated = count;
    error = devctl(fd, DCMD_PROC_MAPINFO, maps,
                   (size_t)allocated * sizeof(*maps), &count);
    if (error != EOK) {
        printf("[owner-wrapper-qemu] map read error=%d\n", error);
        free(maps);
        close(fd);
        return 0;
    }
    if (count > allocated)
        count = allocated;
    int found = 0;
    for (int i = 0; i < count; ++i) {
        union {
            procfs_debuginfo info;
            char bytes[sizeof(procfs_debuginfo) + 512];
        } debug = {0};
        debug.info.vaddr = maps[i].vaddr;
        if (devctl(fd, DCMD_PROC_MAPDEBUG, &debug, sizeof(debug), NULL) != EOK)
            continue;
        if (strstr(debug.info.path, "libairplay.so")) {
            printf("[owner-wrapper-qemu] receiver mapped path=%s\n", debug.info.path);
            if (!strcmp(debug.info.path, "/mnt/app/eso/lib/libairplay.so") ||
                !strcmp(debug.info.path, "mnt/app/eso/lib/libairplay.so"))
                found = 1;
        }
    }
    free(maps);
    close(fd);
    return found;
}

static void pause_short(void)
{
    struct timespec delay = { 0, 100000000 };
    nanosleep(&delay, 0);
}

#ifdef MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU
static void log_retry_paths(const char *stage)
{
    const char *paths[] = {
        "/mnt/owner/carplay_startup.sh", "/mnt/owner/sh",
        "/proc/boot/sh", "/bin/sh", "/proc/boot/dio_manager",
        "/mnt/owner/dio_manager",
        "/mnt/owner/libcarplay_hook.so", "/mnt/owner/libdio_receiver_bridge.so",
        "/mnt/owner/carplay_monitor.sh", "/mnt/owner/carplay_processes.sh"
    };
    for (unsigned index = 0; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        struct stat info;
        int stat_result = stat(paths[index], &info);
        int stat_error = errno;
        int access_result = access(paths[index], X_OK);
        int access_error = errno;
        printf("[owner-supervisor-qemu] %s path=%s stat=%d errno=%d mode=%o access=%d errno=%d\n",
               stage, paths[index], stat_result, stat_error,
               stat_result == 0 ? (unsigned)info.st_mode : 0,
               access_result, access_error);
    }
    fflush(stdout);
}
#endif

static int bridge_ready(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return 0;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons(57343);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    int ready = connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0;
    close(fd);
    return ready;
}

#ifdef MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU
static int bridge_start_new_generation(void)
{
    unsigned char request[16] = {
        'C','P','R','X',2,1,0,0,2,0,0,0,0x58,0x1b,0,0
    };
    unsigned char response[8] = {0};
    struct sockaddr_in address = {0};
    struct timeval timeout = {8, 0};
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int result = -1;
    if (fd < 0) return -1;
    address.sin_family = AF_INET;
    address.sin_port = htons(57343);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
        size_t used = 0;
        while (used < sizeof(request)) {
            ssize_t step = write(fd, request + used, sizeof(request) - used);
            if (step <= 0) break;
            used += (size_t)step;
        }
        if (used == sizeof(request)) {
            used = 0;
            while (used < sizeof(response)) {
                ssize_t step = read(fd, response + used,
                                    sizeof(response) - used);
                if (step <= 0) break;
                used += (size_t)step;
            }
            if (used == sizeof(response) &&
                memcmp(response, "CPRA\002", 5) == 0)
                result = response[5];
        }
    }
    close(fd);
    return result;
}
#endif

static pid_t owner_pid(void)
{
    int fd = open("/mnt/owner/owner.lock", O_RDWR);
    if (fd < 0)
        return -1;
    struct flock lock = {0};
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    int status = fcntl(fd, F_GETLK, &lock);
    close(fd);
    return status == 0 && lock.l_type == F_WRLCK ? lock.l_pid : -1;
}

static pid_t generation_pid(void)
{
    FILE *file = fopen("/mnt/owner/generation", "r");
    if (!file)
        return -1;
    long pid = -1;
    if (fscanf(file, "%ld", &pid) != 1)
        pid = -1;
    fclose(file);
    return (pid_t)pid;
}

int main(void)
{
    /* Let mkifs's startup script finish before a shell waits on mv/rm. */
    for (int tick = 0; tick < 20; ++tick)
        pause_short();
    if (copy_file("/proc/boot/dio_owner_exec", "dio_owner_exec", 0755) ||
        copy_file("/proc/boot/carplay_startup_source", "carplay_startup.sh", 0755) ||
#ifdef MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU
        copy_file("/proc/boot/sh", "sh", 0755) ||
        copy_file("/proc/boot/dio_manager", "dio_manager", 0755) ||
#endif
        copy_file("/proc/boot/monitor_stub", "carplay_monitor.sh", 0755) ||
        copy_file("/proc/boot/libcarplay_hook.so", "libcarplay_hook.so", 0644) ||
        copy_file("/proc/boot/libdio_receiver_bridge.so", "libdio_receiver_bridge.so", 0644)
#if defined(MHI2Q_OWNER_SUPERVISOR_QEMU) || defined(MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU)
        || copy_file("/proc/boot/wireless_dio_supervisor", "wireless_dio_supervisor", 0755)
        || copy_file("/proc/boot/mhi2q-wireless-daemon", "mhi2q-wireless-daemon", 0755)
#endif
        ) {
        printf("[owner-wrapper-qemu] staging failed errno=%d\n", errno);
        return 1;
    }
    int dependency = open("/mnt/owner/carplay_processes.sh", O_CREAT | O_WRONLY, 0644);
    if (dependency < 0) {
        printf("[owner-wrapper-qemu] dependency create failed errno=%d\n", errno);
        return 2;
    }
    close(dependency);
    setenv("H", "/mnt/owner", 1);
#ifdef MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU
    setenv("DIODIR", "/mnt/owner", 1);
#else
    setenv("DIODIR", "/proc/boot", 1);
#endif
    setenv("WLOG", "/mnt/owner/wrapper.log", 1);
    setenv("OWNER_FILE", "/mnt/owner/generation", 1);
    setenv("MHI2Q_DIO_OWNER_LOCK", "/mnt/owner/owner.lock", 1);
    setenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK", "/mnt/owner/priority.lock", 1);
    setenv("MHI2Q_DIO_PUBLISH_OWNER", "1", 1);
#ifdef MHI2Q_OWNER_ACTIVE_WIRED_QEMU
    setenv("FAKEOMX_ASYNC_FILL", "1", 1);
#endif
#ifdef MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU
    setenv("MHI2Q_OWNER_RETRY_MS", "300", 1);
    setenv("MHI2Q_OWNER_READY_MS", "8000", 1);
    log_retry_paths("initial");
#endif
    pid_t child = fork();
    if (child == 0) {
#if defined(MHI2Q_OWNER_SUPERVISOR_QEMU) || defined(MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU)
        char *const args[] = { "/mnt/owner/wireless_dio_supervisor", 0 };
#else
        char *const args[] = { "/mnt/owner/dio_owner_exec", "wireless",
                               "/mnt/owner/carplay_startup.sh", 0 };
#endif
        execv(args[0], args);
        _exit(127);
    }
    if (child <= 0)
        return 3;
    int ready = 0;
    for (int tick = 0; tick < 300; ++tick) {
        int status;
        if (waitpid(child, &status, WNOHANG) == child) {
            printf("[owner-wrapper-qemu] owner exited early status=%d\n", status);
            return 4;
        }
        if (access("/dev/eso/dio_manager", F_OK) == 0 && bridge_ready() &&
#if defined(MHI2Q_OWNER_SUPERVISOR_QEMU) || defined(MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU)
            owner_pid() > 0 && generation_pid() == owner_pid()) {
#else
            owner_pid() == child && generation_pid() == child) {
#endif
            ready = 1;
            break;
        }
        pause_short();
    }
    if (!ready) {
        printf("[owner-wrapper-qemu] stock dio or bridge not ready; lock=%ld generation=%ld expected=%ld\n",
               (long)owner_pid(), (long)generation_pid(), (long)child);
        FILE *log = fopen("/mnt/owner/wrapper.log", "r");
        if (log) {
            char line[256];
            while (fgets(line, sizeof(line), log))
                printf("[owner-wrapper-qemu] wrapper: %s", line);
            fclose(log);
        } else
            printf("[owner-wrapper-qemu] wrapper log open errno=%d\n", errno);
        fflush(stdout);
        kill(child, SIGTERM);
        waitpid(child, 0, 0);
        return 5;
    }
    /* Both /proc/boot and /mnt/app contain libairplay.so in this IFS. */
    if (!receiver_from_installed_path(owner_pid())) {
        printf("[owner-wrapper-qemu] installed receiver path not selected\n");
        fflush(stdout);
        kill(child, SIGTERM);
        waitpid(child, 0, 0);
        return 7;
    }
    printf("[owner-wrapper-qemu] exact wrapper stock dio pid=%ld lock=1 bridge=1\n",
           (long)owner_pid());
    fflush(stdout);
#ifdef MHI2Q_OWNER_ACTIVE_SUPERVISOR_QEMU
    pid_t first_owner = owner_pid();
    int release_seen = 0;
    for (int tick = 0; tick < 500; ++tick) {
        if (access("/dev/shmem/wired_released", F_OK) == 0) {
            release_seen = 1;
            break;
        }
        pause_short();
    }
    for (int tick = 0; release_seen && tick < 250; ++tick) {
        pid_t current = owner_pid();
        if (current > 0 && current != first_owner && bridge_ready() &&
            receiver_from_installed_path(current)) {
            int start_status = bridge_start_new_generation();
            printf("[owner-supervisor-qemu] new receiver START=%d\n",
                   start_status);
            fflush(stdout);
            if (start_status != 0) {
                pause_short();
                continue;
            }
            printf("[owner-supervisor-qemu] wireless generation restarted first=%ld second=%ld\n",
                   (long)first_owner, (long)current);
            fflush(stdout);
            kill(child, SIGTERM);
            waitpid(child, 0, 0);
            return 0;
        }
        pause_short();
    }
    printf("[owner-supervisor-qemu] wireless generation did not restart\n");
    log_retry_paths("retry");
    {
        FILE *wrapper_log = fopen("/mnt/owner/wrapper.log", "r");
        if (wrapper_log) {
            char line[256];
            while (fgets(line, sizeof(line), wrapper_log))
                printf("[owner-supervisor-qemu] wrapper: %s", line);
            fclose(wrapper_log);
        }
    }
    printf("[owner-supervisor-qemu] retry paths script=%d sh=%d binsh=%d bridge=%d lock=%ld\n",
           access("/mnt/owner/carplay_startup.sh", X_OK),
           access("/proc/boot/sh", X_OK), access("/bin/sh", X_OK),
           bridge_ready(), (long)owner_pid());
    fflush(stdout);
    kill(child, SIGTERM);
    waitpid(child, 0, 0);
    return 9;
#elif defined(MHI2Q_OWNER_SUPERVISOR_QEMU)
    int phone_done = 0;
    for (int tick = 0; tick < 1100; ++tick) {
        if (access("/dev/shmem/phone_done", F_OK) == 0) {
            phone_done = 1;
            break;
        }
        pause_short();
    }
    printf("[owner-supervisor-qemu] phone done=%d\n", phone_done);
    fflush(stdout);
#endif
#ifdef MHI2Q_OWNER_ACTIVE_WIRED_QEMU
    for (int tick = 0; tick < 300; ++tick) {
        int status = 0;
        if (waitpid(child, &status, WNOHANG) == child) {
            printf("[owner-wrapper-qemu] wireless owner exited after wired handoff status=%d\n",
                   status);
            fflush(stdout);
            return 0;
        }
        pause_short();
    }
    printf("[owner-wrapper-qemu] wired handoff left wireless owner running\n");
    fflush(stdout);
    kill(child, SIGTERM);
    waitpid(child, 0, 0);
    return 8;
#else
    kill(child, SIGTERM);
    waitpid(child, 0, 0);
#ifdef MHI2Q_OWNER_SUPERVISOR_QEMU
    struct { unsigned queries, sets, clears, present; char hex[514]; } ie = {0};
    int state = open("/dev/shmem/wifi_ie_probe_state", O_RDONLY);
    if (state >= 0) {
        (void)read(state, &ie, sizeof(ie));
        close(state);
    }
    printf("[owner-supervisor-qemu] IE sets=%u clears=%u present=%u\n",
           ie.sets, ie.clears, ie.present);
    fflush(stdout);
    return phone_done && ie.sets > 0 && ie.clears > 0 && !ie.present ? 0 : 6;
#else
    return 0;
#endif
#endif
}
