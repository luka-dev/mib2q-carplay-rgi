/* Virtual primary-screen touch input, compiled only into the owned QNX lab. */
#ifdef MHI2Q_CAPTURE_INPUT_QEMU
#include <time.h>
#include <sys/select.h>
#include <stdlib.h>
#ifdef __QNXNTO__
#include <sys/neutrino.h>
#include <sys/syspage.h>
#endif
static void *lab_touch_device;
static int (*lab_touch_post)(void *, const void *, size_t);
#ifdef MHI2Q_CAPTURE_VIDEO_MEMORY_QEMU
#define LAB_AVC_LIMIT (2u * 1024u * 1024u)
static struct {
    uint8_t bytes[LAB_AVC_LIMIT], configuration[16384];
    size_t length, configuration_length;
    int collecting;
    uint64_t generation, pictures, timestamps[8192];
    size_t timestamp_count;
} lab_avc[2];
static pthread_mutex_t lab_avc_lock = PTHREAD_MUTEX_INITIALIZER;
static void *lab_avc_session;

/* Retain the authenticated stock session; replace it on a later RECORD. */
static void lab_avc_set_session(void *session)
{
    void *(*retain)(const void *) = dlsym(RTLD_NEXT, "CFRetain");
    void (*release)(const void *) = dlsym(RTLD_NEXT, "CFRelease");
    void *old;
    if (!session || !retain || !release) return;
    retain(session);
    pthread_mutex_lock(&lab_avc_lock);
    old = lab_avc_session; lab_avc_session = session;
    pthread_mutex_unlock(&lab_avc_lock);
    if (old) release(old);
}

/* Same UUID advertised by the explicitly enabled stream-111 QNX fixture. */
static int lab_avc_force_cluster(void *session)
{
    static const uint8_t command_bytes[] = {
        0x62,0x70,0x6c,0x69,0x73,0x74,0x30,0x30,0xd2,0x01,0x02,0x03,0x06,0x56,0x70,0x61,
        0x72,0x61,0x6d,0x73,0x54,0x74,0x79,0x70,0x65,0xd1,0x04,0x05,0x54,0x75,0x75,0x69,
        0x64,0x5f,0x10,0x24,0x43,0x31,0x30,0x35,0x37,0x45,0x31,0x32,0x2d,0x41,0x30,0x44,
        0x31,0x2d,0x34,0x45,0x32,0x30,0x2d,0x42,0x30,0x43,0x30,0x2d,0x31,0x30,0x35,0x37,
        0x45,0x31,0x32,0x41,0x30,0x44,0x31,0x30,0x5d,0x66,0x6f,0x72,0x63,0x65,0x4b,0x65,
        0x79,0x46,0x72,0x61,0x6d,0x65,0x08,0x0d,0x14,0x19,0x1c,0x21,0x48,0x00,0x00,0x00,
        0x00,0x00,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x56,
    };
    void *(*data_create)(void *, const uint8_t *, long) = dlsym(RTLD_NEXT, "CFDataCreate");
    void *(*parse)(void *, const void *, unsigned long, void *, void *) = dlsym(RTLD_NEXT, "CFPropertyListCreateWithData");
    void (*release)(const void *) = dlsym(RTLD_NEXT, "CFRelease");
    int (*send_command)(void *, void *, void *, void *) = dlsym(RTLD_NEXT, "AirPlayReceiverSessionSendCommand");
    void *data, *command;
    int result = -6705;
    if (!data_create || !parse || !release || !send_command) return result;
    data = data_create(NULL, command_bytes, sizeof(command_bytes));
    if (!data) return -6728;
    command = parse(NULL, data, 0, NULL, NULL); release(data);
    if (command) { result = send_command(session, command, NULL, NULL); release(command); }
    return result;
}

/* A native control call can block during teardown. Permit only one detached
 * call; the input/clock endpoint still times out and remains usable. */
static int lab_force_active, lab_force_result;
struct lab_force_context {
    void *session;
    unsigned channel;
    int (*force)(void *, void *, void *);
    void (*release)(const void *);
};
static void *lab_avc_force_worker(void *argument)
{
    struct lab_force_context *context = argument;
    int result = context->channel ? lab_avc_force_cluster(context->session) :
        context->force(context->session, NULL, NULL);
    context->release(context->session);
    free(context);
    pthread_mutex_lock(&lab_avc_lock);
    lab_force_result = result; lab_force_active = 0;
    pthread_mutex_unlock(&lab_avc_lock);
    return NULL;
}

