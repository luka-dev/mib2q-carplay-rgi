            ;-- method.MMIVdecController.GetExtensionIndex_void_:
            ;-- MMIVdecController::GetExtensionIndex(void*):
            0x00018ccc      f0412de9       push {r4, r5, r6, r7, r8, lr}; RELOC 32 _ZN17MMIVdecController17GetExtensionIndexEPv @ 0x00018ccc ; MMIVdecController::GetExtensionIndex(void*)
            0x00018cd0      8c559fe5       ldr r5, [0x00019264]        ; [0x19264:4]=0xc4c4
            0x00018cd4      8c659fe5       ldr r6, [0x00019268]        ; [0x19268:4]=0xffffd747
            0x00018cd8      0140a0e1       mov r4, r1
            0x00018cdc      05508fe0       add r5, pc, r5
            0x00018ce0      007091e5       ldr r7, [r1]
            0x00018ce4      066085e0       add r6, r5, r6
            0x00018ce8      0600a0e1       mov r0, r6
            0x00018cec      c4b5ffeb       bl sym.imp.std_strlen
            0x00018cf0      0610a0e1       mov r1, r6
            0x00018cf4      0020a0e1       mov r2, r0
            0x00018cf8      0700a0e1       mov r0, r7
            0x00018cfc      09b5ffeb       bl sym.imp.std_strncmp
            0x00018d00      000050e3       cmp r0, 0
            0x00018d04      04309405       ldreq r3, [r4, 4]
            0x00018d08      12028002       addeq r0, r0, 0x20000001
            0x00018d0c      58259f05       ldreq r2, [0x0001926c]      ; [0x1926c:4]=0x7f100001
        ┌─< 0x00018d10      4101000a       beq 0x1921c
        │   0x00018d14      54659fe5       ldr r6, [0x00019270]        ; [0x19270:4]=0xffffd76c
        │   0x00018d18      007094e5       ldr r7, [r4]
        │   0x00018d1c      066085e0       add r6, r5, r6
        │   0x00018d20      0600a0e1       mov r0, r6
        │   0x00018d24      b6b5ffeb       bl sym.imp.std_strlen
        │   0x00018d28      0610a0e1       mov r1, r6
        │   0x00018d2c      0020a0e1       mov r2, r0
        │   0x00018d30      0700a0e1       mov r0, r7
        │   0x00018d34      fbb4ffeb       bl sym.imp.std_strncmp
        │   0x00018d38      000050e3       cmp r0, 0
        │   0x00018d3c      04309405       ldreq r3, [r4, 4]
        │   0x00018d40      12028002       addeq r0, r0, 0x20000001
        │   0x00018d44      28259f05       ldreq r2, [0x00019274]      ; [0x19274:4]=0x7f100003
       ┌──< 0x00018d48      3301000a       beq 0x1921c
       ││   0x00018d4c      24659fe5       ldr r6, [0x00019278]        ; [0x19278:4]=0xffffd78c
       ││   0x00018d50      007094e5       ldr r7, [r4]
       ││   0x00018d54      066085e0       add r6, r5, r6
       ││   0x00018d58      0600a0e1       mov r0, r6
       ││   0x00018d5c      a8b5ffeb       bl sym.imp.std_strlen
       ││   0x00018d60      0610a0e1       mov r1, r6
       ││   0x00018d64      0020a0e1       mov r2, r0
       ││   0x00018d68      0700a0e1       mov r0, r7
       ││   0x00018d6c      edb4ffeb       bl sym.imp.std_strncmp
       ││   0x00018d70      000050e3       cmp r0, 0
       ││   0x00018d74      04309405       ldreq r3, [r4, 4]
       ││   0x00018d78      12028002       addeq r0, r0, 0x20000001
       ││   0x00018d7c      f8249f05       ldreq r2, [0x0001927c]      ; [0x1927c:4]=0x7f100004
      ┌───< 0x00018d80      2501000a       beq 0x1921c
      │││   0x00018d84      f4649fe5       ldr r6, [0x00019280]        ; [0x19280:4]=0xffffd7aa
      │││   0x00018d88      007094e5       ldr r7, [r4]
      │││   0x00018d8c      066085e0       add r6, r5, r6
      │││   0x00018d90      0600a0e1       mov r0, r6
      │││   0x00018d94      9ab5ffeb       bl sym.imp.std_strlen
      │││   0x00018d98      0610a0e1       mov r1, r6
      │││   0x00018d9c      0020a0e1       mov r2, r0
      │││   0x00018da0      0700a0e1       mov r0, r7
      │││   0x00018da4      dfb4ffeb       bl sym.imp.std_strncmp
      │││   0x00018da8      000050e3       cmp r0, 0
      │││   0x00018dac      04309405       ldreq r3, [r4, 4]
      │││   0x00018db0      12028002       addeq r0, r0, 0x20000001
      │││   0x00018db4      c8249f05       ldreq r2, [0x00019284]      ; [0x19284:4]=0x7f100005
     ┌────< 0x00018db8      1701000a       beq 0x1921c
     ││││   0x00018dbc      c4649fe5       ldr r6, [0x00019288]        ; [0x19288:4]=0xffffd7cb
     ││││   0x00018dc0      007094e5       ldr r7, [r4]
     ││││   0x00018dc4      066085e0       add r6, r5, r6
     ││││   0x00018dc8      0600a0e1       mov r0, r6
     ││││   0x00018dcc      8cb5ffeb       bl sym.imp.std_strlen
     ││││   0x00018dd0      0610a0e1       mov r1, r6
     ││││   0x00018dd4      0020a0e1       mov r2, r0
     ││││   0x00018dd8      0700a0e1       mov r0, r7
     ││││   0x00018ddc      d1b4ffeb       bl sym.imp.std_strncmp
     ││││   0x00018de0      000050e3       cmp r0, 0
     ││││   0x00018de4      04309405       ldreq r3, [r4, 4]
     ││││   0x00018de8      12028002       addeq r0, r0, 0x20000001
     ││││   0x00018dec      98249f05       ldreq r2, [0x0001928c]      ; [0x1928c:4]=0x7f100006
    ┌─────< 0x00018df0      0901000a       beq 0x1921c
    │││││   0x00018df4      94649fe5       ldr r6, [0x00019290]        ; [0x19290:4]=0xffffd7ea
    │││││   0x00018df8      007094e5       ldr r7, [r4]
    │││││   0x00018dfc      066085e0       add r6, r5, r6
    │││││   0x00018e00      0600a0e1       mov r0, r6
    │││││   0x00018e04      7eb5ffeb       bl sym.imp.std_strlen
    │││││   0x00018e08      0610a0e1       mov r1, r6
    │││││   0x00018e0c      0020a0e1       mov r2, r0
    │││││   0x00018e10      0700a0e1       mov r0, r7
    │││││   0x00018e14      c3b4ffeb       bl sym.imp.std_strncmp
    │││││   0x00018e18      000050e3       cmp r0, 0
    │││││   0x00018e1c      04309405       ldreq r3, [r4, 4]
    │││││   0x00018e20      12028002       addeq r0, r0, 0x20000001
    │││││   0x00018e24      68249f05       ldreq r2, [0x00019294]      ; [0x19294:4]=0x7f100007
   ┌──────< 0x00018e28      fb00000a       beq 0x1921c
   ││││││   0x00018e2c      64649fe5       ldr r6, [0x00019298]        ; [0x19298:4]=0xffffd817
   ││││││   0x00018e30      007094e5       ldr r7, [r4]
   ││││││   0x00018e34      066085e0       add r6, r5, r6
   ││││││   0x00018e38      0600a0e1       mov r0, r6
   ││││││   0x00018e3c      70b5ffeb       bl sym.imp.std_strlen
   ││││││   0x00018e40      0610a0e1       mov r1, r6
   ││││││   0x00018e44      0020a0e1       mov r2, r0
   ││││││   0x00018e48      0700a0e1       mov r0, r7
   ││││││   0x00018e4c      b5b4ffeb       bl sym.imp.std_strncmp
   ││││││   0x00018e50      000050e3       cmp r0, 0
   ││││││   0x00018e54      04309405       ldreq r3, [r4, 4]
   ││││││   0x00018e58      12028002       addeq r0, r0, 0x20000001
   ││││││   0x00018e5c      38249f05       ldreq r2, [0x0001929c]      ; [0x1929c:4]=0x7f100002
  ┌───────< 0x00018e60      ed00000a       beq 0x1921c
  │││││││   0x00018e64      34649fe5       ldr r6, [0x000192a0]        ; [0x192a0:4]=0xffffd84a
  │││││││   0x00018e68      007094e5       ldr r7, [r4]
  │││││││   0x00018e6c      066085e0       add r6, r5, r6
  │││││││   0x00018e70      0600a0e1       mov r0, r6
  │││││││   0x00018e74      62b5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018e78      0610a0e1       mov r1, r6
  │││││││   0x00018e7c      0020a0e1       mov r2, r0
  │││││││   0x00018e80      0700a0e1       mov r0, r7
  │││││││   0x00018e84      a7b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018e88      000050e3       cmp r0, 0
  │││││││   0x00018e8c      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018e90      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018e94      08249f05       ldreq r2, [0x000192a4]      ; [0x192a4:4]=0x7f100008
  ────────< 0x00018e98      df00000a       beq 0x1921c
  │││││││   0x00018e9c      04649fe5       ldr r6, [0x000192a8]        ; [0x192a8:4]=0xffffd87b
  │││││││   0x00018ea0      007094e5       ldr r7, [r4]
  │││││││   0x00018ea4      066085e0       add r6, r5, r6
  │││││││   0x00018ea8      0600a0e1       mov r0, r6
  │││││││   0x00018eac      54b5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018eb0      0610a0e1       mov r1, r6
  │││││││   0x00018eb4      0020a0e1       mov r2, r0
  │││││││   0x00018eb8      0700a0e1       mov r0, r7
  │││││││   0x00018ebc      99b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018ec0      000050e3       cmp r0, 0
  │││││││   0x00018ec4      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018ec8      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018ecc      d8239f05       ldreq r2, [0x000192ac]      ; [0x192ac:4]=0x7f10000d
  ────────< 0x00018ed0      d100000a       beq 0x1921c
  │││││││   0x00018ed4      d4639fe5       ldr r6, [0x000192b0]        ; [0x192b0:4]=0xffffd8a0
  │││││││   0x00018ed8      007094e5       ldr r7, [r4]
  │││││││   0x00018edc      066085e0       add r6, r5, r6
  │││││││   0x00018ee0      0600a0e1       mov r0, r6
  │││││││   0x00018ee4      46b5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018ee8      0610a0e1       mov r1, r6
  │││││││   0x00018eec      0020a0e1       mov r2, r0
  │││││││   0x00018ef0      0700a0e1       mov r0, r7
  │││││││   0x00018ef4      8bb4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018ef8      000050e3       cmp r0, 0
  │││││││   0x00018efc      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018f00      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018f04      a8239f05       ldreq r2, [0x000192b4]      ; [0x192b4:4]=0x7f100009
  ────────< 0x00018f08      c300000a       beq 0x1921c
  │││││││   0x00018f0c      a4639fe5       ldr r6, [0x000192b8]        ; [0x192b8:4]=0xffffd8c4
  │││││││   0x00018f10      007094e5       ldr r7, [r4]
  │││││││   0x00018f14      066085e0       add r6, r5, r6
  │││││││   0x00018f18      0600a0e1       mov r0, r6
  │││││││   0x00018f1c      38b5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018f20      0610a0e1       mov r1, r6
  │││││││   0x00018f24      0020a0e1       mov r2, r0
  │││││││   0x00018f28      0700a0e1       mov r0, r7
  │││││││   0x00018f2c      7db4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018f30      000050e3       cmp r0, 0
  │││││││   0x00018f34      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018f38      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018f3c      78239f05       ldreq r2, [0x000192bc]      ; [0x192bc:4]=0x7f10000a
  ────────< 0x00018f40      b500000a       beq 0x1921c
  │││││││   0x00018f44      74639fe5       ldr r6, [0x000192c0]        ; [0x192c0:4]=0xffffd8ef
  │││││││   0x00018f48      007094e5       ldr r7, [r4]
  │││││││   0x00018f4c      066085e0       add r6, r5, r6
  │││││││   0x00018f50      0600a0e1       mov r0, r6
  │││││││   0x00018f54      2ab5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018f58      0610a0e1       mov r1, r6
  │││││││   0x00018f5c      0020a0e1       mov r2, r0
  │││││││   0x00018f60      0700a0e1       mov r0, r7
  │││││││   0x00018f64      6fb4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018f68      000050e3       cmp r0, 0
  │││││││   0x00018f6c      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018f70      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018f74      48239f05       ldreq r2, [0x000192c4]      ; [0x192c4:4]=0x7f10000b
  ────────< 0x00018f78      a700000a       beq 0x1921c
  │││││││   0x00018f7c      44639fe5       ldr r6, [0x000192c8]        ; [0x192c8:4]=0xffffd920
  │││││││   0x00018f80      007094e5       ldr r7, [r4]
  │││││││   0x00018f84      066085e0       add r6, r5, r6
  │││││││   0x00018f88      0600a0e1       mov r0, r6
  │││││││   0x00018f8c      1cb5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018f90      0610a0e1       mov r1, r6
  │││││││   0x00018f94      0020a0e1       mov r2, r0
  │││││││   0x00018f98      0700a0e1       mov r0, r7
  │││││││   0x00018f9c      61b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018fa0      000050e3       cmp r0, 0
  │││││││   0x00018fa4      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018fa8      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018fac      18239f05       ldreq r2, [0x000192cc]      ; [0x192cc:4]=0x7f10000c
  ────────< 0x00018fb0      9900000a       beq 0x1921c
  │││││││   0x00018fb4      14639fe5       ldr r6, [0x000192d0]        ; [0x192d0:4]=0xffffd94d
  │││││││   0x00018fb8      007094e5       ldr r7, [r4]
  │││││││   0x00018fbc      066085e0       add r6, r5, r6
  │││││││   0x00018fc0      0600a0e1       mov r0, r6
  │││││││   0x00018fc4      0eb5ffeb       bl sym.imp.std_strlen
  │││││││   0x00018fc8      0610a0e1       mov r1, r6
  │││││││   0x00018fcc      0020a0e1       mov r2, r0
  │││││││   0x00018fd0      0700a0e1       mov r0, r7
  │││││││   0x00018fd4      53b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00018fd8      000050e3       cmp r0, 0
  │││││││   0x00018fdc      04309405       ldreq r3, [r4, 4]
  │││││││   0x00018fe0      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00018fe4      e8229f05       ldreq r2, [0x000192d4]      ; [0x192d4:4]=0x7f101000
  ────────< 0x00018fe8      8b00000a       beq 0x1921c
  │││││││   0x00018fec      e4629fe5       ldr r6, [0x000192d8]        ; [0x192d8:4]=0xffffd97b
  │││││││   0x00018ff0      007094e5       ldr r7, [r4]
  │││││││   0x00018ff4      066085e0       add r6, r5, r6
  │││││││   0x00018ff8      0600a0e1       mov r0, r6
  │││││││   0x00018ffc      00b5ffeb       bl sym.imp.std_strlen
  │││││││   0x00019000      0610a0e1       mov r1, r6
  │││││││   0x00019004      0020a0e1       mov r2, r0
  │││││││   0x00019008      0700a0e1       mov r0, r7
  │││││││   0x0001900c      45b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019010      000050e3       cmp r0, 0
  │││││││   0x00019014      04309405       ldreq r3, [r4, 4]
  │││││││   0x00019018      12028002       addeq r0, r0, 0x20000001
  │││││││   0x0001901c      b8229f05       ldreq r2, [0x000192dc]      ; [0x192dc:4]=0x7f101001
  ────────< 0x00019020      7d00000a       beq 0x1921c
  │││││││   0x00019024      b4629fe5       ldr r6, [0x000192e0]        ; [0x192e0:4]=0xffffd9aa
  │││││││   0x00019028      007094e5       ldr r7, [r4]
  │││││││   0x0001902c      066085e0       add r6, r5, r6
  │││││││   0x00019030      0600a0e1       mov r0, r6
  │││││││   0x00019034      f2b4ffeb       bl sym.imp.std_strlen
  │││││││   0x00019038      0610a0e1       mov r1, r6
  │││││││   0x0001903c      0020a0e1       mov r2, r0
  │││││││   0x00019040      0700a0e1       mov r0, r7
  │││││││   0x00019044      37b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019048      000050e3       cmp r0, 0
  │││││││   0x0001904c      04309405       ldreq r3, [r4, 4]
  │││││││   0x00019050      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00019054      88229f05       ldreq r2, [0x000192e4]      ; [0x192e4:4]=0x7f101002
  ────────< 0x00019058      6f00000a       beq 0x1921c
  │││││││   0x0001905c      84629fe5       ldr r6, [0x000192e8]        ; [0x192e8:4]=0xffffd9d8
  │││││││   0x00019060      007094e5       ldr r7, [r4]
  │││││││   0x00019064      066085e0       add r6, r5, r6
  │││││││   0x00019068      0600a0e1       mov r0, r6
  │││││││   0x0001906c      e4b4ffeb       bl sym.imp.std_strlen
  │││││││   0x00019070      0610a0e1       mov r1, r6
  │││││││   0x00019074      0020a0e1       mov r2, r0
  │││││││   0x00019078      0700a0e1       mov r0, r7
  │││││││   0x0001907c      29b4ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019080      000050e3       cmp r0, 0
  │││││││   0x00019084      04309405       ldreq r3, [r4, 4]
  │││││││   0x00019088      12028002       addeq r0, r0, 0x20000001
  │││││││   0x0001908c      58229f05       ldreq r2, [0x000192ec]      ; [0x192ec:4]=0x7f101003
  ────────< 0x00019090      6100000a       beq 0x1921c
  │││││││   0x00019094      54629fe5       ldr r6, [0x000192f0]        ; [0x192f0:4]=0xffffda02
  │││││││   0x00019098      007094e5       ldr r7, [r4]
  │││││││   0x0001909c      066085e0       add r6, r5, r6
  │││││││   0x000190a0      0600a0e1       mov r0, r6
  │││││││   0x000190a4      d6b4ffeb       bl sym.imp.std_strlen
  │││││││   0x000190a8      0610a0e1       mov r1, r6
  │││││││   0x000190ac      0020a0e1       mov r2, r0
  │││││││   0x000190b0      0700a0e1       mov r0, r7
  │││││││   0x000190b4      1bb4ffeb       bl sym.imp.std_strncmp
  │││││││   0x000190b8      000050e3       cmp r0, 0
  │││││││   0x000190bc      04309405       ldreq r3, [r4, 4]
  │││││││   0x000190c0      12028002       addeq r0, r0, 0x20000001
  │││││││   0x000190c4      28229f05       ldreq r2, [0x000192f4]      ; [0x192f4:4]=0x7f101004
  ────────< 0x000190c8      5300000a       beq 0x1921c
  │││││││   0x000190cc      24629fe5       ldr r6, [0x000192f8]        ; [0x192f8:4]=0xffffda2c
  │││││││   0x000190d0      007094e5       ldr r7, [r4]
  │││││││   0x000190d4      066085e0       add r6, r5, r6
  │││││││   0x000190d8      0600a0e1       mov r0, r6
  │││││││   0x000190dc      c8b4ffeb       bl sym.imp.std_strlen
  │││││││   0x000190e0      0610a0e1       mov r1, r6
  │││││││   0x000190e4      0020a0e1       mov r2, r0
  │││││││   0x000190e8      0700a0e1       mov r0, r7
  │││││││   0x000190ec      0db4ffeb       bl sym.imp.std_strncmp
  │││││││   0x000190f0      000050e3       cmp r0, 0
  │││││││   0x000190f4      04309405       ldreq r3, [r4, 4]
  │││││││   0x000190f8      12028002       addeq r0, r0, 0x20000001
  │││││││   0x000190fc      f8219f05       ldreq r2, [0x000192fc]      ; [0x192fc:4]=0x7f101005
  ────────< 0x00019100      4500000a       beq 0x1921c
  │││││││   0x00019104      f4619fe5       ldr r6, [0x00019300]        ; [0x19300:4]=0xffffda59
  │││││││   0x00019108      007094e5       ldr r7, [r4]
  │││││││   0x0001910c      066085e0       add r6, r5, r6
  │││││││   0x00019110      0600a0e1       mov r0, r6
  │││││││   0x00019114      bab4ffeb       bl sym.imp.std_strlen
  │││││││   0x00019118      0610a0e1       mov r1, r6
  │││││││   0x0001911c      0020a0e1       mov r2, r0
  │││││││   0x00019120      0700a0e1       mov r0, r7
  │││││││   0x00019124      ffb3ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019128      000050e3       cmp r0, 0
  │││││││   0x0001912c      04309405       ldreq r3, [r4, 4]
  │││││││   0x00019130      12028002       addeq r0, r0, 0x20000001
  │││││││   0x00019134      c8219f05       ldreq r2, [0x00019304]      ; [0x19304:4]=0x7f101006
  ────────< 0x00019138      3700000a       beq 0x1921c
  │││││││   0x0001913c      c4619fe5       ldr r6, [0x00019308]        ; [0x19308:4]=0xffffda87
  │││││││   0x00019140      007094e5       ldr r7, [r4]
  │││││││   0x00019144      066085e0       add r6, r5, r6
  │││││││   0x00019148      0600a0e1       mov r0, r6
  │││││││   0x0001914c      acb4ffeb       bl sym.imp.std_strlen
  │││││││   0x00019150      0610a0e1       mov r1, r6
  │││││││   0x00019154      0020a0e1       mov r2, r0
  │││││││   0x00019158      0700a0e1       mov r0, r7
  │││││││   0x0001915c      f1b3ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019160      000050e3       cmp r0, 0
  │││││││   0x00019164      04309405       ldreq r3, [r4, 4]
  │││││││   0x00019168      12028002       addeq r0, r0, 0x20000001
  │││││││   0x0001916c      98219f05       ldreq r2, [0x0001930c]      ; [0x1930c:4]=0x7f10000e
  ────────< 0x00019170      2900000a       beq 0x1921c
  │││││││   0x00019174      94619fe5       ldr r6, [0x00019310]        ; [0x19310:4]=0xffffdaaf
  │││││││   0x00019178      007094e5       ldr r7, [r4]
  │││││││   0x0001917c      066085e0       add r6, r5, r6
  │││││││   0x00019180      0600a0e1       mov r0, r6
  │││││││   0x00019184      9eb4ffeb       bl sym.imp.std_strlen
  │││││││   0x00019188      0610a0e1       mov r1, r6
  │││││││   0x0001918c      0020a0e1       mov r2, r0
  │││││││   0x00019190      0700a0e1       mov r0, r7
  │││││││   0x00019194      e3b3ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019198      000050e3       cmp r0, 0
  │││││││   0x0001919c      04309405       ldreq r3, [r4, 4]
  │││││││   0x000191a0      12028002       addeq r0, r0, 0x20000001
  │││││││   0x000191a4      68219f05       ldreq r2, [0x00019314]      ; [0x19314:4]=0x7f10000f
  ────────< 0x000191a8      1b00000a       beq 0x1921c
  │││││││   0x000191ac      64619fe5       ldr r6, [0x00019318]        ; [0x19318:4]=0xffffdad6
  │││││││   0x000191b0      007094e5       ldr r7, [r4]
  │││││││   0x000191b4      066085e0       add r6, r5, r6
  │││││││   0x000191b8      0600a0e1       mov r0, r6
  │││││││   0x000191bc      90b4ffeb       bl sym.imp.std_strlen
  │││││││   0x000191c0      0610a0e1       mov r1, r6
  │││││││   0x000191c4      0020a0e1       mov r2, r0
  │││││││   0x000191c8      0700a0e1       mov r0, r7
  │││││││   0x000191cc      d5b3ffeb       bl sym.imp.std_strncmp
  │││││││   0x000191d0      000050e3       cmp r0, 0
  │││││││   0x000191d4      04309405       ldreq r3, [r4, 4]
  │││││││   0x000191d8      12028002       addeq r0, r0, 0x20000001
  │││││││   0x000191dc      38219f05       ldreq r2, [0x0001931c]      ; [0x1931c:4]=0x7f100010
  ────────< 0x000191e0      0d00000a       beq 0x1921c
  │││││││   0x000191e4      34619fe5       ldr r6, [0x00019320]        ; [0x19320:4]=0xffffdafb
  │││││││   0x000191e8      007094e5       ldr r7, [r4]
  │││││││   0x000191ec      066085e0       add r6, r5, r6
  │││││││   0x000191f0      0600a0e1       mov r0, r6
  │││││││   0x000191f4      82b4ffeb       bl sym.imp.std_strlen
  │││││││   0x000191f8      0610a0e1       mov r1, r6
  │││││││   0x000191fc      0020a0e1       mov r2, r0
  │││││││   0x00019200      0700a0e1       mov r0, r7
  │││││││   0x00019204      c7b3ffeb       bl sym.imp.std_strncmp
  │││││││   0x00019208      000050e3       cmp r0, 0
  ────────< 0x0001920c      0400001a       bne 0x19224
  │││││││   0x00019210      043094e5       ldr r3, [r4, 4]
  │││││││   0x00019214      120280e2       add r0, r0, 0x20000001
  │││││││   0x00019218      04219fe5       ldr r2, [0x00019324]        ; [0x19324:4]=0x7f101007
  └└└└└└└─> 0x0001921c      002083e5       str r2, [r3]
            0x00019220      f081bde8       pop {r4, r5, r6, r7, r8, pc}
  ────────> 0x00019224      fc309fe5       ldr r3, [0x00019328]        ; [0x19328:4]=0xffffdb2a
            0x00019228      006094e5       ldr r6, [r4]
            0x0001922c      035085e0       add r5, r5, r3
            0x00019230      0500a0e1       mov r0, r5
            0x00019234      72b4ffeb       bl sym.imp.std_strlen
            0x00019238      0510a0e1       mov r1, r5
            0x0001923c      0020a0e1       mov r2, r0
            0x00019240      0600a0e1       mov r0, r6
            0x00019244      b7b3ffeb       bl sym.imp.std_strncmp
            0x00019248      000050e3       cmp r0, 0
            0x0001924c      1202a003       moveq r0, 0x20000001
            0x00019250      04309405       ldreq r3, [r4, 4]
            0x00019254      d0209f05       ldreq r2, [0x0001932c]      ; [0x1932c:4]=0x7f100012
            0x00019258      d0009f15       ldrne r0, [0x00019330]      ; [0x19330:4]=0x20000011
            0x0001925c      00208305       streq r2, [r3]
            0x00019260      f081bde8       pop {r4, r5, r6, r7, r8, pc}
            0x00019264      c4c40000       andeq ip, r0, r4, asr 9
            0x00019268      47d7ffff       invalid
            0x0001926c      0100107f       svcvc 0x100001
            0x00019270      6cd7ffff       invalid
            0x00019274      0300107f       svcvc 0x100003
            0x00019278      8cd7ffff       invalid
            0x0001927c      0400107f       svcvc 0x100004
            0x00019280      aad7ffff       invalid
            0x00019284      0500107f       svcvc 0x100005
            0x00019288      cbd7ffff       invalid
            0x0001928c      0600107f       svcvc 0x100006
            0x00019290      ead7ffff       invalid
            0x00019294      0700107f       svcvc 0x100007
            0x00019298      17d8ffff       invalid
            0x0001929c      0200107f       svcvc 0x100002
            0x000192a0      4ad8ffff       invalid
            0x000192a4      0800107f       svcvc 0x100008
            0x000192a8      7bd8ffff       invalid
            0x000192ac      0d00107f       svcvc 0x10000d
            0x000192b0      a0d8ffff       invalid
            0x000192b4      0900107f       svcvc 0x100009
            0x000192b8      c4d8ffff       invalid
            0x000192bc      0a00107f       svcvc 0x10000a
            0x000192c0      efd8ffff       invalid
            0x000192c4      0b00107f       svcvc 0x10000b
            0x000192c8      20d9ffff       invalid
            0x000192cc      0c00107f       svcvc 0x10000c
            0x000192d0      4dd9ffff       invalid
            0x000192d4      0010107f       svcvc 0x101000
            0x000192d8      7bd9ffff       invalid
            0x000192dc      0110107f       svcvc 0x101001
            0x000192e0      aad9ffff       invalid
            0x000192e4      0210107f       svcvc 0x101002
            0x000192e8      d8d9ffff       invalid
            0x000192ec      0310107f       svcvc 0x101003
            0x000192f0      02daffff       invalid
            0x000192f4      0410107f       svcvc 0x101004
            0x000192f8      2cdaffff       invalid
            0x000192fc      0510107f       svcvc 0x101005
            0x00019300      59daffff       invalid
            0x00019304      0610107f       svcvc 0x101006
            0x00019308      87daffff       invalid
            0x0001930c      0e00107f       svcvc 0x10000e
            0x00019310      afdaffff       invalid
            0x00019314      0f00107f       svcvc 0x10000f
            0x00019318      d6daffff       invalid
            0x0001931c      1000107f       svcvc 0x100010
            0x00019320      fbdaffff       invalid
            0x00019324      0710107f       svcvc 0x101007
            0x00019328      2adbffff       invalid
            0x0001932c      1200107f       svcvc 0x100012
            0x00019330      11000020       andhs r0, r0, r1, lsl r0
            ;-- method.MMIVdecController.SetOmxParam_void_:
            ;-- MMIVdecController::SetOmxParam(void*):
            0x00018958      003091e5       ldr r3, [r1]                ; RELOC 32 _ZN17MMIVdecController11SetOmxParamEPv @ 0x00018958 ; MMIVdecController::SetOmxParam(void*)
            0x0001895c      30239fe5       ldr r2, [0x00018c94]        ; [0x18c94:4]=0x600000a
            0x00018960      f0412de9       push {r4, r5, r6, r7, r8, lr}
            0x00018964      020053e1       cmp r3, r2
            0x00018968      28539fe5       ldr r5, [0x00018c98]        ; [0x18c98:4]=0xc82c
            0x0001896c      30d04de2       sub sp, sp, 0x30
            0x00018970      0040a0e1       mov r4, r0
            0x00018974      05508fe0       add r5, pc, r5
        ┌─< 0x00018978      090000ca       bgt 0x189a4
        │   0x0001897c      012042e2       sub r2, r2, 1
        │   0x00018980      020053e1       cmp r3, r2
       ┌──< 0x00018984      bc0000aa       bge 0x18c7c
       ││   0x00018988      0c239fe5       ldr r2, [0x00018c9c]        ; [0x18c9c:4]=0x1000017
       ││   0x0001898c      020053e1       cmp r3, r2
      ┌───< 0x00018990      8700000a       beq 0x18bb4
      │││   0x00018994      04239fe5       ldr r2, [0x00018ca0]        ; [0x18ca0:4]=0x6000001
      │││   0x00018998      020053e1       cmp r3, r2
     ┌────< 0x0001899c      ae00001a       bne 0x18c5c
    ┌─────< 0x000189a0      0e0000ea       b 0x189e0
    ││││└─> 0x000189a4      f8229fe5       ldr r2, [0x00018ca4]        ; [0x18ca4:4]=0x600000d
    ││││    0x000189a8      020053e1       cmp r3, r2
    ││││┌─< 0x000189ac      5000000a       beq 0x18af4
   ┌──────< 0x000189b0      030000ca       bgt 0x189c4
   ││││││   0x000189b4      022042e2       sub r2, r2, 2
   ││││││   0x000189b8      020053e1       cmp r3, r2
  ┌───────< 0x000189bc      a600001a       bne 0x18c5c
  ────────< 0x000189c0      740000ea       b 0x18b98
  │└──────> 0x000189c4      dc229fe5       ldr r2, [0x00018ca8]        ; [0x18ca8:4]=0x600000e
  │ │││││   0x000189c8      020053e1       cmp r3, r2
  │┌──────< 0x000189cc      aa00000a       beq 0x18c7c
  │││││││   0x000189d0      d4229fe5       ldr r2, [0x00018cac]        ; [0x18cac:4]=0x7000001
  │││││││   0x000189d4      020053e1       cmp r3, r2
  ────────< 0x000189d8      9f00001a       bne 0x18c5c
  ────────< 0x000189dc      770000ea       b 0x18bc0
  ││└─────> 0x000189e0      047091e5       ldr r7, [r1, 4]
  ││ ││││   0x000189e4      081097e5       ldr r1, [r7, 8]
  ││ ││││   0x000189e8      000051e3       cmp r1, 0
  ││┌─────< 0x000189ec      0a00001a       bne 0x18a1c
  │││││││   0x000189f0      101097e5       ldr r1, [r7, 0x10]
  │││││││   0x000189f4      8db7ffeb       bl rsym._ZN17MMIVdecController13SetCodingTypeE20OMX_VIDEO_CODINGTYPE
  │││││││   0x000189f8      000050e3       cmp r0, 0
  ────────< 0x000189fc      a000000a       beq 0x18c84
  │││││││   0x00018a00      082097e5       ldr r2, [r7, 8]
  │││││││   0x00018a04      3410a0e3       mov r1, 0x34
  │││││││   0x00018a08      103097e5       ldr r3, [r7, 0x10]
  │││││││   0x00018a0c      1262a0e3       mov r6, 0x20000001
  │││││││   0x00018a10      914224e0       mla r4, r1, r2, r4
  │││││││   0x00018a14      5c3084e5       str r3, [r4, 0x5c]
  ────────< 0x00018a18      9a0000ea       b 0x18c88
  ││└─────> 0x00018a1c      010051e3       cmp r1, 1
  ││┌─────< 0x00018a20      9500001a       bne 0x18c7c
  │││││││   0x00018a24      143802e3       movw r3, 0x2814             ; '\x14('
  │││││││   0x00018a28      033090e7       ldr r3, [r0, r3]
  │││││││   0x00018a2c      000053e3       cmp r3, 0
  ────────< 0x00018a30      1f00000a       beq 0x18ab4
  │││││││   0x00018a34      0300a0e1       mov r0, r3
  │││││││   0x00018a38      142097e5       ldr r2, [r7, 0x14]
  │││││││   0x00018a3c      bbb6ffeb       bl rsym._ZN12Deinterlacer11SetColorFmtE16VdecDeintBufType20OMX_COLOR_FORMATTYPE
  │││││││   0x00018a40      120250e3       cmp r0, 0x20000001
  │││││││   0x00018a44      0060a0e1       mov r6, r0
  ────────< 0x00018a48      0a00001a       bne 0x18a78
  │││││││   0x00018a4c      083097e5       ldr r3, [r7, 8]
  │││││││   0x00018a50      34c0a0e3       mov ip, 0x34
  │││││││   0x00018a54      142097e5       ldr r2, [r7, 0x14]
  │││││││   0x00018a58      0400a0e1       mov r0, r4
  │││││││   0x00018a5c      4c129fe5       ldr r1, [0x00018cb0]        ; [0x18cb0:4]=0x7f000004
  │││││││   0x00018a60      9c4323e0       mla r3, ip, r3, r4
  │││││││   0x00018a64      602083e5       str r2, [r3, 0x60]
  │││││││   0x00018a68      bcb6ffeb       bl rsym._ZN17MMIVdecController14SetColorFormatE20OMX_COLOR_FORMATTYPE
  │││││││   0x00018a6c      001050e2       subs r1, r0, 0
  ────────< 0x00018a70      8400000a       beq 0x18c88
  ────────< 0x00018a74      120000ea       b 0x18ac4
  ────────> 0x00018a78      34329fe5       ldr r3, [0x00018cb4]        ; [0x18cb4:4]=0xffffd6b7
  │││││││   0x00018a7c      231700e3       movw r1, 0x723
  │││││││   0x00018a80      30029fe5       ldr r0, [0x00018cb8]        ; [0x18cb8:4]=0xffffaa0c
  │││││││   0x00018a84      0620a0e3       mov r2, 6
  │││││││   0x00018a88      033085e0       add r3, r5, r3
  │││││││   0x00018a8c      00308de5       str r3, [sp]
  │││││││   0x00018a90      14e097e5       ldr lr, [r7, 0x14]
  │││││││   0x00018a94      000085e0       add r0, r5, r0
  │││││││   0x00018a98      0330a0e3       mov r3, 3
  │││││││   0x00018a9c      00c0a0e3       mov ip, 0
  │││││││   0x00018aa0      0cc08de5       str ip, [sp, 0xc]
  │││││││   0x00018aa4      04e08de5       str lr, [sp, 4]
  │││││││   0x00018aa8      08c08de5       str ip, [sp, 8]
  │││││││   0x00018aac      2fb7ffeb       bl sym.imp.MM_OutputDebugMessage
  ────────< 0x00018ab0      740000ea       b 0x18c88
  ────────> 0x00018ab4      141097e5       ldr r1, [r7, 0x14]
  │││││││   0x00018ab8      a8b6ffeb       bl rsym._ZN17MMIVdecController14SetColorFormatE20OMX_COLOR_FORMATTYPE
  │││││││   0x00018abc      001050e2       subs r1, r0, 0
  ────────< 0x00018ac0      0400000a       beq 0x18ad8
  ────────> 0x00018ac4      020051e3       cmp r1, 2
  │││││││   0x00018ac8      15005113       cmpne r1, 0x15
  │││││││   0x00018acc      0400a001       moveq r0, r4
  ────────< 0x00018ad0      6b00001a       bne 0x18c84
  ────────< 0x00018ad4      5e0000ea       b 0x18c54
  ────────> 0x00018ad8      082097e5       ldr r2, [r7, 8]
  │││││││   0x00018adc      3410a0e3       mov r1, 0x34
  │││││││   0x00018ae0      143097e5       ldr r3, [r7, 0x14]
  │││││││   0x00018ae4      1262a0e3       mov r6, 0x20000001
  │││││││   0x00018ae8      914224e0       mla r4, r1, r2, r4
  │││││││   0x00018aec      603084e5       str r3, [r4, 0x60]
  ────────< 0x00018af0      640000ea       b 0x18c88
  ││││││└─> 0x00018af4      042091e5       ldr r2, [r1, 4]
  ││││││    0x00018af8      103090e5       ldr r3, [r0, 0x10]
  ││││││    0x00018afc      38c092e5       ldr ip, [r2, 0x38]
  ││││││    0x00018b00      010053e3       cmp r3, 1
  ││││││    0x00018b04      18c080e5       str ip, [r0, 0x18]
  ││││││┌─< 0x00018b08      5b00001a       bne 0x18c7c
  │││││││   0x00018b0c      1c6090e5       ldr r6, [r0, 0x1c]
  │││││││   0x00018b10      d03602e3       movw r3, 0x26d0
  │││││││   0x00018b14      02e0a0e3       mov lr, 2
  │││││││   0x00018b18      14208de2       add r2, sp, 0x14
  │││││││   0x00018b1c      030090e7       ldr r0, [r0, r3]
  │││││││   0x00018b20      0e10a0e1       mov r1, lr
  │││││││   0x00018b24      9b3d43e2       sub r3, r3, 0x26c0
  │││││││   0x00018b28      1cc08de5       str ip, [sp, 0x1c]
  │││││││   0x00018b2c      20608de5       str r6, [sp, 0x20]
  │││││││   0x00018b30      00c0a0e3       mov ip, 0
  │││││││   0x00018b34      14e08de5       str lr, [sp, 0x14]
  │││││││   0x00018b38      08c08de5       str ip, [sp, 8]
  │││││││   0x00018b3c      18e08de5       str lr, [sp, 0x18]
  │││││││   0x00018b40      00c08de5       str ip, [sp]
  │││││││   0x00018b44      04c08de5       str ip, [sp, 4]
  │││││││   0x00018b48      f6b6ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018b4c      006050e2       subs r6, r0, 0
  ────────< 0x00018b50      4900000a       beq 0x18c7c
  │││││││   0x00018b54      1cc09de5       ldr ip, [sp, 0x1c]
  │││││││   0x00018b58      541700e3       movw r1, 0x754
  │││││││   0x00018b5c      54019fe5       ldr r0, [0x00018cb8]        ; [0x18cb8:4]=0xffffaa0c
  │││││││   0x00018b60      0620a0e3       mov r2, 6
  │││││││   0x00018b64      0330a0e3       mov r3, 3
  │││││││   0x00018b68      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00018b6c      04c08de5       str ip, [sp, 4]
  │││││││   0x00018b70      000085e0       add r0, r5, r0
  │││││││   0x00018b74      20c09de5       ldr ip, [sp, 0x20]
  │││││││   0x00018b78      08c08de5       str ip, [sp, 8]
  │││││││   0x00018b7c      38c19fe5       ldr ip, [0x00018cbc]        ; [0x18cbc:4]=0xffffd6e3
  │││││││   0x00018b80      0c5085e0       add r5, r5, ip
  │││││││   0x00018b84      00508de5       str r5, [sp]
  │││││││   0x00018b88      f8b6ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018b8c      0400a0e1       mov r0, r4
  │││││││   0x00018b90      0610a0e1       mov r1, r6
  ────────< 0x00018b94      2e0000ea       b 0x18c54
  ────────> 0x00018b98      043091e5       ldr r3, [r1, 4]
  │││││││   0x00018b9c      0c3093e5       ldr r3, [r3, 0xc]
  │││││││   0x00018ba0      080053e3       cmp r3, 8
  │││││││   0x00018ba4      143080e5       str r3, [r0, 0x14]
  │││││││   0x00018ba8      10619f15       ldrne r6, [0x00018cc0]      ; [0x18cc0:4]=0x20000011
  ────────< 0x00018bac      3500001a       bne 0x18c88
  ────────< 0x00018bb0      310000ea       b 0x18c7c
  ││││└───> 0x00018bb4      041091e5       ldr r1, [r1, 4]
  ││││ ││   0x00018bb8      e3b6ffeb       bl rsym._ZN17MMIVdecController16SetComponentRoleEPv
  ││││┌───< 0x00018bbc      2c0000ea       b 0x18c74
  ────────> 0x00018bc0      047091e5       ldr r7, [r1, 4]
  │││││││   0x00018bc4      d03602e3       movw r3, 0x26d0
  │││││││   0x00018bc8      02c0a0e3       mov ip, 2
  │││││││   0x00018bcc      24208de2       add r2, sp, 0x24
  │││││││   0x00018bd0      030090e7       ldr r0, [r0, r3]
  │││││││   0x00018bd4      0c10a0e1       mov r1, ip
  │││││││   0x00018bd8      0ce097e5       ldr lr, [r7, 0xc]
  │││││││   0x00018bdc      0c30a0e3       mov r3, 0xc
  │││││││   0x00018be0      0060a0e3       mov r6, 0
  │││││││   0x00018be4      28c08de5       str ip, [sp, 0x28]
  │││││││   0x00018be8      00608de5       str r6, [sp]
  │││││││   0x00018bec      01005ee3       cmp lr, 1
  │││││││   0x00018bf0      00c0a013       movne ip, 0
  │││││││   0x00018bf4      01c0a003       moveq ip, 1
  │││││││   0x00018bf8      04608de5       str r6, [sp, 4]
  │││││││   0x00018bfc      2cc0cde5       strb ip, [sp, 0x2c]
  │││││││   0x00018c00      0fc0a0e3       mov ip, 0xf
  │││││││   0x00018c04      08608de5       str r6, [sp, 8]
  │││││││   0x00018c08      24c08de5       str ip, [sp, 0x24]
  │││││││   0x00018c0c      c5b6ffeb       bl rsym.QVD_IOControl
  │││││││   0x00018c10      008050e2       subs r8, r0, 0
  ────────< 0x00018c14      1800000a       beq 0x18c7c
  │││││││   0x00018c18      a4109fe5       ldr r1, [0x00018cc4]        ; [0x18cc4:4]=0xffffd718
  │││││││   0x00018c1c      0620a0e3       mov r2, 6
  │││││││   0x00018c20      90009fe5       ldr r0, [0x00018cb8]        ; [0x18cb8:4]=0xffffaa0c
  │││││││   0x00018c24      0330a0e3       mov r3, 3
  │││││││   0x00018c28      011085e0       add r1, r5, r1
  │││││││   0x00018c2c      00108de5       str r1, [sp]
  │││││││   0x00018c30      0cc097e5       ldr ip, [r7, 0xc]
  │││││││   0x00018c34      000085e0       add r0, r5, r0
  │││││││   0x00018c38      771700e3       movw r1, 0x777
  │││││││   0x00018c3c      0c608de5       str r6, [sp, 0xc]
  │││││││   0x00018c40      08808de5       str r8, [sp, 8]
  │││││││   0x00018c44      04c08de5       str ip, [sp, 4]
  │││││││   0x00018c48      c8b6ffeb       bl sym.imp.MM_OutputDebugMessage
  │││││││   0x00018c4c      0400a0e1       mov r0, r4
  │││││││   0x00018c50      0810a0e1       mov r1, r8
  ────────> 0x00018c54      03b5ffeb       bl rsym._ZN17MMIVdecController9MapStatusEi
  ────────< 0x00018c58      050000ea       b 0x18c74
  └──└────> 0x00018c5c      64209fe5       ldr r2, [0x00018cc8]        ; [0x18cc8:4]=0x6f100000
   ││ │││   0x00018c60      020053e1       cmp r3, r2
   ││ │││   0x00018c64      f262a093       movls r6, 0x2000000f
   ││┌────< 0x00018c68      0600009a       bls 0x18c88
   ││││││   0x00018c6c      0400a0e1       mov r0, r4
   ││││││   0x00018c70      9bb5ffeb       bl rsym._ZN17MMIVdecController14SetOmxExtParamEP19MMI_OmxParamCmdType
  ────└───> 0x00018c74      0060a0e1       mov r6, r0
   │││┌───< 0x00018c78      020000ea       b 0x18c88
  ─└└──└└─> 0x00018c7c      1262a0e3       mov r6, 0x20000001
     ││ ┌─< 0x00018c80      000000ea       b 0x18c88
  ────────> 0x00018c84      4262a0e3       mov r6, 0x20000004
  ───└└─└─> 0x00018c88      0600a0e1       mov r0, r6
            0x00018c8c      30d08de2       add sp, sp, 0x30
            0x00018c90      f081bde8       pop {r4, r5, r6, r7, r8, pc}
            0x00018c94      0a000006       streq r0, [r0], -sl
            0x00018c98      2cc80000       andeq ip, r0, ip, lsr 16
            0x00018c9c      17000001       invalid
            0x00018ca0      01000006       streq r0, [r0], -r1
            0x00018ca4      0d000006       streq r0, [r0], -sp
            0x00018ca8      0e000006       streq r0, [r0], -lr
            0x00018cac      01000007       streq r0, [r0, -r1]
            0x00018cb0      0400007f       svcvc 4
            0x00018cb4      b7d6ffff       invalid
            0x00018cb8      0caaffff       invalid
            0x00018cbc      e3d6ffff       invalid
            0x00018cc0      11000020       andhs r0, r0, r1, lsl r0
            0x00018cc4      18d7ffff       invalid
            0x00018cc8      0000106f       svcvs 0x100000
