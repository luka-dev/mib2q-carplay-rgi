/* Native guest helper for a combined USBMUX/HID/NCM configuration in iOS QEMU. */
/* Header-free Darwin ABI declarations allow building without an iPhone SDK. */
typedef unsigned int uint32_t;
#define NULL ((void *)0)
extern int printf(const char *, ...);
extern unsigned int sleep(unsigned int);
extern unsigned int alarm(unsigned int);
extern int strcmp(const char *, const char *);
extern unsigned int getuid(void);
extern int setuid(unsigned int);
extern int setgid(unsigned int);
extern int fflush(void *);
extern int snprintf(char *, unsigned long, const char *, ...);
extern int SocketGetInterfaceInfo(int, const char *, char *, uint32_t *, unsigned char *,
                                  uint32_t *, uint32_t *, unsigned long long *,
                                  unsigned long long *, uint32_t *);
extern int notify_register_check(const char *, int *);
extern int notify_set_state(int, unsigned long long);
extern int notify_post(const char *);
extern int notify_cancel(int);
#ifdef MHI2_IOS_VM_SCREEN_RENDER_PROBE
extern int mhi2_screen_render_probe(void);
extern int mhi2_screen_framebuffer_probe(void);
#ifdef MHI2_IOS_VM_CLUSTER_PROBE
extern int mhi2_screen_cluster_probe(void);
#endif
#endif

typedef const void *CFTypeRef;
typedef const void *CFStringRef;
typedef const void *CFDictionaryRef;
typedef void *CFMutableDictionaryRef;
typedef const void *CFArrayRef;
typedef long CFIndex;
typedef uint32_t io_service_t;
extern const unsigned char kCFTypeDictionaryKeyCallBacks[];
extern const unsigned char kCFTypeDictionaryValueCallBacks[];
extern const unsigned char kCFTypeArrayCallBacks[];
extern const CFTypeRef kCFBooleanTrue;
extern CFStringRef CFStringCreateWithCString(void *, const char *, uint32_t);
extern CFMutableDictionaryRef CFDictionaryCreateMutable(void *, CFIndex, const void *, const void *);
extern CFMutableDictionaryRef CFDictionaryCreateMutableCopy(void *, CFIndex, CFDictionaryRef);
extern const void *CFDictionaryGetValue(CFDictionaryRef, const void *);
extern void CFDictionarySetValue(CFMutableDictionaryRef, const void *, const void *);
extern CFArrayRef CFArrayCreate(void *, const void **, CFIndex, const void *);
extern unsigned char CFEqual(CFTypeRef, CFTypeRef);
extern void CFRelease(CFTypeRef);
extern CFIndex CFDictionaryGetCount(CFDictionaryRef);
extern unsigned char CFStringGetCString(CFStringRef, char *, CFIndex, uint32_t);
extern int VTCopySupportedPropertyDictionaryForEncoder(int, int, uint32_t,
    CFDictionaryRef, CFStringRef *, CFDictionaryRef *);
extern int mhi2_video_probe(int);
extern void CFPreferencesSetAppValue(CFStringRef, CFTypeRef, CFStringRef);
extern unsigned char CFPreferencesAppSynchronize(CFStringRef);
extern void *IOServiceMatching(const char *);
extern io_service_t IOServiceGetMatchingService(uint32_t, void *);
extern CFTypeRef IORegistryEntryCreateCFProperty(io_service_t, CFStringRef, void *, uint32_t);
extern int IORegistryEntrySetCFProperties(io_service_t, CFTypeRef);
extern int IOObjectRelease(io_service_t);

static CFStringRef string(const char *value)
{
    return CFStringCreateWithCString(NULL, value, 0x08000100);
}

static CFMutableDictionaryRef dictionary(void)
{
    return CFDictionaryCreateMutable(NULL, 0, kCFTypeDictionaryKeyCallBacks,
                                    kCFTypeDictionaryValueCallBacks);
}

static void put(CFMutableDictionaryRef dict, const char *name, CFTypeRef value)
{
    CFStringRef key = string(name);
    CFDictionarySetValue(dict, key, value);
    CFRelease(key);
}

