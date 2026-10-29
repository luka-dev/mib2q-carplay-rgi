/* Passive, bounded Maps camera snapshot in the owned research guest.
 * No task suspension, injection, remote calls, writes or preference changes. */
typedef unsigned int u32;
typedef unsigned long u64;
typedef long i64;
typedef unsigned long size_t;
typedef void *Object;
extern int printf(const char *, ...), fflush(void *), strcmp(const char *, const char *);
extern char *strstr(const char *, const char *);
extern int snprintf(char *, size_t, const char *, ...);
extern void *freopen(const char *, const char *, void *), *__stdoutp;
extern int getpagesize(void), atoi(const char *);
extern int getpid(void);
extern unsigned alarm(unsigned), sleep(unsigned), getuid(void);
extern void *dlopen(const char *, int), *dlsym(void *, const char *);
extern void *calloc(size_t, size_t);
extern void free(void *);
extern Object objc_getClass(const char *), class_getInstanceVariable(Object, const char *);
extern Object class_createInstance(Object, size_t);
extern Object class_getInstanceMethod(Object, Object), sel_registerName(const char *);
extern void *method_getImplementation(Object);
extern Object class_getSuperclass(Object);
extern const char *class_getName(Object);
extern Object *objc_copyClassList(u32 *);
extern long ivar_getOffset(Object);
extern const char *ivar_getTypeEncoding(Object), *class_getImageName(Object);
extern size_t class_getInstanceSize(Object);
extern u32 mach_task_self_;
extern int task_for_pid(u32, int, u32 *), mach_port_deallocate(u32, u32);
extern int mach_vm_region(u32, u64 *, u64 *, int, int *, u32 *, u32 *);
extern int mach_vm_page_query(u32, u64, int *, int *);
extern int mach_vm_read_overwrite(u32, u64, u64, u64, u64 *);

enum { MAX_CLASS = 48 };
static const char *classes[MAX_CLASS] = {"VKNavCameraController", "VKCameraController", "VKDebugSettings", "VKMapView", "VKScreenCanvas"};
static unsigned nclass = 5;
static const char *const fields[] = {"_requestedDisplayRate", "_baseDisplayRate", "_maxDisplayRate",
    "_enableDynamicFrameRate", "_previousUpdateTime", "_lastSARUpdateTime", "_navDisplayRate",
    "_needsUpdate", "_isDetached", "_runLoopController", "_speedFrameRate", "_previousUpdateFrameStamp",
    "_carDisplayConfig", "_navCameraController", "_mapCameraController", "_cameraController",
    "_canvas", "_screenCanvas", "_inactive", "_displayRate", "_wantsTimerTick",
    "_navigationCameraController", "_navigationDisplayRate", "_carTargetFpsOverrideValue",
    "_applicationState", "_lowPowerModeEnabled", "_thermalLevel", "_thermalPressureLevel",
    "_navCameraMode", "_displayOptions", "_lastCanvasSize", "_consoleFps", "_consoleFpsUpdateTicker"};
enum { NFIELD = 33, MAX_FOUND = 24 };
typedef struct { Object cls; long offsets[NFIELD]; const char *types[NFIELD]; } Layout;
typedef struct { u64 address; unsigned layout; } Candidate;
static Layout layouts[MAX_CLASS];
static Candidate found[MAX_FOUND];
static unsigned nfound;
static u64 snapshot_seen[2][64];
static unsigned snapshot_count[2];
static u64 isa_mask, isa_magic_mask, isa_magic_value;
static int runloop_layout_verified;

static int read_bytes(u32 task, u64 address, void *out, u64 size)
{
    u64 actual = 0;
    return !mach_vm_read_overwrite(task, address, size, (u64)out, &actual) && actual == size;
}

static void check_runloop_layout(void)
{
    Object method = class_getInstanceMethod(objc_getClass("VKNavCameraController"),
        sel_registerName("_endUpdateWithTimestamp:frameStamp:"));
    u32 code[128];
    u64 imp;
    unsigned i;
    if (!method) return;
    imp = (u64)method_getImplementation(method);
    if (!read_bytes(mach_task_self_, imp, code, sizeof(code))) {
        /* PAC stripping for this guest's low user address range; read only,
         * never call the implementation or use a target-process selector. */
        imp &= 0xfffffffffUL;
        if (!read_bytes(mach_task_self_, imp, code, sizeof(code))) return;
    }
    printf("runtime_nav_end_update imp=0x%lx code512=", imp);
    for (i = 0; i < 128; ++i) printf("%08x", code[i]);
    printf("\n");
    for (i = 0; i + 1 < 128; ++i) {
        u32 branch = code[i + 1], callee[16];
        i64 offset;
        u64 target;
        unsigned j;
        if (code[i] != 0xf9000408U || (branch & 0xfc000000U) != 0x94000000U) continue;
        offset = (i64)((int)(branch << 6) >> 6) * 4;
        target = imp + (i + 1) * 4 + offset;
        if (!read_bytes(mach_task_self_, target, callee, sizeof(callee))) continue;
        if (callee[0] != 0xaa0003e8U || callee[1] != 0xf9400000U) continue;
        for (j = 0; j < 16; ++j) if (callee[j] == 0xf9400501U) runloop_layout_verified = 1;
        printf("runtime_runloop_update address=0x%lx code64=", target);
        for (j = 0; j < 16; ++j) printf("%08x", callee[j]);
        printf(" word1_requested_rate_verified=%d\n", runloop_layout_verified);
    }
}

