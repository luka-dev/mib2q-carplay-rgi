/* Receiver control bridge for the exact MU1316 dio_manager image.
 *
 * Linked with capture_probe.c under MHI2Q_CAPTURE_BRIDGE. A QEMU-only
 * self-test exercises the daemon's localhost wire protocol and two
 * start/stop generations against the real stock AirPlay server. On-unit
 * manager lifetime, initial wireless display configuration, and multicast
 * discovery still require validation before deployment.
 */
#include "protocol.h"

#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define BRIDGE_PORT 57343
#if defined(MHI2Q_BRIDGE_STOCK_QEMU) || defined(MHI2Q_BRIDGE_STOCK210)
#define AIRPLAY_PORT 5000
#ifdef MHI2Q_BRIDGE_STOCK210
#define REQUIRED_FEATURES ((UINT64_C(1) << 37) | (UINT64_C(1) << 38))
#else
#define REQUIRED_FEATURES (UINT64_C(1) << 37)
#endif
#else
#define AIRPLAY_PORT 7000
#define REQUIRED_FEATURES ((UINT64_C(1) << 37) | (UINT64_C(1) << 38))
#endif
#ifndef MHI2Q_BRIDGE_INTERFACE_NAME
#define MHI2Q_BRIDGE_INTERFACE_NAME "uap0"
#endif
#ifndef MHI2Q_BRIDGE_DISPLAY_MODE
#define MHI2Q_BRIDGE_DISPLAY_MODE 0
#endif

extern void *mhi2q_probe_manager(void);
extern void *mhi2q_probe_airplay_server(void);
extern unsigned int mhi2q_probe_airplay_bonjour_port(void);
extern unsigned int mhi2q_probe_airplay_bonjour_successes(void);
extern void mhi2q_probe_record_airplay_bonjour(unsigned int port);
extern void mhi2q_probe_record_airplay_server(void *server);
extern char *__progname;
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
extern unsigned int mhi2q_property_handoff_ready(void);
extern unsigned int mhi2q_property_handoff_quiesce(void);
extern unsigned int mhi2q_property_handoff_is_quiesced(void);
#endif
#ifdef MHI2Q_BRIDGE_QEMU_PROPERTY_RACE
extern unsigned int mhi2q_property_handoff_qemu_inflight(void);
extern void mhi2q_property_handoff_qemu_arm(unsigned int armed);
#endif

static pthread_mutex_t bridge_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t active_generation;
static int active;
static int event_posted;
static int server_stopped;
static void *retained_server;
static volatile unsigned int listener_started;
static char advertised_pairing_id[MHI2Q_RBR_ID_MAX + 1];
static pthread_mutex_t identity_lock = PTHREAD_MUTEX_INITIALIZER;

static int wired_owner(void)
{
    const char *mode = getenv("MHI2Q_DIO_OWNER_MODE");
    return mode && strcmp(mode, "wired") == 0;
}

static int hook_ready(void)
{
#ifdef MHI2Q_BRIDGE_STOCK210
    int (*ready)(void)=(int (*)(void))dlsym(RTLD_DEFAULT,"carplay_wireless_receiver_ready");
    return ready && ready();
#else
    return 1;
#endif
}

#ifdef MHI2Q_BRIDGE_STOCK210
static int hook_control(void *server,unsigned operation)
{
    int (*control)(void *,unsigned)=(int (*)(void *,unsigned))dlsym(RTLD_DEFAULT,"carplay_wireless_receiver_control");
    return control?control(server,operation):-1;
}
#endif

static void bridge_log(const char *label, int value)
{
    char line[120];
    int used = snprintf(line, sizeof(line), "[dio-bridge] %s=%d\n", label, value);
    if (used > 0 && (size_t)used < sizeof(line))
        (void)write(2, line, (size_t)used);
}

#ifndef MHI2Q_BRIDGE_STOCK_QEMU
static int bonjour_has_pairing_identity(const void *txt_record, uint16_t length)
{
    const uint8_t *bytes = txt_record;
    size_t offset = 0;
    if (!bytes) return 0;
    while (offset < length) {
        size_t item_length = bytes[offset++];
        if (item_length > (size_t)length - offset) return 0;
        if (item_length > 3 && memcmp(bytes + offset, "pi=", 3) == 0)
            return 1;
        offset += item_length;
    }
    return 0;
}
#endif

static void record_pairing_identity(const void *txt_record, uint16_t length)
{
    char identity[MHI2Q_RBR_ID_MAX + 1] = {0};
    (void)mhi2q_rbr_copy_pairing_id(txt_record, length, identity);
    pthread_mutex_lock(&identity_lock);
    memcpy(advertised_pairing_id, identity, sizeof(identity));
    pthread_mutex_unlock(&identity_lock);
}

