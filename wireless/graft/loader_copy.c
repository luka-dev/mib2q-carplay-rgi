/* QEMU-only: restore exact ELF bytes from an embedded blob into /dev/shmem
 * and exec there. The loader diagnostic never writes to the head unit. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef PROBE_BLUETOOTH_WRAPPER
extern const unsigned char bluetooth_blob_start[];
extern const unsigned char bluetooth_blob_end[];
#define BLOB_START bluetooth_blob_start
#define BLOB_END bluetooth_blob_end
#define OUTPUT_PATH "/dev/shmem/bluetooth-loader-probe"
#define PID_PATH "/dev/shmem/bluetooth-loader-pid"
#define STAGE_PATH "/dev/shmem/bluetooth-wrapper-stage"
#define ENV_PATH "/dev/shmem/bluetooth-wrapper-env"
#define DEFAULT_NAME "bluetooth"
#define LOG_NAME "bluetooth-copy"
#elif defined(PROBE_PERSISTENCE_WRAPPER)
extern const unsigned char persistence_blob_start[];
extern const unsigned char persistence_blob_end[];
#define BLOB_START persistence_blob_start
#define BLOB_END persistence_blob_end
#define OUTPUT_PATH "/dev/shmem/persistence-loader-probe"
#define PID_PATH "/dev/shmem/persistence-loader-pid"
#define STAGE_PATH "/dev/shmem/persistence-wrapper-stage"
#define ENV_PATH "/dev/shmem/persistence-wrapper-env"
#define DEFAULT_NAME "persistenceTNG_mibhigh"
#define LOG_NAME "persistence-copy"
#elif defined(PROBE_FEC_WRAPPER)
extern const unsigned char fec_blob_start[];
extern const unsigned char fec_blob_end[];
#define BLOB_START fec_blob_start
#define BLOB_END fec_blob_end
#define OUTPUT_PATH "/dev/shmem/fec-loader-probe"
#define PID_PATH "/dev/shmem/fec-loader-pid"
#define STAGE_PATH "/dev/shmem/fec-wrapper-stage"
#define ENV_PATH "/dev/shmem/fec-wrapper-env"
#define DEFAULT_NAME "fec"
#define LOG_NAME "fec-copy"
#elif defined(PROBE_DIAGNOSIS_WRAPPER)
extern const unsigned char diagnosis_blob_start[];
extern const unsigned char diagnosis_blob_end[];
#define BLOB_START diagnosis_blob_start
#define BLOB_END diagnosis_blob_end
#define OUTPUT_PATH "/dev/shmem/diagnosis-loader-probe"
#define PID_PATH "/dev/shmem/diagnosis-loader-pid"
#define STAGE_PATH "/dev/shmem/diagnosis-wrapper-stage"
#define ENV_PATH "/dev/shmem/diagnosis-wrapper-env"
#define DEFAULT_NAME "diagnosis"
#define LOG_NAME "diagnosis-copy"
#else
extern const unsigned char btstack_blob_start[];
extern const unsigned char btstack_blob_end[];
#define BLOB_START btstack_blob_start
#define BLOB_END btstack_blob_end
#define OUTPUT_PATH "/dev/shmem/btstack-loader-probe"
#define PID_PATH "/dev/shmem/btstack-loader-pid"
#define STAGE_PATH "/dev/shmem/btstack-wrapper-stage"
#define ENV_PATH "/dev/shmem/btstack-wrapper-env"
#define DEFAULT_NAME "btstack"
#define LOG_NAME "btstack-copy"
#endif

static void mark_stage(char stage)
{
    int fd = open(STAGE_PATH, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd >= 0) {
        (void)write(fd, &stage, 1);
        (void)close(fd);
    }
}

static void mark_environment(void)
{
    int fd = open(ENV_PATH, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd >= 0) {
        const char *preload = getenv("LD_PRELOAD");
        const char *libraries = getenv("LD_LIBRARY_PATH");
        char line[256];
        int length = snprintf(line, sizeof(line), "preload=%s libs=%s",
                              preload ? preload : "(absent)",
                              libraries ? libraries : "(absent)");
        if (length > 0)
            (void)write(fd, line, (size_t)length < sizeof(line) ? (size_t)length : sizeof(line));
        (void)close(fd);
    }
}

int main(int argc, char **incoming)
{
    mark_stage('E');
    mark_environment();
    static const char path[] = OUTPUT_PATH;
    const unsigned char *cursor = BLOB_START;
    const unsigned char *end = BLOB_END;
    /* Preserve launcher-supplied instance identity when this wrapper is
     * eventually spawned by the factory launcher. The defaults are for the
     * standalone loader diagnostic only. */
    char *argv[32] = {(char *)path};
    if (argc > 1) {
        if (argc >= (int)(sizeof(argv) / sizeof(argv[0]))) return 2;
        for (int index = 1; index < argc; ++index)
            argv[index] = incoming[index];
    } else {
        argv[1] = DEFAULT_NAME;
        argv[2] = "0";
    }
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0755);
    if (fd < 0) {
        fprintf(stderr, LOG_NAME ": open errno=%d\n", errno);
        return 1;
    }
    mark_stage('O');
    while (cursor != end) {
        ssize_t written = write(fd, cursor, (size_t)(end - cursor));
        if (written <= 0) {
            fprintf(stderr, LOG_NAME ": write errno=%d\n", errno);
            close(fd);
            return 1;
        }
        cursor += written;
    }
    mark_stage('W');
    if (fchmod(fd, 0755) != 0) {
        fprintf(stderr, LOG_NAME ": chmod errno=%d\n", errno);
        close(fd);
        return 1;
    }
    if (close(fd) != 0) {
        fprintf(stderr, LOG_NAME ": close errno=%d\n", errno);
        return 1;
    }
    fd = open(PID_PATH, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd >= 0) {
        char pid[24];
        int count = snprintf(pid, sizeof(pid), "%d\n", (int)getpid());
        if (count > 0) (void)write(fd, pid, (size_t)count);
        (void)close(fd);
    }
    fprintf(stderr, LOG_NAME ": exact bytes=%u, exec\n",
            (unsigned)(end - BLOB_START));
    mark_stage('X');
#ifdef PROBE_CHILD_LIBPATH
    (void)setenv("LD_LIBRARY_PATH", "/proc/boot:/eso/lib", 1);
#endif
#ifdef PROBE_CHILD_NO_PRELOAD
    /* The factory launcher passes envp=NULL. Without this QEMU-only scrub,
     * its service-manager tracing hooks enter btstack and fault there. */
    (void)unsetenv("LD_PRELOAD");
#endif
#ifdef PROBE_CHILD_MESH_ONLY
    (void)setenv("LD_PRELOAD", "/proc/boot/mesh_probe.so", 1);
#endif
#ifdef PROBE_CHILD_REG_TRACE
    (void)setenv("LD_PRELOAD", "/proc/boot/bluetooth_reg_trace.so", 1);
#endif
#ifdef PROBE_BTSTACK_REG_TRACE
    (void)setenv("LD_PRELOAD", "/proc/boot/btstack_reg_trace.so", 1);
#endif
#ifdef PROBE_CHILD_PERSISTENCE_OPEN_TRACE
    (void)setenv("LD_PRELOAD", "/proc/boot/persistence_open_trace.so", 1);
#endif
    execv(path, argv);
    fprintf(stderr, LOG_NAME ": exec errno=%d\n", errno);
    return 1;
}
