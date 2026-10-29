/* Offline only: 32 bytes at stock VA/file 0x46d64. Forward distance is
 * modulo 4096 and capped at the window size. Preserve the stock equal-target
 * full-drain behavior used when re-establishing an existing BA session.
 * Original dispatch/compaction/locking and branch at 0x46d84 are retained.
 */
.syntax unified
.arch armv7-a
.arm
.text
.global mhi2q_rx_window_candidate
.type mhi2q_rx_window_candidate, %function
mhi2q_rx_window_candidate:
    ldr r10, [r1, #20]
    sub r10, r2, r10
    ubfx r10, r10, #0, #12
    ldr r3, [r1, #24]
    cmp r10, r3
    movge r10, r3
    cmp r10, #0
    moveq r10, r3
.size mhi2q_rx_window_candidate, .-mhi2q_rx_window_candidate
