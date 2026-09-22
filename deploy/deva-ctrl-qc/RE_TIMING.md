# `mpl5_dio_ent` 6-channel timing analysis

Status: this document closes the **DMA-side** 3x-slow timing fault. The dynamic
fragment fix below is implemented in `patch_deva.sh`, assembled/disassembled
successfully, and was staged on the unit. A later direct vehicle test proved
that its six-channel DMA geometry and clock are correct, but audio is still
broken because the independently created CSD stream remains stereo. The current
binary is therefore not a complete 5.1 fix. See
[`../../docs-obsidian/Sources/ROUTER_INPUT_WIDTH_AUDIT.md`](../../docs-obsidian/Sources/ROUTER_INPUT_WIDTH_AUDIT.md).

## Observed failure

The capability-only patch successfully widened `mpl5_dio_ent` from 2 to 6 voices. On the unit:

- QSA accepted `48000 Hz / 6 voices` and routed all six channels to the correct speakers.
- The plugin-facing fragment was 3072 bytes: 256 frames of 6-channel S16.
- One fragment completed every 16 ms, so the effective rate was 16,000 frames/s.
- Playback was therefore exactly three times slow; the APAC ring filled while hook underruns stayed
  at zero.

The earlier "stereo byte clock" description was directionally correct, but the byte conversion is
important: the MIB device is internally S32, while CarPlay supplies S16 through the QSA plugin layer.

### 2026-09-02 follow-up

The current patched driver was subsequently exercised directly, with CarPlay
entirely out of the path. `qsa_play` into `mpl5_dio_ent` negotiated six voices,
a 9216-byte S16 client fragment (the expected 768 frames), and delivered
2,445,312 frames in 50,822 ms: **48,115 frames/s for 48,000 expected**. Two
voices sounded excellent; six voices sounded broken with the same ticking and
wrong pitch as CarPlay.

A later controlled same-device A/B recorded the exact paired values: two
voices delivered 2,445,312 frames in 50,831 ms (**48,107 frames/s**) and sounded
excellent; six delivered them in 50,807 ms (**48,129 frames/s**) and sounded
broken. The earlier 48,115 run and both A/B rates tell the same story: the DMA
clock is not the remaining fault.

Static follow-up found a second consumer of the record minima.
`deva_update_session_info_from_list @0x54848` copies `min_voices=2` to CSD
session `+0x84` and `min_fragsize=0x1800` to session `+0x6c`. The helper in this
document widens only the DEVA timer/DMA side to `0x4800`; it does not rebuild
the CSD stream, context, or buffer pool. Consequently the timer advances
`0x4800` bytes while `session_csd_stream_playback` copies and submits only
`0x1800` bytes per tick.

## Verified driver path

The relevant `deva-ctrl-qc.so` locations are:

| Address | Finding |
|---:|---|
| `0x5707C` | `deva_pcm_and_mixer_device_init_platform_mib` copies each 48-byte caps record into the ADO hardware caps. |
| `0x563C8` | `deva_mib2_aquire` loads the active DMA fragment from record `min_fragsize` (`+0x1C`). It does not use `max_fragsize`. |
| `0x563CC..0x563E0` | The driver fixes `frag_total=10`, stores `frag_size`, allocates `10 * frag_size`, then starts the session timer. |
| `0x580F0`, `0x580F4` | `mpl5_dio_ent` timer old/new timeout is hardcoded to 16,000,000 ns. |
| `0x6ED58` | `session_deva_timer_start` programs the POSIX interval from that timeout. |
| `0x6E85C` | Each timer pulse advances the DMA cursor by exactly one active fragment. |

`mpl5_dio_ent`'s caps record is S32_LE (`format=12`). Its stock active geometry is therefore:

```text
2 channels * 4 bytes * 48,000 frames/s * 0.016 s = 6,144 bytes = 0x1800
```

For six channels the same 16-ms timer requires:

```text
6 channels * 4 bytes * 48,000 frames/s * 0.016 s = 18,432 bytes = 0x4800
```

The QSA plugin converts the S16 client stream to the S32 hardware stream. Consequently the bad
internal `0x1800` fragment appeared to the hook as `0x0C00` (3072 bytes), while the correct internal
`0x4800` fragment should appear as `0x2400` (9216 bytes): 768 six-channel S16 frames per 16-ms tick.

## Why the current caps patch is insufficient

The current patch changes `mpl5_dio_ent` as follows:

```text
max_voices    2      -> 6
max_fragsize  0x1800 -> 0x4800
max_buffer    0xF000 -> 0x2D000
```

It intentionally leaves `min_fragsize=0x1800` so ordinary stereo remains unchanged. That also means
`deva_mib2_aquire` still selects `0x1800` after a six-voice open. The raised maximum permits 6ch
negotiation but never reaches the active DMA geometry.

Changing `min_fragsize` statically is not acceptable: it would also make stock 2ch use the 6ch-sized
DMA fragment. The fix must select the active fragment from the negotiated voice count.

