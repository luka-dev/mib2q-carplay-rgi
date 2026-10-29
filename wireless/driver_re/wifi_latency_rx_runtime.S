/* Guarded inline RX tail at driver VA47748. Root ELF builder redirects one
 * original instruction (ldrb r3,[r5,#36]) here; full RX wrapper owns PI mutex.
 * Preserve native scratch registers/result and eight-byte call alignment.
 * Native resume targets are driver-load-base relative, never absolute pointers.
 */
.syntax unified
.arm
.text
.global wl_rx_runtime_tail_trampoline
.type wl_rx_runtime_tail_trampoline,%function
wl_rx_runtime_tail_trampoline:
    sub sp,sp,#8
    push {r0-r4,r10,r12,lr}
    mov r0,r5
    mov r1,r6
    mov r2,r9
    bl wl_rx_runtime_tail
    cmp r0,#0
    ldreq r4,=0x4774c
    ldrne r4,=0x47790
    bl wl_native_base
    add r0,r0,r4
    str r0,[sp,#36]
    pop {r0-r4,r10,r12,lr}
    add sp,sp,#4
    ldrb r3,[r5,#36]
    ldmia sp!,{pc}
.size wl_rx_runtime_tail_trampoline,.-wl_rx_runtime_tail_trampoline