int AirPlayReceiverServerCreate(void *out_server)
{
    typedef int (*create_fn)(void *);
    typedef void *(*retain_fn)(void *);
    create_fn real = (create_fn)dlsym(RTLD_NEXT, "AirPlayReceiverServerCreate");
    retain_fn retain = (retain_fn)dlsym(RTLD_DEFAULT, "CFRetain");
    int status;
    if (!real)
        return -1;
    status = real(out_server);
    if (status == 0 && !wired_owner() && out_server && *(void **)out_server && retain &&
        mhi2q_probe_manager() &&
        __progname && strcmp(__progname, "dio_manager") == 0) {
        pthread_mutex_lock(&bridge_lock);
        if (!retained_server) {
            retained_server = retain(*(void **)out_server);
            mhi2q_probe_record_airplay_server(retained_server);
        }
        pthread_mutex_unlock(&bridge_lock);
    }
    bridge_log("AirPlayReceiverServerCreate status", status);
    return status;
}

int DNSServiceRegister(void **out_ref, uint32_t flags, uint32_t interface_index,
                       const char *name, const char *regtype,
                       const char *domain, const char *host, uint16_t port,
                       uint16_t txt_length, const void *txt_record,
                       void *callback, void *context)
{
    typedef int (*register_fn)(void **, uint32_t, uint32_t, const char *,
                               const char *, const char *, const char *,
                               uint16_t, uint16_t, const void *, void *, void *);
    register_fn real = (register_fn)dlsym(RTLD_NEXT, "DNSServiceRegister");
    int status;
    if (!real)
        return -1;
    status = real(out_ref, flags, interface_index, name, regtype, domain,
                  host, port, txt_length, txt_record, callback, context);
    if (status == 0 && mhi2q_probe_manager() && __progname &&
        strcmp(__progname, "dio_manager") == 0 &&
        regtype && strcmp(regtype, "_airplay._tcp.") == 0
#ifndef MHI2Q_BRIDGE_STOCK_QEMU
        && bonjour_has_pairing_identity(txt_record, txt_length)
#endif
        ) {
        record_pairing_identity(txt_record, txt_length);
        mhi2q_probe_record_airplay_bonjour(ntohs(port));
        bridge_log("DNSServiceRegister status", status);
    }
    return status;
}

static uint64_t be_integer(const uint8_t *bytes, size_t length)
{
    uint64_t value = 0;
    size_t index;
    for (index = 0; index < length; ++index)
        value = (value << 8) | bytes[index];
    return value;
}

static int plist_offset(const uint8_t *body, size_t length, uint64_t object,
                        uint64_t count, uint64_t table, unsigned int width,
                        size_t *offset)
{
    uint64_t position;
    uint64_t value;
    if (!width || width > 8 || object >= count || table > length ||
        object > (UINT64_MAX - table) / width)
        return -1;
    position = table + object * width;
    if (position > length || width > length - position)
        return -1;
    value = be_integer(body + position, width);
    if (value >= length)
        return -1;
    *offset = (size_t)value;
    return 0;
}

/* Minimal bounded binary-plist reader for the top-level "features" integer.
 * The stock /info response uses a top-level dictionary with short ASCII keys. */
