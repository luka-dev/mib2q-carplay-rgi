/* MU1316 ARM EABI seam. Preserve 8-byte stack alignment and all callee-saved
 * registers. r6 holds remoteDisplayId only at the verified caller PC. */
.syntax unified
.arm
.text
.align 2
.global _ZN4util10TimerGroup10startTimerEj
.type _ZN4util10TimerGroup10startTimerEj, %function
.hidden vc_start_guarded
_ZN4util10TimerGroup10startTimerEj:
    push {r4, lr}
    mov r2, lr
    mov r3, r6
    bl vc_start_guarded
    pop {r4, pc}
.size _ZN4util10TimerGroup10startTimerEj, .-_ZN4util10TimerGroup10startTimerEj
.section .note.GNU-stack,"",%progbits
