/* QEMU TCG plugin: retired guest instructions per function.
 *
 * Symbols come from a "start size name" table (nm -S output of the guest ELF)
 * passed as symbols=/path.  Every translation block is attributed to the
 * function containing its first instruction, which is exact for the
 * non-inlined leaf functions this is used on.
 *
 * Copyright (c) 2026 LuKa (@LuKa_dev)
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <qemu-plugin.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

struct symbol {
    uint64_t start;
    uint64_t end;
    char *name;
    uint64_t instructions;
};

static struct symbol *g_symbols;
static size_t g_count;
static uint64_t g_unknown;
/* focus=<symbol>: per-address retired counts inside one function, so the hot
 * source lines can be recovered with addr2line. */
static const char *g_focus;
static uint64_t g_focus_start, g_focus_end;
static uint64_t *g_focus_counts;

static int compare_symbol(const void *a, const void *b)
{
    uint64_t x = ((const struct symbol *)a)->start;
    uint64_t y = ((const struct symbol *)b)->start;

    return x < y ? -1 : (x > y ? 1 : 0);
}

static int compare_instructions(const void *a, const void *b)
{
    uint64_t x = ((const struct symbol *)a)->instructions;
    uint64_t y = ((const struct symbol *)b)->instructions;

    return x > y ? -1 : (x < y ? 1 : 0);
}

static struct symbol *lookup(uint64_t address)
{
    size_t low = 0, high = g_count;

    while (low < high) {
        size_t mid = low + (high - low) / 2;

        if (address < g_symbols[mid].start) high = mid;
        else if (address >= g_symbols[mid].end) low = mid + 1;
        else return &g_symbols[mid];
    }
    return NULL;
}

/* One counter per (tb, symbol): the instruction count is known at translation
 * time, so the execution callback only adds it. */
struct block {
    struct symbol *symbol;
    uint64_t instructions;
    uint64_t first_focus;   /* first focused instruction address, 0 = none */
    unsigned focus_count;   /* consecutive focused instructions from there */
};

static void vcpu_block_exec(unsigned int cpu_index, void *userdata)
{
    struct block *block = userdata;

    (void)cpu_index;
    unsigned i;

    if (block->symbol) block->symbol->instructions += block->instructions;
    else g_unknown += block->instructions;
    for (i = 0; i < block->focus_count; ++i)
        ++g_focus_counts[(block->first_focus + 4u * i - g_focus_start) / 4u];
}

/* QEMU_PLUGIN_VERSION 7 (QEMU 11): the translate and atexit callbacks lost the plugin id and the
 * registrations take userdata; the older signatures are adapted below. */
static void vcpu_tb_trans(struct qemu_plugin_tb *tb, void *udata)
{
    struct block *block = g_new0(struct block, 1);

    (void)udata;
    block->instructions = qemu_plugin_tb_n_insns(tb);
    block->symbol = lookup(qemu_plugin_tb_vaddr(tb));
    if (g_focus_counts) {
        size_t i;

        for (i = 0; i < block->instructions; ++i) {
            uint64_t address =
                qemu_plugin_insn_vaddr(qemu_plugin_tb_get_insn(tb, i));

            if (address < g_focus_start || address >= g_focus_end) continue;
            if (!block->focus_count) block->first_focus = address;
            ++block->focus_count;
        }
    }
    qemu_plugin_register_vcpu_tb_exec_cb(tb, vcpu_block_exec,
                                         QEMU_PLUGIN_CB_NO_REGS, block);
}