static int plist_has_required_features(const uint8_t *body, size_t length)
{
    const uint8_t *trailer;
    uint64_t count;
    uint64_t top;
    uint64_t table;
    unsigned int offset_width;
    unsigned int ref_width;
    size_t root;
    size_t dict_start;
    size_t pairs;
    size_t index;
    if (length < 40 || memcmp(body, "bplist00", 8) != 0)
        return 0;
    trailer = body + length - 32;
    offset_width = trailer[6];
    ref_width = trailer[7];
    count = be_integer(trailer + 8, 8);
    top = be_integer(trailer + 16, 8);
    table = be_integer(trailer + 24, 8);
    if (!ref_width || ref_width > 8 ||
        plist_offset(body, length, top, count, table, offset_width, &root))
        return 0;
    if ((body[root] >> 4) != 0xd)
        return 0;
    pairs = body[root] & 15;
    dict_start = root + 1;
    if (pairs == 15) {
        unsigned int count_width;
        uint64_t extended_pairs;
        if (dict_start >= length || (body[dict_start] >> 4) != 1 ||
            (body[dict_start] & 15) > 3)
            return 0;
        count_width = 1u << (body[dict_start] & 15);
        ++dict_start;
        if (count_width > length - dict_start)
            return 0;
        extended_pairs = be_integer(body + dict_start, count_width);
        if (extended_pairs > 64)
            return 0;
        pairs = (size_t)extended_pairs;
        dict_start += count_width;
    }
    if (pairs > 64 || dict_start > length ||
        pairs > (length - dict_start) / (2 * ref_width))
        return 0;
    for (index = 0; index < pairs; ++index) {
        size_t key_offset;
        size_t value_offset;
        uint64_t key_ref = be_integer(body + dict_start + index * ref_width,
                                      ref_width);
        uint64_t value_ref = be_integer(body + dict_start +
                                        (pairs + index) * ref_width, ref_width);
        unsigned int key_length;
        unsigned int integer_length;
        if (plist_offset(body, length, key_ref, count, table, offset_width,
                         &key_offset) ||
            plist_offset(body, length, value_ref, count, table, offset_width,
                         &value_offset))
            return 0;
        key_length = body[key_offset] & 15;
        if ((body[key_offset] >> 4) != 5 || key_length != 8 ||
            key_offset + 1 > length || key_length > length - key_offset - 1 ||
            memcmp(body + key_offset + 1, "features", 8) != 0)
            continue;
        if ((body[value_offset] >> 4) != 1 ||
            (body[value_offset] & 15) > 3)
            return 0;
        integer_length = 1u << (body[value_offset] & 15);
        if (value_offset + 1 > length ||
            integer_length > length - value_offset - 1)
            return 0;
        return (be_integer(body + value_offset + 1, integer_length) &
                REQUIRED_FEATURES) == REQUIRED_FEATURES;
    }
    return 0;
}

