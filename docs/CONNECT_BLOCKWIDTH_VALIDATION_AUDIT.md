# MOST Connect BlockWidth validation audit

Date: 2026-09-02  
Scope: static analysis of the amplifier V850 image and the stock MU1316 head-unit
firmware. No firmware binary was modified.

## Result

The proposed reject mechanism is **falsified** at the amplifier's Connect
handler.

`FUN_000C4B30`, registered for local FktID `0x0112`, does not compare the
incoming BlockWidth with the profile width belonging to the requested SinkNr.
It performs these independent checks:

1. the payload length is exactly seven bytes;
2. SinkNr occurs in the seven-byte selector list at `0x00096e2c`;
3. BlockWidth is in the inclusive range `1..0x0174` (`1..372`);
4. the selected sink's runtime connection slot is available and the surrounding
   amplifier state permits the connection.

Both `14` and `26` pass the BlockWidth check. On success the handler stores the
received value in a per-sink runtime table. It never reads the selected
profile's advertised width during this operation. Therefore a Connect carrying
`SinkNr 0x26, BlockWidth 0x001a` is not rejected merely because sink `0x26`
advertises `0x000e`, and this handler cannot leave the amplifier on stereo by
that proposed mechanism.

The head-unit value `0x001a` is static configuration, not a derivation from the
CSD voice count or bits/sample. It exists in two independently compiled HU
locations:

- MIBRoot constructs source 7 with a literal `26` bandwidth;
- AudioProcess contains source-7 table record `07 00 1a` and uses its width byte
  in the outbound allocation/Connect path.

There is no single system-wide constant. A one-byte AudioProcess table edit can
change the AudioProcess-local source-7 width and hence the Connect-side value,
but that is not yet a complete, consistent source-allocation patch: MIBRoot has its own value, its literal is shared by
sources 2 and 7, and the final binding to the lower NAL resources remains
unproved.

## 1. Amplifier Connect handler

### Function layout trap

The V850 compiler split the entry and prologue in a way that can make an
automatic function view misleading:

```text
0x000c4b30  jr      0x000c4e58
0x000c4e58  prepare {r24-r29,lp}, 0x5
0x000c4e5c  jr      0x000c4b34
0x000c4b34  ... actual body ...
0x000c4e54  dispose 0x5,{r24-r29,lp},[lp]
```

The body is `0x000c4b34..0x000c4e54`, not the four-byte entry thunk alone.

### Length and SinkNr validation

The message object reports its payload length at object offset `+4`. The handler
requires seven bytes:

```text
0x000c4b46  ld.h    0x4[r25],r10
0x000c4b4a  cmp     0x7,r10
0x000c4b4c  be      0x000c4b5e
```

After consuming the two-byte SenderHandle, cursor `r26` is `2`. The handler
reads payload byte 2 as SinkNr and scans exactly seven entries beginning at
`0x00096e2c`:

```text
0x000c4b84  mov     0x96e2c,r11
0x000c4b8a  ld.w    0x0[r25],r17
0x000c4b8e  add     r26,r17
0x000c4b90  ld.bu   0x0[r17],r18       ; payload[2] = SinkNr
0x000c4b98  ld.bu   0x0[r19],r10       ; selector-list byte
0x000c4b9c  cmp     r10,r18
0x000c4b9e  bne     0x000c4ba4
0x000c4bac  cmp     0x7,r12
0x000c4bae  bc      0x000c4b8a
```

The selector bytes are:

```text
21 22 23 24 26 2a 32
```

This proves that `0x26` is accepted as a sink selector. It does not validate
any other field against that selector's profile.

### Exact read of payload offsets 3 and 4

After accepting SinkNr, the cursor advances from 2 to 3. The destination is the
half-word at `sp+4`, and the source is `payload + 3`:

```text
0x000c4be4  addi    0x1,r26,r19
0x000c4be8  mov     r19,r26
0x000c4bea  and     r20,r26
0x000c4bec  movea   0x4,sp,r6          ; output = &width
0x000c4bf0  ld.w    0x0[r25],r18       ; payload base
0x000c4bf4  add     r26,r18            ; payload + 3
0x000c4bf6  mov     r18,r7
0x000c4bf8  jarl    0x00099d08,lp      ; read big-endian uint16
```

