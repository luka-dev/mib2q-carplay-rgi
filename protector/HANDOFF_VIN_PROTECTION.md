# Handoff — VIN from FEC

Reading the vehicle VIN out of the MIB2 head, robustly, on units whose FEC
container has been toolbox-patched (broken / non-OEM signature).

Target dump: `MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398`
Reference VIN in this dump: `WAUC4CF55JA099137` (Audi, US).

---

## The two VINs — do not conflate them

The head knows the VIN in two independent ways:

| source | nature | on the bench (no CAN) | our target |
|---|---|---|---|
| **Live bus VIN** — `car` asks the gateway (`eCPFuncVin`), MIBRoot caches it to `/dev/shmem/VIN.txt` (RAM) | volatile, needs CAN | **empty** (`VIN_NoResponse`, `[SERV] ! VIN file`) | no |
| **FEC VIN** — written once into `FecContainer.fec`, lives in flash | persistent, bus-independent | **present** | **yes** |

This is why `MU1316-allversions.txt` shows `VIN MIB =` / `VIN car =` blank on a
bench dump, while the FEC copy reads fine: the bench has no bus, so
`/dev/shmem/VIN.txt` was never created, but the FEC blob sits in flash
regardless. **Read the FEC VIN — it is the only one available off-car.**

---

## FEC container format (`FecContainer.fec`, 243 B in this dump)

```
0x00  u32  version = 1
0x04  u32  reclen  = 0xBF (191)         length of record + signature
0x08  ...  record  (63 B, 0x08..0x47)   <-- SIGNED region
        0x08  12B  header  (11 07 ff ff ff ff 03 11 03 59 cb 56)
        0x14  17B  VIN + 1B NUL          <-- VIN, fixed offset, BEFORE the list
        0x26  ..   FEC licence list      <-- toolbox grows this to add features
0x47  128B  RSA signature
0xC7   44B  unsigned tail (exchange/secondary record; not used for VIN)
```

**Signature:** RSA-1024, `e = 3`, PKCS#1 v1.5, **RIPEMD-160**
(DigestInfo OID `1.3.36.3.2.1` = `3021300906052b2403020105000414`).
Verified empirically: `s^3 mod n` unwraps to `0001 FF..FF 00 <DigestInfo> <20B hash>`,
and a byte-range brute force pins the signed region to exactly `[0x08, 0x47)`.

**Keys:** `/mnt/efs-persist/Keys/FECKey/*_MIB-High_FEC_public_signed.bin`,
288 B each = `128B modulus | 32B exponent | 128B signature-of-the-key`.
This dump's container verifies against `AU_MIB-High_FEC_public_signed.bin`.
The FEC key is itself signed up the chain to DataKey
(`MIB-High_DK_public_signed.bin`), whose root is baked into MIBRoot.

### Consequence for patched units

Adding a licence edits the FEC list at `0x26+`, which changes the RIPEMD-160 of
the record, which breaks the RSA signature. It cannot be re-signed without the
OEM private key, so field/toolbox containers routinely carry a signature that
does **not** verify against the OEM key. **The VIN sits at 0x14, before the
list, so it never moves** — extract it structurally and the patch is irrelevant.

---

## Design rule: signature is advisory, never a gate

The reader must return the VIN whether or not the signature verifies. Signature
status is reported alongside (OEM-verified / forged-or-patched / no-keys), but
it never blocks output.

Verified on four inputs — genuine, flipped-signature, added-licence-byte,
no-keydir — VIN comes out in every case.

---

## Where the VIN lives on the unit

Active, verified container (read this one):
```
/mnt/efs-persist/FEC/FecContainer.fec          # active
/mnt/efs-system/backup/FEC/FecContainer.fec     # mirror
```
Never read these — unverified by definition: `incoming/`, `archiv/`, `garbage/`,
`IllegalFecContainer.fec`, `Temp*`, `New*`.