static int connect_airplay(void)
{
    struct sockaddr_in address;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(AIRPLAY_PORT);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int probe_info(void)
{
    static const char request[] =
        "GET /info HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    uint8_t reply[8192];
    struct timeval timeout = { 1, 0 };
    size_t used = 0;
    size_t index;
    int fd = connect_airplay();
    if (fd < 0)
        return 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    if (write(fd, request, sizeof(request) - 1) !=
        (ssize_t)(sizeof(request) - 1)) {
        close(fd);
        return 0;
    }
    while (used < sizeof(reply)) {
        ssize_t step = read(fd, reply + used, sizeof(reply) - used);
        if (step <= 0)
            break;
        used += (size_t)step;
    }
    close(fd);
    if (used < 20 || memcmp(reply, "HTTP/1.1 200", 12) != 0)
        return 0;
    for (index = 0; index + 4 <= used; ++index) {
        if (memcmp(reply + index, "\r\n\r\n", 4) == 0)
            return plist_has_required_features(reply + index + 4,
                                              used - index - 4);
    }
    return 0;
}

static int control_server(const char *command)
{
    typedef void *(*make_string_fn)(void *, const char *, unsigned int);
    typedef int (*control_fn)(void *, unsigned int, void *, void *, void *, void *);
    typedef void (*release_fn)(void *);
    make_string_fn make_string = (make_string_fn)
        dlsym(RTLD_DEFAULT, "CFStringCreateWithCString");
    control_fn control = (control_fn)
        dlsym(RTLD_DEFAULT, "AirPlayReceiverServerControl");
    release_fn release = (release_fn)dlsym(RTLD_DEFAULT, "CFRelease");
    void *server = mhi2q_probe_airplay_server();
    void *command_string;
    int result;
    if (!server || !make_string || !control || !release)
        return -1;
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
    if (strcmp(command, "startServer") == 0) {
#ifdef MHI2Q_BRIDGE_STOCK210
        if(hook_control(server,2)!=0)return -1;
#else
        typedef int (*resume_fn)(void *);
        resume_fn resume = (resume_fn)dlsym(
            RTLD_DEFAULT, "MHI2QAirPlayReceiverServerResume");
        if (!resume || resume(server) != 0)
            return -1;
#endif
    }
#endif
    command_string = make_string(NULL, command, 0x08000100u);
    if (!command_string)
        return -1;
    result = control(server, 1, command_string, NULL, NULL, NULL);
    release(command_string);
    return result;
}

#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
/* The receiver checks playing and closes new SETUP admission on its HTTP
 * queue before scheduling a stop. Missing support must not authorize a
 * forced close of a potentially active media session. */
static int stop_server_if_idle(void)
{
#ifndef MHI2Q_BRIDGE_STOCK210
    typedef int (*stop_fn)(void *);
    stop_fn stop;
#endif
    void *server = mhi2q_probe_airplay_server();
    if (!server)
        return 0;
#ifdef MHI2Q_BRIDGE_STOCK210
    return hook_control(server,1);
#else
    stop = (stop_fn)dlsym(RTLD_DEFAULT,
                         "MHI2QAirPlayReceiverServerStopIfIdle");
    return stop ? stop(server) : -1;
#endif
}
#endif

/* Called under bridge_lock, after the guarded manager capture. The stock
 * dimension routine selects from dio's own three-entry display table. */
static int post_first_start(void *manager)
{
    uintptr_t address = (uintptr_t)manager;
    uint32_t dimensions[2] = { 0, 0 };
    if (!manager || event_posted || MHI2Q_BRIDGE_DISPLAY_MODE > 2 ||
        if_nametoindex(MHI2Q_BRIDGE_INTERFACE_NAME) == 0 ||
        *(uintptr_t *)(address + 0x168u) != 0)
        return -1;
    if (*(uint32_t *)(address + 0x224u) == 0 ||
        *(uint32_t *)(address + 0x228u) == 0) {
        ((void (*)(uint32_t *, unsigned int))0x001675a4u)(
            dimensions, MHI2Q_BRIDGE_DISPLAY_MODE);
        if (dimensions[0] == 0 || dimensions[1] == 0)
            return -1;
        *(uint32_t *)(address + 0x224u) = dimensions[0];
        *(uint32_t *)(address + 0x228u) = dimensions[1];
        *(uint32_t *)(address + 0x22cu) = dimensions[0];
        *(uint32_t *)(address + 0x230u) = dimensions[1];
        bridge_log("stock display width", (int)dimensions[0]);
        bridge_log("stock display height", (int)dimensions[1]);
    }
    event_posted = 1;
    bridge_log("posting first StartAirPlay", 1);
    ((void (*)(void *, int))0x00157460u)(manager, 0);
    return 0;
}

static uint8_t bridge_start(uint32_t generation)
{
    unsigned int previous_bonjour = mhi2q_probe_airplay_bonjour_successes();
    void *manager = mhi2q_probe_manager();
    int need_fresh_bonjour = 0;
    int attempt;
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
    if (mhi2q_property_handoff_is_quiesced())
        return MHI2Q_RBR_NOT_READY;
#endif
    if (active) {
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
        if (!mhi2q_property_handoff_ready())
            return MHI2Q_RBR_NOT_READY;
#endif
        if (!hook_ready() || mhi2q_probe_airplay_bonjour_port() != AIRPLAY_PORT ||
            !probe_info()) {
            active = 0;
            server_stopped = 1;
            return MHI2Q_RBR_NOT_READY;
        }
        return active_generation == generation ? MHI2Q_RBR_OK : MHI2Q_RBR_BUSY;
    }
    if (!manager)
        return MHI2Q_RBR_NOT_READY;
    if (mhi2q_probe_airplay_server()) {
        if (server_stopped) {
            need_fresh_bonjour = 1;
            if (control_server("startServer") != 0)
                return MHI2Q_RBR_NOT_READY;
            server_stopped = 0;
        } else if (!event_posted && !probe_info()) {
            need_fresh_bonjour = 1;
            if (control_server("startServer") != 0)
                return MHI2Q_RBR_NOT_READY;
        }
    } else {
        need_fresh_bonjour = 1;
        if (!event_posted && post_first_start(manager) != 0)
            return MHI2Q_RBR_NOT_READY;
    }
    for (attempt = 0; attempt < 60; ++attempt) {
        if (mhi2q_probe_manager() != manager)
            return MHI2Q_RBR_NOT_READY;
        if (
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
            mhi2q_property_handoff_ready() &&
#endif
            hook_ready() && mhi2q_probe_airplay_bonjour_port() == AIRPLAY_PORT &&
            mhi2q_probe_airplay_bonjour_successes() >
                (need_fresh_bonjour ? previous_bonjour : 0) &&
            probe_info()) {
            bridge_log("AirPlay /info ready on port", AIRPLAY_PORT);
            active_generation = generation;
            active = 1;
            return MHI2Q_RBR_OK;
        }
        usleep(100000);
    }
    return MHI2Q_RBR_NOT_READY;
}

static uint8_t bridge_stop(uint32_t generation)
{
    int attempt;
    if (!active || active_generation != generation)
        return MHI2Q_RBR_BAD_REQUEST;
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
    {
        int stopped = stop_server_if_idle();
        bridge_log("stop idle result", stopped);
        if (stopped > 0) {
            bridge_log("active media defers stop", 1);
            return MHI2Q_RBR_BUSY;
        }
        if (stopped < 0) return MHI2Q_RBR_NOT_READY;
    }
#endif
    if (!mhi2q_probe_manager())
        return MHI2Q_RBR_NOT_READY;
#ifndef MHI2Q_BRIDGE_PROPERTY_HANDOFF
    if (control_server("stopServer") != 0)
        return MHI2Q_RBR_NOT_READY;
#endif
    for (attempt = 0; attempt < 10; ++attempt) {
        int fd = connect_airplay();
        if (fd < 0) {
            active = 0;
            server_stopped = 1;
            return MHI2Q_RBR_OK;
        }
        close(fd);
        usleep(100000);
    }
    return MHI2Q_RBR_BUSY;
}

/* Stop new copy-property entries and drain an existing callback before the
 * owner can destroy dio_manager. Unlike STOP, this is generation-independent. */
static uint8_t bridge_quiesce(void)
{
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
    int attempt;
    int fd;
    {
        int stopped = stop_server_if_idle();
        bridge_log("quiesce idle result", stopped);
        if (stopped > 0) {
            bridge_log("active media defers quiesce", 1);
            return MHI2Q_RBR_BUSY;
        }
        if (stopped < 0) return MHI2Q_RBR_NOT_READY;
    }
    if (!mhi2q_property_handoff_quiesce())
        return MHI2Q_RBR_NOT_READY;
    fd = connect_airplay();
    if (fd >= 0) close(fd);
    for (attempt = 0; attempt < 10; ++attempt) {
        int fd = connect_airplay();
        if (fd < 0) {
            active = 0;
            server_stopped = 1;
            return MHI2Q_RBR_OK;
        }
        close(fd);
        usleep(100000);
    }
    return MHI2Q_RBR_BUSY;
#else
    return MHI2Q_RBR_NOT_READY;
#endif
}

/* Wired takeover only: the receiver's own session-death command closes
 * active HTTP/media connections before the ordinary guarded QUIESCE. This
 * request is never issued by the wireless daemon or supervisor. */
static uint8_t bridge_wired_drain(void)
{
#ifdef MHI2Q_BRIDGE_PROPERTY_HANDOFF
#ifndef MHI2Q_BRIDGE_QEMU_UNGUARDED_DRAIN
    const char *lock_path = getenv("MHI2Q_DIO_WIRED_PRIORITY_LOCK");
    struct flock lock;
    int lock_fd;
    int held;
    if (!lock_path || !*lock_path)
        lock_path = "/mnt/persist/.mhi2q_dio_wired_priority.lock";
    lock_fd = open(lock_path, O_RDWR);
    if (lock_fd < 0)
        return MHI2Q_RBR_NOT_READY;
    memset(&lock, 0, sizeof(lock));
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    held = fcntl(lock_fd, F_GETLK, &lock) == 0 &&
        lock.l_type == F_WRLCK && lock.l_pid > 0;
    close(lock_fd);
    if (!held)
        return MHI2Q_RBR_NOT_READY;
#endif
    if (!mhi2q_probe_manager() || !mhi2q_probe_airplay_server())
        return MHI2Q_RBR_NOT_READY;
    bridge_log("wired drain begin", 1);
#ifdef MHI2Q_BRIDGE_STOCK210
    if (hook_control(mhi2q_probe_airplay_server(),3) != 0)
#else
    if (control_server("sessionDied") != 0)
#endif
        return MHI2Q_RBR_NOT_READY;
    bridge_log("wired drain result", MHI2Q_RBR_OK);
    return MHI2Q_RBR_OK;
#else
    return MHI2Q_RBR_NOT_READY;
#endif
}

static int read_full(int fd, uint8_t *bytes, size_t length)
{
    size_t used = 0;
    while (used < length) {
        ssize_t step = read(fd, bytes + used, length - used);
        if (step < 0 && errno == EINTR) continue;
        if (step <= 0)
            return -1;
        used += (size_t)step;
    }
    return 0;
}

static int write_full(int fd, const uint8_t *bytes, size_t length)
{
    size_t used = 0;
    while (used < length) {
        ssize_t n = write(fd, bytes + used, length - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        used += (size_t)n;
    }
    return 0;
}

/* Caller holds bridge_lock. Do not create a new identity in response to IPC:
 * the receiver must already have published its persisted identity. */
static uint8_t bridge_identity(uint32_t generation, char id[MHI2Q_RBR_ID_MAX + 1])
{
    typedef int (*hook_copy_fn)(char *, size_t, uint8_t *);
    hook_copy_fn copy = (hook_copy_fn)dlsym(RTLD_DEFAULT, "carplay_wireless_copy_pairing_identity");
    char advertised[MHI2Q_RBR_ID_MAX + 1];
    size_t i, n;
    if (!active || !generation || active_generation != generation || !hook_ready()) return MHI2Q_RBR_NOT_READY;
    pthread_mutex_lock(&identity_lock);
    memcpy(advertised, advertised_pairing_id, sizeof(advertised));
    pthread_mutex_unlock(&identity_lock);
    if (copy) {
        if (copy(id, MHI2Q_RBR_ID_MAX + 1, NULL)) return MHI2Q_RBR_NOT_READY;
        /* In the finished stock210 integration Bonjour must agree too. */
        if (!advertised[0] || strcmp(id, advertised)) return MHI2Q_RBR_NOT_READY;
    } else {
        /* The experimental 280 receiver has already obtained pi from its
         * private keychain before DNSServiceRegister. Copy that exact value. */
        memcpy(id, advertised, sizeof(advertised));
    }
    n = strlen(id);
    if (!n || n > MHI2Q_RBR_ID_MAX) return MHI2Q_RBR_NOT_READY;
    for (i = 0; i < n; ++i)
        if ((uint8_t)id[i] <= 0x20 || (uint8_t)id[i] == 0x7f) return MHI2Q_RBR_NOT_READY;
    return MHI2Q_RBR_OK;
}

static void serve_client(int fd)
{
    uint8_t request[MHI2Q_RBR_REQUEST_SIZE];
    uint8_t response[MHI2Q_RBR_RESPONSE_SIZE];
    char identity[MHI2Q_RBR_ID_MAX + 1] = {0};
    mhi2q_rbr_request decoded;
    uint8_t status = MHI2Q_RBR_BAD_REQUEST;
    struct timeval timeout = { 7, 0 };
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (read_full(fd, request, sizeof(request)) == 0 &&
        mhi2q_rbr_decode_request(request, &decoded) == 0) {
        if (decoded.airplay_port == AIRPLAY_PORT) {
            if (decoded.operation == MHI2Q_RBR_WIRED_DRAIN) {
                /* sessionDied dispatches synchronously on the SDK HTTP
                 * queue; do not hold bridge_lock across its callbacks. */
                status = decoded.generation == 0 ? bridge_wired_drain() :
                    MHI2Q_RBR_BAD_REQUEST;
            } else {
                pthread_mutex_lock(&bridge_lock);
                if (decoded.operation == MHI2Q_RBR_START)
                    status = bridge_start(decoded.generation);
                else if (decoded.operation == MHI2Q_RBR_STOP)
                    status = bridge_stop(decoded.generation);
                else if (decoded.operation == MHI2Q_RBR_IDENTITY)
                    status = bridge_identity(decoded.generation, identity);
                else
                    status = bridge_quiesce();
                pthread_mutex_unlock(&bridge_lock);
            }
        }
    }
    mhi2q_rbr_encode_response(response, status);
    if (write_full(fd, response, sizeof(response)) == 0 &&
        status == MHI2Q_RBR_OK && identity[0]) {
        size_t n = strlen(identity);
        uint8_t size[2] = {(uint8_t)n, (uint8_t)(n >> 8)};
        if (write_full(fd, size, sizeof(size)) == 0)
            (void)write_full(fd, (const uint8_t *)identity, n);
    }
}

static void *serve_loop(void *ignored)
{
    struct sockaddr_in address;
    int listener;
    int one = 1;
    int flags;
    (void)ignored;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0)
        return NULL;
    flags = fcntl(listener, F_GETFD);
    if (flags < 0 || fcntl(listener, F_SETFD, flags | FD_CLOEXEC) != 0 ||
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0) {
        bridge_log("listener setup errno", errno);
        close(listener);
        return NULL;
    }
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(BRIDGE_PORT);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 2) != 0) {
        bridge_log("listen errno", errno);
        close(listener);
        return NULL;
    }
    bridge_log("listening", BRIDGE_PORT);
    for (;;) {
        int client = accept(listener, NULL, NULL);
        if (client >= 0) {
            int client_flags = fcntl(client, F_GETFD);
            if (client_flags >= 0 &&
                fcntl(client, F_SETFD, client_flags | FD_CLOEXEC) == 0)
                serve_client(client);
            close(client);
        } else if (errno != EINTR) {
            break;
        }
    }
    close(listener);
    return NULL;
}

