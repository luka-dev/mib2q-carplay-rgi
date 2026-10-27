            0x000a7eb0      00d8           mov r0, r27
            0x000a7eb2      0132           mov 1, r6
            0x000a7eb4      053a           mov 5, r7
            0x000a7eb6      0342           mov 3, r8
            0x000a7eb8      80fffc0c       jarl 0xa8bb4, lp
            0x000a7ebc      035a           mov 3, r11
            0x000a7ebe      016a           mov 1, r13
            0x000a7ec0      440effff       movhi -1, gp, r1
            0x000a7ec4      213630cb       movea -13520, r1, r6
            0x000a7ec8      243ed1a1       movea -24111, gp, r7
            0x000a7ecc      380654890600   mov 0x68954, r24
            0x000a7ed2      20d69001       movea 400, r0, r26
        ┌─> 0x000a7ed6      0b70           mov r11, r14
        ╎   0x000a7ed8      0e960100       addi 1, r14, r18
        ╎   0x000a7edc      1258           mov r18, r11
        ╎   0x000a7ede      5459           and r20, r11
        ╎   0x000a7ee0      0e50           mov r14, r10
        ╎   0x000a7ee2      c252           shl 2, r10
        ╎   0x000a7ee4      dd51           add r29, r10
        ╎   0x000a7ee6      019a           mov 1, r19
        ╎   0x000a7ee8      6a9f0100       st.w r19, 0[r10]
        ╎   0x000a7eec      0b48           mov r11, r9
        ╎   0x000a7eee      097e0100       addi 1, r9, r15
        ╎   0x000a7ef2      0f58           mov r15, r11
        ╎   0x000a7ef4      5459           and r20, r11
        ╎   0x000a7ef6      1b40           mov r27, r8
        ╎   0x000a7ef8      088e0100       addi 1, r8, r17
        ╎   0x000a7efc      11d8           mov r17, r27
        ╎   0x000a7efe      54d9           and r20, r27
        ╎   0x000a7f00      0980           mov r9, r16
        ╎   0x000a7f02      c282           shl 2, r16
        ╎   0x000a7f04      dd81           add r29, r16
        ╎   0x000a7f06      0890           mov r8, r18
        ╎   0x000a7f08      c292           shl 2, r18
        ╎   0x000a7f0a      d891           add r24, r18
        ╎   0x000a7f0c      32570100       ld.w 0[r18], r10
        ╎   0x000a7f10      70570100       st.w r10, 0[r16]
        ╎   0x000a7f14      0798           mov r7, r19
        ╎   0x000a7f16      cd99           add r13, r19
        ╎   0x000a7f18      137f0000       ld.b 0[r19], r15
        ╎   0x000a7f1c      ea7f400a       mul 10, r15, r1
        ╎   0x000a7f20      848fd3c1       ld.bu -15918[gp], r17
        ╎   0x000a7f24      d179           add r17, r15
        ╎   0x000a7f26      0690           mov r6, r18
        ╎   0x000a7f28      cd91           add r13, r18
        ╎   0x000a7f2a      1287ffff       ld.b -1[r18], r16
        ╎   0x000a7f2e      d079           add r16, r15
        ╎   0x000a7f30      0f7eaeff       addi -82, r15, r15
        ╎   0x000a7f34      0f60           mov r15, r12
        ╎   0x000a7f36      ec00           sxh r12
        ╎   0x000a7f38      fa61           cmp r26, r12
       ┌──< 0x000a7f3a      c705           ble 0xa7f42
       │╎   0x000a7f3c      20669001       movea 400, r0, r12
      ┌───< 0x000a7f40      f505           br 0xa7f4e
      │└──> 0x000a7f42      200e18fc       movea -1000, r0, r1
      │ ╎   0x000a7f46      e161           cmp r1, r12
      │┌──< 0x000a7f48      be05           bge 0xa7f4e
      ││╎   0x000a7f4a      206618fc       movea -1000, r0, r12
      └└──> 0x000a7f4e      0b50           mov r11, r10
        ╎   0x000a7f50      c252           shl 2, r10
        ╎   0x000a7f52      dd51           add r29, r10
        ╎   0x000a7f54      0c98           mov r12, r19
        ╎   0x000a7f56      c29a           shl 2, r19
        ╎   0x000a7f58      3398           not r19, r19
        ╎   0x000a7f5a      419a           add 1, r19
        ╎   0x000a7f5c      d999           add r25, r19
        ╎   0x000a7f5e      338f4106       ld.w 1600[r19], r17         ; 0x640 ; 1600
        ╎   0x000a7f62      6a8f0100       st.w r17, 0[r10]
        ╎   0x000a7f66      0b960100       addi 1, r11, r18
        ╎   0x000a7f6a      1258           mov r18, r11
        ╎   0x000a7f6c      5459           and r20, r11
        ╎   0x000a7f6e      0d860100       addi 1, r13, r16
        ╎   0x000a7f72      1068           mov r16, r13
        ╎   0x000a7f74      5469           and r20, r13
        ╎   0x000a7f76      636a           cmp 3, r13
        └─< 0x000a7f78      f7ad           ble 0xa7ed6
            0x000a7f7a      80ff8a0d       jarl 0xa8d04, lp
            0x000a7f7e      80ffee0c       jarl 0xa8c6c, lp
            0x000a7f82      243628bb       movea -17624, gp, r6
            0x000a7f86      203e3400       movea 52, r0, r7            ; '4'
            0x000a7f8a      83ff4ab5       jarl 0xe34d4, lp
            0x000a7f8e      01e2           mov 1, r28
        ┌─> 0x000a7f90      20361000       movea 16, r0, r6
        ╎   0x000a7f94      053a           mov 5, r7
        ╎   0x000a7f96      0142           mov 1, r8
        ╎   0x000a7f98      80ff1c0c       jarl 0xa8bb4, lp
        ╎   0x000a7f9c      0372           mov 3, r14
        ╎   0x000a7f9e      207e1000       movea 16, r0, r15
        ╎   0x000a7fa2      647fcdbc       st.w r15, -17204[gp]
        ╎   0x000a7fa6      0432           mov 4, r6
        ╎   0x000a7fa8      056a           mov 5, r13
        ╎   0x000a7faa      1b58           mov r27, r11
        ╎   0x000a7fac      0b9e0100       addi 1, r11, r19
        ╎   0x000a7fb0      13d8           mov r19, r27
        ╎   0x000a7fb2      54d9           and r20, r27
        ╎   0x000a7fb4      0b50           mov r11, r10
        ╎   0x000a7fb6      c252           shl 2, r10
        ╎   0x000a7fb8      d851           add r24, r10
        ╎   0x000a7fba      2a8f0100       ld.w 0[r10], r17
        ╎   0x000a7fbe      648fd1bc       st.w r17, -17200[gp]
        ╎   0x000a7fc2      0060           mov r0, r12
       ┌──> 0x000a7fc4      440effff       movhi -1, gp, r1
       ╎╎   0x000a7fc8      219669d6       movea -10647, r1, r18
       ╎╎   0x000a7fcc      cc91           add r12, r18
       ╎╎   0x000a7fce      92870100       ld.bu 0[r18], r16
       ╎╎   0x000a7fd2      1038           mov r16, r7
       ╎╎   0x000a7fd4      0770           mov r7, r14
       ╎╎   0x000a7fd6      e6774402       mul 38, r14, r0
       ╎╎   0x000a7fda      440effff       movhi -1, gp, r1
       ╎╎   0x000a7fde      217ea4d1       movea -11868, r1, r15
       ╎╎   0x000a7fe2      ce79           add r14, r15
       ╎╎   0x000a7fe4      249ec8c1       movea -15928, gp, r19
       ╎╎   0x000a7fe8      dc99           add r28, r19
       ╎╎   0x000a7fea      13570000       ld.b 0[r19], r10
       ╎╎   0x000a7fee      c152           shl 1, r10
       ╎╎   0x000a7ff0      ca79           add r10, r15
       ╎╎   0x000a7ff2      2f8f1200       ld.h 18[r15], r17           ; 0x12
       ╎╎   0x000a7ff6      1130           mov r17, r6
       ╎╎   0x000a7ff8      440effff       movhi -1, gp, r1
       ╎╎   0x000a7ffc      219620d3       movea -11488, r1, r18
       ╎╎   0x000a8000      ce91           add r14, r18
       ╎╎   0x000a8002      2486ccc1       movea -15924, gp, r16
       ╎╎   0x000a8006      dc81           add r28, r16
       ╎╎   0x000a8008      109f0000       ld.b 0[r16], r19
       ╎╎   0x000a800c      c19a           shl 1, r19
       ╎╎   0x000a800e      d391           add r19, r18
       ╎╎   0x000a8010      32571200       ld.h 18[r18], r10           ; 0x12
       ╎╎   0x000a8014      0a58           mov r10, r11
       ╎╎   0x000a8016      c659           add r6, r11
       ╎╎   0x000a8018      eb00           sxh r11
       ╎╎   0x000a801a      633a           cmp 3, r7
      ┌───< 0x000a801c      ba15           bne 0xa8042
      │╎╎   0x000a801e      c4c7d0c1       tst1 0, -15920[gp]
     ┌────< 0x000a8022      e20d           be 0xa803e
     ││╎╎   0x000a8024      047fd1c1       ld.b -15919[gp], r15
     ││╎╎   0x000a8028      c17a           shl 1, r15
     ││╎╎   0x000a802a      440effff       movhi -1, gp, r1
     ││╎╎   0x000a802e      218e0ed5       movea -10994, r1, r17
     ││╎╎   0x000a8032      cf89           add r15, r17
     ││╎╎   0x000a8034      31871200       ld.h 18[r17], r16           ; 0x12
     ││╎╎   0x000a8038      d059           add r16, r11
     ││╎╎   0x000a803a      eb00           sxh r11
    ┌─────< 0x000a803c      b505           br 0xa8042
    │└────> 0x000a803e      205e18fc       movea -1000, r0, r11
    └─└───> 0x000a8042      fa59           cmp r26, r11
      ┌───< 0x000a8044      c705           ble 0xa804c
      │╎╎   0x000a8046      205e9001       movea 400, r0, r11
     ┌────< 0x000a804a      f505           br 0xa8058
     │└───> 0x000a804c      200e18fc       movea -1000, r0, r1
     │ ╎╎   0x000a8050      e159           cmp r1, r11
     │┌───< 0x000a8052      be05           bge 0xa8058
     ││╎╎   0x000a8054      205e18fc       movea -1000, r0, r11
     └└───> 0x000a8058      0d98           mov r13, r19
       ╎╎   0x000a805a      c29a           shl 2, r19
       ╎╎   0x000a805c      dd99           add r29, r19
       ╎╎   0x000a805e      0b90           mov r11, r18
       ╎╎   0x000a8060      c292           shl 2, r18
       ╎╎   0x000a8062      3290           not r18, r18
       ╎╎   0x000a8064      4192           add 1, r18
       ╎╎   0x000a8066      d991           add r25, r18
       ╎╎   0x000a8068      32574106       ld.w 1600[r18], r10         ; 0x640 ; 1600
       ╎╎   0x000a806c      73570100       st.w r10, 0[r19]
       ╎╎   0x000a8070      0d7e0100       addi 1, r13, r15
       ╎╎   0x000a8074      0f68           mov r15, r13
       ╎╎   0x000a8076      5469           and r20, r13
       ╎╎   0x000a8078      0c8e0100       addi 1, r12, r17
       ╎╎   0x000a807c      1160           mov r17, r12
       ╎╎   0x000a807e      5461           and r20, r12
       ╎╎   0x000a8080      6f62           cmp 15, r12
       └──< 0x000a8082      97a5           ble 0xa7fc4
        ╎   0x000a8084      80ff800c       jarl 0xa8d04, lp
        ╎   0x000a8088      80ffe40b       jarl 0xa8c6c, lp
        ╎   0x000a808c      243628bb       movea -17624, gp, r6
        ╎   0x000a8090      203e5800       movea 88, r0, r7            ; 'X'
        ╎   0x000a8094      83ff40b4       jarl 0xe34d4, lp
        ╎   0x000a8098      1c860100       addi 1, r28, r16
        ╎   0x000a809c      10e0           mov r16, r28
        ╎   0x000a809e      54e1           and r20, r28
        ╎   0x000a80a0      63e2           cmp 3, r28
       ┌──< 0x000a80a2      bf05           bgt 0xa80a8
       │└─< 0x000a80a4      bf07ecfe       jr 0xa7f90
