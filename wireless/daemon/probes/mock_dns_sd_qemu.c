/* QEMU-only DNS-SD service for the synthetic CarPlay Control phone. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct FakeRef *DNSServiceRef;
typedef void (*BrowseCallback)(DNSServiceRef, uint32_t, uint32_t, int,
                               const char *, const char *, const char *, void *);
typedef void (*ResolveCallback)(DNSServiceRef, uint32_t, uint32_t, int,
                                const char *, const char *, uint16_t, uint16_t,
                                const uint8_t *, void *);
typedef void (*AddressCallback)(DNSServiceRef, uint32_t, uint32_t, int,
                                const char *, const void *, uint32_t, void *);

struct FakeRef {
    int pipefd[2];
    int kind;
    void *context;
    union {
        BrowseCallback browse;
        ResolveCallback resolve;
        AddressCallback address;
    } callback;
};

static int make_ref(DNSServiceRef *out, int kind, void *callback, void *context)
{
    struct FakeRef *ref;
    if (!out || !callback) return EINVAL;
    ref = (struct FakeRef *)calloc(1, sizeof(*ref));
    if (!ref) return ENOMEM;
    if (pipe(ref->pipefd) != 0) {
        free(ref);
        return ENOMEM;
    }
    ref->kind = kind;
    ref->context = context;
    memcpy(&ref->callback, &callback, sizeof(callback));
    if (write(ref->pipefd[1], "x", 1) != 1) {
        close(ref->pipefd[0]);
        close(ref->pipefd[1]);
        free(ref);
        return EIO;
    }
    *out = ref;
    fprintf(stderr, "[dns-qemu] ref kind=%d fd=%d\n", kind, ref->pipefd[0]);
    return 0;
}

int DNSServiceBrowse(DNSServiceRef *out, uint32_t flags, uint32_t interface,
                     const char *kind, const char *domain,
                     BrowseCallback callback, void *context)
{
    (void)flags;
    if (interface == 0 || !kind || strcmp(kind, "_carplay-ctrl._tcp") != 0 ||
        !domain || strcmp(domain, "local.") != 0) return EINVAL;
    return make_ref(out, 1, (void *)callback, context);
}

int DNSServiceResolve(DNSServiceRef *out, uint32_t flags, uint32_t interface,
                      const char *name, const char *kind, const char *domain,
                      ResolveCallback callback, void *context)
{
    (void)flags;
    if (interface == 0 || !name || strcmp(name, "QEMU-iPhone") != 0 ||
        !kind || strcmp(kind, "_carplay-ctrl._tcp.") != 0 ||
        !domain || strcmp(domain, "local.") != 0) return EINVAL;
    return make_ref(out, 2, (void *)callback, context);
}

int DNSServiceGetAddrInfo(DNSServiceRef *out, uint32_t flags,
                          uint32_t interface, uint32_t protocol,
                          const char *host, AddressCallback callback,
                          void *context)
{
    (void)flags;
    if (interface == 0 || protocol != 3 || !host ||
        strcmp(host, "iphone.local.") != 0) return EINVAL;
    return make_ref(out, 3, (void *)callback, context);
}

int DNSServiceRefSockFD(DNSServiceRef ref)
{
    fprintf(stderr, "[dns-qemu] sock fd=%d\n", ref ? ref->pipefd[0] : -1);
    return ref ? ref->pipefd[0] : -1;
}

int DNSServiceProcessResult(DNSServiceRef ref)
{
    char trigger;
    if (!ref || read(ref->pipefd[0], &trigger, 1) != 1) return EINVAL;
    fprintf(stderr, "[dns-qemu] process kind=%d callback=%p context=%p\n",
            ref->kind, (void *)ref->callback.browse, ref->context);
    if (ref->kind == 1) {
        ref->callback.browse(ref, 2, 1, 0, "QEMU-iPhone",
                             "_carplay-ctrl._tcp.", "local.", ref->context);
    } else if (ref->kind == 2) {
        static const uint8_t txt[]={20,'i','d','=','0','1',':','0','2',':','0','3',':','0','4',':','0','5',':','0','6'};
        ref->callback.resolve(ref, 0, 1, 0, "QEMU-iPhone._carplay-ctrl._tcp.local.",
                              "iphone.local.", htons(7001), sizeof(txt), txt, ref->context);
    } else if (ref->kind == 3) {
        struct sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_len = sizeof(address);
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ref->callback.address(ref, 2, 1, 0, "iphone.local.", &address,
                              120, ref->context);
    } else {
        return EINVAL;
    }
    fprintf(stderr, "[dns-qemu] process kind=%d callback returned\n", ref->kind);
    return 0;
}

void DNSServiceRefDeallocate(DNSServiceRef ref)
{
    if (!ref) return;
    fprintf(stderr, "[dns-qemu] deallocate kind=%d begin\n", ref->kind);
    close(ref->pipefd[0]);
    close(ref->pipefd[1]);
    free(ref);
    fprintf(stderr, "[dns-qemu] deallocate done\n");
}
