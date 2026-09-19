# protector — bind a compiled build to one VIN

Post-build tool that layers on top of your compiled artifact and binds it to a
single VIN. **You write the project in plain C, however you like.** The only
protector-specific things in your source are: define one marked slot, and call
`prot_unlock()` where you need the VIN-bound value. Everything else — the
checksum, the VIN-keyed math — is stamped in afterward by `protect.py`.

```
  your normal .c  ──cc──▶  generic binary (marked slot, blank)
                              │
                  protect.py --vin <VIN>   (runs per flash, knows target VIN + secret)
                              │   CRC32 whole image → CHK
                              │   ENC = keystream(HMAC(secret, VIN‖CHK)) ⊕ (TAG‖payload)
                              ▼
                     image provisioned for ONE car
  boot ▶ prot_unlock(): CRC self → derive key from LIVE VIN → decode → check TAG
         wrong VIN / patched code / wrong secret → fail closed, no plaintext
```

## Files

| file | role |
|---|---|
| `prot_rt.h` | public API + the `PROT_DEFINE_SLOT()` macro |
| `prot_rt.c` | runtime: CRC own image, derive key, decode; `prot_arm_auto` |
| `prot_vin.h/.c` | live VIN reader on the unit (FEC active+backup quorum) |
| `prot_crypto.h` | bundled SHA-256 / HMAC / CRC32 (no OpenSSL — QNX/embedded safe) |
| `protect.py` | post-build stamper (the protector) |
| `build.sh` | one-shot: compile + stamp for a **pre-provided VIN** |
| `example_app.c` | how a developer wires it in (hook model) |
| `Makefile` | host/QNX build, `make build VIN=…`, `make test` |
| `selftest.sh` | end-to-end proof incl. the live FEC path |

## Production flow (VIN provided pre-build)

The VIN is known before the build, so stamping happens inside the build:

```sh
# constants = [ sentinel, your real coefficients... ]
printf '\x5a\x66\x07' > consts.bin
PROT_SECRET='<ci-secret>' ./build.sh WAUC4CF55JA099137 app consts.bin
# QNX/RCC:
CC='qcc -Vgcc_ntoarmv7le' PROT_SECRET='<ci-secret>' ./build.sh WAUC4CF55JA099137 app.armle consts.bin
```

At load the app calls `prot_arm_auto()` — no arguments. It reads the **live VIN on
the unit** from the FEC quorum (`/mnt/efs-persist/FEC/FecContainer.fec` and the
`/mnt/efs-system/backup/…` copy; they must agree), checksums its own image, and
arms. It runs ACTIVE only when the unit's live VIN equals the VIN baked at build.
Verified end-to-end on the real FEC from this dump: matching VIN → ACTIVE, any
other VIN → inert.

```c
PROT_DEFINE_SLOT();
static const uint8_t STOCK[2] = { 1, 0 };
prot_arm_auto(0x5A, STOCK, 2);        // once at load; live VIN + self image
... if (prot_active()) { /* your hook */ } ...
```

**Self-path note.** The integrity check reads the program's own on-disk image.
`prot_arm_auto` finds it via `/proc/self/exefile` (QNX). On a full RCC that
resolves to the readable ifs path; if your deployment can't expose it that way,
call `prot_arm(self_path, vin, …)` with the app's known install path instead. (On
the minimal QEMU BSP used for testing there is no readable file view at all, so
the file-read path can't be exercised there — see QNX validation below.)

## QNX / armv7 validation

Cross-compiled with the QNX 6.5 `armle-v7` toolchain
(`Tools/qnx-65-sdp-docker`, GCC 8.5.0) and run on real QNX 6.5 under QEMU
(`tests/qemu-run.sh`). The arch-sensitive core — SHA-256, HMAC-SHA256, CRC32,
the sha256-CTR keystream, and the unblind roundtrip — is byte-identical to the
host (`qnx_kat.c`, all known-answer vectors pass on armv7):

```
CRC ok · SHA ok · HMAC ok · UNBLIND ok (gain=102 off=7) · QNX-KAT-PASS
```

The end-to-end hook flow (arm → active → math) is proven on the host against the
real FEC. The self-image read is deployment-specific (see note above); it works
on the real RCC's readable ifs, not on the stripped QEMU boot image.

## Use

```c
#include "prot_rt.h"
PROT_DEFINE_SLOT();                       // exactly once in the program

uint8_t buf[PROT_SLOT_CAP]; size_t n;
if (prot_unlock(argv[0], vin, buf, sizeof buf, &n) != 0)
    fail_closed();                        // wrong VIN, patched, or unprovisioned
// buf[0..n] = your feature bits / token / key
```
`vin` must come from the **quorum reader** (`docs/HANDOFF_VIN_PROTECTION.md`), not
a single source.

```sh
# compile once, generic
make                                      # or your own build; add prot_rt.c + -DPROT_SECRET
# stamp per car at provision time
PROT_SECRET='…' python3 protect.py app --vin WAUC4CF55JA099137 --payload 'feat=0x2f01'
make test                                 # run the full selftest
```

