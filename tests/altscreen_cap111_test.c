/* Opt-in raw stream-111 capture through the real socket reader and decode
 * queue. Only the hardware sink is replaced. The capture must hold every
 * payload in arrival order (config included), never alter what reaches the
 * decoder, stay single-shot, respect the real file size cap and refuse a
 * symlinked target. Host builds must never arm it from /tmp. */
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
/* The unit's QNX 6.5 headers have no O_NOFOLLOW: build the renderer without it,
 * so the symlink cases below exercise the lstat/dev+ino path the car runs. */
#undef O_NOFOLLOW
#define main renderer_program_main
#include "../altscreen_render/main.c"
#undef main
#ifdef O_NOFOLLOW
#error "altscreen_cap111_test must build the renderer without O_NOFOLLOW (QNX 6.5)"
#endif

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_cond = PTHREAD_COND_INITIALIZER;
static uint8_t sunk[4096];
static size_t sunk_len;
static unsigned sunk_frames, sunk_ends;

int av_sink_open(void) { return 0; }
int av_sink_frame(const uint8_t *b, uint32_t n, uint64_t t) {
    (void)t;
    pthread_mutex_lock(&test_lock);
    assert(sunk_len + n <= sizeof(sunk));
    memcpy(sunk + sunk_len, b, n);
    sunk_len += n;
    ++sunk_frames;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
    return 0;
}
int av_sink_frame_ex(const uint8_t *b, uint32_t n, uint64_t t, const alt_frame_meta_t *m) {
    (void)m;
    return av_sink_frame(b, n, t);
}
void av_sink_session_end(void) {
    pthread_mutex_lock(&test_lock);
    ++sunk_ends;
    pthread_cond_broadcast(&test_cond);
    pthread_mutex_unlock(&test_lock);
}
int av_sink_loader_ready(void) { return 0; }
int av_sink_video_ready(void) { return 0; }
const char *av_sink_omx_stage(void) { return 0; }
void av_sink_fatal_exit(int code, const char *reason) {
    (void)code;
    (void)reason;
    assert(!"no watchdog death expected");
}
void av_sink_close(void) {}
void av_sink_set_layout(int a, int b, int c) {
    (void)a;
    (void)b;
    (void)c;
}
void altctl_start(int (*ready)(void)) { (void)ready; }
void altctl_stop(void) {}

static const uint8_t k_config[] = {0, 0, 0, 1, 0x67, 0x42, 0x00, 0x1f, 0, 0, 0, 1, 0x68, 0xce};
static const uint8_t k_idr[] = {0, 0, 0, 1, 0x65, 0x88, 0x84, 0x00, 0x10};
static const uint8_t k_delta[] = {0, 0, 0, 1, 0x41, 0x9a, 0x02};

static void put_record(int fd, const uint8_t *payload, uint32_t len, uint16_t flags,
                       uint64_t sequence) {
    uint8_t header[ALT_WIRE_HEADER_SIZE];
    alt_frame_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.generation = 7;
    meta.flags = flags;
    meta.sequence = sequence;
    meta.sender_pts = sequence ? (uint64_t)sequence << 32 : 0;
    alt_wire_encode(header, len, &meta);
    assert(write(fd, header, sizeof(header)) == (ssize_t)sizeof(header));
    assert(write(fd, payload, len) == (ssize_t)len);
}

/* One hook connection: v2 config, IDR and delta, then EOF. Returns once the
 * decode worker has delivered all three payloads and processed END. */