/* Explicit diagnostics only: never request periodic IDRs during pacing. */
static int lab_avc_refresh(unsigned channel, uint64_t *generation)
{
    int (*force)(void *, void *, void *) = dlsym(RTLD_NEXT, "AirPlayReceiverSessionForceKeyFrame");
    void *(*retain)(const void *) = dlsym(RTLD_NEXT, "CFRetain");
    void (*release)(const void *) = dlsym(RTLD_NEXT, "CFRelease");
    struct lab_force_context *context = NULL;
    pthread_t thread;
    int result = -6705;
    pthread_mutex_lock(&lab_avc_lock);
    *generation = lab_avc[channel].generation;
    if (!lab_force_active && lab_avc_session && force && retain && release) {
        context = malloc(sizeof(*context));
        if (context) {
            context->session = retain(lab_avc_session);
            context->channel = channel; context->force = force; context->release = release;
            lab_force_active = 1; lab_force_result = -6705;
        }
    }
    pthread_mutex_unlock(&lab_avc_lock);
    if (!context) return result;
    result = pthread_create(&thread, NULL, lab_avc_force_worker, context);
    if (!result) pthread_detach(thread);
    else {
        release(context->session); free(context);
        pthread_mutex_lock(&lab_avc_lock);
        lab_force_active = 0;
        pthread_mutex_unlock(&lab_avc_lock);
    }
    return result;
}


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
    if (picture) {
        struct timespec stamp;
        lab_avc[channel].pictures++;
        if (lab_avc[channel].timestamp_count < 8192 && !clock_gettime(CLOCK_MONOTONIC, &stamp))
            lab_avc[channel].timestamps[lab_avc[channel].timestamp_count++] =
                (uint64_t)stamp.tv_sec*1000000000ULL + (uint64_t)stamp.tv_nsec;
    }
    if (configuration && !picture && length <= sizeof(lab_avc[channel].configuration)) {
        memcpy(lab_avc[channel].configuration, data, length);
        lab_avc[channel].configuration_length = length;
    }
    if (idr) {
        lab_avc[channel].length = configuration ? 0 : lab_avc[channel].configuration_length;
        memcpy(lab_avc[channel].bytes, lab_avc[channel].configuration, lab_avc[channel].length);
        lab_avc[channel].collecting = 1;
        lab_avc[channel].generation++;
    }
    if (picture && lab_avc[channel].collecting) {
        if (length <= LAB_AVC_LIMIT - lab_avc[channel].length) {
            memcpy(lab_avc[channel].bytes + lab_avc[channel].length, data, length);
            lab_avc[channel].length += length;
        } else lab_avc[channel].collecting = 0;
    }
    pthread_mutex_unlock(&lab_avc_lock);
}