## DMA-side dynamic fix (implemented; necessary but insufficient)

Replace the load at `0x563C8` with a branch to a five-instruction helper:

```armasm
    ldr   r2, [r5, #0x1c]   // original caps.min_fragsize
    ldr   r1, [r6, #0x10]   // negotiated voices
    cmp   r1, #6
    moveq r2, #0x4800       // 6ch S32, 768 frames per 16 ms
    bx    lr
```

This condition is selective in the current 31-record MIB table:

- `mpl1_int_ent` is already fixed at 6ch/`0x4800`, so the assignment is a no-op there.
- patched `mpl5_dio_ent` is the only other record capable of 6 voices; it changes to `0x4800` only
  for a negotiated 6ch open.
- ordinary `mpl5_dio_ent` stereo still loads its stock `0x1800`.

There is no 20-byte zero/code cave in `.text`. A workable candidate is the tail of the compiler
source-path string at `0x8F2C8`; it belongs to the executable first `PT_LOAD` segment. Terminating the
string one byte earlier only shortens a diagnostic `__FILE__` path. The assembled patch is:

| Address | Original | Proposed |
|---:|---|---|
| `0x563C8` | `1c 20 95 e5` (`ldr r2,[r5,#0x1c]`) | `be e3 00 eb` (`bl 0x8f2c8`) |
| `0x8F2C7` | `63` (`c`) | `00` (terminate diagnostic path) |
| `0x8F2C8` | tail of `deva_srf_platform.c` | `1c 20 95 e5 10 10 96 e5 06 00 51 e3 12 2b a0 03 1e ff 2f e1` |

The helper was assembled and linked as ARM code, then disassembled back to the five instructions
above. It preserves all live registers except scratch `r1`/condition flags; both are dead at the
patched call site. The original load result remains in `r2`, and the existing code still stores the
fragment, allocates ten fragments, and starts the unchanged 16-ms timer.

Using string storage as executable space is valid for this ELF because `.rodata` shares the `R E`
load segment, but it is still a binary-injection tradeoff. An ELF section/segment extension would be
cleaner and materially more invasive.

The helper remains correct and necessary: without it, six-channel DMA itself
runs at one third speed. It is insufficient because no acquire, prepare, or
trigger path updates the already-created CSD session. The production fix must
derive the CSD channel count and buffer size from the negotiated PCM channel and
rebuild the stream/context/buffer pool on a 2-to-6 or 6-to-2 transition before
the timer starts. Merely overwriting the two session fields after allocation is
unsafe.

## Hook implication (implemented and staged)

The first guard rejected any negotiated fragment whose frame count differed from the caller's
original request. That caught the broken `256 frames / 16 ms` result, but would also have rejected the
expected fixed hardware period:

```text
params request:       1920-byte stereo S16 = 480 frames
correct QSA result:   9216-byte 6ch S16    = 768 frames
setup translated:    3072-byte stereo S16 = 768 frames
```

The staged hook now tests the MIB period instead: at 48 kHz the actual 6ch fragment must contain 768
frames for the fixed 16-ms timer. It checks the empirical params writeback before the first sample
and independently validates the authoritative `snd_pcm_plugin_setup()` readback. QNX documents
`snd_pcm_channel_params_t` members as write-only, even though this libasound build rewrites them.

## Historical DMA acceptance criteria

These criteria describe the DMA-side change. Items 1 and 2 were later measured
successfully, but they do not validate the independent CSD consumer:

1. QSA setup reports `6ch / 48000 Hz / 9216-byte fragment` (and the hook translates that to 3072
   stereo-facing bytes).
2. Each write contains 768 output frames and completes about every 16 ms.
3. `bufferedWrites` no longer grows monotonically; the APAC ring remains bounded without sustained
   drops or underruns.
4. After the CSD re-provision fix, all six speaker channels remain correctly routed and audio is clean.
5. With spatial disabled, `mpl5_dio_ent` still opens 2ch with the original internal `0x1800`
   geometry.

## Staged artifacts (2026-08-31, no restart)

- `/mnt/app/armle/lib/dll/deva-ctrl-qc.so` — SHA-256
  `8b55b8d8f02b97d6f30191b54ef8a847e4706067f21cbf068d4b02a563ea84c9`.
- `/mnt/app/root/hooks/libcarplay_hook.so` — SHA-256
  `6279a04127d3d82b942e67ef69eb015264382b16e1a5a078bffe3f8b5e15deda`.
- Both files were streamed back from the unit and compared byte-for-byte with the local artifacts.
- The running `io-audio` PID remained unchanged and `dio_manager` remained stopped; nothing was
  restarted.
- Persistent profile was `AUDIO_SPATIAL=0` at the time, to keep the old mapped driver generation
  from being exercised before reboot. **As of 2026-09-02 the default is `AUDIO_SPATIAL=1`**: the
  geometry defect is fixed and confirmed on the car, and stereo works on the same driver.
