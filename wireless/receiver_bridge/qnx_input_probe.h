/* Virtual primary-screen touch input, compiled only into the owned QNX lab. */
#ifdef MHI2Q_CAPTURE_INPUT_QEMU
#include <time.h>
#include <sys/select.h>
#include <stdlib.h>
static void *lab_touch_device;
static int (*lab_touch_post)(void *, const void *, size_t);
#ifdef MHI2Q_CAPTURE_VIDEO_MEMORY_QEMU
#define LAB_AVC_LIMIT (2u * 1024u * 1024u)
static struct {
    uint8_t bytes[LAB_AVC_LIMIT], configuration[16384];
    size_t length, configuration_length;
    int collecting;
} lab_avc[2];
static pthread_mutex_t lab_avc_lock = PTHREAD_MUTEX_INITIALIZER;

static void lab_avc_store(const char *marker, const uint8_t *data, uint32_t length)
{
    unsigned channel = !strcmp(marker, "native-cluster-qemu"), i;
    int configuration = 0, picture = 0, idr = 0;
    if (!data || !length || length > LAB_AVC_LIMIT) return;
    for (i = 0; i + 3 < length; i++) {
        if (!data[i] && !data[i+1] && data[i+2] == 1) {
            unsigned type = data[i+3] & 31;
            configuration |= type == 7 || type == 8;
            picture |= type >= 1 && type <= 5;
            idr |= type == 5;
        }
    }
    pthread_mutex_lock(&lab_avc_lock);
    if (configuration && !picture && length <= sizeof(lab_avc[channel].configuration)) {
        memcpy(lab_avc[channel].configuration, data, length);
        lab_avc[channel].configuration_length = length;
    }
    if (idr) {
        lab_avc[channel].length = configuration ? 0 : lab_avc[channel].configuration_length;
        memcpy(lab_avc[channel].bytes, lab_avc[channel].configuration, lab_avc[channel].length);
        lab_avc[channel].collecting = 1;
    }
    if (picture && lab_avc[channel].collecting) {
        if (length <= LAB_AVC_LIMIT - lab_avc[channel].length) {
            memcpy(lab_avc[channel].bytes + lab_avc[channel].length, data, length);
            lab_avc[channel].length += length;
        } else lab_avc[channel].collecting = 0;
    }
    pthread_mutex_unlock(&lab_avc_lock);
}

static void lab_avc_snapshot(int fd, unsigned channel)
{
    uint8_t *snapshot = NULL;
    size_t length, sent = 0;
    uint32_t header;
    pthread_mutex_lock(&lab_avc_lock);
    length = lab_avc[channel].length;
    if (length) {
        snapshot = malloc(length);
        if (snapshot) memcpy(snapshot, lab_avc[channel].bytes, length);
        else length = 0;
    }
    pthread_mutex_unlock(&lab_avc_lock);
    header = htonl((uint32_t)length);
    if (send(fd, &header, sizeof(header), 0) == sizeof(header)) {
        while (sent < length) {
            ssize_t count = send(fd, snapshot + sent, length - sent, 0);
            if (count <= 0) break;
            sent += (size_t)count;
        }
    }
    free(snapshot);
}
#endif

