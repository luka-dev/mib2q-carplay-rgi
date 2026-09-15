        ╎   ;-- reloc.qomx_CmpntCommon_SetConfig:
        ╎   ;-- qomx_CmpntCommon_SetConfig:
        ╎   0x00008c54      f0402de9       push {r4, r5, r6, r7, lr}     ; RELOC 32 qomx_CmpntCommon_SetConfig @ 0x00008c54
        ╎   0x00008c58      4031d0e5       ldrb r3, [r0, 0x140]
        ╎   0x00008c5c      0260a0e1       mov r6, r2
        ╎   0x00008c60      bc529fe5       ldr r5, [0x00008f24]        ; [0x8f24:4]=0x3be6c "Event = 0x%x"
        ╎   0x00008c64      122003e2       and r2, r3, 0x12
        ╎   0x00008c68      1cd04de2       sub sp, sp, 0x1c
        ╎   0x00008c6c      120052e3       cmp r2, 0x12
        ╎   0x00008c70      05508fe0       add r5, pc, r5
        ╎   0x00008c74      0040a0e1       mov r4, r0
        ╎   0x00008c78      0170a0e1       mov r7, r1
       ┌──< 0x00008c7c      0f00001a       bne 0x8cc0
       │╎   0x00008c80      00e190e5       ldr lr, [r0, 0x100]
       │╎   0x00008c84      05cd80e2       add ip, r0, 0x140
       │╎   0x00008c88      98029fe5       ldr r0, [0x00008f28]        ; [0x8f28:4]=0xffff3b49
       │╎   0x00008c8c      01c08ce2       add ip, ip, 1
       │╎   0x00008c90      e11fa0e3       mov r1, 0x384
       │╎   0x00008c94      00508de9       stmib sp, {ip, lr}
       │╎   0x00008c98      00c0a0e3       mov ip, 0
       │╎   0x00008c9c      88e29fe5       ldr lr, [0x00008f2c]        ; [0x8f2c:4]=0xffff42c2
       │╎   0x00008ca0      000085e0       add r0, r5, r0
       │╎   0x00008ca4      042082e2       add r2, r2, 4
       │╎   0x00008ca8      0230a0e3       mov r3, 2
       │╎   0x00008cac      0ee085e0       add lr, r5, lr
       │╎   0x00008cb0      10c08de5       str ip, [sp, 0x10]
       │╎   0x00008cb4      00e08de5       str lr, [sp]
       │╎   0x00008cb8      0c708de5       str r7, [sp, 0xc]
      ┌───< 0x00008cbc      0f0000ea       b 0x8d00
      │└──> 0x00008cc0      020013e3       tst r3, 2
      │┌──< 0x00008cc4      0e00000a       beq 0x8d04
      ││╎   0x00008cc8      00c190e5       ldr ip, [r0, 0x100]
      ││╎   0x00008ccc      e11fa0e3       mov r1, 0x384
      ││╎   0x00008cd0      50029fe5       ldr r0, [0x00008f28]        ; [0x8f28:4]=0xffff3b49
      ││╎   0x00008cd4      1620a0e3       mov r2, 0x16
      ││╎   0x00008cd8      0230a0e3       mov r3, 2
      ││╎   0x00008cdc      08708de5       str r7, [sp, 8]
      ││╎   0x00008ce0      04c08de5       str ip, [sp, 4]
      ││╎   0x00008ce4      000085e0       add r0, r5, r0
      ││╎   0x00008ce8      40c29fe5       ldr ip, [0x00008f30]        ; [0x8f30:4]=0xffff42c6
      ││╎   0x00008cec      0cc085e0       add ip, r5, ip
      ││╎   0x00008cf0      00c08de5       str ip, [sp]
      ││╎   0x00008cf4      00c0a0e3       mov ip, 0
      ││╎   0x00008cf8      10c08de5       str ip, [sp, 0x10]
      ││╎   0x00008cfc      0cc08de5       str ip, [sp, 0xc]
      └───> 0x00008d00      b9ebffeb       bl sym.imp.MM_OutputDebugMessage
       └──> 0x00008d04      28329fe5       ldr r3, [0x00008f34]        ; [0x8f34:4]=0x7ffff800
        ╎   0x00008d08      030057e1       cmp r7, r3
       ┌──< 0x00008d0c      7c00001a       bne 0x8f04
       │╎   0x00008d10      4031d4e5       ldrb r3, [r4, 0x140]
       │╎   0x00008d14      122003e2       and r2, r3, 0x12
       │╎   0x00008d18      120052e3       cmp r2, 0x12
      ┌───< 0x00008d1c      0e00001a       bne 0x8d5c
      ││╎   0x00008d20      00e194e5       ldr lr, [r4, 0x100]
      ││╎   0x00008d24      05cd84e2       add ip, r4, 0x140
      ││╎   0x00008d28      f8019fe5       ldr r0, [0x00008f28]        ; [0x8f28:4]=0xffff3b49
      ││╎   0x00008d2c      01c08ce2       add ip, ip, 1
      ││╎   0x00008d30      391ea0e3       mov r1, 0x390
      ││╎   0x00008d34      00508de9       stmib sp, {ip, lr}
      ││╎   0x00008d38      00c0a0e3       mov ip, 0
      ││╎   0x00008d3c      f4e19fe5       ldr lr, [0x00008f38]        ; [0x8f38:4]=0xffff42f8
      ││╎   0x00008d40      000085e0       add r0, r5, r0
      ││╎   0x00008d44      042082e2       add r2, r2, 4
      ││╎   0x00008d48      0230a0e3       mov r3, 2
      ││╎   0x00008d4c      0ee085e0       add lr, r5, lr
      ││╎   0x00008d50      10c08de5       str ip, [sp, 0x10]
      ││╎   0x00008d54      00e08de5       str lr, [sp]
     ┌────< 0x00008d58      0e0000ea       b 0x8d98
     │└───> 0x00008d5c      020013e3       tst r3, 2
     │┌───< 0x00008d60      0e00000a       beq 0x8da0
     │││╎   0x00008d64      00e194e5       ldr lr, [r4, 0x100]
     │││╎   0x00008d68      391ea0e3       mov r1, 0x390
     │││╎   0x00008d6c      b4019fe5       ldr r0, [0x00008f28]        ; [0x8f28:4]=0xffff3b49
     │││╎   0x00008d70      1620a0e3       mov r2, 0x16
     │││╎   0x00008d74      0230a0e3       mov r3, 2
     │││╎   0x00008d78      00c0a0e3       mov ip, 0
     │││╎   0x00008d7c      04e08de5       str lr, [sp, 4]
     │││╎   0x00008d80      000085e0       add r0, r5, r0
     │││╎   0x00008d84      b0e19fe5       ldr lr, [0x00008f3c]        ; [0x8f3c:4]=0xffff42fc
     │││╎   0x00008d88      10c08de5       str ip, [sp, 0x10]
     │││╎   0x00008d8c      0ee085e0       add lr, r5, lr
     │││╎   0x00008d90      08c08de5       str ip, [sp, 8]
     │││╎   0x00008d94      00e08de5       str lr, [sp]
     └────> 0x00008d98      0cc08de5       str ip, [sp, 0xc]
      ││╎   0x00008d9c      92ebffeb       bl sym.imp.MM_OutputDebugMessage
      └───> 0x00008da0      00c096e5       ldr ip, [r6]
       │╎   0x00008da4      90005ce3       cmp ip, 0x90
      ┌───< 0x00008da8      2700000a       beq 0x8e4c
      ││╎   0x00008dac      4031d4e5       ldrb r3, [r4, 0x140]
      ││╎   0x00008db0      112003e2       and r2, r3, 0x11
      ││╎   0x00008db4      110052e3       cmp r2, 0x11
     ┌────< 0x00008db8      0f00001a       bne 0x8dfc
     │││╎   0x00008dbc      00e194e5       ldr lr, [r4, 0x100]
     │││╎   0x00008dc0      054d84e2       add r4, r4, 0x140
     │││╎   0x00008dc4      5c019fe5       ldr r0, [0x00008f28]        ; [0x8f28:4]=0xffff3b49
     │││╎   0x00008dc8      014084e2       add r4, r4, 1
     │││╎   0x00008dcc      0cc08de5       str ip, [sp, 0xc]
     │││╎   0x00008dd0      961300e3       movw r1, 0x396
     │││╎   0x00008dd4      08e08de5       str lr, [sp, 8]
     │││╎   0x00008dd8      000085e0       add r0, r5, r0
     │││╎   0x00008ddc      5ce19fe5       ldr lr, [0x00008f40]        ; [0x8f40:4]=0xffff4324
     │││╎   0x00008de0      052082e2       add r2, r2, 5
     │││╎   0x00008de4      0330a0e3       mov r3, 3
     │││╎   0x00008de8      04408de5       str r4, [sp, 4]
     │││╎   0x00008dec      0e5085e0       add r5, r5, lr
     │││╎   0x00008df0      90c0a0e3       mov ip, 0x90
     │││╎   0x00008df4      00508de5       str r5, [sp]
    ┌─────< 0x00008df8      0f0000ea       b 0x8e3c
    │└────> 0x00008dfc      010013e3       tst r3, 1
    │┌────< 0x00008e00      0f00000a       beq 0x8e44
    ││││╎   0x00008e04      00e194e5       ldr lr, [r4, 0x100]
    ││││╎   0x00008e08      961300e3       movw r1, 0x396
    ││││╎   0x00008e0c      14019fe5       ldr r0, [0x00008f28]        ; [0x8f28:4]=0xffff3b49
    ││││╎   0x00008e10      1620a0e3       mov r2, 0x16
    ││││╎   0x00008e14      08c08de5       str ip, [sp, 8]
    ││││╎   0x00008e18      0330a0e3       mov r3, 3
    ││││╎   0x00008e1c      20c19fe5       ldr ip, [0x00008f44]        ; [0x8f44:4]=0xffff4328
    ││││╎   0x00008e20      000085e0       add r0, r5, r0
    ││││╎   0x00008e24      04e08de5       str lr, [sp, 4]
    ││││╎   0x00008e28      0c5085e0       add r5, r5, ip
    ││││╎   0x00008e2c      90c0a0e3       mov ip, 0x90
    ││││╎   0x00008e30      0cc08de5       str ip, [sp, 0xc]
    ││││╎   0x00008e34      00c0a0e3       mov ip, 0
    ││││╎   0x00008e38      00508de5       str r5, [sp]
    └─────> 0x00008e3c      10c08de5       str ip, [sp, 0x10]
     │││╎   0x00008e40      69ebffeb       bl sym.imp.MM_OutputDebugMessage
     └────> 0x00008e44      fc009fe5       ldr r0, [0x00008f48]        ; [0x8f48:4]=0x80001005
     ┌────< 0x00008e48      330000ea       b 0x8f1c
     │└───> 0x00008e4c      083096e5       ldr r3, [r6, 8]
     │ │╎   0x00008e50      f4209fe5       ldr r2, [0x00008f4c]        ; [0x8f4c:4]=0x7000005
     │ │╎   0x00008e54      020053e1       cmp r3, r2
     │ │╎   0x00008e58      0f30a003       moveq r3, 0xf
     │┌───< 0x00008e5c      0b00000a       beq 0x8e90
     │││╎   0x00008e60      e8209fe5       ldr r2, [0x00008f50]        ; [0x8f50:4]=0x7000004
     │││╎   0x00008e64      020053e1       cmp r3, r2
     │││╎   0x00008e68      0730a003       moveq r3, 7
    ┌─────< 0x00008e6c      0700000a       beq 0x8e90
    ││││╎   0x00008e70      dc209fe5       ldr r2, [0x00008f54]        ; [0x8f54:4]=0x7000003
    ││││╎   0x00008e74      020053e1       cmp r3, r2
    ││││╎   0x00008e78      0330a003       moveq r3, 3
   ┌──────< 0x00008e7c      0300000a       beq 0x8e90
   │││││╎   0x00008e80      d0209fe5       ldr r2, [0x00008f58]        ; [0x8f58:4]=0x7000002
   │││││╎   0x00008e84      020053e1       cmp r3, r2
   │││││╎   0x00008e88      0130a003       moveq r3, 1
   │││││╎   0x00008e8c      0030a013       movne r3, 0
   └└─└───> 0x00008e90      4031c4e5       strb r3, [r4, 0x140]
     │ │╎   0x00008e94      0c3096e5       ldr r3, [r6, 0xc]
     │ │╎   0x00008e98      010053e3       cmp r3, 1
     │ │╎   0x00008e9c      4031d405       ldrbeq r3, [r4, 0x140]
     │ │╎   0x00008ea0      10308303       orreq r3, r3, 0x10
     │ │╎   0x00008ea4      4031c405       strbeq r3, [r4, 0x140]
     │ │╎   0x00008ea8      0c3096e5       ldr r3, [r6, 0xc]
     │ │╎   0x00008eac      010053e3       cmp r3, 1
     │┌───< 0x00008eb0      0c00001a       bne 0x8ee8
     │││╎   0x00008eb4      1030d6e5       ldrb r3, [r6, 0x10]
     │││╎   0x00008eb8      000053e3       cmp r3, 0
    ┌─────< 0x00008ebc      0900000a       beq 0x8ee8
    ││││╎   0x00008ec0      054d84e2       add r4, r4, 0x140
    ││││╎   0x00008ec4      0010a0e3       mov r1, 0
    ││││╎   0x00008ec8      014084e2       add r4, r4, 1
    ││││╎   0x00008ecc      0420a0e3       mov r2, 4
    ││││╎   0x00008ed0      0400a0e1       mov r0, r4
    ││││╎   0x00008ed4      9bebffeb       bl sym.imp.std_memset
    ││││╎   0x00008ed8      0400a0e1       mov r0, r4
    ││││╎   0x00008edc      101086e2       add r1, r6, 0x10
    ││││╎   0x00008ee0      7f20a0e3       mov r2, 0x7f                ; '\x7f'
   ┌──────< 0x00008ee4      030000ea       b 0x8ef8
   │└─└───> 0x00008ee8      050d84e2       add r0, r4, 0x140
   │ │ │╎   0x00008eec      0410a0e1       mov r1, r4
   │ │ │╎   0x00008ef0      010080e2       add r0, r0, 1
   │ │ │╎   0x00008ef4      8020a0e3       mov r2, 0x80
   └──────> 0x00008ef8      02ebffeb       bl loc.imp.std_memmove
     │ │╎   0x00008efc      0000a0e3       mov r0, 0
     │┌───< 0x00008f00      050000ea       b 0x8f1c
     ││└──> 0x00008f04      d00194e5       ldr r0, [r4, 0x1d0]
     ││ ╎   0x00008f08      0710a0e1       mov r1, r7
     ││ ╎   0x00008f0c      0620a0e1       mov r2, r6
     ││ ╎   0x00008f10      1cd08de2       add sp, sp, 0x1c
     ││ ╎   0x00008f14      f040bde8       pop {r4, r5, r6, r7, lr}
     ││ └─< 0x00008f18      ebeaffea       b rsym.qomx_PortSetConfig
     └└───> 0x00008f1c      1cd08de2       add sp, sp, 0x1c
            0x00008f20      f080bde8       pop {r4, r5, r6, r7, pc}
            0x00008f24      6cbe0300       andeq fp, r3, ip, ror 28
            0x00008f28      493bffff       invalid
            0x00008f2c      c242ffff       invalid
            0x00008f30      c642ffff       invalid
            0x00008f34      00f8ff7f       svcvc 0xfff800
            0x00008f38      f842ffff       invalid
            0x00008f3c      fc42ffff       invalid
            0x00008f40      2443ffff       invalid
            0x00008f44      2843ffff       invalid
            0x00008f48      05100080       andhi r1, r0, r5
            0x00008f4c      05000007       streq r0, [r0, -r5]
            0x00008f50      04000007       streq r0, [r0, -r4]
            0x00008f54      03000007       streq r0, [r0, -r3]
            0x00008f58      02000007       streq r0, [r0, -r2]