Same VIN also embedded in the packed images (only relevant if reflashing):
`MU1316-RCC_fs0.bin` (two copies), `efs-system.efs`, and the MMX cache
`persistence.sqlite`.

---

## On-unit reader — C (`fecvin.c`)

No crypto lib exists on the RCC (`EscRsa` is static inside MIBRoot), and none is
needed: FecManager already checked the signature at boot. This reader is
structural only, so it works on a forged-signature container. Read-only,
`O_NOFOLLOW`, size-capped, retries `ENOENT` (installs happen via `rename()`),
VIN to stdout only (PII — keep it out of slog).

```c
/* cc -o fecvin fecvin.c   (QNX: qcc -Vgcc_ntoarmv7le -o fecvin fecvin.c) */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FEC_PATH "/mnt/efs-persist/FEC/FecContainer.fec"
#define SIG_LEN  128
#define VIN_OFF  20    /* record starts at 8, VIN at record+12 */
#define VIN_LEN  17
#define MAX_FEC  65536
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

static unsigned long le32(const unsigned char *p)
{
	return (unsigned long)p[0] | (unsigned long)p[1] << 8 |
	       (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;
}

/* structural sanity: a short read or a half-written container must not pass */
static const char *check(const unsigned char *b, size_t n)
{
	unsigned long reclen;
	int i;

	if (n < VIN_OFF + VIN_LEN + 1 + SIG_LEN) return "container too short";
	reclen = le32(b + 4);
	if (reclen < VIN_OFF - 8 + VIN_LEN + 1 + SIG_LEN || 8 + reclen > n)
		return "bad record length";
	for (i = 0; i < VIN_LEN; i++)
		if (!strchr("ABCDEFGHJKLMNPRSTUVWXYZ0123456789", b[VIN_OFF + i]))
			return "VIN field is not a VIN";      /* ISO 3779: no I, O, Q */
	if (b[VIN_OFF + VIN_LEN] != 0) return "VIN not NUL-terminated";
	return NULL;
}

/* FecManager installs by rename(), so a concurrent install shows up as ENOENT */
static ssize_t slurp(const char *path, unsigned char *buf, size_t cap)
{
	struct stat st;
	ssize_t got = 0, r;
	int fd, tries;

	for (tries = 0; tries < 3; tries++) {
		fd = open(path, O_RDONLY | O_NOFOLLOW);
		if (fd >= 0) break;
		if (errno != ENOENT) return -1;
		sleep(1);
	}
	if (fd < 0) return -1;
	if (fstat(fd, &st) < 0) { close(fd); return -1; }
	if (!S_ISREG(st.st_mode) || st.st_size > (off_t)cap) {
		close(fd);
		errno = EINVAL;
		return -1;
	}
	while ((r = read(fd, buf + got, cap - got)) > 0) got += r;
	close(fd);
	return r < 0 ? -1 : got;
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : FEC_PATH;
	unsigned char buf[MAX_FEC];
	const char *bad;
	ssize_t n = slurp(path, buf, sizeof buf);

	if (n < 0) return perror(path), 1;
	if ((bad = check(buf, (size_t)n)) != NULL)
		return fprintf(stderr, "%s: %s\n", path, bad), 1;
	printf("%.*s\n", VIN_LEN, buf + VIN_OFF);   /* stdout only */
	return 0;
}
```

Pure-shell fallback (RCC has `grep`/`cat`/`ksh`, no `dd`/`strings`/`od`):
```sh
grep -ao '\(WAU\|TRU\|WA1\|WVW\|VSS\|TMB\)[A-HJ-NPR-Z0-9]\{14\}' \
     /mnt/efs-persist/FEC/FecContainer.fec
```
Do **not** use `grep -ao '[A-HJ-NPR-Z0-9]\{17\}'` — the byte before the VIN is
`0x56 = 'V'`, so an unanchored 17-run returns `V…` shifted by one. Anchor on the
WMI or use the fixed offset.

---

## Host / dump reader — Python (`fecvin.py`)

