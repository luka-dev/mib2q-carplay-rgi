# QSA virtual-device to CSD topology audit

Date: 2026-09-02

> **Scope correction from the router-width audit:** the shared `dev_id=78`
> result is still correct. The claim that both routed inputs therefore inherit
> the same six-channel stream/context is not. Route setup rebuilds the shared
> session from the attached timer's static `min_*` fields: MPL1 supplies
> `6/0x4800`, while MPL5 supplies `2/0x1800`. See
> [`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md).

## Result

The proposed per-virtual-channel CSD-device asymmetry does not exist in this
driver. `mpl1_int_ent` and `mpl5_dio_ent` are DEVA input timers, not independent
CSD/ACDB devices. When either input is connected to the entertainment TDM
output, the mixer router attaches it to the same CSD playback session:

```text
mpl1_int_ent (ENT input 1) --+
                              +--> TDM_OUT_MPL (output 128)
mpl5_dio_ent (ENT input 5) --+        |
                                      +--> CSD device 0x4e (78)
                                           48000 Hz, 24 bits
                                           width chosen at input attach:
                                             input 1 -> six channels
                                             input 5 -> stereo
```

There is therefore no separate stereo `mpl5` CSD device to widen and no
different ACDB device id for the two paths. That clean negative applies only to
the device id and ACDB selection. It does not imply equal stream width: MPL5
does retain stereo CSD stream geometry while its DMA timer is widened.

The route-lifetime distinction led to the positive finding. `mpl1_int_ent ->
TDM_OUT_MPL` is installed at startup and uses MPL1's six-channel minimum.
`mpl5_dio_ent` is attached dynamically by the external mixer/Audioman route,
whose session updater uses MPL5's stereo minimum. A PCM open alone does not
correct that session.

## Keep the id namespaces separate

Four different number spaces appear in the logs and code. Treating one as a
CSD `dev_id` produces the apparent contradiction in the original hypothesis.

| Meaning | `mpl1_int_ent` | `mpl5_dio_ent` | Evidence |
|---|---:|---:|---|
| QSA playback index (zero-based) | 0 | 3 | `qsa_probe` device enumeration |
| User-facing ordinal | 1 | 4 | device names / task wording |
| ENT mixer-router input id | 1 | 5 | router endpoint lookup at `0x50c74` |
| DEVA timer id printed by slog | 1 | 4 | `deva_release` / `deva_trigger` slog |
| Mixer-router output id | 128 | 128 | endpoint lookup at `0x50c74` |
| CSD OEM device id after routing | `0x4e` (78) | `0x4e` (78) | shared output session and CSD payloads |

The timer id and router input id happen to agree for `mpl1`, but do not agree
for `mpl5`. Neither is an ADSP device id.

## Harness measurement

The IFS builder gained an opt-in measurement mode. It changes no hook, deva
binary, or deva patch. It only selects the QSA playback index, suppresses the
unrelated mixer-outlet capture, disables submission-order callback emulation,
and dumps slog after the probe:

```sh
QSA_DEVICE_TOPOLOGY=1 QSA_PROBE_DEVICE=0 QSA_PROBE_VOICES=6 \
  ./tools/qsa-emu/build_ifs.sh

QSA_DEVICE_TOPOLOGY=1 QSA_PROBE_DEVICE=3 QSA_PROBE_VOICES=6 \
  ./tools/qsa-emu/build_ifs.sh
```

Both probes negotiated six voices, 48 kHz, an 18,432-byte deva fragment and a
9,216-byte S16 client fragment. Both wrote 62 fragments; the existing ring
validator reported 1,142,784 widened bytes and zero mismatches. The comparison
below starts at the `>>> drive playback device ...` marker, after the common
startup graph.

The filtered `CSD_OPEN_*` and `CSD_IOCTL_*` startup traces are byte-for-byte
identical through that marker. The two images differ only in which QSA endpoint
the later probe selects.

| Post-marker event | `mpl1_int_ent` | `mpl5_dio_ent` |
|---|---:|---:|
| new `csd_open` calls | 0 | 0 |
| `CSD_DEV_CMD_CONFIGURE` (`0x11c0a`) | 30 | 0 |
| `CSD_DEV_CMD_ENABLE` (`0x11c01`) | 30 | 0 |
| stream start (`0x11402`) | 30 | 0 |
| stream stop (`0x11404`) | 30 | 0 |
| `CSD_DEV_CMD_DISABLE` (`0x11c02`) | 30 | 0 |
| final timer type | CSD-backed `stream_type=2` | void `stream_type=1` |

The repeated start/stop cycles come from the probe's recovery behaviour in the
stub environment. One cycle is sufficient to identify the device-side data;
all observed cycles carry the same relevant fields. The serial stream
interleaves some zero-length flush records, so its raw count is not used as a
semantic result.

The first `mpl1` cycle is:

```text
seq=196 handle=256 cmd=0x11c0a len=324
  +0000  01 00 00 00 4e 00 00 00 13 1c 01 00 02 00 00 00
  +0010  00 00 00 00 00 00 00 00 00 00 00 00 fc 00 00 00
  +0020  00 00 00 00 00 00 00 00 18 00 00 00 ...

seq=197 handle=256 cmd=0x11c01 len=100
  +0000  01 00 00 00 4e 00 00 00 80 bb 00 00 18 00 00 00
          opcode=1       dev=78       rate=48000     bits=24

seq=198 handle=257 cmd=0x11402 len=12
  +0000  00 00 00 00 00 00 00 00 00 00 00 00
```

The matching disable payload is opcode 1 plus device id 78. There is no channel
count in the 100-byte enable payload.

In contrast, isolated `mpl5` slog repeatedly reports:

```text
deva_trigger: device_id = 4 <deva_timer_play_mpl5_dio_ent>, stream_type=1
```

`stream_type=1` selects `session_void_stream_buffer`, which clears/discards a
fragment. It generates no post-marker `csd_open` or `csd_ioctl`. This harness
does not run the vehicle's Audioman process, so it cannot install the dynamic
`mpl5 -> TDM_OUT_MPL` route. The zero-call trace must not be read as a second,
unobserved CSD topology.

Checked artifacts:

| Artifact | SHA-256 |
|---|---|
| `build/qsa-emu/qsa-topology-mpl1.bin` | `a5cb26adf30cfbc5cce6c7d2be96f03584e2983aa5589bed5a04cc9e39616e17` |
| `build/qsa-emu/qsa-topology-mpl1.log` | `e0f9f5a27a24a52f213bc2d5b4b10271377eba086eb292b879cf38b2e25f1a8a` |
| `build/qsa-emu/qsa-topology-mpl5.bin` | `7789735a8c5f9292428d6691b687a104b68923f0f02a50cd3fbd1ed71ac7a1fb` |
| `build/qsa-emu/qsa-topology-mpl5.log` | `3ca3a4f2b66213ef06eaf1a4110417e9cd4a1c434c9c8522981f721db725962f` |

The logs and images are ignored build artifacts, not repository inputs.

## Static device and route recovery

The measured binary is the patched
`deploy/deva-ctrl-qc/deva-ctrl-qc.so`: ELF32 little-endian ARM EABI5 PIC, image
base zero, GNU build id `9168acff553fc86d6a3c991613d620fd`, SHA-256
`8b55b8d8f02b97d6f30191b54ef8a847e4706067f21cbf068d4b02a563ea84c9`.
Addresses below are image virtual addresses and equal file offsets in the
relevant executable region.

### The virtual PCM is attached to an output session dynamically

`deva_mixer_router_init @0x53b80` installs endpoint lookup callback `0x50c74`.
For MIB entertainment inputs, that lookup accepts input ids 1, 3, 4, 5, 6 and
7. Input 1 resolves to the `mpl1` DEVA timer and input 5 to the `mpl5` timer.
For each of them, output id 128 resolves to the same TDM-out CSD session object.

The same initializer installs the default route input 1 to output 128 at
`0x53d4c..0x53d58`. There is no corresponding default input-5 route.

The switch-routing implementation at `0x51b94` resolves those two endpoint
pointers, sets up the CSD session, and calls `session_csd_attach_device
@0x6dbd8`. The latter matches the channel and then calls
`session_deva_timer_attach_stream @0x6cf3c` with type 2. Type 2 starts the CSD
session. `session_deva_timer_detach_stream @0x6c2f8` restores type 1 and clears
the attached session.

`session_deva_timer_thread @0x6e2d0` dispatches by that type:

| `stream_type` | consumer |
|---:|---|
| 1 | `session_void_stream_buffer @0x64da4` |
| 2 | `session_csd_stream_buffer` |
| 4 | loopback |

This is the code-level reason a bare `mpl5` open has no CSD traffic while a bare
`mpl1` open does in the harness.

### What the measured default MPL1 `dev_id=78` graph configures

The default MPL1 six-channel startup graph uses CSD handles 256 (device), 257
(stream), and 258 (context). Its typed descriptors are:

```text
stream open, seq 2:
  format=0x1c, rate=48000, channels=6, bits=24
  signed=1, interleaved=1, map=01 02 03 06 08 09
  7 buffers * 18432 bytes

context open, seq 20:
  01 00 00 00  01 00 00 00  4e 00 00 00  80 bb 00 00
  01 00 00 00  18 00 00 00  00 00 00 00
  direction=1, dev_id=78, rate=48000, bits=24

setContextMultichannel, seq 21:
  06 00 00 00  40 f9 0f 00
  channels=6, map pointer=<guest address>
```

The context-open descriptor names the device, sample rate and bit depth, but
does not contain a channel count. Six-channel geometry is applied immediately
afterward by `setContextMultichannel` and is also present in the stream format.

`session_csd_setup_playback @0x6aba0` explains those payloads. It reads the
session device id at `session+0x50`, voices at `session+0x84`, and bit depth at
`session+0x86`. For more than two voices it:

1. opens a 32-bit-container stream and calls
   `setStreamFormatPcmMultichannel` with literal six at `0x6aec0`;
2. applies the six-channel stream map at `0x6af38`;
3. opens the context with the session `dev_id`, 48 kHz and the live bit depth at
   `0x6b1ec`;
4. applies the context's literal-six channel map at `0x6b280`.

Thus channel count is derived from session `+0x84` and set on the stream and
context. The router-width audit recovered the source of that session field:
`deva_update_session_info_from_list @0x54848` copies the attached timer record's
`min_voices`, not the live PCM voice count. The absence of a channel field in
the device-enable call remains correct.

### Start re-enables the device, but a PCM open is not the trigger

`enableDevice @0x2dec4` zeroes a 100-byte request and writes only:

```text
word 0 = 1
word 1 = dev_id
word 2 = sample_rate
word 3 = bits_per_sample
```

It issues CSD ioctl `0x11c01` at `0x2df14`. There is no voices/channel member.
The CSD/ADSP side can choose topology/calibration from `dev_id`, but both PCM
inputs reach the same id, so no per-input ACDB choice is made here.

On the observed shared TDM session path, `session_csd_start_playback @0x69fa8`
first sends the TDM SAD header through `set_device_tdm_sad_header @0x29d70`
(`CSD_DEV_CMD_CONFIGURE`, `0x11c0a`) and then calls `enableDevice` at `0x6a0e4`.
The harness sees exactly that pair on every `mpl1` start. Stop sends stream stop
and `CSD_DEV_CMD_DISABLE`.

**Subsequent qualification:** the enable request still has no channel field, but the
preceding TDM SAD request does. Its ASAD mask declares active speaker positions and is
stored independently of the PCM/CSD channel count. AirPlay has been observed writing
`ASAD=0xc0` (two active positions), while the clean session default is `0xfc` (six).
See [`AFE_TDM_MOST_SLOT_AUDIT.md`](AFE_TDM_MOST_SLOT_AUDIT.md).

The corrected answer to “does six-voice open re-provision the device/session?”
has three parts:

* a bare QSA PCM open negotiates and allocates the widened DEVA DMA ring, but
  does not itself issue CSD opens or device commands;
* CSD route/session setup copies the attached timer record's `min_voices` and
  `min_fragsize` into the active session; for MPL5 those remain stereo-sized;
* the CSD-backed route/session start re-sends device configure/enable, but its
  enable payload still carries only id 78, 48 kHz and 24 bits.

For `mpl5`, the bare PCM open performs only the ring-side part because no CSD
session is attached. Audioman's dynamic route attachment is the event that
connects and starts the same output session. There is no device configuration
left behind specifically by `mpl5` startup. SAD metadata is a separate persistent
session field and can be left behind by a previous MPL5 writer; the later AFE audit
records that exception.

## ACDB boundary and conclusion

The firmware's ACDB set is under
`system/etc/acdb/mmx2_8064/`; `acdb_cfg` selects the usual Bluetooth, Codec,
General, Global, Handset, Hdmi, Headset and Speaker blobs. The blobs are opaque
calibration databases and this audit did not decode their internal topology
records. That does not weaken the id comparison: the selection key visible at
the CSD boundary is the same `dev_id=78` for both routed inputs.

This pass still proves that a different MPL5 device id or per-input ACDB
selection cannot explain the vehicle symptom. Its former “no retained stereo
topology” conclusion is superseded: the later router audit found a retained
stereo **stream** geometry before the shared device interface.

The decisive chain is documented in
[`ROUTER_INPUT_WIDTH_AUDIT.md`](ROUTER_INPUT_WIDTH_AUDIT.md): MPL5's active DMA
fragment becomes `0x4800`, while runtime route setup keeps the CSD session at
two channels and `0x1800`. That is now the next fix/test boundary; analysis need
not move into opaque ACDB processing first.
