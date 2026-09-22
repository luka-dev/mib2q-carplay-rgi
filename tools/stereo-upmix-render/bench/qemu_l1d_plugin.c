/* Minimal QEMU TCG plugin: one L1 data cache, APQ8064/Krait geometry by default
 * (16 KiB, 4-way, 64-byte lines, LRU).  Counts guest data accesses and misses.
 * Not a timing model: it answers "does this layout change miss counts". */
#include <qemu-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static unsigned cfg_size = 16 * 1024, cfg_ways = 4, cfg_line = 64;
static unsigned n_sets, line_shift;
static uint64_t *tags;      /* [set][way], 0 = empty sentinel via valid[] */
static unsigned char *valid;
static unsigned *age;       /* LRU stamp */
static unsigned clock_tick;
static uint64_t reads, writes, read_miss, write_miss;

static unsigned ilog2(unsigned v) { unsigned r = 0; while (v > 1) { v >>= 1; ++r; } return r; }

static void access_line(uint64_t line, int is_store)
{
    unsigned set = (unsigned)(line % n_sets);
    uint64_t tag = line / n_sets;
    unsigned base = set * cfg_ways, w, victim = base, oldest;

    for (w = 0; w < cfg_ways; ++w) {
        if (valid[base + w] && tags[base + w] == tag) {
            age[base + w] = ++clock_tick;
            if (is_store) writes++; else reads++;
            return;
        }
    }
    if (is_store) { writes++; write_miss++; } else { reads++; read_miss++; }
    oldest = age[base];
    for (w = 0; w < cfg_ways; ++w) {
        if (!valid[base + w]) { victim = base + w; oldest = 0; break; }
        if (age[base + w] < oldest) { oldest = age[base + w]; victim = base + w; }
    }
    valid[victim] = 1; tags[victim] = tag; age[victim] = ++clock_tick;
}

static void vcpu_mem(unsigned int vcpu_index, qemu_plugin_meminfo_t info,
                     uint64_t vaddr, void *udata)
{
    unsigned size = 1u << qemu_plugin_mem_size_shift(info);
    uint64_t first = vaddr >> line_shift;
    uint64_t last = (vaddr + size - 1) >> line_shift;
    int is_store = qemu_plugin_mem_is_store(info);
    uint64_t l;

    (void)vcpu_index; (void)udata;
    for (l = first; l <= last; ++l) access_line(l, is_store);
}

static void tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n = qemu_plugin_tb_n_insns(tb), i;

    (void)id;
    for (i = 0; i < n; ++i)
        qemu_plugin_register_vcpu_mem_cb(qemu_plugin_tb_get_insn(tb, i), vcpu_mem,
                                         QEMU_PLUGIN_CB_NO_REGS, QEMU_PLUGIN_MEM_RW, NULL);
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    char line[512];
    uint64_t acc = reads + writes, miss = read_miss + write_miss;

    (void)id; (void)p;
    snprintf(line, sizeof(line),
        "metric=l1d_cache geometry=%uB/%uway/%uB_line accesses=%" PRIu64
        " misses=%" PRIu64 " miss_rate=%.5f reads=%" PRIu64 " read_misses=%" PRIu64
        " writes=%" PRIu64 " write_misses=%" PRIu64 "\n",
        cfg_size, cfg_ways, cfg_line, acc, miss,
        acc ? (double)miss / (double)acc : 0.0, reads, read_miss, writes, write_miss);
    qemu_plugin_outs(line);
}

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                                           int argc, char **argv)
{
    int i;

    (void)info;
    for (i = 0; i < argc; ++i) {
        if (!strncmp(argv[i], "size=", 5)) cfg_size = (unsigned)atoi(argv[i] + 5);
        else if (!strncmp(argv[i], "ways=", 5)) cfg_ways = (unsigned)atoi(argv[i] + 5);
        else if (!strncmp(argv[i], "line=", 5)) cfg_line = (unsigned)atoi(argv[i] + 5);
    }
    line_shift = ilog2(cfg_line);
    n_sets = cfg_size / (cfg_line * cfg_ways);
    tags = calloc((size_t)n_sets * cfg_ways, sizeof(*tags));
    valid = calloc((size_t)n_sets * cfg_ways, 1);
    age = calloc((size_t)n_sets * cfg_ways, sizeof(*age));
    if (!tags || !valid || !age) return -1;
    qemu_plugin_register_vcpu_tb_trans_cb(id, tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
    return 0;
}
