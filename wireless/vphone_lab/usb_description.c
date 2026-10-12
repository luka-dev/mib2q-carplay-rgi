/* Header-free iOS ABI: inspect/configure only the owned research VM's USB. */
typedef unsigned int u32;
typedef unsigned long size_t;
typedef long CFIndex;
typedef const void *CFRef;
typedef void *CFMutableRef;
typedef struct { CFIndex location, length; } CFRange;
extern int printf(const char *, ...), strcmp(const char *, const char *);
extern unsigned int getuid(void), alarm(unsigned int);
extern unsigned int sleep(unsigned int);
extern int fflush(void *);
extern void *dlopen(const char *, int), *dlsym(void *, const char *);
extern const char *dlerror(void);
extern void *objc_getClass(const char *), *sel_registerName(const char *);
extern void *objc_msgSend(void *, void *, ...);
extern void *xpc_connection_create_mach_service(const char *, void *, unsigned long long);
extern void xpc_connection_set_event_handler(void *, void (^)(void *));
extern void xpc_connection_activate(void *);
extern void *xpc_dictionary_create(const char **, const void **, size_t);
extern void xpc_connection_send_message(void *, void *);
extern void xpc_connection_cancel(void *), xpc_release(void *);
typedef struct __sFILE FILE;
extern FILE *fopen(const char *, const char *);
extern size_t fread(void *, size_t, size_t, FILE *);
extern int fclose(FILE *);
extern const unsigned char kCFTypeDictionaryKeyCallBacks[], kCFTypeDictionaryValueCallBacks[], kCFTypeArrayCallBacks[];
extern CFRef kCFBooleanTrue;
extern CFRef CFStringCreateWithCString(void *, const char *, u32);
extern CFMutableRef CFDictionaryCreateMutable(void *, CFIndex, const void *, const void *);
extern CFRef CFDictionaryGetValue(CFRef, CFRef), CFArrayCreate(void *, const CFRef *, CFIndex, const void *);
extern void CFDictionarySetValue(CFMutableRef, CFRef, CFRef), CFRelease(CFRef);
extern unsigned char CFStringGetCString(CFRef, char *, CFIndex, u32);
extern CFRef CFPropertyListCreateData(void *, CFRef, long, unsigned long, CFRef *);
extern CFRef CFDataCreate(void *, const unsigned char *, CFIndex);
extern CFRef CFPropertyListCreateWithData(void *, CFRef, unsigned long, long *, CFRef *);
extern unsigned long CFGetTypeID(CFRef), CFDictionaryGetTypeID(void), CFDataGetTypeID(void), CFStringGetTypeID(void);
extern const unsigned char *CFDataGetBytePtr(CFRef);
extern CFIndex CFDataGetLength(CFRef);
extern u32 IOServiceGetMatchingService(u32, void *), IORegistryEntryFromPath(u32, const char *);
extern void *IOServiceMatching(const char *);
extern CFRef IORegistryEntryCreateCFProperty(u32, CFRef, void *, u32);
extern int IOObjectRelease(u32);
extern int IORegistryEntrySetCFProperties(u32, CFRef);
extern int socket(int, int, int), connect(int, const void *, u32), close(int);
extern long send(int, const void *, size_t, int), recv(int, void *, size_t, int);
extern int setsockopt(int, int, int, const void *, u32), inet_pton(int, const char *, void *);
extern u32 if_nametoindex(const char *);
extern unsigned short htons(unsigned short);
extern int memcmp(const void *, const void *, size_t);

static int network_probe(void) {
    struct { unsigned char len, family; unsigned short port; u32 flow; unsigned char ip[16]; u32 scope; } address = {0};
    address.len = sizeof(address); address.family = 30; address.port = htons(5000);
    address.scope = if_nametoindex("en2");
    if (!address.scope || inet_pton(30, "fe80::4254:ff:fe12:3456", address.ip) != 1) return 20;
    int fd = socket(30, 1, 0);
    if (fd < 0) return 21;
    struct { long seconds; int microseconds; } timeout = {5, 0};
    setsockopt(fd, 0xffff, 0x1005, &timeout, sizeof(timeout));
    setsockopt(fd, 0xffff, 0x1006, &timeout, sizeof(timeout));
    int status = connect(fd, &address, sizeof(address));
    printf("QNX link-local connect=%d interface=en2 scope=%u\n", status, address.scope); fflush(0);
    if (status) { close(fd); return 22; }
    static const char request[] = "GET /info HTTP/1.1\r\nHost: mhi2-qnx-lab\r\nConnection: close\r\n\r\n";
    if (send(fd, request, sizeof(request) - 1, 0) != sizeof(request) - 1) { close(fd); return 23; }
    char response[2048]; long count = recv(fd, response, sizeof(response), 0); close(fd);
    printf("native /info received=%ld HTTP200=%d\n", count, count >= 12 && !memcmp(response, "HTTP/1.1 200", 12));
    return count >= 12 && !memcmp(response, "HTTP/1.1 200", 12) ? 0 : 24;
}

