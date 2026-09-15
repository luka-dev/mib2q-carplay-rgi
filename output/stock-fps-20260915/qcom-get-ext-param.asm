            ;-- method.MMIVdecController.GetOmxExtParam_MMI_OmxParamCmdType_:
            ;-- MMIVdecController::GetOmxExtParam(MMI_OmxParamCmdType*):
            0x000165f0      f04f2de9       push {r4, r5, r6, r7, r8, sb, sl, fp, lr}; RELOC 32 _ZN17MMIVdecController14GetOmxExtParamEP19MMI_OmxParamCmdType @ 0x000165f0 ; MMIVdecController::GetOmxExtParam(MMI_OmxParamCmdType*)
            0x000165f4      24599fe5       ldr r5, [0x00016f20]        ; [0x16f20:4]=0xeba0
            0x000165f8      24399fe5       ldr r3, [0x00016f24]        ; [0x16f24:4]=0x3b4
            0x000165fc      57df4de2       sub sp, sp, 0x15c
            0x00016600      05508fe0       add r5, pc, r5
            0x00016604      00b091e5       ldr fp, [r1]
            0x00016608      0160a0e1       mov r6, r1
            0x0001660c      0040a0e1       mov r4, r0
            0x00016610      032095e7       ldr r2, [r5, r3]
            0x00016614      9f334be2       sub r3, fp, 0x7c000002
            0x00016618      313643e2       sub r3, r3, 0x3100000
            0x0001661c      002092e5       ldr r2, [r2]
            0x00016620      54218de5       str r2, [sp, 0x154]
            0x00016624      0e0053e3       cmp r3, 0xe
            0x00016628      03f18f90       addls pc, pc, r3, lsl 2     ; [0x16630:4]=0xea0000f1
        ┌─< 0x0001662c      ee0100ea       b 0x16dec
       ┌──< 0x00016630      f10000ea       b 0x169fc
      ┌───< 0x00016634      100000ea       b 0x1667c
     ┌────< 0x00016638      0f0000ea       b 0x1667c
    ┌─────< 0x0001663c      0a0000ea       b 0x1666c
   ┌──────< 0x00016640      140000ea       b 0x16698
  ┌───────< 0x00016644      190000ea       b 0x166b0
  ────────< 0x00016648      430000ea       b 0x1675c
  ────────< 0x0001664c      950000ea       b 0x168a8
  ────────< 0x00016650      c50000ea       b 0x1696c
  ────────< 0x00016654      100100ea       b 0x16a9c
  ────────< 0x00016658      e30100ea       b 0x16dec
  ────────< 0x0001665c      610000ea       b 0x167e8
  ────────< 0x00016660      2c0100ea       b 0x16b18
  ────────< 0x00016664      630100ea       b 0x16bf8
  ────────< 0x00016668      ad0100ea       b 0x16d24
  ││└─────> 0x0001666c      043091e5       ldr r3, [r1, 4]
  ││ ││││   0x00016670      1202a0e3       mov r0, 0x20000001
  ││ ││││   0x00016674      142094e5       ldr r2, [r4, 0x14]
  ││┌─────< 0x00016678      040000ea       b 0x16690
  │││└└───> 0x0001667c      043091e5       ldr r3, [r1, 4]
  │││  ││   0x00016680      1202a0e3       mov r0, 0x20000001
  │││  ││   0x00016684      181094e5       ldr r1, [r4, 0x18]
  │││  ││   0x00016688      142094e5       ldr r2, [r4, 0x14]
  │││  ││   0x0001668c      101083e5       str r1, [r3, 0x10]
  ││└─────> 0x00016690      0c2083e5       str r2, [r3, 0xc]
  ││  ┌───< 0x00016694      150200ea       b 0x16ef0
  │└──────> 0x00016698      043091e5       ldr r3, [r1, 4]
  │   │││   0x0001669c      1202a0e3       mov r0, 0x20000001
  │   │││   0x000166a0      181094e5       ldr r1, [r4, 0x18]
  │   │││   0x000166a4      1c2094e5       ldr r2, [r4, 0x1c]
  │   │││   0x000166a8      0c1083e5       str r1, [r3, 0xc]
  │  ┌────< 0x000166ac      e40100ea       b 0x16e44
  └───────> 0x000166b0      562f8de2       add r2, sp, 0x158
     ││││   0x000166b4      1130a0e3       mov r3, 0x11
     ││││   0x000166b8      00c0a0e3       mov ip, 0
     ││││   0x000166bc      0210a0e3       mov r1, 2
     ││││   0x000166c0      303122e5       str r3, [r2, -0x130]!
     ││││   0x000166c4      d03602e3       movw r3, 0x26d0
     ││││   0x000166c8      48c08de5       str ip, [sp, 0x48]
     ││││   0x000166cc      030090e7       ldr r0, [r0, r3]
     ││││   0x000166d0      0830a0e3       mov r3, 8
     ││││   0x000166d4      08c08de5       str ip, [sp, 8]
     ││││   0x000166d8      01c08ce2       add ip, ip, 1
     ││││   0x000166dc      2cc08de5       str ip, [sp, 0x2c]
     ││││   0x000166e0      48c08de2       add ip, sp, 0x48
     ││││   0x000166e4      00c08de5       str ip, [sp]
     ││││   0x000166e8      04c0a0e3       mov ip, 4
     ││││   0x000166ec      04c08de5       str ip, [sp, 4]
     ││││   0x000166f0      048096e5       ldr r8, [r6, 4]
     ││││   0x000166f4      0bc0ffeb       bl rsym.QVD_IOControl
     ││││   0x000166f8      007050e2       subs r7, r0, 0
    ┌─────< 0x000166fc      0600001a       bne 0x1671c
    │││││   0x00016700      0730a0e1       mov r3, r7
    │││││   0x00016704      0400a0e1       mov r0, r4
    │││││   0x00016708      48109de5       ldr r1, [sp, 0x48]
    │││││   0x0001670c      0c2088e2       add r2, r8, 0xc
    │││││   0x00016710      77bfffeb       bl rsym._ZN17MMIVdecController14MapPictureTypeEmPmb
    │││││   0x00016714      007050e2       subs r7, r0, 0
   ┌──────< 0x00016718      fb01000a       beq 0x16f0c
   │└─────> 0x0001671c      04089fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
   │ ││││   0x00016720      00c0a0e3       mov ip, 0
   │ ││││   0x00016724      00e89fe5       ldr lr, [0x00016f2c]        ; [0x16f2c:4]=0xffffcede
   │ ││││   0x00016728      1f1c00e3       movw r1, 0xc1f              ; '\x1f\f'
   │ ││││   0x0001672c      0cc088e5       str ip, [r8, 0xc]
   │ ││││   0x00016730      000085e0       add r0, r5, r0
   │ ││││   0x00016734      0620a0e3       mov r2, 6
   │ ││││   0x00016738      0330a0e3       mov r3, 3
   │ ││││   0x0001673c      0ee085e0       add lr, r5, lr
   │ ││││   0x00016740      0cc08de5       str ip, [sp, 0xc]
   │ ││││   0x00016744      00e08de5       str lr, [sp]
   │ ││││   0x00016748      80108de9       stmib sp, {r7, ip}
   │ ││││   0x0001674c      07c0ffeb       bl sym.imp.MM_OutputDebugMessage
   │ ││││   0x00016750      0400a0e1       mov r0, r4
   │ ││││   0x00016754      0710a0e1       mov r1, r7
   │┌─────< 0x00016758      a00100ea       b 0x16de0
  ────────> 0x0001675c      562f8de2       add r2, sp, 0x158
   ││││││   0x00016760      1230a0e3       mov r3, 0x12
   ││││││   0x00016764      0210a0e3       mov r1, 2
   ││││││   0x00016768      0180a0e3       mov r8, 1
   ││││││   0x0001676c      303122e5       str r3, [r2, -0x130]!
   ││││││   0x00016770      d03602e3       movw r3, 0x26d0
   ││││││   0x00016774      4cc08de2       add ip, sp, 0x4c
   ││││││   0x00016778      4c808de5       str r8, [sp, 0x4c]
   ││││││   0x0001677c      030090e7       ldr r0, [r0, r3]
   ││││││   0x00016780      0830a0e3       mov r3, 8
   ││││││   0x00016784      00c08de5       str ip, [sp]
   ││││││   0x00016788      0070a0e3       mov r7, 0
   ││││││   0x0001678c      04c0a0e3       mov ip, 4
   ││││││   0x00016790      08708de5       str r7, [sp, 8]
   ││││││   0x00016794      04c08de5       str ip, [sp, 4]
   ││││││   0x00016798      2c808de5       str r8, [sp, 0x2c]
   ││││││   0x0001679c      049096e5       ldr sb, [r6, 4]
   ││││││   0x000167a0      e0bfffeb       bl rsym.QVD_IOControl
   ││││││   0x000167a4      00a050e2       subs sl, r0, 0
  ┌───────< 0x000167a8      0700000a       beq 0x167cc
  │││││││   0x000167ac      74079fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x000167b0      0620a0e3       mov r2, 6
  │││││││   0x000167b4      0330a0e3       mov r3, 3
  │││││││   0x000167b8      381c00e3       movw r1, 0xc38              ; '8\f'
  │││││││   0x000167bc      000085e0       add r0, r5, r0
  │││││││   0x000167c0      0c708de5       str r7, [sp, 0xc]
  │││││││   0x000167c4      60c79fe5       ldr ip, [0x00016f2c]        ; [0x16f2c:4]=0xffffcede
  ────────< 0x000167c8      570000ea       b 0x1692c
  └───────> 0x000167cc      4c309de5       ldr r3, [sp, 0x4c]
   ││││││   0x000167d0      013053e2       subs r3, r3, 1
   ││││││   0x000167d4      0130a013       movne r3, 1
   ││││││   0x000167d8      0c3089e5       str r3, [sb, 0xc]
   ││││││   0x000167dc      000053e3       cmp r3, 0
   ││││││   0x000167e0      10808915       strne r8, [sb, 0x10]
  ┌───────< 0x000167e4      c80100ea       b 0x16f0c
  ────────> 0x000167e8      047091e5       ldr r7, [r1, 4]
  │││││││   0x000167ec      000057e3       cmp r7, 0
  ────────< 0x000167f0      bd01000a       beq 0x16eec
  │││││││   0x000167f4      003097e5       ldr r3, [r7]
  │││││││   0x000167f8      1c0053e3       cmp r3, 0x1c
  ────────< 0x000167fc      ba01001a       bne 0x16eec
  │││││││   0x00016800      562f8de2       add r2, sp, 0x158
  │││││││   0x00016804      d01602e3       movw r1, 0x26d0
  │││││││   0x00016808      08c0a0e3       mov ip, 8
  │││││││   0x0001680c      1630a0e3       mov r3, 0x16
  │││││││   0x00016810      010090e7       ldr r0, [r0, r1]
  │││││││   0x00016814      0210a0e3       mov r1, 2
  │││││││   0x00016818      303122e5       str r3, [r2, -0x130]!
  │││││││   0x0001681c      0c30a0e1       mov r3, ip
  │││││││   0x00016820      04c08de5       str ip, [sp, 4]
  │││││││   0x00016824      01a0a0e3       mov sl, 1
  │││││││   0x00016828      40c08de2       add ip, sp, 0x40
  │││││││   0x0001682c      2ca08de5       str sl, [sp, 0x2c]
  │││││││   0x00016830      00c08de5       str ip, [sp]
  │││││││   0x00016834      0080a0e3       mov r8, 0
  │││││││   0x00016838      08808de5       str r8, [sp, 8]
  │││││││   0x0001683c      b9bfffeb       bl rsym.QVD_IOControl
  │││││││   0x00016840      009050e2       subs sb, r0, 0
  ────────< 0x00016844      0e00000a       beq 0x16884
  │││││││   0x00016848      d8069fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x0001684c      5a1c00e3       movw r1, 0xc5a              ; 'Z\f'
  │││││││   0x00016850      d8c69fe5       ldr ip, [0x00016f30]        ; [0x16f30:4]=0xffffcf0a
  │││││││   0x00016854      0620a0e3       mov r2, 6
  │││││││   0x00016858      000085e0       add r0, r5, r0
  │││││││   0x0001685c      0330a0e3       mov r3, 3
  │││││││   0x00016860      0cc085e0       add ip, r5, ip
  │││││││   0x00016864      0c808de5       str r8, [sp, 0xc]
  │││││││   0x00016868      00c08de5       str ip, [sp]
  │││││││   0x0001686c      04908de5       str sb, [sp, 4]
  │││││││   0x00016870      08808de5       str r8, [sp, 8]
  │││││││   0x00016874      bdbfffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00016878      0400a0e1       mov r0, r4
  │││││││   0x0001687c      0910a0e1       mov r1, sb
  ────────< 0x00016880      560100ea       b 0x16de0
  ────────> 0x00016884      40309de5       ldr r3, [sp, 0x40]
  │││││││   0x00016888      1202a0e3       mov r0, 0x20000001
  │││││││   0x0001688c      020053e3       cmp r3, 2
  │││││││   0x00016890      44309de5       ldr r3, [sp, 0x44]
  │││││││   0x00016894      14a08705       streq sl, [r7, 0x14]
  │││││││   0x00016898      0ca08705       streq sl, [r7, 0xc]
  │││││││   0x0001689c      0c908715       strne sb, [r7, 0xc]
  │││││││   0x000168a0      183087e5       str r3, [r7, 0x18]
  ────────< 0x000168a4      910100ea       b 0x16ef0
  ────────> 0x000168a8      048091e5       ldr r8, [r1, 4]
  │││││││   0x000168ac      0070a0e3       mov r7, 0
  │││││││   0x000168b0      7c369fe5       ldr r3, [0x00016f34]        ; [0x16f34:4]=0x2000001
  │││││││   0x000168b4      2470cde5       strb r7, [sp, 0x24]
  │││││││   0x000168b8      10e098e5       ldr lr, [r8, 0x10]
  │││││││   0x000168bc      03005ee1       cmp lr, r3
  ────────< 0x000168c0      1d00001a       bne 0x1693c
  │││││││   0x000168c4      08c098e5       ldr ip, [r8, 8]
  │││││││   0x000168c8      01005ce3       cmp ip, 1
  ────────< 0x000168cc      1a00001a       bne 0x1693c
  │││││││   0x000168d0      d00602e3       movw r0, 0x26d0
  │││││││   0x000168d4      0830a0e3       mov r3, 8
  │││││││   0x000168d8      0210a0e3       mov r1, 2
  │││││││   0x000168dc      1c208de2       add r2, sp, 0x1c
  │││││││   0x000168e0      000094e7       ldr r0, [r4, r0]
  │││││││   0x000168e4      14e0a0e3       mov lr, 0x14
  │││││││   0x000168e8      04c08de5       str ip, [sp, 4]
  │││││││   0x000168ec      1ce08de5       str lr, [sp, 0x1c]
  │││││││   0x000168f0      24e08de2       add lr, sp, 0x24
  │││││││   0x000168f4      20c08de5       str ip, [sp, 0x20]
  │││││││   0x000168f8      00e08de5       str lr, [sp]
  │││││││   0x000168fc      08708de5       str r7, [sp, 8]
  │││││││   0x00016900      88bfffeb       bl rsym.QVD_IOControl
  │││││││   0x00016904      00a050e2       subs sl, r0, 0
  │││││││   0x00016908      2430dd05       ldrbeq r3, [sp, 0x24]
  ────────< 0x0001690c      7301000a       beq 0x16ee0
  │││││││   0x00016910      10069fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016914      0620a0e3       mov r2, 6
  │││││││   0x00016918      18c69fe5       ldr ip, [0x00016f38]        ; [0x16f38:4]=0xffffcf3b
  │││││││   0x0001691c      0330a0e3       mov r3, 3
  │││││││   0x00016920      000085e0       add r0, r5, r0
  │││││││   0x00016924      8e1c00e3       movw r1, 0xc8e
  │││││││   0x00016928      0c708de5       str r7, [sp, 0xc]
  ────────> 0x0001692c      0cc085e0       add ip, r5, ip
  │││││││   0x00016930      04a08de5       str sl, [sp, 4]
  │││││││   0x00016934      00c08de5       str ip, [sp]
  ────────< 0x00016938      4d0000ea       b 0x16a74
  ────────> 0x0001693c      e4059fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016940      981c00e3       movw r1, 0xc98
  │││││││   0x00016944      f0c59fe5       ldr ip, [0x00016f3c]        ; [0x16f3c:4]=0xffffcf71
  │││││││   0x00016948      0620a0e3       mov r2, 6
  │││││││   0x0001694c      000085e0       add r0, r5, r0
  │││││││   0x00016950      0330a0e3       mov r3, 3
  │││││││   0x00016954      0cc085e0       add ip, r5, ip
  │││││││   0x00016958      00508de8       stm sp, {ip, lr}
  │││││││   0x0001695c      00c0a0e3       mov ip, 0
  │││││││   0x00016960      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00016964      08c08de5       str ip, [sp, 8]
  ────────< 0x00016968      ea0000ea       b 0x16d18
  ────────> 0x0001696c      d00602e3       movw r0, 0x26d0
  │││││││   0x00016970      0210a0e3       mov r1, 2
  │││││││   0x00016974      1c208de2       add r2, sp, 0x1c
  │││││││   0x00016978      0830a0e3       mov r3, 8
  │││││││   0x0001697c      000094e7       ldr r0, [r4, r0]
  │││││││   0x00016980      01c0a0e3       mov ip, 1
  │││││││   0x00016984      0070a0e3       mov r7, 0
  │││││││   0x00016988      04c08de5       str ip, [sp, 4]
  │││││││   0x0001698c      2470cde5       strb r7, [sp, 0x24]
  │││││││   0x00016990      15e0a0e3       mov lr, 0x15
  │││││││   0x00016994      08708de5       str r7, [sp, 8]
  │││││││   0x00016998      1ce08de5       str lr, [sp, 0x1c]
  │││││││   0x0001699c      24e08de2       add lr, sp, 0x24
  │││││││   0x000169a0      20c08de5       str ip, [sp, 0x20]
  │││││││   0x000169a4      00e08de5       str lr, [sp]
  │││││││   0x000169a8      04a096e5       ldr sl, [r6, 4]
  │││││││   0x000169ac      5dbfffeb       bl rsym.QVD_IOControl
  │││││││   0x000169b0      008050e2       subs r8, r0, 0
  ────────< 0x000169b4      0a00000a       beq 0x169e4
  │││││││   0x000169b8      68059fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x000169bc      0620a0e3       mov r2, 6
  │││││││   0x000169c0      78c59fe5       ldr ip, [0x00016f40]        ; [0x16f40:4]=0xffffcf9e
  │││││││   0x000169c4      0330a0e3       mov r3, 3
  │││││││   0x000169c8      000085e0       add r0, r5, r0
  │││││││   0x000169cc      b31c00e3       movw r1, 0xcb3
  │││││││   0x000169d0      0cc085e0       add ip, r5, ip
  │││││││   0x000169d4      0c708de5       str r7, [sp, 0xc]
  │││││││   0x000169d8      00c08de5       str ip, [sp]
  │││││││   0x000169dc      04808de5       str r8, [sp, 4]
  ────────< 0x000169e0      b30000ea       b 0x16cb4
  ────────> 0x000169e4      2430dde5       ldrb r3, [sp, 0x24]
  │││││││   0x000169e8      1202a0e3       mov r0, 0x20000001
  │││││││   0x000169ec      013053e2       subs r3, r3, 1
  │││││││   0x000169f0      0130a013       movne r3, 1
  │││││││   0x000169f4      0c308ae5       str r3, [sl, 0xc]
  ────────< 0x000169f8      3c0100ea       b 0x16ef0
  │││││└──> 0x000169fc      d00602e3       movw r0, 0x26d0
  │││││ │   0x00016a00      0210a0e3       mov r1, 2
  │││││ │   0x00016a04      28208de2       add r2, sp, 0x28
  │││││ │   0x00016a08      0830a0e3       mov r3, 8
  │││││ │   0x00016a0c      000094e7       ldr r0, [r4, r0]
  │││││ │   0x00016a10      0070a0e3       mov r7, 0
  │││││ │   0x00016a14      01c0a0e3       mov ip, 1
  │││││ │   0x00016a18      08708de5       str r7, [sp, 8]
  │││││ │   0x00016a1c      2cc08de5       str ip, [sp, 0x2c]
  │││││ │   0x00016a20      0680a0e3       mov r8, 6
  │││││ │   0x00016a24      48c08de2       add ip, sp, 0x48
  │││││ │   0x00016a28      48708de5       str r7, [sp, 0x48]
  │││││ │   0x00016a2c      00c08de5       str ip, [sp]
  │││││ │   0x00016a30      04c0a0e3       mov ip, 4
  │││││ │   0x00016a34      28808de5       str r8, [sp, 0x28]
  │││││ │   0x00016a38      04c08de5       str ip, [sp, 4]
  │││││ │   0x00016a3c      049096e5       ldr sb, [r6, 4]
  │││││ │   0x00016a40      38bfffeb       bl rsym.QVD_IOControl
  │││││ │   0x00016a44      00a050e2       subs sl, r0, 0
  │││││┌──< 0x00016a48      0e00000a       beq 0x16a88
  │││││││   0x00016a4c      d4049fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016a50      0820a0e1       mov r2, r8
  │││││││   0x00016a54      e8c49fe5       ldr ip, [0x00016f44]        ; [0x16f44:4]=0xffffcfe2
  │││││││   0x00016a58      0330a0e3       mov r3, 3
  │││││││   0x00016a5c      000085e0       add r0, r5, r0
  │││││││   0x00016a60      ca1c00e3       movw r1, 0xcca
  │││││││   0x00016a64      0cc085e0       add ip, r5, ip
  │││││││   0x00016a68      0c708de5       str r7, [sp, 0xc]
  │││││││   0x00016a6c      00c08de5       str ip, [sp]
  │││││││   0x00016a70      04708de5       str r7, [sp, 4]
  ────────> 0x00016a74      08708de5       str r7, [sp, 8]
  │││││││   0x00016a78      3cbfffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00016a7c      0400a0e1       mov r0, r4
  │││││││   0x00016a80      0a10a0e1       mov r1, sl
  ────────< 0x00016a84      d50000ea       b 0x16de0
  │││││└──> 0x00016a88      48309de5       ldr r3, [sp, 0x48]
  │││││ │   0x00016a8c      1202a0e3       mov r0, 0x20000001
  │││││ │   0x00016a90      d335e0e7       ubfx r3, r3, 0xb, 1
  │││││ │   0x00016a94      0c3089e5       str r3, [sb, 0xc]
  │││││┌──< 0x00016a98      140100ea       b 0x16ef0
  ────────> 0x00016a9c      e87702e3       movw r7, 0x27e8
  │││││││   0x00016aa0      046091e5       ldr r6, [r1, 4]
  │││││││   0x00016aa4      073090e7       ldr r3, [r0, r7]
  │││││││   0x00016aa8      000053e3       cmp r3, 0
  ────────< 0x00016aac      1300000a       beq 0x16b00
  │││││││   0x00016ab0      01c0a0e3       mov ip, 1
  │││││││   0x00016ab4      142086e2       add r2, r6, 0x14
  │││││││   0x00016ab8      0cc086e5       str ip, [r6, 0xc]
  │││││││   0x00016abc      0300a0e1       mov r0, r3
  │││││││   0x00016ac0      0410a0e3       mov r1, 4
  │││││││   0x00016ac4      043093e5       ldr r3, [r3, 4]
  │││││││   0x00016ac8      33ff2fe1       blx r3
  │││││││   0x00016acc      073094e7       ldr r3, [r4, r7]
  │││││││   0x00016ad0      102086e2       add r2, r6, 0x10
  │││││││   0x00016ad4      0510a0e3       mov r1, 5
  │││││││   0x00016ad8      0300a0e1       mov r0, r3
  │││││││   0x00016adc      043093e5       ldr r3, [r3, 4]
  │││││││   0x00016ae0      33ff2fe1       blx r3
  │││││││   0x00016ae4      073094e7       ldr r3, [r4, r7]
  │││││││   0x00016ae8      182086e2       add r2, r6, 0x18
  │││││││   0x00016aec      0610a0e3       mov r1, 6
  │││││││   0x00016af0      0300a0e1       mov r0, r3
  │││││││   0x00016af4      043093e5       ldr r3, [r3, 4]
  │││││││   0x00016af8      33ff2fe1       blx r3
  ────────< 0x00016afc      020100ea       b 0x16f0c
  ────────> 0x00016b00      183086e5       str r3, [r6, 0x18]
  │││││││   0x00016b04      1202a0e3       mov r0, 0x20000001
  │││││││   0x00016b08      0c3086e5       str r3, [r6, 0xc]
  │││││││   0x00016b0c      143086e5       str r3, [r6, 0x14]
  │││││││   0x00016b10      103086e5       str r3, [r6, 0x10]
  ────────< 0x00016b14      f50000ea       b 0x16ef0
  ────────> 0x00016b18      d02602e3       movw r2, 0x26d0
  │││││││   0x00016b1c      08c0a0e3       mov ip, 8
  │││││││   0x00016b20      0c30a0e1       mov r3, ip
  │││││││   0x00016b24      0210a0e3       mov r1, 2
  │││││││   0x00016b28      020090e7       ldr r0, [r0, r2]
  │││││││   0x00016b2c      28208de2       add r2, sp, 0x28
  │││││││   0x00016b30      04c08de5       str ip, [sp, 4]
  │││││││   0x00016b34      0070a0e3       mov r7, 0
  │││││││   0x00016b38      0fc08ce2       add ip, ip, 0xf
  │││││││   0x00016b3c      08708de5       str r7, [sp, 8]
  │││││││   0x00016b40      28c08de5       str ip, [sp, 0x28]
  │││││││   0x00016b44      01c0a0e3       mov ip, 1
  │││││││   0x00016b48      2cc08de5       str ip, [sp, 0x2c]
  │││││││   0x00016b4c      38c08de2       add ip, sp, 0x38
  │││││││   0x00016b50      00c08de5       str ip, [sp]
  │││││││   0x00016b54      04a096e5       ldr sl, [r6, 4]
  │││││││   0x00016b58      f2beffeb       bl rsym.QVD_IOControl
  │││││││   0x00016b5c      008050e2       subs r8, r0, 0
  ────────< 0x00016b60      0700000a       beq 0x16b84
  │││││││   0x00016b64      bc039fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016b68      0620a0e3       mov r2, 6
  │││││││   0x00016b6c      0330a0e3       mov r3, 3
  │││││││   0x00016b70      fb1c00e3       movw r1, 0xcfb
  │││││││   0x00016b74      000085e0       add r0, r5, r0
  │││││││   0x00016b78      0c708de5       str r7, [sp, 0xc]
  │││││││   0x00016b7c      c4c39fe5       ldr ip, [0x00016f48]        ; [0x16f48:4]=0xffffd001
  ────────< 0x00016b80      480000ea       b 0x16ca8
  ────────> 0x00016b84      38309de5       ldr r3, [sp, 0x38]
  │││││││   0x00016b88      000053e3       cmp r3, 0
  ────────< 0x00016b8c      de00000a       beq 0x16f0c
  ────────< 0x00016b90      010000ea       b 0x16b9c
  ────────> 0x00016b94      a330a0e1       lsr r3, r3, 1
  │││││││   0x00016b98      018088e2       add r8, r8, 1
  ────────> 0x00016b9c      01c023e2       eor ip, r3, 1
  │││││││   0x00016ba0      0f0058e3       cmp r8, 0xf
  │││││││   0x00016ba4      00c0a083       movhi ip, 0
  │││││││   0x00016ba8      01c00c92       andls ip, ip, 1
  │││││││   0x00016bac      00005ce3       cmp ip, 0
  ────────< 0x00016bb0      f7ffff1a       bne 0x16b94
  │││││││   0x00016bb4      3c309de5       ldr r3, [sp, 0x3c]
  │││││││   0x00016bb8      108068e2       rsb r8, r8, 0x10
  │││││││   0x00016bbc      64039fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016bc0      031d00e3       movw r1, 0xd03
  │││││││   0x00016bc4      80e39fe5       ldr lr, [0x00016f4c]        ; [0x16f4c:4]=0xffffd023
  │││││││   0x00016bc8      0620a0e3       mov r2, 6
  │││││││   0x00016bcc      1388a0e1       lsl r8, r3, r8
  │││││││   0x00016bd0      000085e0       add r0, r5, r0
  │││││││   0x00016bd4      0c808ae5       str r8, [sl, 0xc]
  │││││││   0x00016bd8      0230a0e3       mov r3, 2
  │││││││   0x00016bdc      0ee085e0       add lr, r5, lr
  │││││││   0x00016be0      04808de5       str r8, [sp, 4]
  │││││││   0x00016be4      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00016be8      00e08de5       str lr, [sp]
  │││││││   0x00016bec      08c08de5       str ip, [sp, 8]
  │││││││   0x00016bf0      debeffeb       bl sym.imp.MM_OutputDebugMessage
  ────────< 0x00016bf4      c40000ea       b 0x16f0c
  ────────> 0x00016bf8      28a39fe5       ldr sl, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016bfc      0070a0e3       mov r7, 0
  │││││││   0x00016c00      48c39fe5       ldr ip, [0x00016f50]        ; [0x16f50:4]=0xffffd045
  │││││││   0x00016c04      0130a0e3       mov r3, 1
  │││││││   0x00016c08      0aa085e0       add sl, r5, sl
  │││││││   0x00016c0c      50808de2       add r8, sp, 0x50
  │││││││   0x00016c10      d11ea0e3       mov r1, 0xd10               ; '\x10\r'
  │││││││   0x00016c14      0a00a0e1       mov r0, sl
  │││││││   0x00016c18      0620a0e3       mov r2, 6
  │││││││   0x00016c1c      0cc085e0       add ip, r5, ip
  │││││││   0x00016c20      04708de5       str r7, [sp, 4]
  │││││││   0x00016c24      00c08de5       str ip, [sp]
  │││││││   0x00016c28      08708de5       str r7, [sp, 8]
  │││││││   0x00016c2c      0c708de5       str r7, [sp, 0xc]
  │││││││   0x00016c30      049096e5       ldr sb, [r6, 4]
  │││││││   0x00016c34      cdbeffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00016c38      0710a0e1       mov r1, r7
  │││││││   0x00016c3c      412fa0e3       mov r2, 0x104
  │││││││   0x00016c40      0800a0e1       mov r0, r8
  │││││││   0x00016c44      65bfffeb       bl sym.imp.std_memset
  │││││││   0x00016c48      d03602e3       movw r3, 0x26d0
  │││││││   0x00016c4c      0210a0e3       mov r1, 2
  │││││││   0x00016c50      28208de2       add r2, sp, 0x28
  │││││││   0x00016c54      030094e7       ldr r0, [r4, r3]
  │││││││   0x00016c58      0830a0e3       mov r3, 8
  │││││││   0x00016c5c      00808de5       str r8, [sp]
  │││││││   0x00016c60      01cca0e3       mov ip, 0x100
  │││││││   0x00016c64      08708de5       str r7, [sp, 8]
  │││││││   0x00016c68      50c08de5       str ip, [sp, 0x50]
  │││││││   0x00016c6c      18c0a0e3       mov ip, 0x18
  │││││││   0x00016c70      28c08de5       str ip, [sp, 0x28]
  │││││││   0x00016c74      01c0a0e3       mov ip, 1
  │││││││   0x00016c78      2cc08de5       str ip, [sp, 0x2c]
  │││││││   0x00016c7c      41cfa0e3       mov ip, 0x104
  │││││││   0x00016c80      04c08de5       str ip, [sp, 4]
  │││││││   0x00016c84      a7beffeb       bl rsym.QVD_IOControl
  │││││││   0x00016c88      008050e2       subs r8, r0, 0
  ────────< 0x00016c8c      0d00000a       beq 0x16cc8
  │││││││   0x00016c90      bcc29fe5       ldr ip, [0x00016f54]        ; [0x16f54:4]=0xffffd06f
  │││││││   0x00016c94      0a00a0e1       mov r0, sl
  │││││││   0x00016c98      0620a0e3       mov r2, 6
  │││││││   0x00016c9c      0330a0e3       mov r3, 3
  │││││││   0x00016ca0      1f1d00e3       movw r1, 0xd1f              ; '\x1f\r'
  │││││││   0x00016ca4      0c708de5       str r7, [sp, 0xc]
  ────────> 0x00016ca8      0cc085e0       add ip, r5, ip
  │││││││   0x00016cac      04708de5       str r7, [sp, 4]
  │││││││   0x00016cb0      00c08de5       str ip, [sp]
  ────────> 0x00016cb4      08708de5       str r7, [sp, 8]
  │││││││   0x00016cb8      acbeffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00016cbc      0400a0e1       mov r0, r4
  │││││││   0x00016cc0      0810a0e1       mov r1, r8
  ────────< 0x00016cc4      450000ea       b 0x16de0
  ────────> 0x00016cc8      54408de2       add r4, sp, 0x54
  │││││││   0x00016ccc      0400a0e1       mov r0, r4
  │││││││   0x00016cd0      cbbdffeb       bl sym.imp.std_strlen
  │││││││   0x00016cd4      0c2099e5       ldr r2, [sb, 0xc]
  │││││││   0x00016cd8      020050e1       cmp r0, r2
  ────────< 0x00016cdc      0300002a       bhs 0x16cf0
  │││││││   0x00016ce0      100089e2       add r0, sb, 0x10
  │││││││   0x00016ce4      0410a0e1       mov r1, r4
  │││││││   0x00016ce8      60bfffeb       bl sym.imp.std_strlcpy
  ────────< 0x00016cec      860000ea       b 0x16f0c
  ────────> 0x00016cf0      60c29fe5       ldr ip, [0x00016f58]        ; [0x16f58:4]=0xffffd091
  │││││││   0x00016cf4      0a00a0e1       mov r0, sl
  │││││││   0x00016cf8      2a1d00e3       movw r1, 0xd2a              ; '*\r'
  │││││││   0x00016cfc      0620a0e3       mov r2, 6
  │││││││   0x00016d00      0330a0e3       mov r3, 3
  │││││││   0x00016d04      0cc085e0       add ip, r5, ip
  │││││││   0x00016d08      0c808de5       str r8, [sp, 0xc]
  │││││││   0x00016d0c      00c08de5       str ip, [sp]
  │││││││   0x00016d10      04808de5       str r8, [sp, 4]
  │││││││   0x00016d14      08808de5       str r8, [sp, 8]
  ────────> 0x00016d18      94beffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00016d1c      2202a0e3       mov r0, 0x20000002
  ────────< 0x00016d20      720000ea       b 0x16ef0
  ────────> 0x00016d24      30a08de2       add sl, sp, 0x30
  │││││││   0x00016d28      047091e5       ldr r7, [r1, 4]
  │││││││   0x00016d2c      10808de2       add r8, sp, 0x10
  │││││││   0x00016d30      0010a0e3       mov r1, 0
  │││││││   0x00016d34      0820a0e3       mov r2, 8
  │││││││   0x00016d38      0a00a0e1       mov r0, sl
  │││││││   0x00016d3c      27bfffeb       bl sym.imp.std_memset
  │││││││   0x00016d40      0010a0e3       mov r1, 0
  │││││││   0x00016d44      0c20a0e3       mov r2, 0xc
  │││││││   0x00016d48      0800a0e1       mov r0, r8
  │││││││   0x00016d4c      23bfffeb       bl sym.imp.std_memset
  │││││││   0x00016d50      d03602e3       movw r3, 0x26d0
  │││││││   0x00016d54      08c0a0e3       mov ip, 8
  │││││││   0x00016d58      0110a0e3       mov r1, 1
  │││││││   0x00016d5c      10c08de5       str ip, [sp, 0x10]
  │││││││   0x00016d60      0820a0e1       mov r2, r8
  │││││││   0x00016d64      14108de5       str r1, [sp, 0x14]
  │││││││   0x00016d68      08e097e5       ldr lr, [r7, 8]
  │││││││   0x00016d6c      030094e7       ldr r0, [r4, r3]
  │││││││   0x00016d70      0c30a0e3       mov r3, 0xc
  │││││││   0x00016d74      00e05ee2       subs lr, lr, 0
  │││││││   0x00016d78      01e0a013       movne lr, 1
  │││││││   0x00016d7c      00a08de5       str sl, [sp]
  │││││││   0x00016d80      18e08de5       str lr, [sp, 0x18]
  │││││││   0x00016d84      00a0a0e3       mov sl, 0
  │││││││   0x00016d88      04c08de5       str ip, [sp, 4]
  │││││││   0x00016d8c      08a08de5       str sl, [sp, 8]
  │││││││   0x00016d90      64beffeb       bl rsym.QVD_IOControl
  │││││││   0x00016d94      008050e2       subs r8, r0, 0
  │││││││   0x00016d98      34309d05       ldreq r3, [sp, 0x34]
  │││││││   0x00016d9c      0c308705       streq r3, [r7, 0xc]
  ────────< 0x00016da0      0c00000a       beq 0x16dd8
  │││││││   0x00016da4      b0319fe5       ldr r3, [0x00016f5c]        ; [0x16f5c:4]=0xffffd0aa
  │││││││   0x00016da8      4a1d00e3       movw r1, 0xd4a              ; 'J\r'
  │││││││   0x00016dac      74019fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016db0      0620a0e3       mov r2, 6
  │││││││   0x00016db4      033085e0       add r3, r5, r3
  │││││││   0x00016db8      00308de5       str r3, [sp]
  │││││││   0x00016dbc      08c097e5       ldr ip, [r7, 8]
  │││││││   0x00016dc0      000085e0       add r0, r5, r0
  │││││││   0x00016dc4      0330a0e3       mov r3, 3
  │││││││   0x00016dc8      0ca08de5       str sl, [sp, 0xc]
  │││││││   0x00016dcc      08808de5       str r8, [sp, 8]
  │││││││   0x00016dd0      04c08de5       str ip, [sp, 4]
  │││││││   0x00016dd4      65beffeb       bl sym.imp.MM_OutputDebugMessage
  ────────> 0x00016dd8      0810a0e1       mov r1, r8
  │││││││   0x00016ddc      0400a0e1       mov r0, r4
  ──└─────> 0x00016de0      a0bcffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  ││ ││││   0x00016de4      f20250e3       cmp r0, 0x2000000f          ; '\x0f'
  ││┌─────< 0x00016de8      4000001a       bne 0x16ef0
  ──────└─> 0x00016dec      6c319fe5       ldr r3, [0x00016f60]        ; [0x16f60:4]=0x6f100001
  ││││││    0x00016df0      03005be1       cmp fp, r3
  ││││││┌─< 0x00016df4      0400000a       beq 0x16e0c
  │││││││   0x00016df8      053683e2       add r3, r3, 0x500000
  │││││││   0x00016dfc      03005be1       cmp fp, r3
  │││││││   0x00016e00      f202a013       movne r0, 0x2000000f
  ────────< 0x00016e04      3900001a       bne 0x16ef0
  ────────< 0x00016e08      0f0000ea       b 0x16e4c
  ││││││└─> 0x00016e0c      043096e5       ldr r3, [r6, 4]
  ││││││    0x00016e10      000053e3       cmp r3, 0
  ││││││┌─< 0x00016e14      3400000a       beq 0x16eec
  │││││││   0x00016e18      002093e5       ldr r2, [r3]
  │││││││   0x00016e1c      140052e3       cmp r2, 0x14
  ────────< 0x00016e20      3100001a       bne 0x16eec
  │││││││   0x00016e24      0c1093e5       ldr r1, [r3, 0xc]
  │││││││   0x00016e28      34219fe5       ldr r2, [0x00016f64]        ; [0x16f64:4]=0x6000010
  │││││││   0x00016e2c      020051e1       cmp r1, r2
  │││││││   0x00016e30      30019f15       ldrne r0, [0x00016f68]      ; [0x16f68:4]=0x20000010
  ────────< 0x00016e34      2d00001a       bne 0x16ef0
  │││││││   0x00016e38      f82702e3       movw r2, 0x27f8
  │││││││   0x00016e3c      1202a0e3       mov r0, 0x20000001
  │││││││   0x00016e40      022094e7       ldr r2, [r4, r2]
  │││└────> 0x00016e44      102083e5       str r2, [r3, 0x10]
  │││┌────< 0x00016e48      280000ea       b 0x16ef0
  ────────> 0x00016e4c      562f8de2       add r2, sp, 0x158
  │││││││   0x00016e50      0e30a0e3       mov r3, 0xe
  │││││││   0x00016e54      27cc84e2       add ip, r4, 0x2700
  │││││││   0x00016e58      0210a0e3       mov r1, 2
  │││││││   0x00016e5c      303122e5       str r3, [r2, -0x130]!
  │││││││   0x00016e60      d03602e3       movw r3, 0x26d0
  │││││││   0x00016e64      20c08ce2       add ip, ip, 0x20
  │││││││   0x00016e68      0070a0e3       mov r7, 0
  │││││││   0x00016e6c      030094e7       ldr r0, [r4, r3]
  │││││││   0x00016e70      0830a0e3       mov r3, 8
  │││││││   0x00016e74      00c08de5       str ip, [sp]
  │││││││   0x00016e78      01c0a0e3       mov ip, 1
  │││││││   0x00016e7c      2cc08de5       str ip, [sp, 0x2c]
  │││││││   0x00016e80      03c08ce2       add ip, ip, 3
  │││││││   0x00016e84      04c08de5       str ip, [sp, 4]
  │││││││   0x00016e88      048096e5       ldr r8, [r6, 4]
  │││││││   0x00016e8c      08708de5       str r7, [sp, 8]
  │││││││   0x00016e90      24beffeb       bl rsym.QVD_IOControl
  │││││││   0x00016e94      006050e2       subs r6, r0, 0
  │││││││   0x00016e98      20370203       movweq r3, 0x2720
  │││││││   0x00016e9c      03309407       ldreq r3, [r4, r3]
  ────────< 0x00016ea0      0e00000a       beq 0x16ee0
  │││││││   0x00016ea4      7c009fe5       ldr r0, [0x00016f28]        ; [0x16f28:4]=0xffffaa0c
  │││││││   0x00016ea8      0620a0e3       mov r2, 6
  │││││││   0x00016eac      b8c09fe5       ldr ip, [0x00016f6c]        ; [0x16f6c:4]=0xffffd0e5
  │││││││   0x00016eb0      0330a0e3       mov r3, 3
  │││││││   0x00016eb4      000085e0       add r0, r5, r0
  │││││││   0x00016eb8      8a1d00e3       movw r1, 0xd8a
  │││││││   0x00016ebc      0cc085e0       add ip, r5, ip
  │││││││   0x00016ec0      0c708de5       str r7, [sp, 0xc]
  │││││││   0x00016ec4      00c08de5       str ip, [sp]
  │││││││   0x00016ec8      c0008de9       stmib sp, {r6, r7}
  │││││││   0x00016ecc      27beffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00016ed0      0400a0e1       mov r0, r4
  │││││││   0x00016ed4      0610a0e1       mov r1, r6
  │││││││   0x00016ed8      62bcffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  ────────< 0x00016edc      030000ea       b 0x16ef0
  ────────> 0x00016ee0      1202a0e3       mov r0, 0x20000001
  │││││││   0x00016ee4      0c3088e5       str r3, [r8, 0xc]
  ────────< 0x00016ee8      000000ea       b 0x16ef0
  ──────└─> 0x00016eec      4202a0e3       mov r0, 0x20000004
  ──└└└└┌─> 0x00016ef0      2c309fe5       ldr r3, [0x00016f24]        ; [0x16f24:4]=0x3b4
  ││    ╎   0x00016ef4      54219de5       ldr r2, [sp, 0x154]
  ││    ╎   0x00016ef8      033095e7       ldr r3, [r5, r3]
  ││    ╎   0x00016efc      003093e5       ldr r3, [r3]
  ││    ╎   0x00016f00      030052e1       cmp r2, r3
  ││   ┌──< 0x00016f04      0300000a       beq 0x16f18
  ││  ┌───< 0x00016f08      010000ea       b 0x16f14
  └└──────> 0x00016f0c      1202a0e3       mov r0, 0x20000001
      ││└─< 0x00016f10      f6ffffea       b 0x16ef0
      └───> 0x00016f14      49bdffeb       bl sym.imp.__stack_chk_fail
       └──> 0x00016f18      57df8de2       add sp, sp, 0x15c
            0x00016f1c      f08fbde8       pop {r4, r5, r6, r7, r8, sb, sl, fp, pc}
            0x00016f20      a0eb0000       andeq lr, r0, r0, lsr 23
            0x00016f24      b4030000       strheq r0, [r0], -r4
            0x00016f28      0caaffff       invalid
            0x00016f2c      deceffff       invalid
            0x00016f30      0acfffff       invalid
            0x00016f34      01000002       andeq r0, r0, 1
            0x00016f38      3bcfffff       invalid
            0x00016f3c      71cfffff       invalid
            0x00016f40      9ecfffff       invalid
            0x00016f44      e2cfffff       invalid
            0x00016f48      01d0ffff       invalid
            0x00016f4c      23d0ffff       invalid
            0x00016f50      45d0ffff       invalid
            0x00016f54      6fd0ffff       invalid
            0x00016f58      91d0ffff       invalid
            0x00016f5c      aad0ffff       invalid
            0x00016f60      0100106f       svcvs 0x100001
            0x00016f64      10000006       invalid
            0x00016f68      10000020       andhs r0, r0, r0, lsl r0
            0x00016f6c      e5d0ffff       invalid
