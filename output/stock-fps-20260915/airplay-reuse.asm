            ;-- method.dio::COMXVideoDecoder.reuseOutputBufferCallback_OMX_BUFFERHEADERTYPE_:
            ;-- dio::COMXVideoDecoder::reuseOutputBufferCallback(OMX_BUFFERHEADERTYPE*):
; dio::COMXVideoDecoder::reuseOutputBufferCallback(OMX_BUFFERHEADERTYP
; E*)
            0x00086580      f0402de9       push {r4, r5, r6, r7, lr}   ; RELOC 32 _ZN3dio16COMXVideoDecoder25reuseOutputBufferCallbackEP20OMX_BUFFERHEADERTYPE @ 0x00086580
            0x00086584      20419fe5       ldr r4, [0x000866ac]        ; [0x866ac:4]=0x327a8
            0x00086588      20619fe5       ldr r6, [0x000866b0]        ; [0x866b0:4]=0x1230
            0x0008658c      0070a0e1       mov r7, r0
            0x00086590      04408fe0       add r4, pc, r4
            0x00086594      14d04de2       sub sp, sp, 0x14
            0x00086598      0150a0e1       mov r5, r1
            0x0008659c      060094e7       ldr r0, [r4, r6]
            0x000865a0      003090e5       ldr r3, [r0]
            0x000865a4      140053e3       cmp r3, 0x14
        ┌─< 0x000865a8      0c0000ca       bgt 0x865e0
        │   0x000865ac      010073e3       cmn r3, 1
       ┌──< 0x000865b0      1d00000a       beq 0x8662c
      ┌───> 0x000865b4      f8109fe5       ldr r1, [0x000866b4]        ; [0x866b4:4]=0xffff65dc
      ╎││   0x000865b8      1420a0e3       mov r2, 0x14
      ╎││   0x000865bc      f4309fe5       ldr r3, [0x000866b8]        ; [0x866b8:4]=0xffff6ff0
      ╎││   0x000865c0      f4c09fe5       ldr ip, [0x000866bc]        ; [0x866bc:4]=0xffff68cc
      ╎││   0x000865c4      011084e0       add r1, r4, r1
      ╎││   0x000865c8      060094e7       ldr r0, [r4, r6]
      ╎││   0x000865cc      033084e0       add r3, r4, r3
      ╎││   0x000865d0      0cc084e0       add ip, r4, ip
      ╎││   0x000865d4      04508de5       str r5, [sp, 4]
      ╎││   0x000865d8      00c08de5       str ip, [sp]
      ╎││   0x000865dc      2f54feeb       bl rsym.LogPrintF
     ┌──└─> 0x000865e0      000055e3       cmp r5, 0
     ╎╎│    0x000865e4      05700103       movweq r7, 0x1005
     ╎╎│    0x000865e8      00704803       movteq r7, 0x8000
     ╎╎│    0x000865ec      0750a001       moveq r5, r7
     ╎╎│┌─< 0x000865f0      1200000a       beq 0x86640
     ╎╎││   0x000865f4      043097e5       ldr r3, [r7, 4]
     ╎╎││   0x000865f8      0510a0e1       mov r1, r5
     ╎╎││   0x000865fc      0020a0e3       mov r2, 0
     ╎╎││   0x00086600      102085e5       str r2, [r5, 0x10]
     ╎╎││   0x00086604      0300a0e1       mov r0, r3
     ╎╎││   0x00086608      443093e5       ldr r3, [r3, 0x44]
     ╎╎││   0x0008660c      33ff2fe1       blx r3
     ╎╎││   0x00086610      000050e3       cmp r0, 0
     ╎╎││   0x00086614      0050a0e1       mov r5, r0
     ╎╎││   0x00086618      0070a0e1       mov r7, r0
    ┌─────< 0x0008661c      0700001a       bne 0x86640
  ┌┌──────> 0x00086620      0500a0e1       mov r0, r5
  ╎╎│╎╎││   0x00086624      14d08de2       add sp, sp, 0x14
  ╎╎│╎╎││   0x00086628      f080bde8       pop {r4, r5, r6, r7, pc}
  ╎╎│╎╎└──> 0x0008662c      1410a0e3       mov r1, 0x14
  ╎╎│╎╎ │   0x00086630      d853feeb       bl rsym.__LogCategory_Initialize
  ╎╎│╎╎ │   0x00086634      000050e3       cmp r0, 0
  ╎╎│└────< 0x00086638      e8ffff0a       beq 0x865e0
  ╎╎│ └───< 0x0008663c      dcffffea       b 0x865b4
  ╎╎└───└─> 0x00086640      060094e7       ldr r0, [r4, r6]
  ╎╎        0x00086644      003090e5       ldr r3, [r0]
  ╎╎        0x00086648      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x0008664c      f3ffffca       bgt 0x86620
  ╎╎        0x00086650      010073e3       cmn r3, 1
  ╎╎    ┌─< 0x00086654      0f00000a       beq 0x86698
  ╎╎   ┌──> 0x00086658      0500a0e1       mov r0, r5
  ╎╎   ╎│   0x0008665c      ae4efeeb       bl rsym._Z13printOMXError13OMX_ERRORTYPE
  ╎╎   ╎│   0x00086660      06c094e7       ldr ip, [r4, r6]
  ╎╎   ╎│   0x00086664      48109fe5       ldr r1, [0x000866b4]        ; [0x866b4:4]=0xffff65dc
  ╎╎   ╎│   0x00086668      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎╎   ╎│   0x0008666c      4c309fe5       ldr r3, [0x000866c0]        ; [0x866c0:4]=0xffff7024
  ╎╎   ╎│   0x00086670      44e09fe5       ldr lr, [0x000866bc]        ; [0x866bc:4]=0xffff68cc
  ╎╎   ╎│   0x00086674      011084e0       add r1, r4, r1
  ╎╎   ╎│   0x00086678      033084e0       add r3, r4, r3
  ╎╎   ╎│   0x0008667c      04708de5       str r7, [sp, 4]
  ╎╎   ╎│   0x00086680      0e4084e0       add r4, r4, lr
  ╎╎   ╎│   0x00086684      00408de5       str r4, [sp]
  ╎╎   ╎│   0x00086688      08008de5       str r0, [sp, 8]
  ╎╎   ╎│   0x0008668c      0c00a0e1       mov r0, ip
  ╎╎   ╎│   0x00086690      0254feeb       bl rsym.LogPrintF
  └───────< 0x00086694      e1ffffea       b 0x86620
   ╎   ╎└─> 0x00086698      5a10a0e3       mov r1, 0x5a                ; 'Z'
   ╎   ╎    0x0008669c      bd53feeb       bl rsym.__LogCategory_Initialize
   ╎   ╎    0x000866a0      000050e3       cmp r0, 0
   └──────< 0x000866a4      ddffff0a       beq 0x86620
       └──< 0x000866a8      eaffffea       b 0x86658
            0x000866ac      a8270300       andeq r2, r3, r8, lsr 15
            0x000866b0      30120000       andeq r1, r0, r0, lsr r2
            0x000866b4      dc65ffff       invalid
            0x000866b8      f06fffff       invalid
            0x000866bc      cc68ffff       invalid
            0x000866c0      2470ffff       invalid