static CFRef str(const char *s) { return CFStringCreateWithCString(0, s, 0x08000100); }
static void put(CFMutableRef d, const char *k, CFRef v) {
    CFRef key = str(k); CFDictionarySetValue(d, key, v); CFRelease(key);
}
static int matches(CFRef d, const char *key, const char *expected) {
    char text[128] = {0}; CFRef k = str(key), v = CFDictionaryGetValue(d, k); CFRelease(k);
    return v && CFGetTypeID(v) == CFStringGetTypeID() && CFStringGetCString(v, text, sizeof(text), 0x08000100) && !strcmp(text, expected);
}
static int dump(CFRef d) {
    CFRef data = CFPropertyListCreateData(0, d, 100, 0, 0);
    if (!data || CFDataGetLength(data) > 6000) return 1;
    printf("%.*s\n", (int)CFDataGetLength(data), CFDataGetBytePtr(data)); CFRelease(data); return 0;
}

int main(int argc, char **argv) {
    alarm(20);
    if (argc != 3 || getuid() != 0 || (strcmp(argv[1], "probe") && strcmp(argv[1], "apply") && strcmp(argv[1], "restore") && strcmp(argv[1], "reconnect") && strcmp(argv[1], "network") && strcmp(argv[1], "apply-destroy") && strcmp(argv[1], "iap-list") && strcmp(argv[1], "iap-wake"))) return 2;
    /* Reject physical controllers before loading any mutating SPI. */
    u32 service = IOServiceGetMatchingService(0, IOServiceMatching("AppleVirtIOUSBDeviceController"));
    if (!service) return 3;
    CFRef k = str("DeviceDescription");
    CFRef original = IORegistryEntryCreateCFProperty(service, k, 0, 0); CFRelease(k);
    if (!original || CFGetTypeID(original) != CFDictionaryGetTypeID() || !matches(original, "uniqueDeviceID", argv[2]) ||
            !matches(original, "productString", "iPhone Research Environment Virtual Machine")) return 4;
    /* FE01 comes from the research chip, not an iPhone product identifier. */
    u32 chosen = IORegistryEntryFromPath(0, "IODeviceTree:/chosen");
    k = str("chip-id"); CFRef chip = IORegistryEntryCreateCFProperty(chosen, k, 0, 0); CFRelease(k);
    if (!chip || CFGetTypeID(chip) != CFDataGetTypeID() || CFDataGetLength(chip) != 4 || *(const u32 *)CFDataGetBytePtr(chip) != 0xFE01) return 5;
    CFRelease(chip); IOObjectRelease(chosen);
    printf("owned research USB description verified\n"); fflush(0);
    if (!strcmp(argv[1], "probe")) return dump(original);
    if (!strcmp(argv[1], "network")) return network_probe();
    if (!strcmp(argv[1], "iap-wake")) {
        void *connection = xpc_connection_create_mach_service("com.apple.iap2d.xpc", 0, 2);
        if (!connection) return 27;
        xpc_connection_set_event_handler(connection, ^(void *event) { (void)event; printf("native accessory XPC event\n"); fflush(0); });
        xpc_connection_activate(connection);
        void *message = xpc_dictionary_create(0, 0, 0);
        xpc_connection_send_message(connection, message); xpc_release(message);
        sleep(10); xpc_connection_cancel(connection); xpc_release(connection); return 0;
    }
    if (!strcmp(argv[1], "iap-list")) {
        if (!dlopen("/System/Library/Frameworks/ExternalAccessory.framework/ExternalAccessory", 2)) return 25;
        void *type = objc_getClass("EAAccessoryManager");
        if (!type) return 26;
        void *manager = objc_msgSend(type, sel_registerName("sharedAccessoryManager"));
        void *accessories = objc_msgSend(manager, sel_registerName("connectedAccessories"));
        printf("native ExternalAccessory count=%lu\n", (unsigned long)objc_msgSend(accessories, sel_registerName("count")));
        fflush(0); sleep(10); return 0;
    }

    CFRef backup = 0;
    if (!strcmp(argv[1], "restore")) {
        unsigned char bytes[8192];
        FILE *file = fopen("/private/var/tmp/mhi2-vphone-lab/usb-original.plist", "rb");
        if (!file) return 12;
        size_t count = fread(bytes, 1, sizeof(bytes), file); fclose(file);
        if (!count || count == sizeof(bytes)) return 13;
        CFRef data = CFDataCreate(0, bytes, (CFIndex)count);
        backup = CFPropertyListCreateWithData(0, data, 0, 0, 0); CFRelease(data);
        if (!backup || CFGetTypeID(backup) != CFDictionaryGetTypeID() ||
                !matches(backup, "uniqueDeviceID", argv[2]) ||
                !matches(backup, "productString", "iPhone Research Environment Virtual Machine")) return 14;
    }

    void *io = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", 2);
    if (!io) { printf("IOKit load: %s\n", dlerror()); return 6; }
    int (*create)(void *, CFMutableRef *) = dlsym(io, "IOUSBDeviceControllerCreate");
    CFMutableRef (*from)(void *, CFMutableRef) = dlsym(io, "IOUSBDeviceDescriptionCreateFromController");
    CFMutableRef (*info)(CFMutableRef) = dlsym(io, "_IOUSBDeviceDescriptionGetInfo");
    int (*set)(CFMutableRef, CFRef) = dlsym(io, "IOUSBDeviceControllerSetDescription");
    int (*cycle)(CFMutableRef, u32) = dlsym(io, "IOUSBDeviceControllerGoOffAndOnBus");
    if (!create || !from || !info || !set || !cycle) return 7;
    CFMutableRef controller = 0; int status = create(0, &controller);
    printf("controller create=0x%x\n", status); fflush(0);
    if (status || !controller) return 8;
    CFMutableRef description = from(0, controller), saved = from(0, controller);
    if (!description || !saved) return 9;
    CFMutableRef dict = info(description);
    if (!dict || !matches(dict, "uniqueDeviceID", argv[2])) return 10;
    put(dict, "AllowMultipleCreates", kCFBooleanTrue);
    CFMutableRef config = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
    CFRef name = str("MHI2 vphone USBMUX + iAP2 + NCM"); put(config, "Description", name); CFRelease(name);
    put(config, "DefaultConfiguration", kCFBooleanTrue);
    /* Preserve both VZ NCM channels and their original interface numbers. */
    CFRef interfaces[] = {str("AppleUSBMux"), str("AppleUSBNCMControl"), str("AppleUSBNCMData"),
        str("AppleUSBNCMControlAux"), str("AppleUSBNCMDataAux"), str("IapOverUsbHid")};
    CFRef array = CFArrayCreate(0, interfaces, 6, kCFTypeArrayCallBacks); put(config, "Interfaces", array); CFRelease(array);
    for (unsigned i = 0; i < 6; i++) CFRelease(interfaces[i]);
    CFRef configs[] = {config}; array = CFArrayCreate(0, configs, 1, kCFTypeArrayCallBacks);
    if (backup) {
        CFRef key = str("ConfigurationDescriptors");
        CFRef restored = CFDictionaryGetValue(backup, key); CFRelease(key);
        if (!restored) return 15;
        put(dict, "ConfigurationDescriptors", restored); CFRelease(backup);
    } else if (!strcmp(argv[1], "apply") || !strcmp(argv[1], "apply-destroy")) put(dict, "ConfigurationDescriptors", array);
    CFRelease(array); CFRelease(config);
    if (!strcmp(argv[1], "apply-destroy")) {
        CFMutableRef command = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
        CFRef name = str("DeviceDestroy"); put(command, "USBDeviceCommand", name); CFRelease(name);
        status = IORegistryEntrySetCFProperties(service, command); CFRelease(command);
        printf("device destroy=0x%x\n", status); fflush(0);
        if (status) {
            /* Even a rejected command can partly tear down the device. */
            put(info(saved), "AllowMultipleCreates", kCFBooleanTrue);
            int restored = set(controller, saved);
            printf("destroy failure rollback description=0x%x\n", restored); fflush(0);
            if (!restored) printf("destroy failure rollback bus=0x%x\n", cycle(controller, 1000));
            return 16;
        }
        /* Let terminated function drivers release their old queues and ports. */
        sleep(2);
    }
    status = set(controller, description); printf("set description=0x%x\n", status); fflush(0);
    /* Replacing interface drivers already rebuilds the bus. Keep a second
       cycle explicit; removing it did not cure the observed AVP failure. */
    if (!status && !strcmp(argv[1], "reconnect")) {
        status = cycle(controller, 1000); printf("bus cycle=0x%x\n", status);
    }
    /* A failed replacement must not leave the known-good USB description gone. */
    if (status) {
        put(info(saved), "AllowMultipleCreates", kCFBooleanTrue);
        int restored = set(controller, saved); printf("rollback description=0x%x\n", restored);
        if (!restored) printf("rollback bus=0x%x\n", cycle(controller, 1000));
    }
    CFRelease(saved); CFRelease(description); CFRelease(controller); CFRelease(original); IOObjectRelease(service);
    return status ? 11 : 0;
}