Structural VIN + advisory signature. Extracts VIN even with a broken signature;
`sig_status()` tells you whose key signed it (or that none did).

```python
import glob, hashlib, os, re, struct, sys

SIG_LEN, VIN_OFF, VIN_LEN = 128, 20, 17
VIN_CHARS = "ABCDEFGHJKLMNPRSTUVWXYZ0123456789"       # ISO 3779: no I, O, Q
DIGEST_INFO = bytes.fromhex("3021300906052b2403020105000414")  # RIPEMD-160

def read_vin(blob):
    """VIN as a string, tolerant of a broken/forged signature. None if absent."""
    fixed = blob[VIN_OFF:VIN_OFF + VIN_LEN]
    if len(fixed) == VIN_LEN and all(chr(c) in VIN_CHARS for c in fixed) \
       and blob[VIN_OFF + VIN_LEN:VIN_OFF + VIN_LEN + 1] == b"\x00":
        return fixed.decode()
    for m in re.finditer(rb"[%s]{17}\x00" % VIN_CHARS.encode(), blob):
        return m.group()[:VIN_LEN].decode()      # layout shifted: scan fallback
    return None

def sig_status(blob, keydir):
    """Advisory: 'OEM key <name>' | 'no key verifies ...' | 'no keys ...'."""
    if len(blob) < 8 + SIG_LEN:
        return "container too short to hold a signature"
    reclen = struct.unpack_from("<I", blob, 4)[0]
    sig_off = 8 + reclen - SIG_LEN
    if not (0 < sig_off <= len(blob) - SIG_LEN):
        return "record length bogus - signature not locatable"
    want = hashlib.new("ripemd160", blob[8:sig_off]).digest()
    sig = blob[sig_off:sig_off + SIG_LEN]
    keys = sorted(glob.glob(os.path.join(keydir, "*_signed.bin"))) if keydir else []
    if not keys:
        return "no keys to check against"
    for path in keys:
        key = open(path, "rb").read()
        if len(key) < 160:
            continue
        m = pow(int.from_bytes(sig, "big"), int.from_bytes(key[128:160], "big"),
                int.from_bytes(key[:128], "big")).to_bytes(SIG_LEN, "big")
        if m.startswith(b"\x00\x01"):
            i = m.find(b"\x00", 2)
            end = i + 1 + len(DIGEST_INFO) + 20
            # e=3: DigestInfo+hash MUST be right-aligned (end at SIG_LEN), else a
            # Bleichenbacher cube-root forgery with a garbage tail would pass.
            if i > 10 and set(m[2:i]) == {0xFF} and end == SIG_LEN and \
               m[i + 1:].startswith(DIGEST_INFO) and m[end - 20:end] == want:
                return "OEM key " + os.path.basename(path)
    return "no key verifies (forged / toolbox-patched)"

if __name__ == "__main__":
    if not 2 <= len(sys.argv) <= 3:
        sys.exit("usage: fecvin.py FecContainer.fec [Keys/FECKey]")
    blob = open(sys.argv[1], "rb").read()
    vin = read_vin(blob) or sys.exit("no VIN in %s" % sys.argv[1])
    print(vin)
    print("  signature:", sig_status(blob, sys.argv[2] if len(sys.argv) == 3 else None),
          file=sys.stderr)
```

Note `ripemd160` needs OpenSSL legacy provider; Python's `hashlib.new("ripemd160")`
works where the platform OpenSSL still ships it (it did here). If missing, drop
signature verification — VIN reading does not depend on it.

---

## VIN sanity (optional, prevents fat-finger before writing anywhere)

North-America / China VINs carry a check digit at position 9. Verified: this
dump's VIN check digit is `5`, matching.

```python
TRANS = {**{c: i for i, c in enumerate("0123456789")},
         **{c: v for c, v in zip("ABCDEFGHJKLMNPRSTUVWXYZ",
             [1,2,3,4,5,6,7,8,1,2,3,4,5,7,9,2,3,4,5,6,7,8,9])}}
WEIGHTS = [8,7,6,5,4,3,2,10,0,9,8,7,6,5,4,3,2]

def check_digit(vin):
    r = sum(TRANS[c] * w for c, w in zip(vin, WEIGHTS)) % 11
    return "X" if r == 10 else str(r)
```