The reader's body at `0x00099d0a` performs:

```text
0x00099d20  ld.bu   0x0[r10],r11       ; payload[3]
0x00099d24  shl     0x8,r11
0x00099d26  st.h    r11,0x0[r6]
0x00099d30  ld.bu   0x0[r7],r14        ; payload[4]
0x00099d34  add     r14,r13
0x00099d36  st.h    r13,0x0[r6]
```

Thus the handler constructs exactly `(payload[3] << 8) | payload[4]`.

### What BlockWidth is compared against

The next instructions load that unsigned half-word and perform only a generic
range check:

```text
0x000c4bfc  ld.hu   0x4[sp],r11        ; incoming BlockWidth
0x000c4c00  cmp     0x1,r11
0x000c4c02  blt     0x000c4c0c         ; reject below 1
0x000c4c04  movea   0x174,r0,r1
0x000c4c08  cmp     r1,r11
0x000c4c0a  ble     0x000c4c2c         ; accept at or below 372
                                        ; otherwise reject
```

There is no lookup of the SinkInfo profile arrays at `0x96e3f`, `0x96e51`,
`0x96e63`, `0x96e75`, `0x96e87`, or `0x96e99`, and no call to the
`SinkInfo.Get` implementation at `0x000c7424`. In particular, the handler never
loads or computes sink `0x26`'s value `14`.

After the connection-label and state checks, the accepted values are recorded:

```text
0x000c4df8..0x000c4e06  store payload[5..6] connection label
                         into per-sink table gp-0xe2c + 2*index
0x000c4e0a..0x000c4e14  store the received BlockWidth from sp+4
                         into per-sink table gp-0xe06 + 2*index
```

The latter store is positive evidence that the incoming width is treated as
runtime connection state, not checked for equality with the advertised sink
width.

## 2. Exact HU construction of `0x001a`

### MIBRoot: one ARM literal shared by two sources

MIBRoot function `sub_207DD4` constructs the source-resource records. It loads
the literal once:

```text
VA 0x00207e0c / file 0x00107e0c
bytes  1a 80 a0 e3
ARM    mov r8,#0x1a
```

That `r8` value is stored into two records:

```text
0x00207e44  source 2 bandwidth = r8 = 26
0x00207ee4  source number = 7
0x00207ee8  source 7 bandwidth = r8 = 26
0x00207ef4  source 7 source ID = 52
0x00207ef8  insert source-7 record
```

The immediate occupies one byte of this ARM instruction, so changing file byte
`0x00107e0c` from `1a` to `0e` would change the literal to 14. It would also
change **both source 2 and source 7**, so it is not a source-7-specific edit.
No firmware edit was made in this audit.

### AudioProcess: a source-7-specific one-byte table entry

AudioProcess has an independent `(source number, type, width)` table at VA
`0x002d3f80`. The target row is:

```text
VA 0x002d3f92 / file 0x001d3f92:  07 00 1a
VA 0x002d3f94 / file 0x001d3f94:        1a  ; source-7 width byte
```

`CPMOSTSrcDevCtrl::initializeTables` at `0x001c86b4` copies byte 2 of every
three-byte ROM record into its per-source runtime object
(`0x001c87b4..0x001c87c0`). `sub_28668C` is the lookup that returns that width.
Its callers include:

```text
0x00284a78  CGGenDSISrcDevCtrl::respAlloc path
0x0028d260  CGGenMOSTSnkDevCtrl::raiseConnSR zero-width fallback
```

The nonzero value then reaches `raiseConnSR`, which emits its high and low bytes
at `0x0028d078..0x0028d094`.

Therefore the outbound Connect-side value for source 7 has a precise,
source-specific one-byte site: AudioProcess file offset `0x001d3f94`. A change
`1a -> 0e` there is a known-size edit **for that lookup only**.

It is not yet a defensible complete fix by itself. MIBRoot independently asks
for 26 during source allocation, and `car` contains lower NAL resources of width
26. The exact binding of `MmxSourcePresCtrl/7` to a particular NAL resource was
not proved, so the number of sites required to change the physical source
allocation consistently is not yet closed.