static void snapshot(u32 task, Candidate candidate, unsigned sample)
{
    Layout *layout = &layouts[candidate.layout];
    u64 isa;
    unsigned i;
    for (i = 0; i < snapshot_count[sample]; ++i)
        if (snapshot_seen[sample][i] == candidate.address) return;
    if (snapshot_count[sample] >= 64) return;
    snapshot_seen[sample][snapshot_count[sample]++] = candidate.address;
    if (!read_bytes(task, candidate.address, &isa, 8) ||
        (isa & isa_mask) != ((u64)layout->cls & isa_mask)) {
        printf("candidate sample=%u address=0x%lx class_changed=1\n", sample, candidate.address);
        return;
    }
    printf("candidate sample=%u class=%s address=0x%lx", sample, classes[candidate.layout], candidate.address);
    for (i = 0; i < NFIELD; ++i) {
        union { unsigned char bytes[24]; u64 words[3]; double doubles[3]; int ints[6]; } data = {{0}};
        const char *type = layout->types[i];
        u64 address;
        if (layout->offsets[i] < 0 || !type) continue;
        address = candidate.address + (u64)layout->offsets[i];
        if (!read_bytes(task, address, &data, sizeof(data))) continue;
        if (!strcmp(type, "q") || !strcmp(type, "l")) printf(" %s=%ld", fields[i], (i64)data.words[0]);
        else if (!strcmp(type, "Q") || !strcmp(type, "L")) printf(" %s=%lu", fields[i], data.words[0]);
        else if (!strcmp(type, "i")) printf(" %s=%d", fields[i], data.ints[0]);
        else if (!strcmp(type, "B") || !strcmp(type, "c") || !strcmp(type, "C")) printf(" %s=%u", fields[i], data.bytes[0]);
        else if (!strcmp(type, "d")) {
            double value = data.doubles[0];
            if (value >= -1e12 && value <= 1e12) printf(" %s=%.6f", fields[i], value);
        } else if (!strcmp(type, "f")) {
            union { u32 word; float value; } value = {data.ints[0]};
            if (value.value >= -1e8 && value.value <= 1e8) printf(" %s=%.6f", fields[i], (double)value.value);
        } else if (type[0] == '@' || type[0] == '^') {
            printf(" %s=0x%lx", fields[i], data.words[0]);
            if (i == 9 && data.words[0]) {
                u64 words[2];
                if (read_bytes(task, data.words[0], words, sizeof(words)))
                    printf(" %s=%ld", runloop_layout_verified ? "runloop_requestedDisplayRate" : "runloop_word1_candidate", (i64)words[1]);
            }
        }
        else if (i == 10) {
            u64 begin = data.words[0], end = data.words[1];
            /* Vector<pair<double,int64_t>> layout is checked by its extent;
             * rates remain candidates until confirmed against executing code. */
            if (begin && !(begin & 7) && end >= begin && end - begin <= 16 * 16 && !((end - begin) % 16)) {
                u64 at;
                printf(" speed_table=[");
                for (at = begin; at < end; at += 16) {
                    union { u64 q[2]; double d[2]; } pair;
                    if (!read_bytes(task, at, &pair, 16)) break;
                    if (pair.d[0] >= -1e12 && pair.d[0] <= 1e12 && (i64)pair.q[1] >= -1 && pair.q[1] <= 240)
                        printf("%s%.6f:%ld", at == begin ? "" : ",", pair.d[0], (i64)pair.q[1]);
                }
                printf("]");
            }
        } else if ((i == 23 || i == 29) && type[0] == '{') {
            unsigned char bytes[216];
            unsigned k;
            unsigned extent = i == 29 ? sizeof(bytes) : 24;
            printf(" %s_pointer_candidate=0x%lx", fields[i], data.words[0]);
            if (data.words[0] && read_bytes(task, data.words[0], bytes, extent)) {
                printf(" raw%u=", extent);
                for (k = 0; k < extent; ++k) printf("%02x", bytes[k]);
            }
        } else if (i == 30 && strstr(type, "CGSize") &&
                   data.doubles[0] >= 0 && data.doubles[0] < 20000 &&
                   data.doubles[1] >= 0 && data.doubles[1] < 20000) {
            printf(" canvas_size=%.2fx%.2f", data.doubles[0], data.doubles[1]);
        }
    }
    printf("\n");
    /* Follow identity-checked links from the map to its actual canvas and
     * camera even when their allocation lies beyond the bounded heap scan. */
    if (candidate.layout == 3 || strstr(classes[candidate.layout], "Canvas")) {
        static const unsigned links[] = {14, 15, 16, 21};
        unsigned k;
        for (k = 0; k < sizeof(links) / sizeof(links[0]); ++k) {
            unsigned field = links[k], c;
            u64 pointer = 0, linked_isa = 0;
            if (layout->offsets[field] < 0 ||
                !read_bytes(task, candidate.address + (u64)layout->offsets[field], &pointer, 8) || !pointer ||
                !read_bytes(task, pointer, &linked_isa, 8)) continue;
            for (c = 0; c < nclass; ++c) if ((linked_isa & isa_mask) == ((u64)layouts[c].cls & isa_mask)) {
                printf("link sample=%u from=0x%lx field=%s to=0x%lx class=%s\n", sample,
                       candidate.address, fields[field], pointer, classes[c]);
                snapshot(task, (Candidate){pointer, c}, sample);
                break;
            }
        }
    }
}