---

## Recommended: proportionate build↔VIN binding

The pragmatic middle. Threat model: stop grep-and-clone and casual byte-patching
(the toolbox-copy crowd); accept that a determined rooted reverser eventually
extracts the key (that ceiling needs an off-box secret — TEE/server — which is
the heavier option). No VM, no code scatter, no ASM desync: those don't move the
ceiling and they cost runtime + maintainability.

**Principle:** the value the build actually needs at runtime is stored *keyed by*
`KDF(secret, VIN, CRC(own code))`. Not a boolean `if (vin == …)` (one `nop` kills
it). Wrong VIN → wrong key → garbage. Patched check code → CRC shifts → garbage.
No plaintext VIN, no plaintext payload in the image. One HMAC per boot, off any
hot path.

Reference (stdlib only, tested — right VIN+code returns the payload; wrong VIN /
patched code / wrong secret all return None; `strings` on the blob leaks nothing):

```python
import hashlib, hmac

def _keystream(key, n):                      # sha256 counter-mode, dependency-free
    out, c = b"", 0
    while len(out) < n:
        out += hashlib.sha256(key + c.to_bytes(4, "big")).digest()
        c += 1
    return out[:n]

def _kdf(secret, vin, code_crc):
    return hmac.new(secret, vin + code_crc.to_bytes(4, "big"), hashlib.sha256).digest()

MAGIC = b"OK"                                 # consumer checks this, not an if(vin==)

def provision(secret, vin, payload, code_crc):
    """Build-time: emit the blob baked into the image. No plaintext secrets in it."""
    K = _kdf(secret, vin, code_crc)
    body = MAGIC + payload
    return bytes(a ^ b for a, b in zip(body, _keystream(K, len(body))))

def unlock(secret, vin, blob, code_crc):
    """Runtime: derive from the LIVE vin + own code. None if it doesn't bind."""
    K = _kdf(secret, vin, code_crc)
    body = bytes(a ^ b for a, b in zip(blob, _keystream(K, len(blob))))
    return body[len(MAGIC):] if body[:len(MAGIC)] == MAGIC else None
```

On device (C shape): `_fn_start`/`_fn_end` from the linker script bound the check
function, `crc32` over that region is `code_crc`, one `hmac_sha256(secret,
vin||crc)` gives `K`, XOR the baked blob with a sha256-CTR keystream, accept iff
it starts with `MAGIC`. The live VIN comes from the quorum reader below, not a
single source.

**Cheap hygiene that does most of the anti-clone work (near-zero cost):**
- Strip the binary; no symbol names on the check/unlock functions.
- No log lines or format strings near it. (MIBRoot ships `[SERV] VIN: %s`,
  `Send VIN: %s` — that is exactly what a lazy reverser greps; yours must be
  silent. Never log the VIN or the payload.)
- `secret` and `code_crc` as plain constants, not obviously-magic values.
- These three alone defeat the grep-and-copy clone for almost nothing.

**Honest scope:** `secret` is baked into the build, so it is extractable by a
determined attacker who runs the code once (oracle) — this is anti-clone /
anti-casual-patch grade, not cryptographic. To harden the *same design* later,
move `secret` to per-unit secure storage or a server challenge; the code above
does not change, only where `secret` comes from.

## Protector as a build-time pass (recommended architecture)

You write the project in plain C, however you like. Protection is a **separate
post-build tool** that layers on top of the compiled artifact and stamps in the
checksum + VIN-keyed math. The source stays clean and maintainable; none of the
crypto/anti-tamper logic lives in it.

