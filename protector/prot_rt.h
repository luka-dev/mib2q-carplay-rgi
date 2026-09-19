/* prot_rt.h - build-to-VIN binding, runtime side.
 *
 * You write your project in plain C. You add ONE marked slot (PROT_DEFINE_SLOT)
 * and call prot_unlock() where you need the VIN-bound payload. A separate
 * post-build tool (protect.py) stamps the slot per VIN at provision time.
 *
 * Ceiling (be honest): the build secret lives in the image, so a rooted
 * attacker who runs the code once can extract the derived key. This raises
 * clone/patch cost; it is not cryptographic secrecy. Move PROT_SECRET to
 * per-unit secure storage or a server challenge for a real wall.
 */
#ifndef PROT_RT_H
#define PROT_RT_H

#include <stddef.h>
#include <stdint.h>

/* Slot layout (must match protect.py):
 *   0   8   magic  P R O T 00 S L T  (NUL inside so `strings` won't show it)
 *   8   4   CHK  u32 LE      CRC32 of the protected file span, stamped at provision
 *   12  2   LEN  u16 LE      payload length actually stored
 *   14  N   ENC              payload ^ keystream(KDF(secret, VIN, CHK))
 * PROT_SLOT_CAP bounds N. Keep the payload small (feature bits, a key, a token).
 *
 * NOTE: the runtime does NOT use a contiguous string literal for the magic
 * (that would put a second copy in .rodata and confuse the locator). It rebuilds
 * the 8 bytes at runtime; see find_slot() in prot_rt.c. Only the data slot holds
 * the contiguous sequence. */
#define PROT_SLOT_MAGLEN 8
#define PROT_TAG         4               /* integrity tag prepended inside ENC */
#define PROT_SLOT_CAP    64              /* max payload bytes */
/* ENC region holds TAG || payload, so it needs PROT_TAG + PROT_SLOT_CAP bytes */
#define PROT_SLOT_SIZE   (PROT_SLOT_MAGLEN + 4 + 2 + PROT_TAG + PROT_SLOT_CAP)

/* The protector locates the slot by its magic (portable across ELF/Mach-O/flat),
 * so no custom section is needed. Define exactly once in your app. */
#define PROT_DEFINE_SLOT()                                                     \
    __attribute__((used, aligned(4)))                                         \
    volatile uint8_t PROT_SLOT[PROT_SLOT_SIZE] = { 'P','R','O','T',0,         \
        'S','L','T' /* rest zero-filled: CHK, LEN, ENC placeholders */ }

/* Recover the VIN-bound payload.
 *   self_path : path to this executable's on-disk image (the flashed bytes).
 *   vin       : 17-char VIN from the quorum reader (NOT a single source).
 *   out/cap   : caller buffer; *out_len set on success.
 * Returns 0 on success. Non-zero means: code was patched (CRC mismatch),
 * wrong VIN, wrong secret, or a malformed image - caller must treat the
 * payload as unavailable and fail closed. There is no plaintext fallback.
 *
 * COST: prot_unlock() reads the image and runs one CRC + a few SHA-256 blocks.
 * Call it ONCE (prot_init), never inside a DSP/decode loop. */
int prot_unlock(const char *self_path, const char *vin,
                uint8_t *out, size_t cap, size_t *out_len);

/* ---- one-shot init + cached gate (use this from real code) ----
 *
 * Split your code by cost, not importance:
 *   - HOT (DSP, per-sample decode): mark with PROT_HOT, never call crypto here.
 *     Read the cached gate with prot_ready()/prot_gate() - a plain memory read.
 *   - CRITICAL-BUT-LIGHT (feature enable, a small config/key decode, a licence
 *     branch): mark with PROT_CRITICAL. Its correctness depends on the cached
 *     payload, so patching the image kills the CRC -> prot_init fails ->
 *     these paths get nothing valid. That is the "guaranteed bound" part.
 *
 * prot_init() unlocks once and caches the payload. Returns 0 on success.
 * After a 0 return, prot_ready()==1 and prot_gate() serves the payload bytes
 * with zero further crypto - safe to touch from anywhere, including near
 * (but not inside) hot loops. */
int  prot_init(const char *self_path, const char *vin);
int  prot_ready(void);                       /* 1 iff prot_init succeeded */

/* ---- constant blinding: fold the VIN into the MATH, no payload to dump ----
 *
 * The stronger model. Instead of decrypting a "payload" that then sits in memory
 * (a dump lifts it), you store your real constants XOR-blinded by KDF(secret,
 * VIN, CRC). prot_unblind() removes the blind using the LIVE VIN, in place, and
 * you feed the result straight into your algorithm. Wrong VIN -> wrong mask ->
 * your coefficients are garbage -> the algorithm produces WRONG NUMBERS. There
 * is no boolean gate to nop and no plaintext secret blob - the wrongness IS the
 * protection. A VIN check that exits/refuses is then only a formality on top.
 *
 *   consts : your constant array, stored blinded in the image by protect.py;
 *            unblinded in place here. n <= PROT_SLOT_CAP.
 * Returns 0 if the image is intact and a VIN was derived (NOT that the VIN is
 * "right" - a wrong VIN also returns 0 and leaves garbage in consts, on purpose:
 * silent breakage, not a detectable check). Returns non-zero only on a patched
 * image (CRC mismatch) or malformed slot. */
int  prot_unblind(const char *self_path, const char *vin, uint8_t *consts, size_t n);

