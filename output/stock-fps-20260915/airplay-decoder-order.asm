            ;-- method.dio::COMXVideoDecoder.enableDecoderOrderMode_bool_:
            ;-- dio::COMXVideoDecoder::enableDecoderOrderMode(bool):
            0x00086ea8      f0452de9       push {r4, r5, r6, r7, r8, sl, lr}; RELOC 32 _ZN3dio16COMXVideoDecoder22enableDecoderOrderModeEb @ 0x00086ea8 ; dio::COMXVideoDecoder::enableDecoderOrderMode(bool)
            0x00086eac      2cd04de2       sub sp, sp, 0x2c
            0x00086eb0      0080a0e1       mov r8, r0
            0x00086eb4      10708de2       add r7, sp, 0x10
            0x00086eb8      1420a0e3       mov r2, 0x14
            0x00086ebc      01a0a0e1       mov sl, r1
            0x00086ec0      0700a0e1       mov r0, r7
            0x00086ec4      0010a0e3       mov r1, 0
            0x00086ec8      9d4efeeb       bl sym.imp.memset
            0x00086ecc      043098e5       ldr r3, [r8, 4]
            0x00086ed0      1c519fe5       ldr r5, [0x00086ff4]        ; [0x86ff4:4]=0x31e54
            0x00086ed4      1420a0e3       mov r2, 0x14
            0x00086ed8      000053e3       cmp r3, 0
            0x00086edc      10208de5       str r2, [sp, 0x10]
            0x00086ee0      05600103       movweq r6, 0x1005
            0x00086ee4      05508fe0       add r5, pc, r5
            0x00086ee8      00604803       movteq r6, 0x8000
            0x00086eec      012100e3       movw r2, 0x101
            0x00086ef0      14208de5       str r2, [sp, 0x14]
            0x00086ef4      0640a001       moveq r4, r6
        ┌─< 0x00086ef8      2100000a       beq 0x86f84
        │   0x00086efc      f4109fe5       ldr r1, [0x00086ff8]        ; [0x86ff8:4]=0xffff70b0
        │   0x00086f00      0300a0e1       mov r0, r3
        │   0x00086f04      24208de2       add r2, sp, 0x24
        │   0x00086f08      283093e5       ldr r3, [r3, 0x28]
        │   0x00086f0c      011085e0       add r1, r5, r1
        │   0x00086f10      33ff2fe1       blx r3
        │   0x00086f14      000050e3       cmp r0, 0
        │   0x00086f18      0040a0e1       mov r4, r0
        │   0x00086f1c      0060a0e1       mov r6, r0
       ┌──< 0x00086f20      1700001a       bne 0x86f84
       ││   0x00086f24      043098e5       ldr r3, [r8, 4]
       ││   0x00086f28      0720a0e1       mov r2, r7
       ││   0x00086f2c      24109de5       ldr r1, [sp, 0x24]
       ││   0x00086f30      0300a0e1       mov r0, r3
       ││   0x00086f34      183093e5       ldr r3, [r3, 0x18]
       ││   0x00086f38      33ff2fe1       blx r3
       ││   0x00086f3c      004050e2       subs r4, r0, 0
      ┌───< 0x00086f40      0e00001a       bne 0x86f80
      │││   0x00086f44      00005ae3       cmp sl, 0
      │││   0x00086f48      1ca08de5       str sl, [sp, 0x1c]
      │││   0x00086f4c      0130a013       movne r3, 1
      │││   0x00086f50      20308d15       strne r3, [sp, 0x20]
      │││   0x00086f54      043098e5       ldr r3, [r8, 4]
      │││   0x00086f58      0720a0e1       mov r2, r7
      │││   0x00086f5c      24109de5       ldr r1, [sp, 0x24]
      │││   0x00086f60      0300a0e1       mov r0, r3
      │││   0x00086f64      1c3093e5       ldr r3, [r3, 0x1c]
      │││   0x00086f68      33ff2fe1       blx r3
      │││   0x00086f6c      004050e2       subs r4, r0, 0
     ┌────< 0x00086f70      0200001a       bne 0x86f80
  ┌┌┌─────> 0x00086f74      0400a0e1       mov r0, r4
  ╎╎╎││││   0x00086f78      2cd08de2       add sp, sp, 0x2c
  ╎╎╎││││   0x00086f7c      f085bde8       pop {r4, r5, r6, r7, r8, sl, pc}
  ╎╎╎└└───> 0x00086f80      0460a0e1       mov r6, r4
  ╎╎╎  └└─> 0x00086f84      70709fe5       ldr r7, [0x00086ffc]        ; [0x86ffc:4]=0x1230
  ╎╎╎       0x00086f88      070095e7       ldr r0, [r5, r7]
  ╎╎╎       0x00086f8c      003090e5       ldr r3, [r0]
  ╎╎╎       0x00086f90      5a0053e3       cmp r3, 0x5a                ; 'Z'
  └───────< 0x00086f94      f6ffffca       bgt 0x86f74
   ╎╎       0x00086f98      010073e3       cmn r3, 1
   ╎╎   ┌─< 0x00086f9c      0f00000a       beq 0x86fe0
   ╎╎  ┌──> 0x00086fa0      0400a0e1       mov r0, r4
   ╎╎  ╎│   0x00086fa4      5c4cfeeb       bl rsym._Z13printOMXError13OMX_ERRORTYPE
   ╎╎  ╎│   0x00086fa8      07c095e7       ldr ip, [r5, r7]
   ╎╎  ╎│   0x00086fac      4c109fe5       ldr r1, [0x00087000]        ; [0x87000:4]=0xffff6948
   ╎╎  ╎│   0x00086fb0      5a20a0e3       mov r2, 0x5a                ; 'Z'
   ╎╎  ╎│   0x00086fb4      48309fe5       ldr r3, [0x00087004]        ; [0x87004:4]=0xffff7024
   ╎╎  ╎│   0x00086fb8      48e09fe5       ldr lr, [0x00087008]        ; [0x87008:4]=0xffff6760
   ╎╎  ╎│   0x00086fbc      011085e0       add r1, r5, r1
   ╎╎  ╎│   0x00086fc0      033085e0       add r3, r5, r3
   ╎╎  ╎│   0x00086fc4      04608de5       str r6, [sp, 4]
   ╎╎  ╎│   0x00086fc8      0e5085e0       add r5, r5, lr
   ╎╎  ╎│   0x00086fcc      00508de5       str r5, [sp]
   ╎╎  ╎│   0x00086fd0      08008de5       str r0, [sp, 8]
   ╎╎  ╎│   0x00086fd4      0c00a0e1       mov r0, ip
   ╎╎  ╎│   0x00086fd8      b051feeb       bl rsym.LogPrintF
   └──────< 0x00086fdc      e4ffffea       b 0x86f74
    ╎  ╎└─> 0x00086fe0      5a10a0e3       mov r1, 0x5a                ; 'Z'
    ╎  ╎    0x00086fe4      6b51feeb       bl rsym.__LogCategory_Initialize
    ╎  ╎    0x00086fe8      000050e3       cmp r0, 0
    └─────< 0x00086fec      e0ffff0a       beq 0x86f74
       └──< 0x00086ff0      eaffffea       b 0x86fa0
            0x00086ff4      541e0300       andeq r1, r3, r4, asr lr
            0x00086ff8      b070ffff       invalid
            0x00086ffc      30120000       andeq r1, r0, r0, lsr r2
            0x00087000      4869ffff       invalid
            0x00087004      2470ffff       invalid
            0x00087008      6067ffff       invalid