static void run_session(void) {
    int pair[2];
    uint32_t generation;
    size_t before;
    unsigned frames, ends;
    pthread_mutex_lock(&test_lock);
    before = sunk_len;
    frames = sunk_frames;
    ends = sunk_ends;
    pthread_mutex_unlock(&test_lock);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    put_record(pair[1], k_config, sizeof(k_config), ALT_WIRE_CONFIG, 0);
    put_record(pair[1], k_idr, sizeof(k_idr), ALT_WIRE_IDR, 1);
    put_record(pair[1], k_delta, sizeof(k_delta), 0, 2);
    assert(shutdown(pair[1], SHUT_WR) == 0);
    generation = decode_queue_begin(pair[0]);
    assert(generation);
    serve_client(pair[0], generation);
    decode_queue_end(generation);
    pthread_mutex_lock(&test_lock);
    while (sunk_frames < frames + 3 || sunk_ends < ends + 2)  /* BEGIN + END */
        pthread_cond_wait(&test_cond, &test_lock);
    /* The decoder saw exactly the wire payloads, capture or not. */
    assert(sunk_len == before + sizeof(k_config) + sizeof(k_idr) + sizeof(k_delta));
    assert(!memcmp(sunk + before, k_config, sizeof(k_config)));
    assert(!memcmp(sunk + before + sizeof(k_config), k_idr, sizeof(k_idr)));
    assert(!memcmp(sunk + before + sizeof(k_config) + sizeof(k_idr), k_delta, sizeof(k_delta)));
    pthread_mutex_unlock(&test_lock);
    close(pair[0]);
    close(pair[1]);
}

static size_t read_file(const char *path, uint8_t *out, size_t cap) {
    FILE *f = fopen(path, "rb");
    size_t n;
    if (!f) return (size_t)-1;
    n = fread(out, 1, cap, f);
    fclose(f);
    return n;
}

/* Renderer lines go to a file while sessions run; an assertion there still
 * names that file on the real stderr (write(2) is async-signal-safe). */
static int g_real_stderr = -1;
static char g_abort_hint[256];
static void on_abort(int sig) {
    if (g_real_stderr >= 0)
        (void)!write(g_real_stderr, g_abort_hint, strlen(g_abort_hint));
    signal(sig, SIG_DFL);
    raise(sig);
}

static void touch(const char *path) {
    int fd = open(path, O_CREAT | O_WRONLY, 0600);
    assert(fd >= 0);
    close(fd);
}