**Pipeline:**
```
  your normal .c  ──cc──▶  ELF with a marked slot + a linked-in rt (plain C)
                              │
                    protector (per flash, knows the target VIN + build secret)
                              │  - CRC32 over the marked code region
                              │  - stamp CHK slot = CRC
                              │  - stamp ENC slot = payload ⊕ KDF(secret, VIN, CRC)
                              ▼
                        provisioned image for ONE VIN
  boot ▶ rt: CRC own region ─▶ derive K(secret, live-VIN, CRC) ─▶ decode ENC
              wrong VIN → garbage │ patched region → CRC≠CHK → garbage
```

The developer adds **one thing** — a marked slot the protector fills:

```c
__attribute__((used)) volatile uint8_t PROT_SLOT[24] = {
    'P','R','O','T','|','1',      /* locator magic */
    0xCE,0xC0,0xDE,0xC0,          /* CHK placeholder (LE u32) -> real code CRC */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0   /* ENC placeholder -> payload ^ K(VIN,CRC) */
};
```

The protector (post-build; `_ks`/`_kdf` shared with the binding reference above):

```python
import zlib, sys
MAGIC = b"PROT|1"

def protect(path, secret, vin, payload):
    b = bytearray(open(path, "rb").read())
    i = b.find(MAGIC)
    if i < 0: sys.exit("no PROT slot")
    region = bytes(b[:i]) + b"\0"*24 + bytes(b[i+24:])   # code with slot blanked
    crc = zlib.crc32(region) & 0xffffffff
    K = _kdf(secret, vin, crc)
    b[i+6:i+10]  = crc.to_bytes(4, "little")             # stamp CHK
    b[i+10:i+24] = bytes(a ^ c for a, c in zip(payload.ljust(14, b"\0"), _ks(K, 14)))
    open(path, "wb").write(b)
    return crc
```

On-device `rt` mirrors it: CRC its own region, compare to CHK (integrity), derive
`K` from the **live** VIN (quorum reader below), decode ENC. Verified end-to-end
on a real compiled binary: right VIN untouched → payload; wrong VIN → garbage;
one flipped code byte → `TAMPERED`.

**Two constraints that shape the design — decide these explicitly:**

1. **Compile-time cannot know the runtime VIN** unless the build is provisioned
   per VIN. So the clean split is: *compile once, generic* → *protector stamps
   per-VIN at flash/provision time* (as above). One image per car. If you insist
   on a single generic image for all cars, the protector can only inject the
   *mechanism*; the key is then fully runtime and the per-VIN binding is weaker.
   **Recommended: stamp at provisioning.**

2. **Do not mutate *all* the math.** Auto-keying every arithmetic op kills the
   optimizer, tanks performance on the embedded target, and is fragile — for no
   ceiling gain (still extractable). Protect the **marked region / the decision
   path** only. Full IR-level math obfuscation (obfuscator-LLVM style) is a much
   bigger, heavier tool; out of proportion here. Mark what matters, leave the
   rest fast and debuggable.

Same honest ceiling as everywhere in this doc: the build secret is in the image,
so a rooted attacker who runs it once extracts `K`. The protector raises
clone/patch cost; it does not change the ceiling. Move `secret` to per-unit
secure storage or a server challenge when you need the real wall.

## How robust is this, honestly

Two different questions — keep them apart:

**Reading the VIN: robust.** Fixed offset 0x14 confirmed on 3 independent copies
(active FEC, backup FEC, both embeds in `RCC_fs0.bin`); a scan fallback covers a
shifted layout; structural checks reject half-written / truncated containers.
Known limits: (a) offset assumes `version == 1` — a different FEC format version
could move it, the scan fallback mitigates but can false-positive on a 17-char
non-VIN run, so prefer the fixed offset and only fall back; (b) `ripemd160`
depends on the platform OpenSSL legacy provider.

**Trusting the VIN: NOT robust, and cannot be made so from the container alone.**
Field FEC signatures are routinely forged/toolbox-patched, so a valid-looking
container proves nothing — it tells you what the blob *claims*. Anyone with write
access to `efs-persist` can put any VIN at 0x14. The signature advisory only
distinguishes "OEM key signed this" from "nobody's key did"; it does not upgrade
a forged VIN into a trusted one. **Do not build anything security-relevant on
"the FEC says VIN X" from a single source.**

