            ;-- method.dio::COMXVideoDecoder.job_decodeFrame_dio::CBlob_const__unsigned_long_long__bool_:
            ;-- dio::COMXVideoDecoder::job_decodeFrame(dio::CBlob const&, unsigned long long, bool):
; dio::COMXVideoDecoder::job_decodeFrame(dio::CBlob const&, unsigned l
; ong long, bool)
            0x00088318      f0472de9       push {r4, r5, r6, r7, r8, sb, sl, lr}; RELOC 32 _ZN3dio16COMXVideoDecoder15job_decodeFrameERKNS_5CBlobEyb @ 0x00088318
            0x0008831c      04429fe5       ldr r4, [0x00088528]        ; [0x88528:4]=0x30a10
            0x00088320      04629fe5       ldr r6, [0x0008852c]        ; [0x8852c:4]=0x1230
            0x00088324      0070a0e1       mov r7, r0
            0x00088328      04408fe0       add r4, pc, r4
            0x0008832c      18d04de2       sub sp, sp, 0x18
            0x00088330      0030a0e3       mov r3, 0
            0x00088334      0150a0e1       mov r5, r1
            0x00088338      060094e7       ldr r0, [r4, r6]
            0x0008833c      0290a0e1       mov sb, r2
            0x00088340      14308de5       str r3, [sp, 0x14]
            0x00088344      3880dde5       ldrb r8, [sp, 0x38]
            0x00088348      003090e5       ldr r3, [r0]
            0x0008834c      140053e3       cmp r3, 0x14
        ┌─< 0x00088350      0e0000ca       bgt 0x88390
        │   0x00088354      010073e3       cmn r3, 1
       ┌──< 0x00088358      4900000a       beq 0x88484
      ┌───> 0x0008835c      08c095e5       ldr ip, [r5, 8]
      ╎││   0x00088360      1420a0e3       mov r2, 0x14
      ╎││   0x00088364      c4119fe5       ldr r1, [0x00088530]        ; [0x88530:4]=0xffff68e8
      ╎││   0x00088368      c4319fe5       ldr r3, [0x00088534]        ; [0x88534:4]=0xffff7528
      ╎││   0x0008836c      04c08de5       str ip, [sp, 4]
      ╎││   0x00088370      011084e0       add r1, r4, r1
      ╎││   0x00088374      bcc19fe5       ldr ip, [0x00088538]        ; [0x88538:4]=0xffff62f0
      ╎││   0x00088378      033084e0       add r3, r4, r3
      ╎││   0x0008837c      060094e7       ldr r0, [r4, r6]
      ╎││   0x00088380      0cc084e0       add ip, r4, ip
      ╎││   0x00088384      08808de5       str r8, [sp, 8]
      ╎││   0x00088388      00c08de5       str ip, [sp]
      ╎││   0x0008838c      c34cfeeb       bl rsym.LogPrintF
     ┌──└─> 0x00088390      2c0097e5       ldr r0, [r7, 0x2c]
     ╎╎│    0x00088394      14108de2       add r1, sp, 0x14
     ╎╎│    0x00088398      6420a0e3       mov r2, 0x64                ; 'd'
     ╎╎│    0x0008839c      3246feeb       bl rsym._ZN3dio11CBufferPool9getBufferERP20OMX_BUFFERHEADERTYPEi
     ╎╎│    0x000883a0      00a050e2       subs sl, r0, 0
     ╎╎│    0x000883a4      01700113       movwne r7, 0x1001
     ╎╎│    0x000883a8      00704813       movtne r7, 0x8000
     ╎╎│    0x000883ac      0750a011       movne r5, r7
     ╎╎│┌─< 0x000883b0      3f00001a       bne 0x884b4
     ╎╎││   0x000883b4      14309de5       ldr r3, [sp, 0x14]
     ╎╎││   0x000883b8      082095e5       ldr r2, [r5, 8]
     ╎╎││   0x000883bc      0c1093e5       ldr r1, [r3, 0xc]
     ╎╎││   0x000883c0      020051e1       cmp r1, r2
     ╎╎││   0x000883c4      00700133       movwlo r7, 0x1000
     ╎╎││   0x000883c8      00704833       movtlo r7, 0x8000
     ╎╎││   0x000883cc      0750a031       movlo r5, r7
    ┌─────< 0x000883d0      3700003a       blo 0x884b4
    │╎╎││   0x000883d4      080093e5       ldr r0, [r3, 8]
    │╎╎││   0x000883d8      041095e5       ldr r1, [r5, 4]
    │╎╎││   0x000883dc      9e42feeb       bl sym.imp.memcpy
    │╎╎││   0x000883e0      060094e7       ldr r0, [r4, r6]
    │╎╎││   0x000883e4      14c09de5       ldr ip, [sp, 0x14]
    │╎╎││   0x000883e8      000058e3       cmp r8, 0
    │╎╎││   0x000883ec      082095e5       ldr r2, [r5, 8]
    │╎╎││   0x000883f0      8080a013       movne r8, 0x80
    │╎╎││   0x000883f4      003090e5       ldr r3, [r0]
    │╎╎││   0x000883f8      1080a003       moveq r8, 0x10
    │╎╎││   0x000883fc      470bdfed       vldr d16, [0x00088520]
    │╎╎││   0x00088400      14a08ce5       str sl, [ip, 0x14]
    │╎╎││   0x00088404      140053e3       cmp r3, 0x14
    │╎╎││   0x00088408      10208ce5       str r2, [ip, 0x10]
    │╎╎││   0x0008840c      40808ce5       str r8, [ip, 0x40]
    │╎╎││   0x00088410      0e0bcced       vstr d16, [ip, 0x38]
    │╎╎││   0x00088414      30908ce5       str sb, [ip, 0x30]
   ┌──────< 0x00088418      0d0000ca       bgt 0x88454
   ││╎╎││   0x0008841c      010073e3       cmn r3, 1
  ┌───────< 0x00088420      1c00000a       beq 0x88498
  ────────> 0x00088424      04119fe5       ldr r1, [0x00088530]        ; [0x88530:4]=0xffff68e8
  │││╎╎││   0x00088428      1420a0e3       mov r2, 0x14
  │││╎╎││   0x0008842c      08319fe5       ldr r3, [0x0008853c]        ; [0x8853c:4]=0xffff7564
  │││╎╎││   0x00088430      04c08de5       str ip, [sp, 4]
  │││╎╎││   0x00088434      011084e0       add r1, r4, r1
  │││╎╎││   0x00088438      f8c09fe5       ldr ip, [0x00088538]        ; [0x88538:4]=0xffff62f0
  │││╎╎││   0x0008843c      033084e0       add r3, r4, r3
  │││╎╎││   0x00088440      060094e7       ldr r0, [r4, r6]
  │││╎╎││   0x00088444      0cc084e0       add ip, r4, ip
  │││╎╎││   0x00088448      00c08de5       str ip, [sp]
  │││╎╎││   0x0008844c      934cfeeb       bl rsym.LogPrintF
  │││╎╎││   0x00088450      14c09de5       ldr ip, [sp, 0x14]
  ─└──────> 0x00088454      043097e5       ldr r3, [r7, 4]
  │ │╎╎││   0x00088458      0c10a0e1       mov r1, ip
  │ │╎╎││   0x0008845c      0300a0e1       mov r0, r3
  │ │╎╎││   0x00088460      403093e5       ldr r3, [r3, 0x40]
  │ │╎╎││   0x00088464      33ff2fe1       blx r3
  │ │╎╎││   0x00088468      000050e3       cmp r0, 0
  │ │╎╎││   0x0008846c      0050a0e1       mov r5, r0
  │ │╎╎││   0x00088470      0070a0e1       mov r7, r0
  │┌──────< 0x00088474      0e00001a       bne 0x884b4
  ────────> 0x00088478      0500a0e1       mov r0, r5
  │││╎╎││   0x0008847c      18d08de2       add sp, sp, 0x18
  │││╎╎││   0x00088480      f087bde8       pop {r4, r5, r6, r7, r8, sb, sl, pc}
  │││╎╎└──> 0x00088484      1410a0e3       mov r1, 0x14
  │││╎╎ │   0x00088488      424cfeeb       bl rsym.__LogCategory_Initialize
  │││╎╎ │   0x0008848c      000050e3       cmp r0, 0
  │││└────< 0x00088490      beffff0a       beq 0x88390
  │││ └───< 0x00088494      b0ffffea       b 0x8835c
  └───────> 0x00088498      1410a0e3       mov r1, 0x14
   ││   │   0x0008849c      3d4cfeeb       bl rsym.__LogCategory_Initialize
   ││   │   0x000884a0      000050e3       cmp r0, 0
   ││   │   0x000884a4      14c09d15       ldrne ip, [sp, 0x14]
  ────────< 0x000884a8      ddffff1a       bne 0x88424
   ││   │   0x000884ac      14c09de5       ldr ip, [sp, 0x14]
  ────────< 0x000884b0      e7ffffea       b 0x88454
   └└───└─> 0x000884b4      060094e7       ldr r0, [r4, r6]
            0x000884b8      003090e5       ldr r3, [r0]
            0x000884bc      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x000884c0      ecffffca       bgt 0x88478
            0x000884c4      010073e3       cmn r3, 1
        ┌─< 0x000884c8      0f00000a       beq 0x8850c
       ┌──> 0x000884cc      0500a0e1       mov r0, r5
       ╎│   0x000884d0      1147feeb       bl rsym._Z13printOMXError13OMX_ERRORTYPE
       ╎│   0x000884d4      06c094e7       ldr ip, [r4, r6]
       ╎│   0x000884d8      50109fe5       ldr r1, [0x00088530]        ; [0x88530:4]=0xffff68e8
       ╎│   0x000884dc      5a20a0e3       mov r2, 0x5a                ; 'Z'
       ╎│   0x000884e0      58309fe5       ldr r3, [0x00088540]        ; [0x88540:4]=0xffff7024
       ╎│   0x000884e4      4ce09fe5       ldr lr, [0x00088538]        ; [0x88538:4]=0xffff62f0
       ╎│   0x000884e8      011084e0       add r1, r4, r1
       ╎│   0x000884ec      033084e0       add r3, r4, r3
       ╎│   0x000884f0      04708de5       str r7, [sp, 4]
       ╎│   0x000884f4      0e4084e0       add r4, r4, lr
       ╎│   0x000884f8      00408de5       str r4, [sp]
       ╎│   0x000884fc      08008de5       str r0, [sp, 8]
       ╎│   0x00088500      0c00a0e1       mov r0, ip
       ╎│   0x00088504      654cfeeb       bl rsym.LogPrintF
  ────────< 0x00088508      daffffea       b 0x88478
       ╎└─> 0x0008850c      5a10a0e3       mov r1, 0x5a                ; 'Z'
       ╎    0x00088510      204cfeeb       bl rsym.__LogCategory_Initialize
       ╎    0x00088514      000050e3       cmp r0, 0
  ────────< 0x00088518      d6ffff0a       beq 0x88478
       └──< 0x0008851c      eaffffea       b 0x884cc
            0x00088520      00000000       andeq r0, r0, r0
            0x00088524      00000000       andeq r0, r0, r0
            0x00088528      100a0300       andeq r0, r3, r0, lsl sl
            0x0008852c      30120000       andeq r1, r0, r0, lsr r2
            0x00088530      e868ffff       invalid
            0x00088534      2875ffff       invalid
            0x00088538      f062ffff       invalid
            0x0008853c      6475ffff       invalid
            0x00088540      2470ffff       invalid
