/* Disposable QEMU probe: observe factory client/server service registration.
 * This shared object is never included in the btstack graft or deployment. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef int (*register_service_fn)(void *);

#ifndef TRACE_PATH
#define TRACE_PATH "/dev/shmem/bluetooth-reg-trace.log"
#endif
#ifndef TRACE_LABEL
#define TRACE_LABEL "REGTRACE"
#endif

int trace_register_service(void *service)
    __asm__("_ZN4comm19ServiceRegistration15registerServiceEv");

int trace_register_service(void *service)
{
    static unsigned int calls;
    char line[160];
    register_service_fn original = (register_service_fn)dlsym(
        RTLD_NEXT, "_ZN4comm19ServiceRegistration15registerServiceEv");
    if (!original) {
        int count = snprintf(line, sizeof(line), TRACE_LABEL " dlsym failed: %s\n", dlerror());
        int fd = open(TRACE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644);
        if (fd >= 0) {
            if (count > 0) (void)write(fd, line,
                                       (size_t)count < sizeof(line) ? (size_t)count : sizeof(line) - 1);
            (void)close(fd);
        }
        return 1;
    }
    int result = original(service);
    int count = snprintf(line, sizeof(line), TRACE_LABEL " call=%u self=%p result=0x%08x\n",
                         ++calls, service, (unsigned int)(uintptr_t)result);
    int fd = open(TRACE_PATH, O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd >= 0) {
        if (count > 0) (void)write(fd, line,
                                   (size_t)count < sizeof(line) ? (size_t)count : sizeof(line) - 1);
        (void)close(fd);
    }
    return result;
}