#ifdef MHI2Q_BRIDGE_PRESTART
static void *prestart_loop(void *ignored)
{
    int attempt;
    (void)ignored;
#ifdef MHI2Q_BRIDGE_QEMU_REPEAT_MEDIA
    sleep(1);
#else
    sleep(8);
#endif
    for (attempt = 0; attempt < 30; ++attempt) {
        void *manager = mhi2q_probe_manager();
        if (!manager)
            return NULL;
        if (if_nametoindex(MHI2Q_BRIDGE_INTERFACE_NAME) != 0) {
            pthread_mutex_lock(&bridge_lock);
            if (!event_posted && !mhi2q_probe_airplay_server() &&
                post_first_start(manager) == 0) {
                pthread_mutex_unlock(&bridge_lock);
                bridge_log("prestarted stock receiver", 1);
#ifdef MHI2Q_BRIDGE_QEMU_ACTIVE_SESSION
                for (attempt = 0; attempt < 60; ++attempt) {
                    if (mhi2q_property_handoff_ready()) {
                        int fd = connect_airplay();
                        if (fd >= 0) {
                            int marker;
                            close(fd);
                            marker = open("/dev/shmem/independent_srp_ready",
                                          O_WRONLY | O_CREAT | O_TRUNC, 0600);
                            if (marker >= 0) {
                                close(marker);
                                bridge_log("active session fixture ready", 1);
                            }
                            break;
                        }
                    }
                    usleep(100000);
                }
#endif
                return NULL;
            }
            pthread_mutex_unlock(&bridge_lock);
            if (event_posted || mhi2q_probe_airplay_server())
                return NULL;
        }
        sleep(1);
    }
    bridge_log("prestart interface unavailable", 1);
    return NULL;
}
#endif

