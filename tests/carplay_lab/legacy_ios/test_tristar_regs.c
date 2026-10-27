/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "tristar-lab-regs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    LabTristarRegs state;
    lab_tristar_reset(&state);
    lab_tristar_write(&state, 0xff);
    lab_tristar_write(&state, 0x12);
    lab_tristar_write(&state, 0x34);
    lab_tristar_start_send(&state);
    lab_tristar_write(&state, 0xff);
    assert(lab_tristar_read(&state) == 0x12);
    assert(lab_tristar_read(&state) == 0x34);
    lab_tristar_start_send(&state);
    lab_tristar_write(&state, 0x5f);
    lab_tristar_write(&state, 0);
    lab_tristar_start_send(&state);
    lab_tristar_write(&state, 0x5d);
    assert(lab_tristar_read(&state) == 0);
    assert(lab_tristar_read(&state) == 0);
    assert(lab_tristar_read(&state) == 0x74);

    uint32_t random = 0x18a5351d;
    for (unsigned iteration = 0; iteration < 100000; iteration++) {
        random = random * 1664525u + 1013904223u;
        uint8_t address = (uint8_t)(random >> 24);
        uint8_t value = (uint8_t)(random >> 16);
        lab_tristar_start_send(&state);
        lab_tristar_write(&state, address);
        for (unsigned i = 0; i < 300; i++) {
            lab_tristar_write(&state, (uint8_t)(value + i));
        }
        assert(state.regs[0x5f] == 0x74);
        lab_tristar_start_send(&state);
        lab_tristar_write(&state, address);
        for (unsigned i = 0; i < 300; i++) {
            uint8_t reg = state.index;
            uint8_t expected = state.regs[reg];
            assert(lab_tristar_read(&state) == expected);
        }
    }
    lab_tristar_reset(&state);
    assert(state.index == 0 && state.address_pending);
    assert(state.regs[0] == 0 && state.regs[0xff] == 0);
    puts("Tristar boundary checks and 100000 register fuzz transactions passed");
    return 0;
}