## 3. Reconciling 14, 20, and 26

The three values describe different layers unless packed storage is proved:

| Value | Exact known meaning |
| ---: | --- |
| 14 | amplifier sink `0x26` advertises 2 protection bytes + six 16-bit samples |
| 20 | hypothetical packed representation: 2 + six 24-bit samples |
| 26 | HU source resource and Connect value |

The inference `six channels × 24 bits = 18 payload bytes` assumes tightly packed
three-byte samples. That is not the representation proved at the deva boundary.
QSA/deva place each sample in a 32-bit word; the valid sample precision is 24
bits, but the six-channel container stride is:

```text
6 channels * 4-byte words = 24 audio bytes per frame
2 protected-stream bytes + 24 audio bytes = 26
```

Thus `26` has two arithmetically valid decompositions:

```text
2 + 8 * 3 bytes   (eight packed 24-bit samples)
2 + 6 * 4 bytes   (six 24-bit samples in 32-bit slots)
```

Its equality to sink `0x32`'s advertised width does not by itself prove that the
HU copied the 7.1 profile width. The source table can equally be describing the
six 32-bit containers already present in deva's DMA path. Conversely, static
configuration alone does not prove which of those layouts appears on the wire.

No CPU-side sample repacker from six packed 24-bit samples to MOST bytes is
visible in MIBRoot, AudioProcess, or `car`. The lower `car` configuration creates
26-byte synchronous NAL resources, including:

```text
car::CGMostSubsystemConfig / sub_19F7E8
0x001a01c4  CGNalResource(resource 33,  bandwidth 26, ...)
0x001a02c0  CGNalResource(resource 100, bandwidth 26, ...)
```

That code configures endpoints and widths; it does not copy or convert audio
samples. The previously decoded resource-33 direction is StreamPort 0 toward
MOST, but this audit still has no static proof binding source 7 to that numeric
resource. The actual slot packing lies below these host binaries, at the
Qualcomm LPASS/AFE `TDM_OUT_MPL` and INIC/MLB/MOST formatter boundary, or in
firmware not present in the available image.

Therefore the conversion to on-wire format is **not visible statically** in the
available binaries. There may be no 18-to-24-byte conversion at all if the
32-bit TDM slots are preserved. Capturing the one-hot frame at `TDM_OUT_MPL` and
then in the MOST synchronous slots remains the measurement that distinguishes
the layouts and localizes the channel loss.

## Decision

The amplifier does not reject `0x26/0x001a` because of a BlockWidth/profile
mismatch. The width-rejection explanation for falling back to stereo is closed.

The surviving static anomaly is still that the controller connects a 26-byte
source to a sink advertising 14 bytes. Its acoustic effect cannot be derived
from these control-plane handlers. The next useful evidence is audio content at
the two data-plane boundaries, not another Connect-table patch.

## Provenance

- Amplifier V850 image:
  `/Users/luka/Desktop/AUDI_2/Firmwares/SoundAmp/uPD70f3479_8W0035465_0420_H11/CodeFlash_24082026_172027.bin`
  
  SHA-256: `8776fdb84d05571cf4addf42655bc875da7c47cc35166e3cee6c60bc5841fe4b`
- MU1316 rootfs:
  `/Users/luka/Desktop/AUDI_2/Firmwares/HU/MU1316-MHI2Q_US_AUG22_P5087-JTB-07426.03.1811930398/rootifs_out/`
- `AudioProcess` SHA-256:
  `3d591ef3263521ba520d301cfc9487e86badff736d53eb0d77dbe0a678df72f3`
- `MIBRoot` SHA-256:
  `bab21755106ea5306f387102c0f81a5544f79f415de5564baec76fc7aaa05c81`
- `car` SHA-256:
  `f97e32e7602ea573c43769c45280bad3460187c1bde98894f0843ab0192c7f0c`

The ARM HU path was cross-checked with radare2 bytes/disassembly and read-only
IDA decompilation/xrefs. The V850 handler was decoded with Ghidra
`V850:LE:32:default` and checked against the raw CodeFlash bytes. Only temporary
copies of analysis projects were used; the firmware files were not changed.