#ifdef MHI2Q_BRIDGE_QEMU_SELFTEST
#ifdef MHI2Q_BRIDGE_QEMU_PROPERTY_RACE
struct property_race_state {
    volatile unsigned int stop;
    volatile unsigned int attempts;
    volatile unsigned int successes;
};

static void *property_race_loop(void *opaque)
{
    struct property_race_state *state = opaque;
    while (!__sync_val_compare_and_swap(&state->stop, 0, 0) &&
           __sync_val_compare_and_swap(&state->attempts, 0, 0) < 40) {
        __sync_add_and_fetch(&state->attempts, 1);
        if (probe_info())
            __sync_add_and_fetch(&state->successes, 1);
    }
    return NULL;
}
#endif

static int selftest_request(uint8_t operation, uint32_t generation,
                            uint16_t airplay_port)
{
    struct sockaddr_in address;
    uint8_t request[MHI2Q_RBR_REQUEST_SIZE] = {
        'C', 'P', 'R', 'X', MHI2Q_RBR_VERSION, 0, 0, 0,
        0, 0, 0, 0, AIRPLAY_PORT & 255, AIRPLAY_PORT >> 8, 0, 0
    };
    uint8_t response[MHI2Q_RBR_RESPONSE_SIZE];
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int result = -1;
    if (fd < 0)
        return -1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(BRIDGE_PORT);
    request[5] = operation;
    request[8] = (uint8_t)generation;
    request[9] = (uint8_t)(generation >> 8);
    request[10] = (uint8_t)(generation >> 16);
    request[11] = (uint8_t)(generation >> 24);
    request[12] = (uint8_t)airplay_port;
    request[13] = (uint8_t)(airplay_port >> 8);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0 &&
        write(fd, request, sizeof(request)) == sizeof(request) &&
        read_full(fd, response, sizeof(response)) == 0 &&
        memcmp(response, "CPRA", 4) == 0)
        result = response[5];
    close(fd);
    return result;
}

