/* OFFLINE ONLY: pure ARM policy; no driver cave/relocation/deployment.
 * r0 = persistent sidecar state, r1 = immutable input snapshot.
 * Returns action mask in r0: ARM=1 STOP=2 FLUSH=4 IGNORE=8.
 * State words: generation, alive, virtual_valid, virtual_due, armed,
 * physical_due, arm_epoch. Input: event, now_tick, pending, progressed, generation,
 * age_cap_valid, age_cap_due, captured_arm_epoch. Events RX=0 EXPIRY=1 DELETE=2 CREATE=3.
 * Caller serializes lifetime + RX + expiry; callback cookie retains sidecar
 * and generation independently of table allocation. All deltas <2^31.
 * ARM duration is ABSOLUTE state.physical_due - now, not callout_msec ticks.
 */
.syntax unified
.arm
.text
.global rx_timer_churn_policy
rx_timer_churn_policy:
    push {r4-r10,lr}
    mov r4,r0
    mov r5,r1
    mov r10,#0
    ldr r6,[r5]
    cmp r6,#3
    beq create
    ldr r0,[r4,#4]
    cmp r0,#0
    beq ignore
    ldr r0,[r4]
    ldr r1,[r5,#16]
    cmp r0,r1
    bne ignore
    cmp r6,#2
    beq delete
    cmp r6,#1
    bhi ignore
    cmp r6,#1
    bne validated_event
    ldr r0,[r4,#16]
    cmp r0,#0
    beq ignore
    ldr r0,[r4,#24]
    ldr r1,[r5,#28]
    cmp r0,r1
    bne ignore
validated_event:
    ldr r7,[r5,#4]
    ldr r8,[r4,#12]
    ldr r0,[r4,#8]
    cmp r0,#0
    beq expired_virtual
    sub r0,r8,r7
    cmp r0,#0
    bgt valid_virtual
expired_virtual:
    mov r0,#0
    str r0,[r4,#8]
valid_virtual:
    cmp r6,#1
    beq expiry
    ldr r0,[r4,#8]
    cmp r0,#0
    beq reset_virtual
    ldr r0,[r5,#12]
    cmp r0,#0
    beq choose_due
reset_virtual:
    add r8,r7,#6
    mov r0,#1
    str r0,[r4,#8]
    str r8,[r4,#12]
choose_due:
    ldr r0,[r5,#8]
    cmp r0,#0
    beq empty
    ldr r0,[r5,#20]
    cmp r0,#0
    beq chosen
    ldr r9,[r5,#24]
    sub r0,r9,r8
    cmp r0,#0
    movlt r8,r9
chosen:
    sub r0,r8,r7
    cmp r0,#0
    ble flush
    ldr r0,[r4,#16]
    cmp r0,#0
    beq arm
    ldr r0,[r4,#20]
    sub r0,r0,r8
    cmp r0,#0
    ble done
    orr r10,r10,#2
arm:
    ldr r0,[r4,#24]
    add r0,r0,#1
    str r0,[r4,#24]
    mov r0,#1
    str r0,[r4,#16]
    str r8,[r4,#20]
    orr r10,r10,#1
    b done
expiry:
    ldr r0,[r4,#16]
    cmp r0,#0
    beq ignore
    mov r0,#0
    str r0,[r4,#16]
    ldr r0,[r4,#8]
    cmp r0,#0
    beq flush
    b choose_due
empty:
    ldr r0,[r4,#16]
    cmp r0,#0
    orrne r10,r10,#2
    mov r0,#0
    str r0,[r4,#16]
    b done
flush:
    ldr r0,[r4,#16]
    cmp r0,#0
    orrne r10,r10,#2
    mov r0,#0
    str r0,[r4,#16]
    str r0,[r4,#8]
    orr r10,r10,#4
    b done
delete:
    ldr r0,[r4,#16]
    cmp r0,#0
    orrne r10,r10,#2
    mov r0,#0
    str r0,[r4,#4]
    str r0,[r4,#8]
    str r0,[r4,#16]
    b bump_generation
create:
    ldr r0,[r4,#4]
    cmp r0,#0
    bne ignore
    mov r0,#1
    str r0,[r4,#4]
    mov r0,#0
    str r0,[r4,#8]
    str r0,[r4,#16]
bump_generation:
    ldr r0,[r4]
    add r0,r0,#1
    cmp r0,#0
    moveq r0,#1
    str r0,[r4]
    b done
ignore:
    mov r10,#8
done:
    mov r0,r10
    pop {r4-r10,pc}
