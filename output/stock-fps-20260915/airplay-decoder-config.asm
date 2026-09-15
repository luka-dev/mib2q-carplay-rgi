            ;-- method.dio::COMXVideoDecoder.configure_unsigned_int__unsigned_int__unsigned_int__unsigned_int__unsigned_int__unsigned_int__unsigned_int_:
            ;-- dio::COMXVideoDecoder::configure(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int):
; dio::COMXVideoDecoder::configure(unsigned int, unsigned int, unsigne
; d int, unsigned int, unsigned int, unsigned int, unsigned int)
            0x00089ffc      04c090e5       ldr ip, [r0, 4]             ; RELOC 32 _ZN3dio16COMXVideoDecoder9configureEjjjjjjj @ 0x00089ffc
            0x0008a000      f0452de9       push {r4, r5, r6, r7, r8, sl, lr}
            0x0008a004      00005ce3       cmp ip, 0
            0x0008a008      c4529fe5       ldr r5, [0x0008a2d4]        ; [0x8a2d4:4]=0x2ed24
            0x0008a00c      d4d04de2       sub sp, sp, 0xd4
            0x0008a010      0040a0e1       mov r4, r0
            0x0008a014      05508fe0       add r5, pc, r5
        ┌─< 0x0008a018      8900000a       beq 0x8a244
        │   0x0008a01c      101080e5       str r1, [r0, 0x10]
        │   0x0008a020      0110a0e3       mov r1, 1
        │   0x0008a024      142080e5       str r2, [r0, 0x14]
        │   0x0008a028      183080e5       str r3, [r0, 0x18]
        │   0x0008a02c      f0309de5       ldr r3, [sp, 0xf0]
        │   0x0008a030      1c3080e5       str r3, [r0, 0x1c]
        │   0x0008a034      f4309de5       ldr r3, [sp, 0xf4]
        │   0x0008a038      203080e5       str r3, [r0, 0x20]
        │   0x0008a03c      f8309de5       ldr r3, [sp, 0xf8]
        │   0x0008a040      243080e5       str r3, [r0, 0x24]
        │   0x0008a044      fc309de5       ldr r3, [sp, 0xfc]
        │   0x0008a048      283080e5       str r3, [r0, 0x28]
        │   0x0008a04c      1242feeb       bl rsym._ZN3dio16COMXVideoDecoder22enableDecoderOrderModeEb
        │   0x0008a050      000050e3       cmp r0, 0
        │   0x0008a054      0060a0e1       mov r6, r0
        │   0x0008a058      0080a0e1       mov r8, r0
       ┌──< 0x0008a05c      7400001a       bne 0x8a234
       ││   0x0008a060      70708de2       add r7, sp, 0x70
       ││   0x0008a064      0010a0e1       mov r1, r0
       ││   0x0008a068      6020a0e3       mov r2, 0x60                ; '`'
       ││   0x0008a06c      0700a0e1       mov r0, r7
       ││   0x0008a070      3342feeb       bl sym.imp.memset
       ││   0x0008a074      043094e5       ldr r3, [r4, 4]
       ││   0x0008a078      011000e3       movw r1, 1
       ││   0x0008a07c      7c608de5       str r6, [sp, 0x7c]
       ││   0x0008a080      78608de5       str r6, [sp, 0x78]
       ││   0x0008a084      0300a0e1       mov r0, r3
       ││   0x0008a088      001240e3       movt r1, 0x200
       ││   0x0008a08c      6080a0e3       mov r8, 0x60                ; '`'
       ││   0x0008a090      01a100e3       movw sl, 0x101
       ││   0x0008a094      70808de5       str r8, [sp, 0x70]
       ││   0x0008a098      74a08de5       str sl, [sp, 0x74]
       ││   0x0008a09c      0720a0e1       mov r2, r7
       ││   0x0008a0a0      183093e5       ldr r3, [r3, 0x18]
       ││   0x0008a0a4      33ff2fe1       blx r3
       ││   0x0008a0a8      006050e2       subs r6, r0, 0
      ┌───< 0x0008a0ac      3f00001a       bne 0x8a1b0
      │││   0x0008a0b0      043094e5       ldr r3, [r4, 4]
      │││   0x0008a0b4      011000e3       movw r1, 1
      │││   0x0008a0b8      106094e5       ldr r6, [r4, 0x10]
      │││   0x0008a0bc      001240e3       movt r1, 0x200
      │││   0x0008a0c0      14e094e5       ldr lr, [r4, 0x14]
      │││   0x0008a0c4      0300a0e1       mov r0, r3
      │││   0x0008a0c8      18c094e5       ldr ip, [r4, 0x18]
      │││   0x0008a0cc      0720a0e1       mov r2, r7
      │││   0x0008a0d0      a0608de5       str r6, [sp, 0xa0]
      │││   0x0008a0d4      a4e08de5       str lr, [sp, 0xa4]
      │││   0x0008a0d8      07e0a0e3       mov lr, 7
      │││   0x0008a0dc      0cc8a0e1       lsl ip, ip, 0x10
      │││   0x0008a0e0      bce08de5       str lr, [sp, 0xbc]
      │││   0x0008a0e4      b4c08de5       str ip, [sp, 0xb4]
      │││   0x0008a0e8      1c3093e5       ldr r3, [r3, 0x1c]
      │││   0x0008a0ec      33ff2fe1       blx r3
      │││   0x0008a0f0      006050e2       subs r6, r0, 0
     ┌────< 0x0008a0f4      2d00001a       bne 0x8a1b0
     ││││   0x0008a0f8      10708de2       add r7, sp, 0x10
     ││││   0x0008a0fc      0610a0e1       mov r1, r6
     ││││   0x0008a100      0820a0e1       mov r2, r8
     ││││   0x0008a104      0700a0e1       mov r0, r7
     ││││   0x0008a108      0d42feeb       bl sym.imp.memset
     ││││   0x0008a10c      043094e5       ldr r3, [r4, 4]
     ││││   0x0008a110      011000e3       movw r1, 1
     ││││   0x0008a114      14a08de5       str sl, [sp, 0x14]
     ││││   0x0008a118      10808de5       str r8, [sp, 0x10]
     ││││   0x0008a11c      0300a0e1       mov r0, r3
     ││││   0x0008a120      0180a0e3       mov r8, 1
     ││││   0x0008a124      001240e3       movt r1, 0x200
     ││││   0x0008a128      18808de5       str r8, [sp, 0x18]
     ││││   0x0008a12c      0720a0e1       mov r2, r7
     ││││   0x0008a130      1c808de5       str r8, [sp, 0x1c]
     ││││   0x0008a134      183093e5       ldr r3, [r3, 0x18]
     ││││   0x0008a138      33ff2fe1       blx r3
     ││││   0x0008a13c      006050e2       subs r6, r0, 0
    ┌─────< 0x0008a140      1a00001a       bne 0x8a1b0
    │││││   0x0008a144      04c094e5       ldr ip, [r4, 4]
    │││││   0x0008a148      043000e3       movw r3, 4
    │││││   0x0008a14c      102094e5       ldr r2, [r4, 0x10]
    │││││   0x0008a150      011000e3       movw r1, 1
    │││││   0x0008a154      18e094e5       ldr lr, [r4, 0x18]
    │││││   0x0008a158      003f47e3       movt r3, 0x7f00
    │││││   0x0008a15c      14a094e5       ldr sl, [r4, 0x14]
    │││││   0x0008a160      0c00a0e1       mov r0, ip
    │││││   0x0008a164      5c608de5       str r6, [sp, 0x5c]
    │││││   0x0008a168      001240e3       movt r1, 0x200
    │││││   0x0008a16c      40208de5       str r2, [sp, 0x40]
    │││││   0x0008a170      0ee8a0e1       lsl lr, lr, 0x10
    │││││   0x0008a174      44a08de5       str sl, [sp, 0x44]
    │││││   0x0008a178      0720a0e1       mov r2, r7
    │││││   0x0008a17c      60308de5       str r3, [sp, 0x60]
    │││││   0x0008a180      54e08de5       str lr, [sp, 0x54]
    │││││   0x0008a184      1c309ce5       ldr r3, [ip, 0x1c]
    │││││   0x0008a188      33ff2fe1       blx r3
    │││││   0x0008a18c      006050e2       subs r6, r0, 0
   ┌──────< 0x0008a190      0600001a       bne 0x8a1b0
   ││││││   0x0008a194      0400a0e1       mov r0, r4
   ││││││   0x0008a198      b842feeb       bl rsym._ZN3dio16COMXVideoDecoder13createBuffersEv
   ││││││   0x0008a19c      006050e2       subs r6, r0, 0
   ││││││   0x0008a1a0      0800a001       moveq r0, r8
  ┌───────< 0x0008a1a4      0100001a       bne 0x8a1b0
  ────────> 0x0008a1a8      d4d08de2       add sp, sp, 0xd4
  │││││││   0x0008a1ac      f085bde8       pop {r4, r5, r6, r7, r8, sl, pc}
  └└└└└───> 0x0008a1b0      20719fe5       ldr r7, [0x0008a2d8]        ; [0x8a2d8:4]=0x1230
       ││   0x0008a1b4      0680a0e1       mov r8, r6
       ││   0x0008a1b8      073095e7       ldr r3, [r5, r7]
       ││   0x0008a1bc      003093e5       ldr r3, [r3]
    ┌┌┌───> 0x0008a1c0      5a0053e3       cmp r3, 0x5a                ; 'Z'
   ┌──────< 0x0008a1c4      020000da       ble 0x8a1d4
  ┌───────> 0x0008a1c8      010078e2       rsbs r0, r8, 1
  ╎│╎╎╎││   0x0008a1cc      0000a033       movlo r0, 0
  ────────< 0x0008a1d0      f4ffffea       b 0x8a1a8
  ╎└──────> 0x0008a1d4      010073e3       cmn r3, 1
  ╎┌──────< 0x0008a1d8      0f00000a       beq 0x8a21c
  ────────> 0x0008a1dc      0600a0e1       mov r0, r6
  ╎│╎╎╎││   0x0008a1e0      cd3ffeeb       bl rsym._Z13printOMXError13OMX_ERRORTYPE
  ╎│╎╎╎││   0x0008a1e4      07c095e7       ldr ip, [r5, r7]
  ╎│╎╎╎││   0x0008a1e8      ec109fe5       ldr r1, [0x0008a2dc]        ; [0x8a2dc:4]=0xffff6508
  ╎│╎╎╎││   0x0008a1ec      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎│╎╎╎││   0x0008a1f0      e8309fe5       ldr r3, [0x0008a2e0]        ; [0x8a2e0:4]=0xffff7024
  ╎│╎╎╎││   0x0008a1f4      e8e09fe5       ldr lr, [0x0008a2e4]        ; [0x8a2e4:4]=0xffff667c
  ╎│╎╎╎││   0x0008a1f8      011085e0       add r1, r5, r1
  ╎│╎╎╎││   0x0008a1fc      033085e0       add r3, r5, r3
  ╎│╎╎╎││   0x0008a200      04808de5       str r8, [sp, 4]
  ╎│╎╎╎││   0x0008a204      0e5085e0       add r5, r5, lr
  ╎│╎╎╎││   0x0008a208      00508de5       str r5, [sp]
  ╎│╎╎╎││   0x0008a20c      08008de5       str r0, [sp, 8]
  ╎│╎╎╎││   0x0008a210      0c00a0e1       mov r0, ip
  ╎│╎╎╎││   0x0008a214      2145feeb       bl rsym.LogPrintF
  ────────< 0x0008a218      eaffffea       b 0x8a1c8
  ╎└──────> 0x0008a21c      070095e7       ldr r0, [r5, r7]
  ╎ ╎╎╎││   0x0008a220      5a10a0e3       mov r1, 0x5a                ; 'Z'
  ╎ ╎╎╎││   0x0008a224      db44feeb       bl rsym.__LogCategory_Initialize
  ╎ ╎╎╎││   0x0008a228      000050e3       cmp r0, 0
  ────────< 0x0008a22c      eaffff1a       bne 0x8a1dc
  └───────< 0x0008a230      e4ffffea       b 0x8a1c8
    ╎╎╎└──> 0x0008a234      9c709fe5       ldr r7, [0x0008a2d8]        ; [0x8a2d8:4]=0x1230
    ╎╎╎ │   0x0008a238      073095e7       ldr r3, [r5, r7]
    ╎╎╎ │   0x0008a23c      003093e5       ldr r3, [r3]
    └─────< 0x0008a240      deffffea       b 0x8a1c0
     ╎╎ └─> 0x0008a244      8c709fe5       ldr r7, [0x0008a2d8]        ; [0x8a2d8:4]=0x1230
     ╎╎     0x0008a248      076095e7       ldr r6, [r5, r7]
     ╎╎     0x0008a24c      003096e5       ldr r3, [r6]
     ╎╎     0x0008a250      5a0053e3       cmp r3, 0x5a                ; 'Z'
     ╎╎ ┌─< 0x0008a254      120000ca       bgt 0x8a2a4
     ╎╎ │   0x0008a258      010073e3       cmn r3, 1
     ╎╎┌──< 0x0008a25c      1400000a       beq 0x8a2b4
    ┌─────> 0x0008a260      074095e7       ldr r4, [r5, r7]
    ╎╎╎││   0x0008a264      0a8001e3       movw r8, 0x100a             ; '\n\x10'
    ╎╎╎││   0x0008a268      6c109fe5       ldr r1, [0x0008a2dc]        ; [0x8a2dc:4]=0xffff6508
    ╎╎╎││   0x0008a26c      008048e3       movt r8, 0x8000
    ╎╎╎││   0x0008a270      70309fe5       ldr r3, [0x0008a2e8]        ; [0x8a2e8:4]=0xffff76cc
    ╎╎╎││   0x0008a274      0400a0e1       mov r0, r4
    ╎╎╎││   0x0008a278      04c08de5       str ip, [sp, 4]
    ╎╎╎││   0x0008a27c      011085e0       add r1, r5, r1
    ╎╎╎││   0x0008a280      5cc09fe5       ldr ip, [0x0008a2e4]        ; [0x8a2e4:4]=0xffff667c
    ╎╎╎││   0x0008a284      033085e0       add r3, r5, r3
    ╎╎╎││   0x0008a288      5a20a0e3       mov r2, 0x5a                ; 'Z'
    ╎╎╎││   0x0008a28c      0860a0e1       mov r6, r8
    ╎╎╎││   0x0008a290      0cc085e0       add ip, r5, ip
    ╎╎╎││   0x0008a294      00c08de5       str ip, [sp]
    ╎╎╎││   0x0008a298      0045feeb       bl rsym.LogPrintF
    ╎╎╎││   0x0008a29c      003094e5       ldr r3, [r4]
    ╎└────< 0x0008a2a0      c6ffffea       b 0x8a1c0
    ╎┌──└─> 0x0008a2a4      0a8001e3       movw r8, 0x100a             ; '\n\x10'
    ╎╎╎│    0x0008a2a8      008048e3       movt r8, 0x8000
    ╎╎╎│    0x0008a2ac      0860a0e1       mov r6, r8
    ╎╎└───< 0x0008a2b0      c2ffffea       b 0x8a1c0
    ╎╎ └──> 0x0008a2b4      0600a0e1       mov r0, r6
    ╎╎      0x0008a2b8      5a10a0e3       mov r1, 0x5a                ; 'Z'
    ╎╎      0x0008a2bc      b544feeb       bl rsym.__LogCategory_Initialize
    ╎╎      0x0008a2c0      000050e3       cmp r0, 0
    ╎╎      0x0008a2c4      00309605       ldreq r3, [r6]
    ╎└────< 0x0008a2c8      f5ffff0a       beq 0x8a2a4
    ╎       0x0008a2cc      04c094e5       ldr ip, [r4, 4]
    └─────< 0x0008a2d0      e2ffffea       b 0x8a260
            0x0008a2d4      24ed0200       andeq lr, r2, r4, lsr 26
            0x0008a2d8      30120000       andeq r1, r0, r0, lsr r2
            0x0008a2dc      0865ffff       invalid
            0x0008a2e0      2470ffff       invalid
            0x0008a2e4      7c66ffff       invalid
            0x0008a2e8      cc76ffff       invalid
