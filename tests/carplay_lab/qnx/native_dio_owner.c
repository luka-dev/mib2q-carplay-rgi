/* Bounded owner for the isolated native-wireless image. Stock dio_manager
 * normally exits after its CarPlay session; a head unit's service manager
 * restarts it. Keep the QNX RAM pairing store across that normal lifetime.
 * Never restart a crashed/signalled or unsuccessfully exited child. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <process.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    unsigned epoch;
    signal(SIGCHLD, SIG_DFL);
    for (epoch = 1; epoch <= 4; ++epoch) {
        int status; pid_t got;
        /* The historical trace probe declares waitAudioAvailableAck as void,
         * destroying its Boolean return. Audio acceptance must exercise the
         * actual native callback, without that ABI-changing observer. */
#ifdef MHI2Q_NATIVE_AUDIO_NO_DIOPROBE
#ifdef MHI2Q_NATIVE_AUDIO_SEPARATE_PERIPHERAL
        const char *preload="/proc/boot/libdio_capture_probe.so:/proc/boot/libcarplay_hook.so:/proc/boot/libvphone_audio_peripheral.so";
#else
        const char *preload="/proc/boot/libdio_capture_probe.so:/proc/boot/libcarplay_hook.so";
#endif
#else
        const char *preload="/proc/boot/libdio_capture_probe.so:/proc/boot/libcarplay_hook.so:/proc/boot/libdioprobe.so";
#endif
        if (setenv("LD_PRELOAD", preload, 1)) return 1;
        pid_t child = spawnl(P_NOWAIT, "/proc/boot/dio_manager", "dio_manager", (char *)NULL);
        unsetenv("LD_PRELOAD");
        if (child <= 1) { perror("native-dio-owner spawn"); return 2; }
        printf("[native-dio-owner] starting epoch=%u child=%ld\n", epoch, (long)child); fflush(stdout);
        do { got = waitpid(child, &status, 0); } while (got < 0 && errno == EINTR);
        if (got != child) { perror("native-dio-owner wait"); return 3; }
        printf("[native-dio-owner] ended epoch=%u wait_status=%d\n", epoch, status); fflush(stdout);
        if (!WIFEXITED(status) || WEXITSTATUS(status)) return 4;
        sleep(1);
    }
    puts("[native-dio-owner] normal lifetime bound reached"); fflush(stdout);
    return 0;
}
