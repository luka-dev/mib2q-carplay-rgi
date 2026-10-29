/* Offline experiment only; replaces 4 bytes at stock VA/file 0x46ed8.
 * The original BEQ skipped the delete-path moal_stop_timer whenever the
 * stale one-shot callback had already cleared the active byte (table+0x24),
 * so wlan_11n_delete_rxreorder_tbl_entry could free the 44-byte timer block
 * while the io-pkt queue still held its callout. NOP makes the native stop
 * unconditional; callout_stop on a non-queued callout is a mutex-guarded
 * no-op, and the follow-up clear of the active byte is unchanged. This
 * fragment is loaded only into the Unicorn test address space.
 */
.syntax unified
.arch armv7-a
.arm
.text
.global mhi2q_timer_harden_candidate
.type mhi2q_timer_harden_candidate, %function
mhi2q_timer_harden_candidate:
    nop
.size mhi2q_timer_harden_candidate, .-mhi2q_timer_harden_candidate