static void *selftest_loop(void *ignored)
{
    int status = -1;
    int attempt;
    (void)ignored;
    sleep(20);
    for (attempt = 0; attempt < 3; ++attempt) {
        status = selftest_request(MHI2Q_RBR_START, 1, AIRPLAY_PORT);
        if (status == MHI2Q_RBR_OK)
            break;
        sleep(1);
    }
    bridge_log("START generation 1", status);
    bridge_log("START generation 1 attempts", attempt + 1);
    bridge_log("wrong AirPlay port rejected",
               selftest_request(MHI2Q_RBR_START, 1,
                                AIRPLAY_PORT == 5000 ? 7000 : 5000));
    bridge_log("duplicate START generation 1",
               selftest_request(MHI2Q_RBR_START, 1, AIRPLAY_PORT));
    bridge_log("conflicting START generation 2",
               selftest_request(MHI2Q_RBR_START, 2, AIRPLAY_PORT));
    bridge_log("wrong STOP generation 2",
               selftest_request(MHI2Q_RBR_STOP, 2, AIRPLAY_PORT));
    bridge_log("STOP generation 1", selftest_request(MHI2Q_RBR_STOP, 1, AIRPLAY_PORT));
    bridge_log("START generation 2", selftest_request(MHI2Q_RBR_START, 2, AIRPLAY_PORT));
#ifdef MHI2Q_BRIDGE_QEMU_PROPERTY_RACE
    {
        struct property_race_state race = {0};
        pthread_t racer;
        int started;
        mhi2q_property_handoff_qemu_arm(1);
        started = pthread_create(&racer, NULL, property_race_loop, &race) == 0;
        for (attempt = 0; started && attempt < 200; ++attempt) {
            if (mhi2q_property_handoff_qemu_inflight())
                break;
            usleep(1000);
        }
        bridge_log("QEMU property race callback active",
                   (int)mhi2q_property_handoff_qemu_inflight());
#ifdef MHI2Q_BRIDGE_QEMU_QUIESCE
        status = selftest_request(MHI2Q_RBR_QUIESCE, 2, AIRPLAY_PORT);
        bridge_log("QEMU property race QUIESCE", status);
        bridge_log("QEMU START after QUIESCE",
                   selftest_request(MHI2Q_RBR_START, 3, AIRPLAY_PORT));
#else
        status = selftest_request(MHI2Q_RBR_STOP, 2, AIRPLAY_PORT);
        bridge_log("QEMU property race first STOP", status);
#endif
#ifdef MHI2Q_BRIDGE_QEMU_FORCE_BUSY_TEARDOWN
        if (status == MHI2Q_RBR_BUSY) {
            void *manager = mhi2q_probe_manager();
            if (manager) {
                bridge_log("QEMU forcing teardown after BUSY", 1);
                ((void (*)(void *))0x00154ad8u)(manager);
            }
        }
#endif
        __sync_lock_test_and_set(&race.stop, 1);
        mhi2q_property_handoff_qemu_arm(0);
        if (started)
            pthread_join(racer, NULL);
#ifndef MHI2Q_BRIDGE_QEMU_QUIESCE
        for (attempt = 0; status == MHI2Q_RBR_BUSY && attempt < 10; ++attempt) {
            usleep(100000);
            status = selftest_request(MHI2Q_RBR_STOP, 2, AIRPLAY_PORT);
        }
        bridge_log("STOP generation 2", status);
#endif
#ifdef MHI2Q_BRIDGE_QEMU_TEARDOWN
        if (status == MHI2Q_RBR_OK) {
            void *manager = mhi2q_probe_manager();
            if (manager) {
                bridge_log("QEMU invoking stock thread teardown", 1);
                ((void (*)(void *))0x00154ad8u)(manager);
            }
        }
#endif
        bridge_log("QEMU property race attempts", (int)race.attempts);
        bridge_log("QEMU property race successes", (int)race.successes);
    }
#else
    bridge_log("STOP generation 2", selftest_request(MHI2Q_RBR_STOP, 2, AIRPLAY_PORT));
#ifdef MHI2Q_BRIDGE_QEMU_TEARDOWN
    {
        void *manager = mhi2q_probe_manager();
        if (manager) {
            bridge_log("QEMU invoking stock thread teardown", 1);
            ((void (*)(void *))0x00154ad8u)(manager);
        }
    }
#endif
#endif
    return NULL;
}
#endif

