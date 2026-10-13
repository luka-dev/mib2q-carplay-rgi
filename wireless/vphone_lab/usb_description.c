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
extern void **class_copyMethodList(void *, unsigned int *);
extern void *method_getName(void *);
extern const char *sel_getName(void *), *class_getName(void *);
extern void **objc_copyClassList(unsigned int *);
extern const char *class_getImageName(void *);
extern char *strstr(const char *, const char *);
extern void free(void *);
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
extern CFRef IORegistryEntrySearchCFProperty(u32, const char *, CFRef, void *, u32);
extern int IOObjectRelease(u32), IOObjectIsEqualTo(u32, u32), IOObjectConformsTo(u32, const char *);
extern int IORegistryEntryGetParentEntry(u32, const char *, u32 *);
extern int IORegistryEntryGetChildIterator(u32, const char *, u32 *);
extern u32 IOIteratorNext(u32), IORegistryEntryInPlane(u32, const char *);
extern unsigned long CFNumberGetTypeID(void);
extern unsigned char CFNumberGetValue(CFRef, int, void *);
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

/* All callers first validate the controller identity, model and research chip. */
static int controller_port(u32 controller) {
    CFRef key = str("primary-port-id"), value = IORegistryEntryCreateCFProperty(controller, key, 0, 0);
    CFRelease(key);
    if (!value) return 0;
    static const unsigned char expected[] = {2, 0, 0, 0};
    int valid = CFGetTypeID(value) == CFDataGetTypeID() && CFDataGetLength(value) == 4 && !memcmp(CFDataGetBytePtr(value), expected, 4);
    CFRelease(value); return valid ? 1 : -1;
}

static int owned_manager(u32 controller) {
    u32 manager = IOServiceGetMatchingService(0, IOServiceMatching("IOAccessoryManager"));
    if (!manager) return 0;
    CFRef key = str("IOMatchCategory"), value = IORegistryEntryCreateCFProperty(manager, key, 0, 0);
    CFRelease(key); char category[80] = {0};
    int valid = IOObjectConformsTo(manager, "IOAccessoryDockUSB_UFP") && value && CFGetTypeID(value) == CFStringGetTypeID() &&
        CFStringGetCString(value, category, sizeof(category), 0x08000100) && !strcmp(category, "MHI2ResearchAccessoryManager");
    if (value) CFRelease(value);
    key = str("IOAccessoryPrimaryDevicePort"); value = IORegistryEntryCreateCFProperty(manager, key, 0, 0); CFRelease(key);
    int port = 0;
    valid = valid && value && CFGetTypeID(value) == CFNumberGetTypeID() && CFNumberGetValue(value, 3, &port) && port == 2;
    if (value) CFRelease(value);
    u32 parent = 0;
    valid = valid && !IORegistryEntryGetParentEntry(manager, "IOService", &parent) && IOObjectIsEqualTo(parent, controller);
    if (parent) IOObjectRelease(parent);
    IOObjectRelease(manager); return valid ? 1 : -1;
}

static CFMutableRef manager_personality(int merger) {
    CFMutableRef p = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
    const char *keys[] = {"CFBundleIdentifier", "IOClass", "IOProviderClass", "IOMatchCategory"};
    const char *values[] = {merger ? "com.apple.iokit.IOHIDFamily" : "com.apple.iokit.IOAccessoryManager",
        merger ? "IOHIDProviderPropertyMerger" : "IOAccessoryDockUSB_UFP", "AppleVirtIOUSBDeviceController",
        merger ? "MHI2ResearchPortProperties" : "MHI2ResearchAccessoryManager"};
    for (unsigned i = 0; i < 4; i++) { CFRef v = str(values[i]); put(p, keys[i], v); CFRelease(v); }
    return p;
}

static int catalogue_send(void *io, u32 operation, CFRef object) {
    CFRef (*serialize)(CFRef, unsigned long) = dlsym(io, "IOCFSerialize");
    int (*send_data)(u32, u32, const char *, u32) = dlsym(io, "IOCatalogueSendData");
    if (!serialize || !send_data) return 31;
    CFRef data = serialize(object, 0);
    if (!data) return 32;
    int status = send_data(0, operation, (const char *)CFDataGetBytePtr(data), (u32)CFDataGetLength(data));
    CFRelease(data); printf("owned catalogue operation=%u result=0x%x\n", operation, status); fflush(0);
    return status;
}

