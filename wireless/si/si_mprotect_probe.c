/* QNX 6.5 mprotect-on-text probe: can a process make its own .text writable
 * and execute a self-patched branch? Prints MPROT:OK / MPROT:FAIL <errno>. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

__attribute__((noinline)) static int target(int x) {
    return x + 1; /* placeholder; body gets overwritten by the probe */
}

int main(void) {
    unsigned long page = (unsigned long)(uintptr_t)target & ~0xFFFUL;
    unsigned char *body = (unsigned char *)(uintptr_t)target;
    unsigned char saved[16];
    memcpy(saved, body, 16);

    if (mprotect((void *)page, 0x1000,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        printf("MPROT:FAIL rw mprotect errno=%d\n", errno);
        return 1;
    }
    /* mov r0, #0x2a; bx lr  (ARM) — overwrite the body */
    const unsigned char patch[8] = {0x2a, 0x00, 0xa0, 0xe3, 0x1e, 0xff, 0x2f, 0xe1};
    memcpy(body, patch, 8);
    int r = target(1);
    memcpy(body, saved, 16);
    mprotect((void *)page, 0x1000, PROT_READ | PROT_EXEC);
    if (r == 0x2a) {
        printf("MPROT:OK self-patch executed, target()=%d\n", r);
        return 0;
    }
    printf("MPROT:FAIL executed old body r=%d\n", r);
    return 1;
}