void mhi2q_bridge_on_manager(void *manager)
{
    pthread_t worker;
    (void)manager;
    /* SI's wired child carries the same preload chain. Its AirPlay lifecycle
     * belongs to USB/Cinemo; do not prestart or listen for wireless START. */
    if (wired_owner())
        return;
    if (!__sync_bool_compare_and_swap(&listener_started, 0, 1))
        return;
    if (pthread_create(&worker, NULL, serve_loop, NULL) == 0)
        pthread_detach(worker);
    else {
        __sync_lock_test_and_set(&listener_started, 0);
        return;
    }
#ifdef MHI2Q_BRIDGE_PRESTART
    if (pthread_create(&worker, NULL, prestart_loop, NULL) == 0)
        pthread_detach(worker);
#endif
#ifdef MHI2Q_BRIDGE_QEMU_SELFTEST
    if (pthread_create(&worker, NULL, selftest_loop, NULL) == 0)
        pthread_detach(worker);
#endif
}

void mhi2q_bridge_on_manager_release(void *manager)
{
    typedef void (*release_fn)(void *);
    release_fn release = (release_fn)dlsym(RTLD_DEFAULT, "CFRelease");
    (void)manager;
    pthread_mutex_lock(&bridge_lock);
    active = 0;
    server_stopped = 0;
    event_posted = 0;
    mhi2q_probe_record_airplay_server(NULL);
    if (retained_server && release) {
        release(retained_server);
        bridge_log("retained server released", 1);
    }
    retained_server = NULL;
    pthread_mutex_unlock(&bridge_lock);
}
