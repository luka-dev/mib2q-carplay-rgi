/* Offline experiment only; replaces 16 bytes at stock VA/file 0x47368.
 * R3 is the backwards scan's highest occupied slot. Preserve the original
 * ABI and branch target; set next_start=(start+last_slot+1)&4095.
 * This fragment is loaded only into the Unicorn test address space.
 */
.syntax unified
.arch armv7-a
.arm
.text
.global mhi2q_rx_expiry_candidate
.type mhi2q_rx_expiry_candidate, %function
mhi2q_rx_expiry_candidate:
    add r2, r2, r3
    add r2, r2, #1
    ldr r0, [r4, #12]
    ubfx r2, r2, #0, #12
.size mhi2q_rx_expiry_candidate, .-mhi2q_rx_expiry_candidate
