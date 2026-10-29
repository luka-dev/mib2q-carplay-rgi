#include "../hook/altscreen/command_policy.h"
#include <assert.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    alt_command_policy_t q;
    alt_command_item_t retired[32], item = {0}, replaced;
    int has;
    alt_zoom_dispatch_t selected = {0};
    int has_selected = 0, invalidated = 0;
    alt_command_init(&q);
    alt_command_generation(&q, 1, 1, retired);
    uint64_t now = 1000000;
    for (size_t i = 0; i + 2 < size && i < 512; i += 3) {
        now += (uint64_t)data[i] * 1000u;
        if (has_selected && !alt_zoom_dispatch_valid(&q, &selected, now))
            invalidated = 1;
        switch (data[i + 1] % 7u) {
        case 0:
            alt_zoom_offer(&q, q.generation, (int)(int8_t)data[i + 2], now);
            break;
        case 1:
            item.gen = q.generation;
            item.key = data[i + 2] % 5u;
            alt_command_offer(&q, item, &replaced, &has);
            break;
        case 2: {
            if (has_selected)
                break; /* the only worker is still preparing it */
            int choice = alt_command_select(&q, now);
            if (choice >= 0) {
                assert((unsigned)choice < q.count);
                alt_command_take(&q, (unsigned)choice, &item);
            } else if (choice == ALT_COMMAND_ZOOM) {
                assert(alt_zoom_ready(&q, now));
                alt_zoom_take(&q, now, &selected);
                has_selected = 1;
                invalidated = 0;
                assert(alt_zoom_dispatch_valid(&q, &selected, now));
            }
            break;
        }
        case 3:
            alt_command_generation(&q, q.generation + 1, 1, retired);
            break;
        case 4:
            alt_command_generation(&q, q.generation, 0, retired);
            break;
        case 5:
            if (has_selected) {
                assert(!invalidated || !alt_zoom_dispatch_valid(&q, &selected, now));
                has_selected = 0; /* canceled or committed, no further revival */
            }
            break;
        default:
            alt_zoom_expire(&q, now);
            break;
        }
        if (has_selected) {
            int valid = alt_zoom_dispatch_valid(&q, &selected, now);
            assert(!invalidated || !valid); /* invalid stamps never revive */
            if (!valid)
                invalidated = 1;
        }
        assert(q.count <= ALT_COMMAND_CAPACITY && q.zoom_steps <= 8 && q.zoom_steps >= -8);
        for (unsigned j = 0; j < q.count; ++j) {
            assert(q.items[j].gen == q.generation);
            if (alt_command_replaceable(q.items[j].key))
                for (unsigned k = j + 1; k < q.count; ++k)
                    assert(q.items[j].key != q.items[k].key);
        }
        assert(alt_zoom_offer(&q, q.generation - 1, 1, now) < 0);
    }
    return 0;
}
