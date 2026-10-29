/* DNS-SD ABI: apple-oss-distributions/mDNSResponder/mDNSShared/dns_sd.h.
 * Exit 0: daemon answers, 2: service absent, 1: other failure. The caller
 * bounds this child: DNSServiceGetProperty itself may block on a broken IPC. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/un.h>

int main(void)
{
    /* Factory libdns_sd retries connect() when the endpoint is absent.
     * Avoid that wait; an existing endpoint still requires a real IPC reply. */
    const char *path = getenv("DNSSD_UDS_PATH");
    struct stat st;
    if (!path || !*path || strlen(path) >= sizeof(((struct sockaddr_un *)0)->sun_path))
        path = "/var/run/mdnsd/mdnsd";
    if (stat(path, &st)) return errno == ENOENT ? 2 : 1;
    typedef int32_t (*get_property_fn)(const char *, void *, uint32_t *);
    void *library = dlopen("libdns_sd.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!library) return 1;
    get_property_fn get = (get_property_fn)dlsym(library, "DNSServiceGetProperty");
    if (!get) { dlclose(library); return 1; }
    uint32_t version = 0, size = sizeof(version);
    int32_t status = get("DaemonVersion", &version, &size);
    dlclose(library);
    if (status == -65563) return 2;
    if (status || size != sizeof(version) || !version) return 1;
    return 0;
}