static void lab_avc_snapshot(int fd, unsigned channel, int fresh)
{
    uint8_t *snapshot = NULL;
    size_t length, sent = 0;
    uint32_t header;
    uint64_t generation = 0;
    int ready = !fresh;
    unsigned attempt;
    struct timespec start, now;
    uint64_t deadline = 0;
    if (!clock_gettime(CLOCK_MONOTONIC, &start))
        deadline = (uint64_t)start.tv_sec*1000000000ULL + start.tv_nsec + 2000000000ULL;
    if (fresh && !lab_avc_refresh(channel, &generation)) {
        for (attempt = 0; attempt < 250; attempt++) {
            pthread_mutex_lock(&lab_avc_lock);
            ready = !lab_force_active && !lab_force_result && lab_avc[channel].generation > generation && lab_avc[channel].collecting && lab_avc[channel].length;
            pthread_mutex_unlock(&lab_avc_lock);
            if (ready) break;
            if (deadline && !clock_gettime(CLOCK_MONOTONIC, &now) &&
                (uint64_t)now.tv_sec*1000000000ULL + now.tv_nsec >= deadline) break;
            usleep(10000);
        }
    }
    pthread_mutex_lock(&lab_avc_lock);
    length = ready && lab_avc[channel].collecting ? lab_avc[channel].length : 0;
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

#ifdef MHI2Q_CAPTURE_VIDEO_MEMORY_QEMU
static void lab_avc_timing(int fd, unsigned channel)
{
    uint32_t count, i, header;
    uint32_t *words = malloc(8192u*8u);
    size_t sent = 0, length;
    if (!words) { header = 0; (void)send(fd, &header, sizeof(header), 0); return; }
    pthread_mutex_lock(&lab_avc_lock);
    count = (uint32_t)lab_avc[channel].timestamp_count;
    for (i = 0; i < count; i++) {
        uint64_t stamp = lab_avc[channel].timestamps[i];
        words[i*2] = htonl((uint32_t)(stamp >> 32));
        words[i*2+1] = htonl((uint32_t)stamp);
    }
    pthread_mutex_unlock(&lab_avc_lock);
    header = htonl(count);
    if (send(fd, &header, sizeof(header), 0) == sizeof(header)) {
        length = count*8u;
        while (sent < length) {
            ssize_t n = send(fd, (uint8_t *)words + sent, length - sent, 0);
            if (n <= 0) break;
            sent += (size_t)n;
        }
    }
    free(words);
}
#endif

static void lab_clock_snapshot(int fd)
{
    uint64_t values[7] = {0};
    uint32_t words[14];
    struct timespec stamp;
    unsigned i;
    size_t sent = 0;
    if (!clock_gettime(CLOCK_MONOTONIC, &stamp))
        values[0] = (uint64_t)stamp.tv_sec*1000000000ULL + stamp.tv_nsec;
    if (!clock_gettime(CLOCK_REALTIME, &stamp))
        values[1] = (uint64_t)stamp.tv_sec*1000000000ULL + stamp.tv_nsec;
#ifdef __QNXNTO__
    values[2] = ClockCycles();
    values[3] = SYSPAGE_ENTRY(qtime)->cycles_per_sec;
    values[4] = SYSPAGE_ENTRY(qtime)->timer_rate;
    values[5] = (uint64_t)(int64_t)SYSPAGE_ENTRY(qtime)->timer_scale;
    values[6] = SYSPAGE_ENTRY(qtime)->nsec_inc;
#endif
    for (i = 0; i < 7; i++) {
        words[i*2] = htonl((uint32_t)(values[i] >> 32));
        words[i*2+1] = htonl((uint32_t)values[i]);
    }
    while (sent < sizeof(words)) {
        ssize_t n = send(fd, (uint8_t *)words+sent, sizeof(words)-sent, 0);
        if (n <= 0) break;
        sent += (size_t)n;
    }
}

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
            lab_avc_snapshot(fd, command[2] == 'C', 0); close(fd); continue;
        }
        if (received == sizeof(command) && (!memcmp(command, "MFM", 3) || !memcmp(command, "MFC", 3)) &&
                !memcmp(command+3, "\0\0\0\0", 4)) {
            lab_avc_snapshot(fd, command[2] == 'C', 1); close(fd); continue;
        }
        if (received == sizeof(command) && (!memcmp(command, "MDM", 3) || !memcmp(command, "MDC", 3)) &&
                !memcmp(command+3, "\0\0\0\0", 4)) {
            lab_avc_timing(fd, command[2] == 'C'); close(fd); continue;
        }
#endif
        #ifdef MHI2Q_VPHONE_AUDIO_PERIPHERAL
        if (received == sizeof(command) && !memcmp(command, "MHA\0\0\0\0", 7)) {
            extern size_t mhi2q_audio_peripheral_copy(void *,size_t,unsigned *,unsigned *);
            uint8_t *pcm=malloc(65536);unsigned rate=0,channels=0;
            size_t length=pcm?mhi2q_audio_peripheral_copy(pcm,65536,&rate,&channels):0;
            uint32_t header[3]={htonl((uint32_t)length),htonl(rate),htonl(channels)};
            (void)send(fd,header,sizeof(header),0);
            size_t sent=0;while(sent<length){ssize_t n=send(fd,pcm+sent,length-sent,0);if(n<=0)break;sent+=(size_t)n;}
            free(pcm);close(fd);continue;
        }
        #endif
        if (received == sizeof(command) && !memcmp(command, "MCL\0\0\0\0", 7)) {
            lab_clock_snapshot(fd); close(fd); continue;
        }
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
