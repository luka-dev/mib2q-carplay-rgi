            ;-- method.dio::COMXVideoDecoder.onFillBufferDoneCallback_void__OMX_BUFFERHEADERTYPE_:
            ;-- dio::COMXVideoDecoder::onFillBufferDoneCallback(void*, OMX_BUFFERHEADERTYPE*):
; dio::COMXVideoDecoder::onFillBufferDoneCallback(void*, OMX_BUFFERHEA
; DERTYPE*)
            0x00086d18      f0402de9       push {r4, r5, r6, r7, lr}   ; RELOC 32 _ZN3dio16COMXVideoDecoder24onFillBufferDoneCallbackEPvP20OMX_BUFFERHEADERTYPE @ 0x00086d18
            0x00086d1c      005052e2       subs r5, r2, 0
            0x00086d20      44419fe5       ldr r4, [0x00086e6c]        ; [0x86e6c:4]=0x3200c
            0x00086d24      14d04de2       sub sp, sp, 0x14
            0x00086d28      0060a0e1       mov r6, r0
            0x00086d2c      04408fe0       add r4, pc, r4
        ┌─< 0x00086d30      4800000a       beq 0x86e58
        │   0x00086d34      34719fe5       ldr r7, [0x00086e70]        ; [0x86e70:4]=0x1230
        │   0x00086d38      070094e7       ldr r0, [r4, r7]
        │   0x00086d3c      003090e5       ldr r3, [r0]
        │   0x00086d40      140053e3       cmp r3, 0x14
       ┌──< 0x00086d44      100000ca       bgt 0x86d8c
       ││   0x00086d48      010073e3       cmn r3, 1
      ┌───< 0x00086d4c      1e00000a       beq 0x86dcc
     ┌────> 0x00086d50      1c319fe5       ldr r3, [0x00086e74]        ; [0x86e74:4]=0xffff67cc
     ╎│││   0x00086d54      1420a0e3       mov r2, 0x14
     ╎│││   0x00086d58      04508de5       str r5, [sp, 4]
     ╎│││   0x00086d5c      033084e0       add r3, r4, r3
     ╎│││   0x00086d60      00308de5       str r3, [sp]
     ╎│││   0x00086d64      103095e5       ldr r3, [r5, 0x10]
     ╎│││   0x00086d68      08119fe5       ldr r1, [0x00086e78]        ; [0x86e78:4]=0xffff6300
     ╎│││   0x00086d6c      070094e7       ldr r0, [r4, r7]
     ╎│││   0x00086d70      08308de5       str r3, [sp, 8]
     ╎│││   0x00086d74      011084e0       add r1, r4, r1
     ╎│││   0x00086d78      14c095e5       ldr ip, [r5, 0x14]
     ╎│││   0x00086d7c      f8309fe5       ldr r3, [0x00086e7c]        ; [0x86e7c:4]=0xffff7068
     ╎│││   0x00086d80      033084e0       add r3, r4, r3
     ╎│││   0x00086d84      0cc08de5       str ip, [sp, 0xc]
     ╎│││   0x00086d88      4452feeb       bl rsym.LogPrintF
    ┌──└──> 0x00086d8c      103095e5       ldr r3, [r5, 0x10]
    ╎╎│ │   0x00086d90      000053e3       cmp r3, 0
    ╎╎│┌──< 0x00086d94      0800000a       beq 0x86dbc
    ╎╎│││   0x00086d98      e0209fe5       ldr r2, [0x00086e80]        ; [0x86e80:4]=0x10d0
    ╎╎│││   0x00086d9c      0630a0e1       mov r3, r6
    ╎╎│││   0x00086da0      081095e5       ldr r1, [r5, 8]
    ╎╎│││   0x00086da4      440096e5       ldr r0, [r6, 0x44]
    ╎╎│││   0x00086da8      022094e7       ldr r2, [r4, r2]
    ╎╎│││   0x00086dac      00508de5       str r5, [sp]
    ╎╎│││   0x00086db0      1e4afeeb       bl rsym._ZN3dio13CScreenRender17onJob_renderFrameEPhPFiPvS2_ES2_S2_
    ╎╎│││   0x00086db4      000050e3       cmp r0, 0
   ┌──────< 0x00086db8      0800001a       bne 0x86de0
   │╎╎│└──> 0x00086dbc      0050a0e3       mov r5, 0
  ┌────┌──> 0x00086dc0      0500a0e1       mov r0, r5
  ╎│╎╎│╎│   0x00086dc4      14d08de2       add sp, sp, 0x14
  ╎│╎╎│╎│   0x00086dc8      f080bde8       pop {r4, r5, r6, r7, pc}
  ╎│╎╎└───> 0x00086dcc      1410a0e3       mov r1, 0x14
  ╎│╎╎ ╎│   0x00086dd0      f051feeb       bl rsym.__LogCategory_Initialize
  ╎│╎╎ ╎│   0x00086dd4      000050e3       cmp r0, 0
  ╎│└─────< 0x00086dd8      ebffff0a       beq 0x86d8c
  ╎│ └────< 0x00086ddc      dbffffea       b 0x86d50
  ╎└──────> 0x00086de0      016001e3       movw r6, 0x1001
  ╎    ╎│   0x00086de4      006048e3       movt r6, 0x8000
  ╎    ╎│   0x00086de8      0650a0e1       mov r5, r6
  ╎   ┌───> 0x00086dec      070094e7       ldr r0, [r4, r7]
  ╎   ╎╎│   0x00086df0      003090e5       ldr r3, [r0]
  ╎   ╎╎│   0x00086df4      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ────────< 0x00086df8      f0ffffca       bgt 0x86dc0
  ╎   ╎╎│   0x00086dfc      010073e3       cmn r3, 1
  ╎  ┌────< 0x00086e00      0f00000a       beq 0x86e44
  ╎ ┌─────> 0x00086e04      0600a0e1       mov r0, r6
  ╎ ╎│╎╎│   0x00086e08      c34cfeeb       bl rsym._Z13printOMXError13OMX_ERRORTYPE
  ╎ ╎│╎╎│   0x00086e0c      07c094e7       ldr ip, [r4, r7]
  ╎ ╎│╎╎│   0x00086e10      60109fe5       ldr r1, [0x00086e78]        ; [0x86e78:4]=0xffff6300
  ╎ ╎│╎╎│   0x00086e14      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ╎ ╎│╎╎│   0x00086e18      64309fe5       ldr r3, [0x00086e84]        ; [0x86e84:4]=0xffff7024
  ╎ ╎│╎╎│   0x00086e1c      50e09fe5       ldr lr, [0x00086e74]        ; [0x86e74:4]=0xffff67cc
  ╎ ╎│╎╎│   0x00086e20      011084e0       add r1, r4, r1
  ╎ ╎│╎╎│   0x00086e24      033084e0       add r3, r4, r3
  ╎ ╎│╎╎│   0x00086e28      04608de5       str r6, [sp, 4]
  ╎ ╎│╎╎│   0x00086e2c      0e4084e0       add r4, r4, lr
  ╎ ╎│╎╎│   0x00086e30      00408de5       str r4, [sp]
  ╎ ╎│╎╎│   0x00086e34      08008de5       str r0, [sp, 8]
  ╎ ╎│╎╎│   0x00086e38      0c00a0e1       mov r0, ip
  ╎ ╎│╎╎│   0x00086e3c      1752feeb       bl rsym.LogPrintF
  └───────< 0x00086e40      deffffea       b 0x86dc0
    ╎└────> 0x00086e44      5a10a0e3       mov r1, 0x5a                ; 'Z'
    ╎ ╎╎│   0x00086e48      d251feeb       bl rsym.__LogCategory_Initialize
    ╎ ╎╎│   0x00086e4c      000050e3       cmp r0, 0
    ╎ ╎└──< 0x00086e50      daffff0a       beq 0x86dc0
    └─────< 0x00086e54      eaffffea       b 0x86e04
      ╎ └─> 0x00086e58      056001e3       movw r6, 0x1005
      ╎     0x00086e5c      0c709fe5       ldr r7, [0x00086e70]        ; [0x86e70:4]=0x1230
      ╎     0x00086e60      006048e3       movt r6, 0x8000
      ╎     0x00086e64      0650a0e1       mov r5, r6
      └───< 0x00086e68      dfffffea       b 0x86dec
            0x00086e6c      0c200300       andeq r2, r3, ip
            0x00086e70      30120000       andeq r1, r0, r0, lsr r2
            0x00086e74      cc67ffff       invalid
            0x00086e78      0063ffff       invalid
            0x00086e7c      6870ffff       invalid
            0x00086e80      d0100000       ldrdeq r1, r2, [r0], -r0
            0x00086e84      2470ffff       invalid