int main(int argc, char **argv)
{
    u32 task = 0;
    u64 address = 0, read_total = 0;
    unsigned regions = 0, visited_pages = 0, i, j;
    unsigned page_size;
    unsigned char *page;
    u64 *mask;
    int rc;
    int self_test = argc == 3 && !strcmp(argv[1], "self");
    int target_pid = self_test ? getpid() : (argc == 3 ? atoi(argv[1]) : 0);
    char sample_path[1024];
    Object control = 0;
    Object *all_classes;
    u32 class_count = 0;
    if (getuid() || argc != 3 || target_pid <= 1) return 2;
    alarm(25);
    if (!dlopen("/System/Library/Frameworks/Foundation.framework/Foundation", 2) ||
        !dlopen("/System/Library/PrivateFrameworks/VectorKit.framework/VectorKit", 2)) return 3;
    mask = dlsym((void *)-2, "objc_debug_isa_class_mask");
    if (!mask || !*mask) { printf("isa_mask unavailable\n"); return 4; }
    isa_mask = *mask;
    mask = dlsym((void *)-2, "objc_debug_isa_magic_mask");
    if (mask) isa_magic_mask = *mask;
    mask = dlsym((void *)-2, "objc_debug_isa_magic_value");
    if (mask) isa_magic_value = *mask;
    if (self_test) {
        /* Allocate only a helper-local control object. No private singleton
         * selector is assumed to exist on this guest's exact framework build. */
        control = class_createInstance(objc_getClass("VKDebugSettings"), 0);
        if (!control) return 9;
        printf("self_control address=0x%lx isa=0x%lx magic_mask=0x%lx magic_value=0x%lx\n",
               (u64)control, *(u64 *)control, isa_magic_mask, isa_magic_value);
    }
    /* Browsing and following an annotation can use another camera subclass;
     * enumerate the loaded family rather than assuming active navigation. */
    all_classes = objc_copyClassList(&class_count);
    if (!all_classes || class_count > 100000) return 11;
    for (i = 0; i < class_count && nclass < MAX_CLASS; ++i) {
        Object ancestor = class_getSuperclass(all_classes[i]);
        unsigned depth = 0;
        const char *name = class_getName(all_classes[i]);
        const char *image = class_getImageName(all_classes[i]);
        if (strstr(name, "Canvas") && image && strstr(image, "/VectorKit.framework/")) {
            unsigned k;
            for (k = 0; k < nclass; ++k) if (!strcmp(classes[k], name)) break;
            if (k == nclass) classes[nclass++] = name;
            if (nclass == MAX_CLASS) break;
        }
        while (ancestor && depth++ < 32) {
            if (ancestor == objc_getClass("VKCameraController")) {
                const char *name = class_getName(all_classes[i]);
                unsigned k;
                for (k = 0; k < nclass; ++k) if (!strcmp(classes[k], name)) break;
                if (k == nclass) classes[nclass++] = name;
                break;
            }
            ancestor = class_getSuperclass(ancestor);
        }
    }
    free(all_classes);
    for (i = 0; i < nclass; ++i) {
        layouts[i].cls = objc_getClass(classes[i]);
        if (!layouts[i].cls) return 5;
        printf("layout class=%s ptr=0x%lx size=%lu", classes[i],
               (u64)layouts[i].cls, class_getInstanceSize(layouts[i].cls));
        for (j = 0; j < NFIELD; ++j) {
            Object ivar = class_getInstanceVariable(layouts[i].cls, fields[j]);
            layouts[i].offsets[j] = ivar ? ivar_getOffset(ivar) : -1;
            layouts[i].types[j] = ivar ? ivar_getTypeEncoding(ivar) : 0;
            if (ivar) printf(" %s@%ld:%.1s", fields[j], layouts[i].offsets[j], layouts[i].types[j]);
        }
        printf("\n");
    }
    check_runloop_layout();
    rc = task_for_pid(mach_task_self_, target_pid, &task);
    printf("target pid=%d task_for_pid=%d isa_mask=0x%lx\n", target_pid, rc, isa_mask);
    fflush(0);
    if (rc || !task) return 6;
    for (i = 0; i < nclass; ++i) {
        u64 remote[2];
        if (read_bytes(task, (u64)layouts[i].cls, remote, sizeof(remote))) {
            printf("class_mapping class=%s isa_matches=%d superclass_matches=%d\n", classes[i],
                   (remote[0] & isa_mask) == (*(u64 *)layouts[i].cls & isa_mask),
                   (remote[1] & isa_mask) == ((u64)class_getSuperclass(layouts[i].cls) & isa_mask));
        } else printf("class_mapping class=%s target_unmapped=1\n", classes[i]);
    }
    page_size = (unsigned)getpagesize();
    if (page_size < 4096 || page_size > 65536) return 7;
    page = calloc(1, page_size);
    if (!page) return 8;
    while (regions < 8192 && visited_pages < 65536 && read_total < 384UL * 1024 * 1024 && nfound < MAX_FOUND) {
        u64 size = 0, end, at;
        int info[64] = {0};
        u32 count = 64, object = 0;
        rc = mach_vm_region(task, &address, &size, 9, info, &count, &object);
        if (object) mach_port_deallocate(mach_task_self_, object);
        if (rc || !size || address + size < address) break;
        ++regions;
        end = address + size;
        /* Private writable non-executable regions only. Skip giant reserved
         * mappings and query residency before any read to avoid paging them in. */
        if ((info[0] & 7) == 3 && !info[3] && size <= 256UL * 1024 * 1024) {
            for (at = address; at < end && visited_pages < 65536 && read_total < 384UL * 1024 * 1024 && nfound < MAX_FOUND; at += page_size) {
                int disposition = 0, refs = 0;
                u64 length = end - at < page_size ? end - at : page_size;
                unsigned offset;
                ++visited_pages;
                if (mach_vm_page_query(task, at, &disposition, &refs) || !(disposition & 1)) continue;
                if (!read_bytes(task, at, page, length)) continue;
                read_total += length;
                for (offset = 0; offset + 8 <= length && nfound < MAX_FOUND; offset += 8) {
                    u64 raw = *(u64 *)(page + offset), value = raw & isa_mask;
                    if (!(raw & 1) || (isa_magic_mask && (raw & isa_magic_mask) != isa_magic_value)) continue;
                    for (i = 0; i < nclass; ++i) if (value == ((u64)layouts[i].cls & isa_mask)) {
                        unsigned k, same = 0;
                        for (k = 0; k < nfound; ++k) if (found[k].layout == i) ++same;
                        /* Keep room for navigation and map-view objects even
                         * when many inactive camera subclasses come first. */
                        if (same < ((i == 0 || i == 3 || i == 4) ? 6U : 1U))
                            found[nfound++] = (Candidate){at + offset, i};
                        break;
                    }
                }
            }
        }
        address = end;
    }
    printf("scan regions=%u resident_bytes=%lu queried_pages=%u candidates=%u bounded=1\n", regions, read_total, visited_pages, nfound);
    if (snprintf(sample_path, sizeof(sample_path), "%s/sample0.log", argv[2]) >= (int)sizeof(sample_path) ||
        !freopen(sample_path, "w", __stdoutp)) return 12;
    for (i = 0; i < nfound; ++i) snapshot(task, found[i], 0);
    fflush(0);
    sleep(2);
    if (snprintf(sample_path, sizeof(sample_path), "%s/sample1.log", argv[2]) >= (int)sizeof(sample_path) ||
        !freopen(sample_path, "w", __stdoutp)) return 12;
    for (i = 0; i < nfound; ++i) snapshot(task, found[i], 1);
    printf("MAPS_RATE_READ_ONLY_DONE candidates_are_not_proof_of_live_scene=1\n");
    if (self_test) {
        int matched = 0;
        for (i = 0; i < nfound; ++i) if (found[i].address == (u64)control) matched = 1;
        printf("MAPS_RATE_SELF_TEST_%s\n", matched ? "PASS" : "FAIL");
        if (!matched) return 10;
    }
    fflush(0);
    free(page);
    mach_port_deallocate(mach_task_self_, task);
    return 0;
}
