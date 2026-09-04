# Stereo upmix WAV renderer

This host-only tool runs a 48 kHz, signed 16-bit stereo WAV through the exact
`hook/audio/stereo_upmix.c` implementation used by the CarPlay hook. It writes
an uncompressed six-channel WAV in this explicit order:

`L, R, C, LFE, Ls, Rs`

The output uses `WAVE_FORMAT_EXTENSIBLE` and the `FL|FR|FC|LFE|SL|SR` speaker
mask, so players do not have to guess the channel order. The current algorithm
keeps dry audio in L/R, writes digital zero to C, derives conservative
decorrelated ambience for Ls/Rs, and can synthesize a restrained bass-assist
signal in LFE. Native APAC 5.1 never passes through this tool or the stereo
upmixer: its original C, LFE, Ls, and Rs remain untouched.

Build from the repository root:

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror \
  -Ihook/audio \
  tools/stereo-upmix-render/stereo_upmix_render.c \
  hook/audio/stereo_upmix.c \
  -o /tmp/stereo_upmix_render
```

Render with the production candidate rear gain (about `0.18`, hard maximum
`0.25`) and explicitly enable bass assist:

```sh
/tmp/stereo_upmix_render \
  --mode ambient-lite --lfe assist --surround 0.18 \
  input_stereo_48k_s16.wav output_5_1.wav
```

The renderer defaults to `--lfe off`, making every audition unambiguous. Bass
assist is enabled only by the explicit `--lfe assist` above. The checked-in
vehicle startup selects assist explicitly; changing the renderer default does
not alter that deployment setting. The exact synthesis is:

```text
LFE = (5833 / 32768) * LR4_80Hz((L + R) / 2)
```

`LR4_80Hz` is a fourth-order Linkwitz-Riley low-pass made from two identical
second-order Butterworth sections at 80 Hz. Enabling it pairs the LFE gain
(`5833`, about `0.178`) with a front gain of `30802` (about `0.940`) to reserve
headroom. `--lfe off` restores the non-assist front gain of `32670` (about
`0.997`) and emits digital zero in LFE.

Render the fixed six-channel rail with the ambience generator disabled:

```sh
/tmp/stereo_upmix_render --mode off input_stereo_48k_s16.wav output_dry.wav
```

In `off` mode, the output is exactly `L,R,0,0,0,0`; it intentionally disables
the active profile's front trim so this file is also a compatibility oracle for
the hook's old widening path. This mode forces LFE off and unity L/R even if
`--lfe assist` is given. The tool reports the effective front, surround, and
LFE gains after every render.

The parser accepts integer PCM in classic WAV or PCM
`WAVE_FORMAT_EXTENSIBLE`, walks arbitrary RIFF chunks (including an unusual
`data`-before-`fmt` ordering), and rejects any input that is not exactly stereo,
48 kHz and 16 valid bits. Processing is streamed in blocks no larger than the
hook's 1024-frame realtime limit; no whole-track PCM buffer is allocated.

For a quick geometry check:

```sh
ffprobe -v error -show_entries \
  stream=codec_name,sample_rate,channels,channel_layout,duration \
  -of default=noprint_wrappers=1 output_5_1.wav
```