static int catalogue_entitled(void) {
    void *security = dlopen("/System/Library/Frameworks/Security.framework/Security", 2);
    CFRef (*self_task)(void *) = security ? dlsym(security, "SecTaskCreateFromSelf") : 0;
    CFRef (*entitlement)(CFRef, CFRef, CFRef *) = security ? dlsym(security, "SecTaskCopyValueForEntitlement") : 0;
    if (!self_task || !entitlement) return 0;
    CFRef task = self_task(0), key = str("com.apple.private.security.iocatalog-management");
    CFRef value = task ? entitlement(task, key, 0) : 0;
    int valid = value == kCFBooleanTrue;
    if (value) CFRelease(value);
    if (task) CFRelease(task);
    CFRelease(key); return valid;
}

static int manager_configure(u32 controller, int remove) {
    int existing = owned_manager(controller);
    int property = controller_port(controller);
    if (existing < 0 || property < 0 || (existing && property != 1)) { printf("existing accessory metadata belongs to another configuration\n"); return 36; }
    if (existing) {
        printf("owned native manager already present\n");
        if (remove) { printf("restart the VM to remove the live manager and provider properties\n"); return 39; }
        return 0;
    }
    if (!catalogue_entitled()) { printf("catalogue entitlement missing; refusing a possible false success\n"); return 31; }
    void *io = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", 2);
    if (!io) return 31;
    CFMutableRef manager = manager_personality(0), merger = manager_personality(1);
    if (remove) {
        /* Remove only our catalogue descriptions. This does not delete properties. */
        int first = catalogue_send(io, 3, manager), second = catalogue_send(io, 3, merger);
        CFRelease(manager); CFRelease(merger);
        if (first || second) { printf("owned catalogue removal failed\n"); return 33; }
        printf("owned descriptions removed; provider property resets at VM restart\n");
        return 0;
    }
    CFMutableRef properties = CFDictionaryCreateMutable(0, 0, kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks);
    static const unsigned char port[] = {2, 0, 0, 0};
    CFRef data = CFDataCreate(0, port, sizeof(port)); put(properties, "primary-port-id", data); CFRelease(data);
    put(merger, "IOProviderMergeProperties", properties); CFRelease(properties);
    CFRef item[] = {merger}; CFRef array = CFArrayCreate(0, item, 1, kCFTypeArrayCallBacks);
    int status = catalogue_send(io, 1, array); CFRelease(array); CFRelease(merger);
    if (status) { CFRelease(manager); return 33; }
    for (unsigned i = 0; i < 5 && controller_port(controller) == 0; i++) sleep(1);
    if (controller_port(controller) != 1) { CFRelease(manager); printf("provider primary-port-id not established\n"); return 35; }
    printf("virtual provider primary-port-id=2 verified\n"); fflush(0);
    item[0] = manager; array = CFArrayCreate(0, item, 1, kCFTypeArrayCallBacks);
    status = catalogue_send(io, 1, array); CFRelease(array); CFRelease(manager);
    if (status) return 33;
    for (unsigned i = 0; i < 5 && owned_manager(controller) == 0; i++) sleep(1);
    int verified = owned_manager(controller) == 1;
    printf("owned native manager verified=%d\n", verified);
    return verified ? 0 : 34;
}

static int client_children(u32 service, const char *type) {
    u32 iterator = 0, count = 0;
    int complete = 0;
    if (IORegistryEntryGetChildIterator(service, "IOService", &iterator)) { printf("registry-child=%s query-failed\n", type); return 1; }
    for (unsigned i = 0; i < 128; i++) {
        u32 child = IOIteratorNext(iterator);
        if (!child) { complete = 1; break; }
        if (IOObjectConformsTo(child, type)) count++;
        IOObjectRelease(child);
    }
    IOObjectRelease(iterator); printf("registry-child=%s count=%u complete=%d\n", type, count, complete);
    return !complete;
}