int main(void) {
    char dir[] = "/tmp/altscreen-cap111-test.XXXXXX";
    char marker[160], out[160], other[160], link_path[160], log_path[160];
    char absent[160], dangling[160], no_dir[160];
    uint8_t file[4096], expect[4096];
    size_t n, one = sizeof(k_config) + sizeof(k_idr) + sizeof(k_delta);
    struct stat st;
    int saved_stderr;
    char log_text[4096];

    alarm(20);
    signal(SIGPIPE, SIG_IGN);
    /* Host gate: tests that include main.c can never arm from a stray file. */
    assert(g_cap111_marker == NULL);
    assert(mkdtemp(dir));
    snprintf(marker, sizeof(marker), "%s/marker", dir);
    snprintf(out, sizeof(out), "%s/cap.h264", dir);
    snprintf(other, sizeof(other), "%s/other", dir);
    snprintf(link_path, sizeof(link_path), "%s/link.h264", dir);
    snprintf(log_path, sizeof(log_path), "%s/stderr.log", dir);
    snprintf(absent, sizeof(absent), "%s/absent", dir);
    snprintf(dangling, sizeof(dangling), "%s/dangling.h264", dir);
    snprintf(no_dir, sizeof(no_dir), "%s/missing/cap.h264", dir);
    saved_stderr = dup(STDERR_FILENO);
    assert(saved_stderr >= 0);
    g_real_stderr = saved_stderr;
    snprintf(g_abort_hint, sizeof(g_abort_hint),
             "altscreen_cap111_test: FAILED; assertion and renderer log in %s\n", log_path);
    signal(SIGABRT, on_abort);
    assert(freopen(log_path, "w", stderr));
    setvbuf(stderr, NULL, _IONBF, 0);
    assert(!decode_worker_start());

    /* Default host gate: even with the marker file present nothing is read. */
    touch(marker);
    g_cap111_path = out;
    run_session();
    assert(stat(out, &st) != 0 && stat(marker, &st) == 0);

    /* Marker path injected but absent: disarmed, no file. */
    unlink(marker);
    g_cap111_marker = marker;
    run_session();
    assert(stat(out, &st) != 0);

    /* Armed: single-shot, every payload in arrival order, config included. */
    touch(marker);
    run_session();
    assert(stat(marker, &st) != 0);                   /* consumed when it armed */
    assert(stat(out, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & 077) == 0);
    memcpy(expect, k_config, sizeof(k_config));
    memcpy(expect + sizeof(k_config), k_idr, sizeof(k_idr));
    memcpy(expect + sizeof(k_config) + sizeof(k_idr), k_delta, sizeof(k_delta));
    n = read_file(out, file, sizeof(file));
    assert(n == one && !memcmp(file, expect, one));

    /* The next connection without a new marker appends nothing. */
    run_session();
    assert(read_file(out, file, sizeof(file)) == one);

    /* Re-armed: appends (never truncates) and stops before the payload that
     * would cross the cap measured on the real file size. */
    touch(marker);
    g_cap111_limit = one + sizeof(k_config) + sizeof(k_idr);
    run_session();
    n = read_file(out, file, sizeof(file));
    assert(n == g_cap111_limit && !memcmp(file, expect, one));
    assert(!memcmp(file + one, k_config, sizeof(k_config)));
    assert(!memcmp(file + one + sizeof(k_config), k_idr, sizeof(k_idr)));

    /* Already at the cap: arms, closes at once, file unchanged. */
    touch(marker);
    run_session();
    assert(stat(marker, &st) != 0 && read_file(out, file, sizeof(file)) == g_cap111_limit);

    /* A symlinked target is refused; its destination stays untouched. */
    touch(other);
    assert(symlink(other, link_path) == 0);
    g_cap111_path = link_path;
    g_cap111_limit = 12u * 1024u * 1024u;
    touch(marker);
    run_session();
    assert(stat(marker, &st) != 0);                   /* consumed, nothing written */
    assert(stat(other, &st) == 0 && st.st_size == 0);

    /* A dangling symlink is refused too: O_CREAT never creates its target. */
    assert(symlink(absent, dangling) == 0);
    g_cap111_path = dangling;
    touch(marker);
    run_session();
    assert(stat(marker, &st) != 0 && lstat(absent, &st) != 0);

    /* An open failure (missing directory) only logs; the session is unchanged. */
    g_cap111_path = no_dir;
    touch(marker);
    run_session();
    assert(stat(marker, &st) != 0 && lstat(no_dir, &st) != 0);

    decode_worker_stop();
    fflush(stderr);
    assert(dup2(saved_stderr, STDERR_FILENO) == STDERR_FILENO);
    signal(SIGABRT, SIG_DFL);
    g_real_stderr = -1;
    close(saved_stderr);
    n = read_file(log_path, (uint8_t *)log_text, sizeof(log_text) - 1);
    assert(n != (size_t)-1);
    log_text[n] = '\0';
    {
        /* One line per capture, one conversion per argument. */
        char line[96];
        snprintf(line, sizeof(line), "altscreen_render: cap111 wrote %u bytes\n", (unsigned)one);
        assert(strstr(log_text, line));
        snprintf(line, sizeof(line), "altscreen_render: cap111 wrote %u bytes\n",
                 (unsigned)(sizeof(k_config) + sizeof(k_idr)));
        assert(strstr(log_text, line));
        assert(strstr(log_text, "altscreen_render: cap111 wrote 0 bytes\n"));
        assert(strstr(log_text, "altscreen_render: cap111 target is not a regular file; capture off\n"));
        assert(strstr(log_text, "altscreen_render: cap111 open FAILED errno="));
    }
    unlink(out);
    unlink(other);
    unlink(link_path);
    unlink(dangling);
    unlink(log_path);
    rmdir(dir);
    alarm(0);
    puts("altscreen_cap111_test: host gate, disarmed no-op, single-shot Annex-B capture, append + "
         "real-size cap, symlink refusal without O_NOFOLLOW, decoder input unchanged PASS");
    return 0;
}
