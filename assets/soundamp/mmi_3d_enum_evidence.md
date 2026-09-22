# MU1316 native 3D enum evidence

Source: `Firmwares/HU/MHI2Q_US_AUG22_P5087_MU1316/MHI2Q_US_AUG22_P5087_MU1316/RE/MIBRoot/MIBRoot`.
ARM LE32 ELF; LOAD0 VA=file offset+0x100000. Read-only r2 listings.
The debug callback is not relied upon as the production path. The named requestSet3DSound handler maps a mode through the table and subtracts one before dispatch.

```text
WARN: Relocs has not been applied. Please use `-e bin.relocs.apply=true` or `-e bin.cache=true` next time
            0x001eb0fc      f0412de9       push {r4, r5, r6, r7, r8, lr}
            0x001eb100      20d04de2       sub sp, sp, 0x20
            0x001eb104      0070a0e1       mov r7, r0
            0x001eb108      0160a0e1       mov r6, r1
            0x001eb10c      1c008de2       add r0, sp, 0x1c
            0x001eb110      00119fe5       ldr r1, [0x001eb218]        ; [0x1eb218:4]=0x10bc1ec
            0x001eb114      0340a0e1       mov r4, r3
            0x001eb118      0250a0e1       mov r5, r2
            0x001eb11c      4e00fdeb       bl method.CHBTraceScope.CHBTraceScope_CHBTraceScopeDef_const_ ; CHBTraceScope::CHBTraceScope(CHBTraceScopeDef const&)
            0x001eb120      1c309de5       ldr r3, [sp, 0x1c]
            0x001eb124      0e30d3e5       ldrb r3, [r3, 0xe]
            0x001eb128      400013e3       tst r3, 0x40                ; '@'
        ┌─< 0x001eb12c      0f00000a       beq 0x1eb170
        │   0x001eb130      003096e5       ldr r3, [r6]
        │   0x001eb134      352100e3       movw r2, 0x135
        │   0x001eb138      00e095e5       ldr lr, [r5]
        │   0x001eb13c      1c008de2       add r0, sp, 0x1c
        │   0x001eb140      020053e1       cmp r3, r2
        │   0x001eb144      008094e5       ldr r8, [r4]
        │   0x001eb148      cc109fe5       ldr r1, [0x001eb21c]        ; [0x1eb21c:4]=0xde3c48 "%s %s requestSet3DSound(%s , terminal_ETerminal=%d, value=%d) "
        │   0x001eb14c      cc209f95       ldrls r2, [0x001eb220]      ; [0x1eb220:4]=0xde07a4
        │   0x001eb150      ccc09f85       ldrhi ip, [0x001eb224]      ; [0x1eb224:4]=0xdd6782 "INVALID!!!"
        │   0x001eb154      04e08de5       str lr, [sp, 4]
        │   0x001eb158      03c19297       ldrls ip, [r2, r3, lsl 2]
        │   0x001eb15c      c4209fe5       ldr r2, [0x001eb228]        ; [0x1eb228:4]=0xde1c11 "[Audio]"
        │   0x001eb160      c4309fe5       ldr r3, [0x001eb22c]        ; [0x1eb22c:4]=0xde1c19 "[AMPresCtrl]"
        │   0x001eb164      00c08de5       str ip, [sp]
        │   0x001eb168      08808de5       str r8, [sp, 8]
        │   0x001eb16c      1baefceb       bl sym.imp.CHBTraceScope::sendMessageUnconditionally_char_const__...__const ; CHBTraceScope::sendMessageUnconditionally(char const*, ...) const
        └─> 0x001eb170      0600a0e1       mov r0, r6
            0x001eb174      0510a0e1       mov r1, r5
            0x001eb178      18208de2       add r2, sp, 0x18
            0x001eb17c      14308de2       add r3, sp, 0x14
            0x001eb180      01c0e0e3       mvn ip, 1
            0x001eb184      14c08de5       str ip, [sp, 0x14]
            0x001eb188      18c08de5       str ip, [sp, 0x18]
            0x001eb18c      21deffeb       bl 0x1e2a18
            0x001eb190      000050e3       cmp r0, 0
        ┌─< 0x001eb194      0200000a       beq 0x1eb1a4
        │   0x001eb198      18609de5       ldr r6, [sp, 0x18]
        │   0x001eb19c      020076e3       cmn r6, 2
       ┌──< 0x001eb1a0      0900001a       bne 0x1eb1cc
       │└─> 0x001eb1a4      1c309de5       ldr r3, [sp, 0x1c]
       │    0x001eb1a8      0e30d3e5       ldrb r3, [r3, 0xe]
       │    0x001eb1ac      020013e3       tst r3, 2
       │┌─< 0x001eb1b0      1400000a       beq 0x1eb208
       ││   0x001eb1b4      1c008de2       add r0, sp, 0x1c
       ││   0x001eb1b8      70109fe5       ldr r1, [0x001eb230]        ; [0x1eb230:4]=0xde213b "%s %s Error mapping HMI connection label to AM source/sink"
       ││   0x001eb1bc      64209fe5       ldr r2, [0x001eb228]        ; [0x1eb228:4]=0xde1c11 "[Audio]"
       ││   0x001eb1c0      64309fe5       ldr r3, [0x001eb22c]        ; [0x1eb22c:4]=0xde1c19 "[AMPresCtrl]"
       ││   0x001eb1c4      52b2fceb       bl sym.imp.CHBTraceScope::sendErrorUnconditionally_char_const__...__const ; CHBTraceScope::sendErrorUnconditionally(char const*, ...) const
      ┌───< 0x001eb1c8      0e0000ea       b 0x1eb208
      │└──> 0x001eb1cc      20508de2       add r5, sp, 0x20
      │ │   0x001eb1d0      2230a0e3       mov r3, 0x22                ; '\"'
      │ │   0x001eb1d4      0400a0e1       mov r0, r4
      │ │   0x001eb1d8      14409de5       ldr r4, [sp, 0x14]
      │ │   0x001eb1dc      103025e5       str r3, [r5, -0x10]!
      │ │   0x001eb1e0      badeffeb       bl 0x1e2cd0
      │ │   0x001eb1e4      0610a0e1       mov r1, r6
      │ │   0x001eb1e8      0420a0e1       mov r2, r4
      │ │   0x001eb1ec      0530a0e1       mov r3, r5
      │ │   0x001eb1f0      01c040e2       sub ip, r0, 1
      │ │   0x001eb1f4      590e87e2       add r0, r7, 0x590
      │ │   0x001eb1f8      080080e2       add r0, r0, 8
      │ │   0x001eb1fc      7cc0bfe6       sxth ip, ip
      │ │   0x001eb200      00c08de5       str ip, [sp]
      │ │   0x001eb204      46a5ffeb       bl 0x1d4724
      └─└─> 0x001eb208      1c008de2       add r0, sp, 0x1c
            0x001eb20c      13bafceb       bl method.CHBTraceScope._CHBTraceScope__ ; CHBTraceScope::~CHBTraceScope()
            0x001eb210      20d08de2       add sp, sp, 0x20
            0x001e2cd0      0030a0e3       mov r3, 0
            0x001e2cd4      34209fe5       ldr r2, [0x001e2d10]        ; [0x1e2d10:4]=0xddee50
            0x001e2cd8      10402de9       push {r4, lr}
            0x001e2cdc      001090e5       ldr r1, [r0]
            0x001e2ce0      0300a0e1       mov r0, r3
        ┌─> 0x001e2ce4      034092e7       ldr r4, [r2, r3]
        ╎   0x001e2ce8      01c080e2       add ip, r0, 1
        ╎   0x001e2cec      043083e2       add r3, r3, 4
        ╎   0x001e2cf0      040051e1       cmp r1, r4
       ┌──< 0x001e2cf4      0300000a       beq 0x1e2d08
       │╎   0x001e2cf8      7c00ffe6       uxth r0, ip
       │╎   0x001e2cfc      090050e3       cmp r0, 9
       │└─< 0x001e2d00      f7ffff1a       bne 0x1e2ce4
       │    0x001e2d04      0000a0e3       mov r0, 0
       └──> 0x001e2d08      7000bfe6       sxth r0, r0
            0x001e2d0c      1080bde8       pop {r4, pc}
            0x001e2d10      50eedd00       sbcseq lr, sp, r0, asr lr
0x00ddee50  0x00000000 0x00000001 0x00000002 0x00000004  ................
0x00ddee60  0x00000008 0x00000010 0x00000020 0x00000040  ........ ...@...
0x00ddee70  0x00000080                                   ....
        ╎   0x00892e68      f0412de9       push {r4, r5, r6, r7, r8, lr}
        ╎   0x00892e6c      0150a0e1       mov r5, r1
        ╎   0x00892e70      0240a0e1       mov r4, r2
        ╎   0x00892e74      0110a0e3       mov r1, 1
        ╎   0x00892e78      58209fe5       ldr r2, [0x00892ed8]        ; [0x892ed8:4]=0xdb96d4 "DMMAudioManagementMIB"
        ╎   0x00892e7c      0380a0e1       mov r8, r3
        ╎   0x00892e80      0060a0e1       mov r6, r0
        ╎   0x00892e84      f871dde1       ldrsh r7, [sp, 0x18]
        ╎   0x00892e88      5311e2eb       bl sym.imp.CHBProxyBase::isRequestPossible_int__char_const__const ; CHBProxyBase::isRequestPossible(int, char const*) const
        ╎   0x00892e8c      000050e3       cmp r0, 0
        ╎   0x00892e90      f081bd08       popeq {r4, r5, r6, r7, r8, pc}
        ╎   0x00892e94      0110a0e3       mov r1, 1
        ╎   0x00892e98      0600a0e1       mov r0, r6
        ╎   0x00892e9c      c4fcffeb       bl 0x8921b4
        ╎   0x00892ea0      0120a0e3       mov r2, 1
        ╎   0x00892ea4      0030a0e1       mov r3, r0
        ╎   0x00892ea8      402080e5       str r2, [r0, 0x40]
        ╎   0x00892eac      0f2082e2       add r2, r2, 0xf
        ╎   0x00892eb0      0600a0e1       mov r0, r6
        ╎   0x00892eb4      442083e5       str r2, [r3, 0x44]
        ╎   0x00892eb8      0310a0e1       mov r1, r3
        ╎   0x00892ebc      505083e5       str r5, [r3, 0x50]
        ╎   0x00892ec0      544083e5       str r4, [r3, 0x54]
        ╎   0x00892ec4      002098e5       ldr r2, [r8]
        ╎   0x00892ec8      bc75c3e1       strh r7, [r3, 0x5c]
        ╎   0x00892ecc      582083e5       str r2, [r3, 0x58]
        ╎   0x00892ed0      f041bde8       pop {r4, r5, r6, r7, r8, lr}
        └─< 0x00892ed4      5e14e2ea       b sym.imp.CHBProxyBase::sendRequestEvent_CHBServiceRequestEvent__const ; CHBProxyBase::sendRequestEvent(CHBServiceRequestEvent*) const
            0x00892ed8      d496db00       ldrsbeq sb, [fp], 0x64
```