QNX cross-build for the RCC (armle-v7): `make CC='qcc -Vgcc_ntoarmv7le'`.

## What it stops (verified by `selftest.sh`)

- **Clone to another car** — wrong VIN → key wrong → TAG mismatch → locked.
- **Binary/ASM patch of the library** — any changed code byte → CRC ≠ CHK → locked.
- **grep-and-copy** — no plaintext VIN or payload in the image (`strings` clean).
- **Rebuild with a guessed secret** — TAG mismatch → locked.
- **Per-VIN images differ** — no shared plaintext blob to lift.

## Two modes: blind (fold VIN into the math) vs token

**blind (recommended) — `protect.py --mode blind`, runtime `prot_unblind()`.**
Your real constants are stored XOR-blinded by `KDF(secret, VIN, CRC)`. At startup
you unblind them into locals and feed them straight into your algorithm. With the
**wrong VIN the constants come out wrong, so the code produces wrong numbers.**
There is no boolean gate to nop and no decrypted "license blob" left in memory -
the wrongness itself is the protection. A VIN check that exits/refuses is then
only a *formality tripwire*: an attacker can remove it and the math is still
broken. Proven in `selftest.sh` (money shot): with the tripwire compiled out, the
wrong VIN yields `gain=64 offset=253` instead of `gain=102 offset=7`.

Why this beats the token model against a dump: there is no persistent decrypted
secret sitting in BSS. The only correct values that ever materialize are the
working constants, and only on the right car. (Honest residual below.)

**token — `--mode blind` off (default), runtime `prot_unlock()`.** Body = TAG ||
payload; wrong VIN → tag mismatch → rejected. Use for a small token/key you need
verbatim. `prot_or_die()` is the tripwire for this mode. Note this DOES hold the
decrypted payload in memory after unlock — a dump lifts it. Prefer blind.

## Graceful fail-open: never brick stock behavior

If you have a legitimate FREE baseline ("stock") and the VIN-bound values are a
premium/licensed variant, use `prot_select()` instead of letting the math break.
It returns `PROT_PREMIUM` (right VIN + intact image + sentinel) or `PROT_STOCK`
(anything else — wrong/absent VIN, a patched image, or the protection subsystem
hiccuped on the unit) and hands back your `defaults` in the STOCK case. A genuine
on-unit failure degrades to stock, not to broken.

```c
static const uint8_t STOCK[2] = { 1, 0 };            // base: passthrough
uint8_t c[2];
prot_mode_t m = prot_select(argv[0], vin, 0x5A, STOCK, c, 2);
int gain = c[0], offset = c[1];                       // premium iff m==PROT_PREMIUM
```

**Safety rule (non-negotiable):** `defaults` must be the unprotected baseline you
are fine with anyone running. This path is deliberately fail-open, so an attacker
CAN force it (corrupt the slot, block the file read). That is acceptable ONLY
because forcing it yields STOCK, never PREMIUM — the premium constants require the
right VIN and never appear on any failure path. Proven in `selftest.sh`: no
failure path (unprovisioned, patched, wrong VIN) ever produces the PREMIUM line.

Which to use:
- **Deny model** (`prot_unblind`, broken math): the VIN-bound thing IS the product,
  no meaningful baseline — a wrong VIN must not work at all.
- **Fail-open model** (`prot_select`, stock fallback): there's a free baseline;
  unlicensed/failed = stock, licensed = premium, unit never bricks.
- **Hook model** (`prot_arm` + `prot_active`, see below): our mods are additive
  hooks that stay inert on a wrong VIN. This is usually what a MIB mod wants.

## Hook model: our hooks stay inert on a wrong/absent VIN

For a mod that hooks/detours stock functions, you usually want: **wrong or absent
VIN → none of our hooks do anything, the head runs its original stock path as if
the mod weren't installed.** Not broken, not bricked — invisible.

Arm once at load, then gate every hook on a cached bool:

```c
prot_arm(self, vin, 0x5A, STOCK, 2);          // once, at load; caches mode+consts

PROT_HOT long hooked_process(const short *in, int n){
    if (!prot_active())                        // wrong/absent VIN -> inert
        return stock_process(in, n);           // call through: stock path untouched
    int gain = prot_gate(0), offset = prot_gate(1);   // VIN-blinded constants
    long a=0; for(int i=0;i<n;i++) a += (long)in[i]*gain + offset; return a;
}
```

`prot_active()` is a cached bool — no crypto in the hook, safe in hot paths.
Proven in `selftest.sh`: right VIN → `ACTIVE`; wrong/absent VIN, unprovisioned,
or patched image → `inert` and the stock result is unchanged; no failure path
ever goes `ACTIVE`.

