            ;-- method.dio::CScreenRender.config_dio::st_screen_config_const_:
            ;-- dio::CScreenRender::config(dio::st_screen_config const&):
            0x0008bcc8      f0472de9       push {r4, r5, r6, r7, r8, sb, sl, lr}; RELOC 32 _ZN3dio13CScreenRender6configERKNS_16st_screen_configE @ 0x0008bcc8 ; dio::CScreenRender::config(dio::st_screen_config const&)
            0x0008bccc      80749fe5       ldr r7, [0x0008c154]        ; [0x8c154:4]=0x2d060
            0x0008bcd0      80349fe5       ldr r3, [0x0008c158]        ; [0x8c158:4]=0xffff7e9c
            0x0008bcd4      0150a0e1       mov r5, r1
            0x0008bcd8      07708fe0       add r7, pc, r7
            0x0008bcdc      006091e5       ldr r6, [r1]
            0x0008bce0      033087e0       add r3, r7, r3
            0x0008bce4      04e091e5       ldr lr, [r1, 4]
            0x0008bce8      10c091e5       ldr ip, [r1, 0x10]
            0x0008bcec      48d04de2       sub sp, sp, 0x48
            0x0008bcf0      0040a0e1       mov r4, r0
            0x0008bcf4      030093e8       ldm r3, {r0, r1}
            0x0008bcf8      142095e5       ldr r2, [r5, 0x14]
            0x0008bcfc      30a08de2       add sl, sp, 0x30
            0x0008bd00      0030a0e3       mov r3, 0
            0x0008bd04      28608de5       str r6, [sp, 0x28]
            0x0008bd08      30008de5       str r0, [sp, 0x30]
            0x0008bd0c      040084e2       add r0, r4, 4
            0x0008bd10      b410cae1       strh r1, [sl, 4]
            0x0008bd14      0310a0e1       mov r1, r3
            0x0008bd18      2ce08de5       str lr, [sp, 0x2c]
            0x0008bd1c      20c08de5       str ip, [sp, 0x20]
            0x0008bd20      24208de5       str r2, [sp, 0x24]
            0x0008bd24      18308de5       str r3, [sp, 0x18]
            0x0008bd28      1c308de5       str r3, [sp, 0x1c]
            0x0008bd2c      8436feeb       bl sym.imp.screen_create_context
            0x0008bd30      006050e2       subs r6, r0, 0
        ┌─< 0x0008bd34      0401001a       bne 0x8c14c
        │   0x0008bd38      080084e2       add r0, r4, 8
        │   0x0008bd3c      041094e5       ldr r1, [r4, 4]
        │   0x0008bd40      b03efeeb       bl sym.imp.screen_create_window
        │   0x0008bd44      006050e2       subs r6, r0, 0
       ┌──< 0x0008bd48      ff00001a       bne 0x8c14c
       ││   0x0008bd4c      08349fe5       ldr r3, [0x0008c15c]        ; [0x8c15c:4]=0x11a0
       ││   0x0008bd50      080094e5       ldr r0, [r4, 8]
       ││   0x0008bd54      033097e7       ldr r3, [r7, r3]
       ││   0x0008bd58      001093e5       ldr r1, [r3]
       ││   0x0008bd5c      3735feeb       bl sym.imp.screen_create_window_group
       ││   0x0008bd60      006050e2       subs r6, r0, 0
      ┌───< 0x0008bd64      f800001a       bne 0x8c14c
      │││   0x0008bd68      f0839fe5       ldr r8, [0x0008c160]        ; [0x8c160:4]=0x10a4
      │││   0x0008bd6c      18608de2       add r6, sp, 0x18
      │││   0x0008bd70      ec139fe5       ldr r1, [0x0008c164]        ; [0x8c164:4]=0xffff89a0
      │││   0x0008bd74      0600a0e1       mov r0, r6
      │││   0x0008bd78      242095e5       ldr r2, [r5, 0x24]
      │││   0x0008bd7c      011087e0       add r1, r7, r1
      │││   0x0008bd80      eb3bfeeb       bl sym.imp.sprintf
      │││   0x0008bd84      080097e7       ldr r0, [r7, r8]
      │││   0x0008bd88      003090e5       ldr r3, [r0]
      │││   0x0008bd8c      320053e3       cmp r3, 0x32                ; '2'
     ┌────< 0x0008bd90      880000da       ble 0x8bfb8
   ┌┌─────> 0x0008bd94      0600a0e1       mov r0, r6
   ╎╎││││   0x0008bd98      089094e5       ldr sb, [r4, 8]
   ╎╎││││   0x0008bd9c      2936feeb       bl sym.imp.strlen
   ╎╎││││   0x0008bda0      0630a0e1       mov r3, r6
   ╎╎││││   0x0008bda4      1410a0e3       mov r1, 0x14
   ╎╎││││   0x0008bda8      0020a0e1       mov r2, r0
   ╎╎││││   0x0008bdac      0900a0e1       mov r0, sb
   ╎╎││││   0x0008bdb0      f23dfeeb       bl sym.imp.screen_set_window_property_cv
   ╎╎││││   0x0008bdb4      006050e2       subs r6, r0, 0
  ┌───────< 0x0008bdb8      b000001a       bne 0x8c080
  │╎╎││││   0x0008bdbc      0a00a0e1       mov r0, sl
  │╎╎││││   0x0008bdc0      089094e5       ldr sb, [r4, 8]
  │╎╎││││   0x0008bdc4      1f36feeb       bl sym.imp.strlen
  │╎╎││││   0x0008bdc8      0a30a0e1       mov r3, sl
  │╎╎││││   0x0008bdcc      0710a0e3       mov r1, 7
  │╎╎││││   0x0008bdd0      0020a0e1       mov r2, r0
  │╎╎││││   0x0008bdd4      0900a0e1       mov r0, sb
  │╎╎││││   0x0008bdd8      e83dfeeb       bl sym.imp.screen_set_window_property_cv
  │╎╎││││   0x0008bddc      48208de2       add r2, sp, 0x48
  │╎╎││││   0x0008bde0      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bde4      3310a0e3       mov r1, 0x33                ; '3'
  │╎╎││││   0x0008bde8      046022e5       str r6, [r2, -4]!
  │╎╎││││   0x0008bdec      2a3cfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008bdf0      006050e2       subs r6, r0, 0
  ────────< 0x0008bdf4      a100001a       bne 0x8c080
  │╎╎││││   0x0008bdf8      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bdfc      0e10a0e3       mov r1, 0xe
  │╎╎││││   0x0008be00      202085e2       add r2, r5, 0x20
  │╎╎││││   0x0008be04      243cfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008be08      006050e2       subs r6, r0, 0
  ────────< 0x0008be0c      9b00001a       bne 0x8c080
  │╎╎││││   0x0008be10      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008be14      3010a0e3       mov r1, 0x30                ; '0'
  │╎╎││││   0x0008be18      282085e2       add r2, r5, 0x28
  │╎╎││││   0x0008be1c      1e3cfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008be20      006050e2       subs r6, r0, 0
  ────────< 0x0008be24      9500001a       bne 0x8c080
  │╎╎││││   0x0008be28      48208de2       add r2, sp, 0x48
  │╎╎││││   0x0008be2c      0130a0e3       mov r3, 1
  │╎╎││││   0x0008be30      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008be34      2d10a0e3       mov r1, 0x2d                ; '-'
  │╎╎││││   0x0008be38      083022e5       str r3, [r2, -8]!
  │╎╎││││   0x0008be3c      163cfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008be40      006050e2       subs r6, r0, 0
  ────────< 0x0008be44      8d00001a       bne 0x8c080
  │╎╎││││   0x0008be48      080097e7       ldr r0, [r7, r8]
  │╎╎││││   0x0008be4c      003090e5       ldr r3, [r0]
  │╎╎││││   0x0008be50      320053e3       cmp r3, 0x32                ; '2'
  ────────< 0x0008be54      0e0000ca       bgt 0x8be94
  │╎╎││││   0x0008be58      010073e3       cmn r3, 1
  ────────< 0x0008be5c      a600000a       beq 0x8c0fc
  ────────> 0x0008be60      04c095e5       ldr ip, [r5, 4]
  │╎╎││││   0x0008be64      3220a0e3       mov r2, 0x32                ; '2'
  │╎╎││││   0x0008be68      00e095e5       ldr lr, [r5]
  │╎╎││││   0x0008be6c      f4129fe5       ldr r1, [0x0008c168]        ; [0x8c168:4]=0xffff8004
  │╎╎││││   0x0008be70      f4329fe5       ldr r3, [0x0008c16c]        ; [0x8c16c:4]=0xffff7df4
  │╎╎││││   0x0008be74      08c08de5       str ip, [sp, 8]
  │╎╎││││   0x0008be78      011087e0       add r1, r7, r1
  │╎╎││││   0x0008be7c      ecc29fe5       ldr ip, [0x0008c170]        ; [0x8c170:4]=0xffff7f78
  │╎╎││││   0x0008be80      033087e0       add r3, r7, r3
  │╎╎││││   0x0008be84      080097e7       ldr r0, [r7, r8]
  │╎╎││││   0x0008be88      0cc087e0       add ip, r7, ip
  │╎╎││││   0x0008be8c      00508de8       stm sp, {ip, lr}
  │╎╎││││   0x0008be90      023efeeb       bl rsym.LogPrintF
  ────────> 0x0008be94      2810a0e3       mov r1, 0x28                ; '('
  │╎╎││││   0x0008be98      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008be9c      01208de0       add r2, sp, r1
  │╎╎││││   0x0008bea0      fd3bfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008bea4      006050e2       subs r6, r0, 0
  ────────< 0x0008bea8      7400001a       bne 0x8c080
  │╎╎││││   0x0008beac      080097e7       ldr r0, [r7, r8]
  │╎╎││││   0x0008beb0      003090e5       ldr r3, [r0]
  │╎╎││││   0x0008beb4      320053e3       cmp r3, 0x32                ; '2'
  ────────< 0x0008beb8      4c0000da       ble 0x8bff0
  ────────> 0x0008bebc      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bec0      2310a0e3       mov r1, 0x23                ; '#'
  │╎╎││││   0x0008bec4      20208de2       add r2, sp, 0x20
  │╎╎││││   0x0008bec8      f33bfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008becc      006050e2       subs r6, r0, 0
  ────────< 0x0008bed0      6a00001a       bne 0x8c080
  │╎╎││││   0x0008bed4      0400a0e1       mov r0, r4
  │╎╎││││   0x0008bed8      081095e5       ldr r1, [r5, 8]
  │╎╎││││   0x0008bedc      0c2095e5       ldr r2, [r5, 0xc]
  │╎╎││││   0x0008bee0      5c33feeb       bl rsym._ZN3dio13CScreenRender20setWindowsSourceSizeEjj
  │╎╎││││   0x0008bee4      006050e2       subs r6, r0, 0
  ────────< 0x0008bee8      6400001a       bne 0x8c080
  │╎╎││││   0x0008beec      48a08de2       add sl, sp, 0x48
  │╎╎││││   0x0008bef0      ff30a0e3       mov r3, 0xff
  │╎╎││││   0x0008bef4      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bef8      1010a0e3       mov r1, 0x10
  │╎╎││││   0x0008befc      10302ae5       str r3, [sl, -0x10]!
  │╎╎││││   0x0008bf00      0a20a0e1       mov r2, sl
  │╎╎││││   0x0008bf04      e43bfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008bf08      006050e2       subs r6, r0, 0
  ────────< 0x0008bf0c      5b00001a       bne 0x8c080
  │╎╎││││   0x0008bf10      0a20a0e1       mov r2, sl
  │╎╎││││   0x0008bf14      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bf18      0110a0e3       mov r1, 1
  │╎╎││││   0x0008bf1c      38608de5       str r6, [sp, 0x38]
  │╎╎││││   0x0008bf20      dd3bfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008bf24      006050e2       subs r6, r0, 0
  ────────< 0x0008bf28      5400001a       bne 0x8c080
  │╎╎││││   0x0008bf2c      48208de2       add r2, sp, 0x48
  │╎╎││││   0x0008bf30      0430a0e3       mov r3, 4
  │╎╎││││   0x0008bf34      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bf38      2e10a0e3       mov r1, 0x2e                ; '.'
  │╎╎││││   0x0008bf3c      0c3022e5       str r3, [r2, -0xc]!
  │╎╎││││   0x0008bf40      d53bfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008bf44      006050e2       subs r6, r0, 0
  ────────< 0x0008bf48      4c00001a       bne 0x8c080
  │╎╎││││   0x0008bf4c      08c095e5       ldr ip, [r5, 8]
  │╎╎││││   0x0008bf50      0510a0e3       mov r1, 5
  │╎╎││││   0x0008bf54      0c3095e5       ldr r3, [r5, 0xc]
  │╎╎││││   0x0008bf58      10208de2       add r2, sp, 0x10
  │╎╎││││   0x0008bf5c      080094e5       ldr r0, [r4, 8]
  │╎╎││││   0x0008bf60      10c08de5       str ip, [sp, 0x10]
  │╎╎││││   0x0008bf64      14308de5       str r3, [sp, 0x14]
  │╎╎││││   0x0008bf68      cb3bfeeb       bl sym.imp.screen_set_window_property_iv
  │╎╎││││   0x0008bf6c      006050e2       subs r6, r0, 0
  ────────< 0x0008bf70      4200001a       bne 0x8c080
  │╎╎││││   0x0008bf74      080097e7       ldr r0, [r7, r8]
  │╎╎││││   0x0008bf78      003090e5       ldr r3, [r0]
  │╎╎││││   0x0008bf7c      320053e3       cmp r3, 0x32                ; '2'
  ────────< 0x0008bf80      2a0000da       ble 0x8c030
  ────────> 0x0008bf84      1c1095e5       ldr r1, [r5, 0x1c]
  │╎╎││││   0x0008bf88      000051e3       cmp r1, 0
  ────────< 0x0008bf8c      3600001a       bne 0x8c06c
  ────────> 0x0008bf90      0f00b5e8       ldm r5!, {r0, r1, r2, r3}
  │╎╎││││   0x0008bf94      104084e2       add r4, r4, 0x10
  │╎╎││││   0x0008bf98      0f00a4e8       stm r4!, {r0, r1, r2, r3}
  │╎╎││││   0x0008bf9c      0f00b5e8       ldm r5!, {r0, r1, r2, r3}
  │╎╎││││   0x0008bfa0      0f00a4e8       stm r4!, {r0, r1, r2, r3}
  │╎╎││││   0x0008bfa4      070095e8       ldm r5, {r0, r1, r2}
  │╎╎││││   0x0008bfa8      070084e8       stm r4, {r0, r1, r2}
  ────────> 0x0008bfac      0600a0e1       mov r0, r6
  │╎╎││││   0x0008bfb0      48d08de2       add sp, sp, 0x48
  │╎╎││││   0x0008bfb4      f087bde8       pop {r4, r5, r6, r7, r8, sb, sl, pc}
  │╎╎└────> 0x0008bfb8      010073e3       cmn r3, 1
  │╎╎┌────< 0x0008bfbc      4900000a       beq 0x8c0e8
  ────────> 0x0008bfc0      a0119fe5       ldr r1, [0x0008c168]        ; [0x8c168:4]=0xffff8004
  │╎╎││││   0x0008bfc4      3220a0e3       mov r2, 0x32                ; '2'
  │╎╎││││   0x0008bfc8      a4319fe5       ldr r3, [0x0008c174]        ; [0x8c174:4]=0xffff7dcc
  │╎╎││││   0x0008bfcc      9cc19fe5       ldr ip, [0x0008c170]        ; [0x8c170:4]=0xffff7f78
  │╎╎││││   0x0008bfd0      011087e0       add r1, r7, r1
  │╎╎││││   0x0008bfd4      080097e7       ldr r0, [r7, r8]
  │╎╎││││   0x0008bfd8      033087e0       add r3, r7, r3
  │╎╎││││   0x0008bfdc      0cc087e0       add ip, r7, ip
  │╎╎││││   0x0008bfe0      04608de5       str r6, [sp, 4]
  │╎╎││││   0x0008bfe4      00c08de5       str ip, [sp]
  │╎╎││││   0x0008bfe8      ac3dfeeb       bl rsym.LogPrintF
  │└──────< 0x0008bfec      68ffffea       b 0x8bd94
  ────────> 0x0008bff0      010073e3       cmn r3, 1
  │┌──────< 0x0008bff4      4500000a       beq 0x8c110
  ────────> 0x0008bff8      14c095e5       ldr ip, [r5, 0x14]
  ││╎││││   0x0008bffc      3220a0e3       mov r2, 0x32                ; '2'
  ││╎││││   0x0008c000      10e095e5       ldr lr, [r5, 0x10]
  ││╎││││   0x0008c004      5c119fe5       ldr r1, [0x0008c168]        ; [0x8c168:4]=0xffff8004
  ││╎││││   0x0008c008      68319fe5       ldr r3, [0x0008c178]        ; [0x8c178:4]=0xffff7e34
  ││╎││││   0x0008c00c      08c08de5       str ip, [sp, 8]
  ││╎││││   0x0008c010      011087e0       add r1, r7, r1
  ││╎││││   0x0008c014      54c19fe5       ldr ip, [0x0008c170]        ; [0x8c170:4]=0xffff7f78
  ││╎││││   0x0008c018      033087e0       add r3, r7, r3
  ││╎││││   0x0008c01c      080097e7       ldr r0, [r7, r8]
  ││╎││││   0x0008c020      0cc087e0       add ip, r7, ip
  ││╎││││   0x0008c024      00508de8       stm sp, {ip, lr}
  ││╎││││   0x0008c028      9c3dfeeb       bl rsym.LogPrintF
  ────────< 0x0008c02c      a2ffffea       b 0x8bebc
  ────────> 0x0008c030      010073e3       cmn r3, 1
  ────────< 0x0008c034      3a00000a       beq 0x8c124
  ────────> 0x0008c038      1cc095e5       ldr ip, [r5, 0x1c]
  ││╎││││   0x0008c03c      3220a0e3       mov r2, 0x32                ; '2'
  ││╎││││   0x0008c040      20119fe5       ldr r1, [0x0008c168]        ; [0x8c168:4]=0xffff8004
  ││╎││││   0x0008c044      30319fe5       ldr r3, [0x0008c17c]        ; [0x8c17c:4]=0xffff7e6c
  ││╎││││   0x0008c048      04c08de5       str ip, [sp, 4]
  ││╎││││   0x0008c04c      011087e0       add r1, r7, r1
  ││╎││││   0x0008c050      18c19fe5       ldr ip, [0x0008c170]        ; [0x8c170:4]=0xffff7f78
  ││╎││││   0x0008c054      033087e0       add r3, r7, r3
  ││╎││││   0x0008c058      080097e7       ldr r0, [r7, r8]
  ││╎││││   0x0008c05c      0cc087e0       add ip, r7, ip
  ││╎││││   0x0008c060      00c08de5       str ip, [sp]
  ││╎││││   0x0008c064      8d3dfeeb       bl rsym.LogPrintF
  ────────< 0x0008c068      c5ffffea       b 0x8bf84
  ────────> 0x0008c06c      0400a0e1       mov r0, r4
  ││╎││││   0x0008c070      da35feeb       bl rsym._ZN3dio13CScreenRender21setNumOfRenderBuffersEj
  ││╎││││   0x0008c074      000050e3       cmp r0, 0
  ────────< 0x0008c078      c4ffff0a       beq 0x8bf90
  ││╎││││   0x0008c07c      0060a0e1       mov r6, r0
  └───────> 0x0008c080      080097e7       ldr r0, [r7, r8]
   │╎││││   0x0008c084      003090e5       ldr r3, [r0]
   │╎││││   0x0008c088      5a0053e3       cmp r3, 0x5a                ; 'Z'
  ┌───────< 0x0008c08c      120000ca       bgt 0x8c0dc
  ││╎││││   0x0008c090      010073e3       cmn r3, 1
  ────────< 0x0008c094      2700000a       beq 0x8c138
  ────────> 0x0008c098      f834feeb       bl sym.imp.__get_errno_ptr
  ││╎││││   0x0008c09c      000090e5       ldr r0, [r0]
  ││╎││││   0x0008c0a0      303dfeeb       bl sym.imp.strerror
  ││╎││││   0x0008c0a4      08c097e7       ldr ip, [r7, r8]
  ││╎││││   0x0008c0a8      b8109fe5       ldr r1, [0x0008c168]        ; [0x8c168:4]=0xffff8004
  ││╎││││   0x0008c0ac      5a20a0e3       mov r2, 0x5a                ; 'Z'
  ││╎││││   0x0008c0b0      c8309fe5       ldr r3, [0x0008c180]        ; [0x8c180:4]=0xffff7d1c
  ││╎││││   0x0008c0b4      b4e09fe5       ldr lr, [0x0008c170]        ; [0x8c170:4]=0xffff7f78
  ││╎││││   0x0008c0b8      011087e0       add r1, r7, r1
  ││╎││││   0x0008c0bc      033087e0       add r3, r7, r3
  ││╎││││   0x0008c0c0      04608de5       str r6, [sp, 4]
  ││╎││││   0x0008c0c4      0e7087e0       add r7, r7, lr
  ││╎││││   0x0008c0c8      08608de5       str r6, [sp, 8]
  ││╎││││   0x0008c0cc      00708de5       str r7, [sp]
  ││╎││││   0x0008c0d0      0c008de5       str r0, [sp, 0xc]
  ││╎││││   0x0008c0d4      0c00a0e1       mov r0, ip
  ││╎││││   0x0008c0d8      703dfeeb       bl rsym.LogPrintF
  └───────> 0x0008c0dc      0400a0e1       mov r0, r4
   │╎││││   0x0008c0e0      623dfeeb       bl rsym._ZN3dio13CScreenRender4stopEv
  ────────< 0x0008c0e4      b0ffffea       b 0x8bfac
   │╎└────> 0x0008c0e8      3210a0e3       mov r1, 0x32                ; '2'
   │╎ │││   0x0008c0ec      293dfeeb       bl rsym.__LogCategory_Initialize
   │╎ │││   0x0008c0f0      000050e3       cmp r0, 0
   │└─────< 0x0008c0f4      26ffff0a       beq 0x8bd94
  ────────< 0x0008c0f8      b0ffffea       b 0x8bfc0
  ────────> 0x0008c0fc      3210a0e3       mov r1, 0x32                ; '2'
   │  │││   0x0008c100      243dfeeb       bl rsym.__LogCategory_Initialize
   │  │││   0x0008c104      000050e3       cmp r0, 0
  ────────< 0x0008c108      61ffff0a       beq 0x8be94
  ────────< 0x0008c10c      53ffffea       b 0x8be60
   └──────> 0x0008c110      3210a0e3       mov r1, 0x32                ; '2'
      │││   0x0008c114      1f3dfeeb       bl rsym.__LogCategory_Initialize
      │││   0x0008c118      000050e3       cmp r0, 0
  ────────< 0x0008c11c      66ffff0a       beq 0x8bebc
  ────────< 0x0008c120      b4ffffea       b 0x8bff8
  ────────> 0x0008c124      3210a0e3       mov r1, 0x32                ; '2'
      │││   0x0008c128      1a3dfeeb       bl rsym.__LogCategory_Initialize
      │││   0x0008c12c      000050e3       cmp r0, 0
  ────────< 0x0008c130      93ffff0a       beq 0x8bf84
  ────────< 0x0008c134      bfffffea       b 0x8c038
  ────────> 0x0008c138      5a10a0e3       mov r1, 0x5a                ; 'Z'
      │││   0x0008c13c      153dfeeb       bl rsym.__LogCategory_Initialize
      │││   0x0008c140      000050e3       cmp r0, 0
  ────────< 0x0008c144      e4ffff0a       beq 0x8c0dc
  ────────< 0x0008c148      d2ffffea       b 0x8c098
      └└└─> 0x0008c14c      0c809fe5       ldr r8, [0x0008c160]        ; [0x8c160:4]=0x10a4
  ────────< 0x0008c150      caffffea       b 0x8c080
            0x0008c154      60d00200       andeq sp, r2, r0, rrx
            0x0008c158      9c7effff       invalid
            0x0008c15c      a0110000       andeq r1, r0, r0, lsr 3
            0x0008c160      a4100000       andeq r1, r0, r4, lsr 1
            0x0008c164      a089ffff       invalid
            0x0008c168      0480ffff       invalid
            0x0008c16c      f47dffff       invalid
            0x0008c170      787fffff       invalid
            0x0008c174      cc7dffff       invalid
            0x0008c178      347effff       invalid
            0x0008c17c      6c7effff       invalid
            0x0008c180      1c7dffff       invalid
