# tools/keys

Key material used by the type-103 / APAC analysis.

**Default is ignored.** `.gitignore` excludes everything here except files opted back in
by name. Live AirPlay session keys — the 32-byte `shk` from a type-103 SETUP — are secret
per session and must never be committed, printed or pasted; keep captures mode 0600 and
local (see `docs/audio/BUFFERED_AUDIO_PATH.md` §6).

## Tracked files

| file | what | provenance |
|---|---|---|
| `pioneer_24md_rsa_modulus.hex` | 2048-bit RSA **public** modulus, 256 bytes as 512 hex chars | Extracted from Pioneer SPHERA 1.030010 (`24MD` container) during the type-103 receiver study — see `docs/audio/BUFFERED_AUDIO_PATH.md` and `tools/pioneer_24md.py`. Public verification value only; no private component is present or derivable from it. |