/* Optional formality: exit the process if the VIN doesn't match a stamped check
 * tag. Cosmetic - the real defense is the broken math above. Returns on match;
 * on mismatch it does NOT return (calls _exit). Use it as a tripwire, never as
 * the only barrier. */
void prot_or_die(const char *self_path, const char *vin);

/* ---- graceful fail-open: never brick stock behavior ----
 *
 * When you have a legitimate FREE baseline ("stock") and the VIN-bound values are
 * a premium/licensed variant, use this instead of letting the math break. It
 * folds the VIN and decides:
 *   - right VIN, image intact  -> PROT_PREMIUM, `out` holds the unblinded constants
 *   - wrong/absent VIN, or the protection subsystem hiccuped (can't read self,
 *     no VIN yet on the bench), or a patched image -> PROT_STOCK, `out` = `defaults`
 * It NEVER returns an error that bricks the unit - a genuine on-unit failure
 * degrades to stock, not to broken.
 *
 * SAFETY RULE (non-negotiable): `defaults` must be the UNPROTECTED baseline you
 * are fine with anyone running. This path is deliberately fail-open, so an
 * attacker CAN force it (corrupt the slot, block the file). That is acceptable
 * ONLY because forcing it yields stock, never premium: the premium constants
 * require the right VIN and never appear on any failure path. Never put anything
 * you actually need to protect behind the STOCK fallback.
 *
 * A stamped slot for this path carries a 1-byte sentinel as the first constant;
 * prot_select checks it so a wrong VIN cleanly yields STOCK instead of garbage.
 *   sentinel : the byte you stamped at consts[0] (e.g. 0x5A).
 *   defaults : n stock bytes used on any non-premium outcome.
 *   out      : n bytes; premium constants on PROT_PREMIUM, else a copy of defaults. */
typedef enum { PROT_PREMIUM = 0, PROT_STOCK = 1 } prot_mode_t;
prot_mode_t prot_select(const char *self_path, const char *vin, uint8_t sentinel,
                        const uint8_t *defaults, uint8_t *out, size_t n);

/* ---- hook gating: our hooks stay INERT on a wrong/absent VIN ----
 *
 * For mods that hook/detour stock functions. Resolve ONCE at load, then every
 * hook checks prot_active() (a cached bool, no crypto) and, when inactive, calls
 * straight through to the original - the head runs its stock path untouched, as
 * if the mod were not installed. Wrong/absent VIN, a patched image, or an on-unit
 * hiccup -> inactive -> stock. Nothing to brick, and forcing "active" gains
 * nothing: our premium constants come from the VIN and are wrong without it.
 *
 * prot_arm() runs prot_select once and caches (mode + the unblinded constants).
 * Returns the mode. After it:
 *   prot_active()  -> 1 iff PROT_PREMIUM (right VIN). Cheap, safe in hot hooks.
 *   prot_gate(i)   -> cached constant byte i (see above). No crypto.
 *
 * SAFETY: prot_active() is a boolean an attacker can flip to 1. That only helps
 * them if your hook's EFFECT is plaintext behind the bool. Make the hook's actual
 * output depend on prot_gate() constants (blinded), so a forced-active hook on a
 * wrong VIN produces broken output, not your real feature. The bool is for
 * graceful inertness; the blinded constants are the real barrier. */
prot_mode_t prot_arm(const char *self_path, const char *vin, uint8_t sentinel,
                     const uint8_t *defaults, size_t n);
int prot_active(void);                       /* 1 iff armed with the right VIN */

/* Production entry point: arm using the LIVE VIN read on the unit (prot_read_vin,
 * quorum across FEC/backup/shmem) and this program's own on-disk image. No VIN or
 * path arguments - call this once at load and gate hooks on prot_active().
 * Any failure (no/disagreeing VIN, unreadable self, patched image) -> STOCK/inert.
 * Requires linking prot_vin.c. */
prot_mode_t prot_arm_auto(uint8_t sentinel, const uint8_t *defaults, size_t n);

/* MMX-friendly entry: like prot_arm_auto but you pass this app's known install
 * path (e.g. "/eso/bin/apps/yourapp") instead of relying on /proc/self/exefile.
 * On MMX the app image is a fixed read-only mount, so the path is known at build
 * time and this is the reliable form. Live VIN still read via prot_read_vin. */
prot_mode_t prot_arm_path(const char *self_path, uint8_t sentinel,
                          const uint8_t *defaults, size_t n);

/* Best-effort path to this program's own on-disk image (QNX /proc/self/exefile,
 * else Linux /proc/self/exe, else argv0 if you stashed it). Returns 0 on success
 * and fills buf. Used by prot_arm_auto. */
int prot_self_path(char *buf, size_t cap);
/* Cached payload byte i (0 if not ready or out of range) - for gate flags,
 * small coefficient sets, a key the critical path expands. No crypto. */
uint8_t prot_gate(size_t i);
size_t  prot_gate_len(void);

/* Annotations. On QNX/ELF PROT_CRITICAL groups the key functions into their own
 * section so they sit together (and, if you enable targeted CRC, can be checked
 * as a region). PROT_HOT is documentation-only: it marks "keep this fast, no
 * protector calls inside". Both degrade to plain functions where sections aren't
 * available (e.g. the macOS demo build). */
#if defined(__ELF__)
#  define PROT_CRITICAL __attribute__((used, section(".prot_crit")))
#else
#  define PROT_CRITICAL __attribute__((used))
#endif
#define PROT_HOT        /* intentionally empty: a reminder, not a transform */

#endif /* PROT_RT_H */
