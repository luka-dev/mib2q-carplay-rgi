#ifndef MHI2Q_SI_HOOK_PROTOCOL_H
#define MHI2Q_SI_HOOK_PROTOCOL_H

/* Contract between the SI ELF graft (cave + trampolines) and si_adapter.so.
 * Both sides pin the same SI input hash; the graft manifest repeats these
 * constants so a mismatch fails at install time instead of at runtime.
 * ARM EABI, little-endian, 32-bit. */

#define SI_HOOK_MAGIC_SIG  0x58484953u  /* 'SIHX' little endian */
#ifdef SI_STAGE2
#define SI_HOOK_ABI_VER    2u
#else
#define SI_HOOK_ABI_VER    1u
#endif

/* Immutable hook page is R+X. The separate root is in the RW bss page tail. */
#define SI_CAVE_OFF_SIG     0x00u
#define SI_CAVE_OFF_VER     0x04u

/* Slot offsets inside struct si_adapter_table (see below). */
#define SI_SLOT_OFF_PREPARE 12u
#define SI_SLOT_OFF_CLEANUP 16u
#define SI_SLOT_OFF_JOB     20u

/* struct si_adapter_table — owned by si_adapter.so, single-writer publish:
 * table filled first, then dmb, then one 32-bit store of the pointer into the
 * RW root. Trampolines re-read the root on every hit; never cached.
 * Handlers use C calling convention (AAPCS) and must be exception-free.
 * Stage 1: prepare/cleanup are notifications; the verdict is ignored.
 * Wireless routing requires a new reviewed handler ABI before stage 2.
 *   job: non-zero = job consumed (dispatcher returns 0), 0 = original dispatch.
 * The trampoline itself never routes a native USB device record to the
 * adapter: gating on the record's connectionType stays in the adapter. */

/* Adapter job opcodes carried in the FakeConnectDevice `method` dword.
 * Values outside SI's method-mask namespace: SI masks use bits 0..5 (0x01..0x20),
 * so opcodes start at 0x1000. The dispatcher trampoline calls the adapter for
 * every job; the adapter filters opcodes itself. */
#define SI_JOB_OP_NOOP     0x1000u  /* contour proof: consumed, no state change */
#define SI_JOB_OP_CONTROL  0x1001u
#define SI_JOB_OP_TICK     0x1002u

/* Absolute SI addresses pinned by the manifest (MHI2Q re-arm SI 606236a8...).
 * The adapter resolves them as fixed VAs (ET_EXEC, base 0x100000). */
#define SI_VA_JOB_ALLOC    0x001293D8u  /* FakeConnectJob_allocCtor -> 0x28B job */
#define SI_VA_JOB_ENQUEUE  0x0013F4F4u  /* Mediator_enqueueJob(job, oob)         */
#define SI_VA_JOB_VTABLE   0x001BF248u  /* FakeConnectJob vtable address point */
#define SI_VA_JOB_DELETE   0x00129050u  /* Native deleting destructor         */
#define SI_VA_CTRL_AO_ID   0x00000001u  /* CarPlay AO id = app index 0 + 1;
                                           * живое подтверждение — в QEMU-контуре */
#define SI_VA_UPSERT       0x0016EEF4u  /* DataProxy::addUniqueID(proxy, entry):
                                           * дедуп по uniqueID + аллокация
                                           * deviceID (лимит/вычистка) + вставка;
                                           * entry+0 результата = deviceID        */

/* Default cave root VA from build_candidate.py (manifest may override it via
 * MHI2Q_SI_HOOK_ROOT). */
#ifndef SI_HOOK_DEFAULT_ROOT_VA
#define SI_HOOK_DEFAULT_ROOT_VA 0x01C5E00u
#endif
/* Адрес указателя на DataProxy (заполняется адаптером после готовности SI;
 * финальный адрес закрепляет live-прогон — см. "Открытое" в STAGE1_RESULTS). */
#ifndef SI_VA_DATAPROXY_PTR
#define SI_VA_DATAPROXY_PTR 0x00000000u  /* 0 = этап 2 не активен */
#endif

#endif /* MHI2Q_SI_HOOK_PROTOCOL_H */
