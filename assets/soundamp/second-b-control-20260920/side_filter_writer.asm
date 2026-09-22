            0x000ab73c      035a           mov 3, r11
            0x000ab73e      435f0b00       st.b r11, 11[sp]            ; 0xb
            0x000ab742      63070200       st.h r0, 2[sp]
            0x000ab746      0752           mov 7, r10
            0x000ab748      43570400       st.b r10, 4[sp]             ; 4
            0x000ab74c      435f0500       st.b r11, 5[sp]
            0x000ab750      8467a3c2       ld.bu -15710[gp], r12
        ┌─< 0x000ab754      b515           br 0xab77a
       ┌──> 0x000ab756      20768815       movea 5512, r0, r14
       ╎│   0x000ab75a      63770000       st.h r14, 0[sp]
      ┌───< 0x000ab75e      b515           br 0xab784
     ┌────> 0x000ab760      207e112b       movea 11025, r0, r15        ; '\x11+'
     ╎│╎│   0x000ab764      637f0000       st.h r15, 0[sp]
    ┌─────< 0x000ab768      e50d           br 0xab784
   ┌──────> 0x000ab76a      2086e803       movea 1000, r0, r16
   ╎│╎│╎│   0x000ab76e      63870000       st.h r16, 0[sp]
   ╎│╎│╎│   0x000ab772      43070500       st.b r0, 5[sp]
  ┌───────< 0x000ab776      f505           br 0xab784
  ────────< 0x000ab778      e505           br 0xab784
  │╎│╎│╎└─> 0x000ab77a      6162           cmp 1, r12
  │╎│╎│└──< 0x000ab77c      d2ed           be 0xab756
  │╎│╎│     0x000ab77e      6262           cmp 2, r12
  │╎│└────< 0x000ab780      82f5           be 0xab760
  │└──────< 0x000ab782      c5f5           br 0xab76a
  └─└─└───> 0x000ab784      0532           mov 5, r6
            0x000ab786      053a           mov 5, r7
            0x000ab788      0142           mov 1, r8
            0x000ab78a      bfff2ad4       jarl 0xa8bb4, lp
            0x000ab78e      245ec0bc       movea -17216, gp, r11
            0x000ab792      a3ef0b00       ld.bu 11[sp], r29           ; 0xb
            0x000ab796      1d68           mov r29, r13
            0x000ab798      0d8e0100       addi 1, r13, r17
            0x000ab79c      11e8           mov r17, r29
            0x000ab79e      54e9           and r20, r29
            0x000ab7a0      0d90           mov r13, r18
            0x000ab7a2      c292           shl 2, r18
            0x000ab7a4      cb91           add r11, r18
            0x000ab7a6      059a           mov 5, r19
            0x000ab7a8      729f0100       st.w r19, 0[r18]
            0x000ab7ac      1d60           mov r29, r12
            0x000ab7ae      0c560100       addi 1, r12, r10
            0x000ab7b2      0ae8           mov r10, r29
            0x000ab7b4      54e9           and r20, r29
            0x000ab7b6      0c70           mov r12, r14
            0x000ab7b8      c272           shl 2, r14
            0x000ab7ba      cb71           add r11, r14
            0x000ab7bc      2f0656060e00   mov 0xe0656, r15
            0x000ab7c2      6e7f0100       st.w r15, 0[r14]
            0x000ab7c6      23360000       movea 0, sp, r6
            0x000ab7ca      bffffade       jarl 0xa96c4, lp
            0x000ab7ce      1d30           mov r29, r6
            0x000ab7d0      80ffa807       jarl 0xabf78, lp
            0x000ab7d4      0ae8           mov r10, r29
            0x000ab7d6      bfff2ed5       jarl 0xa8d04, lp
            0x000ab7da      bfff92d4       jarl 0xa8c6c, lp
            0x000ab7de      243628bb       movea -17624, gp, r6
            0x000ab7e2      203e2c00       movea 44, r0, r7            ; ','
            0x000ab7e6      83ffee7c       jarl 0xe34d4, lp