## Preventing a swapped / spoofed VIN reader

"One reader on one source" is the weak model — swap the reader, or swap the one
file it reads, and you win. Harden in layers, cheapest first:

**1. Quorum across independent sources (do this first).** The head stores the VIN
in several places a swapper would have to forge *in lockstep*: active FEC, backup
FEC, the embeds in the flashed image, the MMX cache (`/core/vin`,
`/core/registeredVin` in `persistence.sqlite`), and — on-car — the live bus copy
`/dev/shmem/VIN.txt`. Read them all; a lone edited source shows up as a minority.
Verified: consistent sources → agreement; one swapped active FEC → `CONFLICT`.

```python
import re, sys

VIN = re.compile(rb"(?:WAU|WA1|TRU|WVW|WV1|VSS|TMB|1V[WY])[A-HJ-NPR-Z0-9]{14}")

def _first(path, pat=VIN):
    try:
        m = pat.search(open(path, "rb").read())
        return m.group().decode() if m else None
    except OSError:
        return None

def sources(root):
    """(label, vin) for every independent VIN copy under a unit/dump root."""
    R = root.rstrip("/")
    yield "fec.active",     _first(R + "/mnt/efs-persist/FEC/FecContainer.fec")
    yield "fec.backup",     _first(R + "/mnt/efs-system/backup/FEC/FecContainer.fec")
    yield "shmem.live",     _first(R + "/dev/shmem/VIN.txt")            # only on-car
    yield "mmx.registered", _first(R + "/mnt/persist/var/fw/persistence.sqlite",
                                   re.compile(rb'registeredVin","d":"([A-HJ-NPR-Z0-9]{17})'))

def quorum(root):
    seen = {}
    for label, vin in sources(root):
        if vin:
            seen.setdefault(vin, []).append(label)
    if not seen:
        return None, "no VIN in any source", seen
    if len(seen) == 1:
        vin, who = next(iter(seen.items()))
        return vin, "agreed by %d source(s): %s" % (len(who), ", ".join(who)), seen
    return None, "CONFLICT - sources disagree, do not trust", seen
```

**2. Integrity of the reader itself.** A swapped reader binary makes all of the
above moot. Run the reader from the signed, read-only ifs image (rootifs /
`ifs3`), where it already lives — never a copy dropped into writable
`efs-persist`. Pin its known SHA-1 (`sha1sum` exists on the RCC) and check it
before trusting output. Hardcode the source paths in production (drop the `argv`
override), keep `O_NOFOLLOW`.

**3. The only thing that actually stops a determined swap: your own secret.**
Cross-checks raise the bar but every source above is attacker-writable; nothing
in the container carries a secret you hold. If *you* provision the units, bind
the VIN with a MAC the swapper can't reproduce — `HMAC-SHA256(your_key, VIN)`
stored out-of-band, or sign the VIN with your private key and verify with a
public key baked into your signed image. Then a swapped VIN fails your MAC even
if every on-device copy was edited in lockstep. Without a secret, "can't be
substituted" is unachievable — you can only make substitution *detectable*
(layers 1–2), not *impossible*.

## Boundaries / open items

- **No re-signing.** Editing the signed record (VIN or licence list) cannot be
  re-signed without the OEM private key. Legitimate VIN change for a retrofit
  goes through component protection (adaptation + SVM / ODIS), not by hand-editing
  a signed FEC.
- **Key chain not walked.** `sig_status` checks the container against the FEC
  public key as-is; it does not verify that key up to DataKey. Fine for "is this
  the OEM key or not", not a full trust decision.
- **Unsigned tail (0xC7+) and the FEC licence list (0x26+) are not parsed** —
  out of scope for VIN. Parse the list if you ever need "which features are on".