**Safety:** `prot_active()` is a boolean an attacker can flip to 1. That only
helps them if the hook's *effect* is plaintext behind the bool. Keep the hook's
output dependent on `prot_gate()` (blinded) constants, so a forced-active hook on
a wrong VIN produces broken output, never your real feature. The bool gives you
graceful inertness; the blinded constants are the real barrier.

## Scoping: don't burden the hot path, guarantee-bind the key one

Split code by **cost, not importance**. The protector runs its crypto **once**,
at `prot_init()`, and caches the result. Everything after reads that cache with a
plain memory access — so a DSP / per-sample decode loop pays nothing.

| mark | what it means | may call protector? |
|---|---|---|
| `PROT_HOT` | heavy per-sample work (DSP, decode) — keep fast | **no** — only `prot_ready()` / `prot_gate()` (plain reads) |
| `PROT_CRITICAL` | critical but light (feature gate, small key/config decode, a licence branch) | its correctness *depends on* the cached payload |

The binding is guaranteed for the critical code because its result comes from the
VIN-keyed payload: patch the image anywhere → CRC ≠ CHK → `prot_init` fails →
the critical path gets no valid gate → the feature is dead. The hot loop is never
touched by any of this.

```c
prot_init(self, vin);                 // ONE crypto call for the whole process

PROT_HOT   long dsp(const short*in,int n){        // no protector call inside
    if (!prot_ready()) return 0;                  // one bool read
    int g = prot_gate(0);                         // cached; no crypto
    long a=0; for(int i=0;i<n;i++) a+=in[i]*g; return a;
}
PROT_CRITICAL int feature_enabled(void){          // rare, light, bound
    return prot_ready() && prot_gate(0)=='f';
}
```

Verified in `selftest.sh`: right VIN → `dsp_process=81600` (hot loop ran, gated by
the cached value); wrong VIN / patched image → `prot_init` fails, feature dead.

On QNX/ELF, `PROT_CRITICAL` also groups the key functions into a `.prot_crit`
section, so they sit together — useful if you later want a *targeted* region CRC
instead of whole-image (lighter still). The default whole-image CRC already
covers them and, being one-shot at boot, costs nothing per sample.

**Do not** wrap heavy math itself in the VIN key (re-deriving per sample). That
burdens exactly what you wanted left alone, for no ceiling gain. Bind the gate,
cache it, let the hot path read it.

## Honest ceiling — read this

`PROT_SECRET` is baked into the runtime (and passed to `protect.py`). A **rooted
attacker who runs a provisioned image once can extract the derived key** and,
with more work, the secret. This is **anti-clone / anti-casual-patch grade, not
cryptographic secrecy.** Modded MIB heads already run unsigned firmware (secure
boot bypassed), so treat this as a cost-raiser, not a wall.

Concretely observed on a build of this code: the secret string was NOT visible to
`strings`, yet `-O2` precomputed the HMAC ipad/opad and left `key^0x36` /
`key^0x5c` in `.rodata` - the secret is recoverable by XORing back. And a runtime
dump exposes whatever the code is currently working with.

What blind mode changes about the dump: there is no persistent decrypted blob, and
no gate to patch. But be honest about the residual - **on the legitimate car with
the right VIN, the correct constants must materialize to run**, so a dump taken
there still yields the working constants; the attacker can then hardcode them and
drop the VIN dependence. Blind raises the work from "copy a blob" to "reverse the
real constants and rebuild a VIN-independent binary," and removes the easy
byte-patch - it does not move the ceiling. The ceiling still needs an off-box
secret:

The design is built so the *only* thing you change to get a real wall is where
`secret` comes from:
- **per-unit secure storage** (TrustZone/secure element) instead of a baked
  constant → extraction from one unit doesn't transfer, and
- **server challenge/response** for anything that must truly gate → the decision
  isn't on the attacker's CPU at all.

Neither changes the code here — only the source of `secret`.

## Keep it proportionate

- Protect the **decision path / one payload**, not "all the math." Auto-keying
  every op kills the optimizer and gains no ceiling.
- Never log the VIN or payload. MIBRoot ships `[SERV] VIN: %s` — that is exactly
  what a lazy reverser greps. Yours stays silent.
- Strip the binary; no symbols on `prot_*`.
- Keep `PROT_SECRET` out of the source tree — CI/provision secret only.

## Notes / limits

- The magic may appear more than once (the compiler can echo the constant into
  `.rodata`). Handled: `protect.py` stamps the unprovisioned (zero-tail) slot;
  the runtime tries every candidate and accepts the one whose CRC validates.
- CRC is over the **on-disk file** (the flashed, immutable bytes), which avoids
  file-vs-memory relocation differences. On QNX the runtime reads its own ifs
  path; here `example_app` uses `argv[0]`.
- Payload cap is `PROT_SLOT_CAP` (64 B). It's for small secrets (feature bits,
  a token, a key), not bulk data.
- CRC32 is an integrity check against accidental/opportunistic patching, keyed
  through the KDF — not a MAC. A determined attacker who fixes the CRC still hits
  the TAG/secret wall, subject to the ceiling above.