int main(int argc, char **argv)
{
    printf("[mhi2-usb-fixture] start argc=%d uid=%u\n", argc, getuid());
    fflush(NULL);
    io_service_t service = 0;
    for (unsigned i = 0; i < 45; i++) {
        service = IOServiceGetMatchingService(0, IOServiceMatching("AppleT8027USBXDCI"));
        if (service) break;
        sleep(1);
    }
    if (!service) return 1;
    CFStringRef key = string("DeviceDescription");
    CFStringRef identity_key = string("uniqueDeviceID");
    CFDictionaryRef original = NULL;
    for (unsigned i = 0; i < 45; i++) {
        original = IORegistryEntryCreateCFProperty(service, key, NULL, 0);
        if (original && CFDictionaryGetValue(original, identity_key)) break;
        if (original) CFRelease(original);
        original = NULL;
        sleep(1);
    }
    CFRelease(key);
    if (!original) return 2;
    key = identity_key;
    CFStringRef expected = string("00008030-1122334455667788");
    CFTypeRef identity = CFDictionaryGetValue(original, key);
    if (!identity || !CFEqual(identity, expected)) return 3;
    CFRelease(key);
    CFRelease(expected);

#ifdef MHI2_IOS_VM_SCREEN_RENDER_PROBE
#ifdef MHI2_IOS_VM_CLUSTER_PROBE
    if (argc == 2 && !strcmp(argv[1], "--cluster-framebuffer-probe")) {
        if (getuid() != 0) return 4;
        int result = mhi2_screen_cluster_probe();
        CFRelease(original);
        IOObjectRelease(service);
        return result;
    }
#endif
    if (argc == 2 && (!strcmp(argv[1], "--screen-render-probe") ||
                     !strcmp(argv[1], "--screen-framebuffer-probe"))) {
        if (getuid() != 0) return 4;
        int result = !strcmp(argv[1], "--screen-framebuffer-probe") ?
            mhi2_screen_framebuffer_probe() : mhi2_screen_render_probe();
        CFRelease(original);
        IOObjectRelease(service);
        return result;
    }
#endif
    if (argc == 2 && !strcmp(argv[1], "--encoder-mode-hold")) {
        if (getuid() != 0) return 4;
#ifndef MHI2_ENCODER_HOLD_SECONDS
#define MHI2_ENCODER_HOLD_SECONDS 300
#endif
#if MHI2_ENCODER_HOLD_SECONDS != 300 && MHI2_ENCODER_HOLD_SECONDS != 600
#error Invalid bounded encoder hold
#endif
        alarm(MHI2_ENCODER_HOLD_SECONDS + 10);
        printf("[mhi2-encoder-session] native mode held for %u seconds; no probe pixels\n", MHI2_ENCODER_HOLD_SECONDS);
        fflush(NULL);
        for (unsigned seconds = 0; seconds < MHI2_ENCODER_HOLD_SECONDS; ++seconds) {
            sleep(1);
            if ((seconds + 1) % 30 == 0) {
                printf("[mhi2-encoder-session] elapsed=%u\n", seconds + 1);
                fflush(NULL);
            }
        }
        alarm(0);
        CFRelease(original);
        IOObjectRelease(service);
        return 0;
    }
    if (argc == 2 && (!strcmp(argv[1], "--video-info") || !strcmp(argv[1], "--video-info-software") ||
            !strcmp(argv[1], "--software-encoder-session"))) {
        if (getuid() != 0) return 4;
        int result = mhi2_video_probe(!strcmp(argv[1], "--software-encoder-session") ? 2 :
            !strcmp(argv[1], "--video-info-software"));
        CFRelease(original);
        IOObjectRelease(service);
        return result;
    }
    if (argc == 2 && !strcmp(argv[1], "--codec-info")) {
        const uint32_t codecs[] = {0x66747663, 0x61766331, 0x66747668};
        unsigned failures = 0;
        if (getuid() != 0) return 4;
        alarm(30); /* Bound even a native library deadlock in this probe process. */
        for (unsigned i = 0; i < 3; i++) {
            CFStringRef encoder = NULL;
            CFDictionaryRef properties = NULL;
            char identifier[256] = {0};
            printf("[mhi2-codec-info] begin codec=0x%x\n", codecs[i]);
            fflush(NULL);
            int result = VTCopySupportedPropertyDictionaryForEncoder(800, 480,
                codecs[i], NULL, &encoder, &properties);
            if (result) ++failures;
            if (encoder) CFStringGetCString(encoder, identifier, sizeof(identifier), 0x08000100);
            printf("[mhi2-codec-info] result=%d codec=0x%x id=%s properties=%ld\n",
                result, codecs[i], identifier, properties ? CFDictionaryGetCount(properties) : -1L);
            fflush(NULL);
            if (encoder) CFRelease(encoder);
            if (properties) CFRelease(properties);
        }
        alarm(0);
        CFRelease(original);
        IOObjectRelease(service);
        return failures ? 7 : 0;
    }

    if (argc == 2 && !strcmp(argv[1], "--network-info")) {
        if (getuid() != 0) return 4;
        for (unsigned attempt = 0; attempt < 120; attempt++) {
            /* Self-configured NCM uses anpi*, ordinary USB Ethernet uses en*. */
            const char *prefixes[] = {"en", "anpi"};
            for (unsigned slot = 0; slot < 32; slot++) {
                char name[16];
                uint32_t ifindex = 0, media = 0, flags = 0, type = 0;
                unsigned long long extended = 0, other = 0;
                unsigned char mac[6] = {0};
                snprintf(name, sizeof(name), "%s%u", prefixes[slot / 16], slot % 16);
                int error = SocketGetInterfaceInfo(-1, name, NULL, &ifindex, mac,
                    &media, &flags, &extended, &other, &type);
                if (!error && ifindex && (attempt == 0 || attempt % 10 == 0)) {
                    printf("[mhi2-network-info] %s index=%u flags=0x%x extended=0x%llx other=0x%llx type=0x%x mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                        name, ifindex, flags, extended, other, type,
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                    fflush(NULL);
                }
            }
            sleep(1);
        }
        CFRelease(original);
        IOObjectRelease(service);
        return 0;
    }

    if (argc == 2 && !strcmp(argv[1], "--complete-setup")) {
        /* Explicit mobile-user state fixture for this synthetic, activated VM. */
        if (getuid() != 0 || setgid(501) || setuid(501)) return 4;
        CFStringRef domain = string("com.apple.purplebuddy");
        const char *keys[] = {"SetupDone", "SetupFinishedAllSteps"};
        for (unsigned i = 0; i < 2; i++) {
            key = string(keys[i]);
            CFPreferencesSetAppValue(key, kCFBooleanTrue, domain);
            CFRelease(key);
        }
        int synced = CFPreferencesAppSynchronize(domain);
        CFRelease(domain);
        int token = 0;
        int notified = notify_register_check("com.apple.purplebuddy.setupStateChanged", &token);
        if (!notified) {
            notified = notify_set_state(token, 3);
            if (!notified) notified = notify_post("com.apple.purplebuddy.setupStateChanged");
            notify_cancel(token);
        }
        notify_post("com.apple.purplebuddy.setupdone");
        notify_post("com.apple.purplebuddy.setupexited");
        printf("[mhi2-setup-fixture] preferences synced=%d, notify=%d\n", synced, notified);
        CFRelease(original);
        IOObjectRelease(service);
        return !synced || notified;
    }
    if (argc != 1 || getuid() != 0) return 5;

    CFMutableDictionaryRef description = CFDictionaryCreateMutableCopy(NULL, 0, original);
    put(description, "AllowMultipleCreates", kCFBooleanTrue);
    CFMutableDictionaryRef configuration = dictionary();
    CFStringRef name = string("MHI2 VM USBMUX + iAP2 HID + NCM");
    put(configuration, "Description", name);
    put(configuration, "DefaultConfiguration", kCFBooleanTrue);
    CFRelease(name);
    const void *interfaces[4] = {
        string("AppleUSBMux"), string("IapOverUsbHid"),
        string("AppleUSBNCMControlDirect"), string("AppleUSBNCMDataAux")
    };
    CFArrayRef array = CFArrayCreate(NULL, interfaces, 4, kCFTypeArrayCallBacks);
    put(configuration, "Interfaces", array);
    CFRelease(array);
    for (unsigned i = 0; i < 4; i++) CFRelease(interfaces[i]);
    const void *configs[1] = {configuration};
    array = CFArrayCreate(NULL, configs, 1, kCFTypeArrayCallBacks);
    put(description, "ConfigurationDescriptors", array);
    CFRelease(array);
    CFRelease(configuration);
    CFMutableDictionaryRef command = dictionary();
    name = string("DeviceDestroy");
    put(command, "USBDeviceCommand", name);
    CFRelease(name);
    int destroyed = IORegistryEntrySetCFProperties(service, command);
    printf("[mhi2-usb-fixture] destroy old description result=0x%x\n", destroyed);
    if (destroyed) return 6;
    /* Terminated IAP interfaces release their reserved port asynchronously. */
    sleep(2);
    name = string("SetDeviceDescription");
    put(command, "USBDeviceCommand", name);
    put(command, "USBDeviceCommandParameter", description);
    CFRelease(name);
    int result = IORegistryEntrySetCFProperties(service, command);
    printf("[mhi2-usb-fixture] combined description result=0x%x\n", result);
    CFRelease(command);
    CFRelease(description);
    CFRelease(original);
    IOObjectRelease(service);
    return result != 0;
}