static void *lab_touch_worker(void *unused)
{
    struct sockaddr_in address;
    struct timeval timeout = {1, 0};
    int listener, reuse = 1;
    unsigned requests = 0;
    (void)unused;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) return NULL;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET; address.sin_port = htons(32503);
    address.sin_addr.s_addr = inet_addr("10.0.3.15");
    (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    (void)setsockopt(listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (bind(listener, (void *)&address, sizeof(address)) || listen(listener, 2)) {
        close(listener); return NULL;
    }
    {
        static const char ready[] = "[native-input-qemu] ready primary=800x480\n";
        (void)write(2, ready, sizeof(ready)-1);
    }
    while (requests < 256) {
        uint8_t command[7], report[5];
        size_t received = 0;
        int fd, result = -6705;
        uint32_t response;
        unsigned x, y;
        fd_set readable;
        struct timeval wait = {1, 0};
        FD_ZERO(&readable); FD_SET(listener, &readable);
        if (select(listener+1, &readable, NULL, NULL, &wait) <= 0) continue;
        fd = accept(listener, NULL, NULL);
        if (fd < 0) continue;
        requests++;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        while (received < sizeof(command)) {
            ssize_t count = recv(fd, command + received, sizeof(command) - received, 0);
            if (count <= 0) break;
            received += (size_t)count;
        }
#ifdef MHI2Q_CAPTURE_VIDEO_MEMORY_QEMU
        if (received == sizeof(command) && (!memcmp(command, "MHM\0\0\0\0", 7) ||
                                           !memcmp(command, "MHC\0\0\0\0", 7))) {
            lab_avc_snapshot(fd, command[2] == 'C'); close(fd); continue;
        }
#endif
        x = received == sizeof(command) ? ((unsigned)command[3] << 8) | command[4] : 800;
        y = received == sizeof(command) ? ((unsigned)command[5] << 8) | command[6] : 480;
        if (received == sizeof(command) && !memcmp(command, "MHT", 3) && x < 800 && y < 480) {
            report[0] = 1; report[1] = (uint8_t)x; report[2] = (uint8_t)(x >> 8);
            report[3] = (uint8_t)y; report[4] = (uint8_t)(y >> 8);
            result = lab_touch_post(lab_touch_device, report, sizeof(report));
            /* Always release the finger, including a failed press. */
            usleep(80000); report[0] = 0;
            {
                int released = lab_touch_post(lab_touch_device, report, sizeof(report));
                if (!result) result = released;
            }
        }
        response = htonl((uint32_t)result);
        (void)send(fd, &response, sizeof(response), 0);
        close(fd);
    }
    close(listener);
    return NULL;
}

static void lab_touch_register(void *screen)
{
    typedef const void *ref;
    ref (*string)(void *, const char *, unsigned) = dlsym(RTLD_NEXT, "CFStringCreateWithCString");
    ref (*copy)(ref, unsigned, ref, ref, int *) = dlsym(RTLD_NEXT, "_ScreenCopyProperty");
    ref (*data)(void *, const uint8_t *, long) = dlsym(RTLD_NEXT, "CFDataCreate");
    void (*release)(ref) = dlsym(RTLD_NEXT, "CFRelease");
    int (*create)(void **, ref) = dlsym(RTLD_NEXT, "HIDDeviceCreateVirtual");
    int (*property)(void *, ref, ref, ref) = dlsym(RTLD_NEXT, "HIDDeviceSetProperty");
    int (*reg)(void *) = dlsym(RTLD_NEXT, "HIDRegisterDevice");
    /* USB HID digitizer/finger: one touch bit, padding, absolute 16-bit X/Y. */
    static const uint8_t descriptor[] = {
        0x05,0x0d,0x09,0x04,0xa1,0x01,0x09,0x22,0xa1,0x02,
        0x09,0x33,0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
        0x75,0x07,0x81,0x01,0x05,0x01,0x09,0x30,0x26,0x1f,0x03,
        0x75,0x10,0x81,0x02,0x09,0x31,0x26,0xdf,0x01,0x81,0x02,0xc0,0xc0
    };
    ref key, uuid, description, name;
    int result = -6705;
    pthread_t thread;
    if (lab_touch_device || !string || !copy || !data || !release || !create || !property || !reg) return;
    lab_touch_post = dlsym(RTLD_NEXT, "HIDDevicePostReport");
    if (!lab_touch_post) return;
    key = string(NULL, "uuid", 0x08000100);
    if (!key) return;
    uuid = copy(screen, 0, key, NULL, &result); release(key);
    if (!uuid || result) { if (uuid) release(uuid); return; }
    result = create(&lab_touch_device, NULL);
    if (!result) {
        key = string(NULL, "displayUUID", 0x08000100);
        result = key ? property(lab_touch_device, key, NULL, uuid) : -6728;
        if (key) release(key);
    }
    release(uuid);
    if (!result) {
        key = string(NULL, "hidDescriptor", 0x08000100);
        description = data(NULL, descriptor, sizeof(descriptor));
        result = key && description ? property(lab_touch_device, key, NULL, description) : -6728;
        if (description) release(description);
        if (key) release(key);
    }
    if (!result) {
        key = string(NULL, "name", 0x08000100); name = string(NULL, "MHI2 QNX lab touch", 0x08000100);
        result = key && name ? property(lab_touch_device, key, NULL, name) : -6728;
        if (key) release(key);
        if (name) release(name);
    }
    if (!result) result = reg(lab_touch_device);
    {
        char line[80]; int used = snprintf(line, sizeof(line), "[native-input-qemu] register result=%d\n", result);
        if (used > 0 && (size_t)used < sizeof(line)) (void)write(2, line, (size_t)used);
    }
    if (result || pthread_create(&thread, NULL, lab_touch_worker, NULL)) {
        if (lab_touch_device) release(lab_touch_device);
        lab_touch_device = NULL; return;
    }
    pthread_detach(thread);
}
#endif