static void plugin_exit(void *p)
{
    GString *report = g_string_new(NULL);
    uint64_t total = g_unknown;
    size_t i;

    (void)p;
    for (i = 0; i < g_count; ++i) total += g_symbols[i].instructions;
    qsort(g_symbols, g_count, sizeof(*g_symbols), compare_instructions);
    g_string_append_printf(report,
        "metric=instructions_per_function total=%" PRIu64 " unattributed=%" PRIu64 "\n",
        total, g_unknown);
    for (i = 0; i < g_count && i < 40; ++i) {
        if (!g_symbols[i].instructions) break;
        g_string_append_printf(report, "  %10" PRIu64 "  %6.2f%%  %s\n",
            g_symbols[i].instructions,
            total ? 100.0 * (double)g_symbols[i].instructions / (double)total : 0.0,
            g_symbols[i].name);
    }
    if (g_focus_counts) {
        uint64_t address;

        g_string_append_printf(report, "metric=instructions_per_address focus=%s\n",
                               g_focus);
        for (address = g_focus_start; address < g_focus_end; address += 4) {
            uint64_t n = g_focus_counts[(address - g_focus_start) / 4u];

            if (n) g_string_append_printf(report, "  0x%08" PRIx64 " %" PRIu64 "\n",
                                          address, n);
        }
    }
    qemu_plugin_outs(report->str);
    g_string_free(report, TRUE);
}

/* nm -S: "<addr> <size> <type> <name>", sizes and addresses in hex. */
static int load_symbols(const char *path)
{
    char line[512];
    FILE *file = fopen(path, "r");
    size_t capacity = 0;

    if (!file) return 0;
    while (fgets(line, sizeof(line), file)) {
        unsigned long long start, size;
        char type, name[256];

        if (sscanf(line, "%llx %llx %c %255s", &start, &size, &type, name) != 4)
            continue;
        if (!size || (type != 't' && type != 'T')) continue;
        if (g_count == capacity) {
            capacity = capacity ? capacity * 2 : 256;
            g_symbols = g_renew(struct symbol, g_symbols, capacity);
        }
        g_symbols[g_count].start = start & ~1ull; /* Thumb bit */
        g_symbols[g_count].end = (start & ~1ull) + size;
        g_symbols[g_count].name = g_strdup(name);
        g_symbols[g_count].instructions = 0;
        ++g_count;
    }
    fclose(file);
    qsort(g_symbols, g_count, sizeof(*g_symbols), compare_symbol);
    return g_count > 0;
}

#if QEMU_PLUGIN_VERSION < 7
static void vcpu_tb_trans_v1(qemu_plugin_id_t id, struct qemu_plugin_tb *tb) { (void)id; vcpu_tb_trans(tb, NULL); }
static void plugin_exit_v1(qemu_plugin_id_t id, void *p) { (void)id; plugin_exit(p); }
#endif

QEMU_PLUGIN_EXPORT int qemu_plugin_install(qemu_plugin_id_t id,
                                           const qemu_info_t *info,
                                           int argc, char **argv)
{
    const char *symbols = NULL;
    int i;

    (void)info;
    for (i = 0; i < argc; ++i) {
        if (!strncmp(argv[i], "symbols=", 8)) symbols = argv[i] + 8;
        else if (!strncmp(argv[i], "focus=", 6)) g_focus = argv[i] + 6;
    }
    if (!symbols || !load_symbols(symbols)) {
        fprintf(stderr, "fnprof: symbols=<nm -S output> is required\n");
        return -1;
    }
    if (g_focus) {
        size_t i;

        for (i = 0; i < g_count; ++i) {
            if (strcmp(g_symbols[i].name, g_focus)) continue;
            g_focus_start = g_symbols[i].start;
            g_focus_end = g_symbols[i].end;
            g_focus_counts = g_new0(uint64_t,
                                    (g_focus_end - g_focus_start) / 4u + 1u);
            break;
        }
        if (!g_focus_counts) {
            fprintf(stderr, "fnprof: focus symbol %s not found\n", g_focus);
            return -1;
        }
    }
#if QEMU_PLUGIN_VERSION >= 7
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans, NULL);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);
#else
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans_v1);
    qemu_plugin_register_atexit_cb(id, plugin_exit_v1, NULL);
#endif
    return 0;
}