static int iap_registry(u32 controller) {
    void *io = dlopen("/usr/lib/libIOAccessoryManager.dylib", 2);
    u32 (*upstream)(u32) = io ? dlsym(io, "IOAccessoryManagerGetUpstreamService") : 0;
    if (!upstream) return 30;
    printf("owned-manager=%d provider-port=%d\n", owned_manager(controller), controller_port(controller));
    const char *classes[] = {"IOAccessoryManager", "IOAccessoryPortUSB", "IOAccessoryEAInterface", "AppleAuthCP"};
    int incomplete = 0;
    for (unsigned i = 0; i < 4; i++) {
        u32 service = IOServiceGetMatchingService(0, IOServiceMatching(classes[i]));
        printf("registry-class=%s present=%d", classes[i], service != 0);
        if (service && i < 2) printf(" accessory-plane=%u", IORegistryEntryInPlane(service, "IOAccessory"));
        if (service && i == 1) {
            u32 parent = upstream(service); printf(" upstream-service=0x%x", parent); if (parent) IOObjectRelease(parent);
            CFRef key = str("function-dock_parent");
            CFRef link = IORegistryEntrySearchCFProperty(service, "IOService", key, 0, 3); CFRelease(key);
            printf(" dock-parent-property=%d", link != 0);
            if (link && CFGetTypeID(link) == CFDataGetTypeID()) printf(" dock-parent-bytes=%ld", CFDataGetLength(link));
            if (link) CFRelease(link);
        }
        printf("\n");
        if (service && i < 2) incomplete |= client_children(service, i ? "IOAccessoryPortUserClient" : "IOAccessoryManagerUserClient");
        if (service) IOObjectRelease(service);
    }
    return incomplete ? 40 : 0;
}

static int iap_inspect(const char *selected, int list_classes) {
    if (!dlopen("/System/Library/CoreAccessories/PlugIns/Transports/IOAccessoryManager.transport/IOAccessoryManager", 2)) {
        printf("transport load failed: %s\n", dlerror()); return 28;
    }
    void *type = objc_getClass(selected);
    const char *image = type ? class_getImageName(type) : 0;
    if (!image || !strstr(image, "IOAccessoryManager.transport/IOAccessoryManager")) return 29;
    unsigned count = 0; void **methods = class_copyMethodList(type, &count);
    unsigned shown = count < 80 ? count : 80;
    printf("class=%s methods=%u shown=%u\n", class_getName(type), count, shown);
    for (unsigned i = 0; i < shown; i++) printf("selector=%s\n", sel_getName(method_getName(methods[i])));
    free(methods);
    if (list_classes) {
        void **classes = objc_copyClassList(&count);
        for (unsigned i = 0; i < count; i++) {
            image = class_getImageName(classes[i]);
            if (image && strstr(image, "IOAccessoryManager.transport/IOAccessoryManager")) printf("transport-class=%s\n", class_getName(classes[i]));
        }
        free(classes);
    }
    return 0;
}

int main(int argc, char **argv) {
    alarm(20);
    if ((argc != 3 && !(argc == 4 && !strcmp(argv[1], "iap-inspect"))) || getuid() != 0 || (strcmp(argv[1], "probe") && strcmp(argv[1], "apply") && strcmp(argv[1], "restore") && strcmp(argv[1], "reconnect") && strcmp(argv[1], "network") && strcmp(argv[1], "apply-destroy") && strcmp(argv[1], "iap-list") && strcmp(argv[1], "iap-wake") && strcmp(argv[1], "iap-inspect") && strcmp(argv[1], "iap-registry") && strcmp(argv[1], "manager-apply") && strcmp(argv[1], "manager-remove"))) return 2;
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
    if (!strcmp(argv[1], "manager-apply") || !strcmp(argv[1], "manager-remove")) return manager_configure(service, !strcmp(argv[1], "manager-remove"));
    if (!strcmp(argv[1], "iap-registry")) return iap_registry(service);
    if (!strcmp(argv[1], "iap-inspect")) return iap_inspect(argc == 4 ? argv[3] : "ACCTransportPluginIOAccessoryManager", argc == 3);
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
