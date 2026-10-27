0x000a2874      80071a02       jr 0xa2a8e
0x000a2878      63370500       st.w r6, 4[sp]                          ; 4
0x000a287c      23c70500       ld.w 4[sp], r24                         ; 4
0x000a2880      18571200       ld.b 18[r24], r10                       ; 0x12
0x000a2884      0a564000       addi 64, r10, r10
0x000a2888      247f2da2       ld.w -24020[gp], r15
0x000a288c      4f571200       st.b r10, 18[r15]                       ; 0x12
0x000a2890      20661200       movea 18, r0, r12
0x000a2894      98cf1300       ld.bu 18[r24], r25                      ; 0x12
0x000a2898      18871300       ld.b 19[r24], r16                       ; 0x13
0x000a289c      200e7fff       movea -129, r0, r1
0x000a28a0      4181           and r1, r16
0x000a28a2      10d0           mov r16, r26
0x000a28a4      54d1           and r20, r26
0x000a28a6      205e1300       movea 19, r0, r11
0x000a28aa      248f2da2       ld.w -24020[gp], r17
0x000a28ae      51d71300       st.b r26, 19[r17]                       ; 0x13
0x000a28b2      18971400       ld.b 20[r24], r18                       ; 0x14
0x000a28b6      249f2da2       ld.w -24020[gp], r19
0x000a28ba      53971400       st.b r18, 20[r19]                       ; 0x14
0x000a28be      987f1500       ld.bu 20[r24], r15                      ; 0x14
0x000a28c2      c87a           shl 8, r15
0x000a28c4      0fd8           mov r15, r27
0x000a28c6      20e61500       movea 21, r0, r28
0x000a28ca      18571500       ld.b 21[r24], r10                       ; 0x15
0x000a28ce      24872da2       ld.w -24020[gp], r16
0x000a28d2      50571500       st.b r10, 21[r16]                       ; 0x15
0x000a28d6      b88f1500       ld.bu 21[r24], r17                      ; 0x15
0x000a28da      d1d9           add r17, r27
0x000a28dc      55d9           and r21, r27
0x000a28de      989f0700       ld.bu 6[r24], r19
0x000a28e2      c89a           shl 8, r19
0x000a28e4      5599           and r21, r19
0x000a28e6      b8970700       ld.bu 7[r24], r18
0x000a28ea      d299           add r18, r19
0x000a28ec      13e8           mov r19, r29
0x000a28ee      55e9           and r21, r29
0x000a28f0      0532           mov 5, r6
0x000a28f2      203e4b00       movea 75, r0, r7                        ; 'K'
0x000a28f6      1b40           mov r27, r8
0x000a28f8      844f35ba       ld.bu -17868[gp], r9
0x000a28fc      84fff479       jarl 0xea2f0, lp
0x000a2900      047f34ba       ld.b -17868[gp], r15
0x000a2904      617a           cmp 1, r15
0x000a2906      8a0d           bne 0xa2916
0x000a2908      1930           mov r25, r6
0x000a290a      203e7f00       movea 127, r0, r7                       ; '\x7f'
0x000a290e      bfff56dd       jarl 0xa0664, lp
0x000a2912      80077801       jr 0xa2a8a
0x000a2916      6eea           cmp 14, r29
0x000a2918      8e0d           bge 0xa2928
0x000a291a      1930           mov r25, r6
0x000a291c      203e1300       movea 19, r0, r7
0x000a2920      bfff44dd       jarl 0xa0664, lp
0x000a2924      80076601       jr 0xa2a8a
0x000a2928      2b0618a00000   mov 0xa018, r11
0x000a292e      ebd9           cmp r11, r27
0x000a2930      c215           be 0xa2958
0x000a2932      200e0003       movea 768, r0, r1
0x000a2936      e1d9           cmp r1, r27
0x000a2938      8215           be 0xa2958
0x000a293a      200eef02       movea 751, r0, r1
0x000a293e      e1d9           cmp r1, r27
0x000a2940      c20d           be 0xa2958
0x000a2942      200ee703       movea 999, r0, r1
0x000a2946      e1d9           cmp r1, r27
0x000a2948      820d           be 0xa2958
0x000a294a      1930           mov r25, r6
0x000a294c      203e3100       movea 49, r0, r7                        ; '1'
0x000a2950      bfff14dd       jarl 0xa0664, lp
0x000a2954      80073601       jr 0xa2a8a
0x000a2958      61d2           cmp 1, r26
0x000a295a      b605           blt 0xa2960
0x000a295c      63d2           cmp 3, r26
0x000a295e      870d           ble 0xa296e
0x000a2960      1930           mov r25, r6
0x000a2962      203e1200       movea 18, r0, r7
0x000a2966      bffffedc       jarl 0xa0664, lp
0x000a296a      80072001       jr 0xa2a8a
0x000a296e      ebd9           cmp r11, r27
0x000a2970      9a5d           bne 0xa2a22
0x000a2972      1c860100       addi 1, r28, r16
0x000a2976      10e0           mov r16, r28
0x000a2978      fc00           sxh r28
0x000a297a      1850           mov r24, r10
0x000a297c      dc51           add r28, r10
0x000a297e      8a770100       ld.bu 0[r10], r14
0x000a2982      0e68           mov r14, r13
0x000a2984      c86a           shl 8, r13
0x000a2986      248f2da2       ld.w -24020[gp], r17
0x000a298a      dc89           add r28, r17
0x000a298c      51770000       st.b r14, 0[r17]
0x000a2990      1c960100       addi 1, r28, r18
0x000a2994      12e0           mov r18, r28
0x000a2996      fc00           sxh r28
0x000a2998      1898           mov r24, r19
0x000a299a      dc99           add r28, r19
0x000a299c      93670100       ld.bu 0[r19], r12
0x000a29a0      cc69           add r12, r13
0x000a29a2      5569           and r21, r13
0x000a29a4      247f2da2       ld.w -24020[gp], r15
0x000a29a8      dc79           add r28, r15
0x000a29aa      4f670000       st.b r12, 0[r15]
0x000a29ae      1c860100       addi 1, r28, r16
0x000a29b2      10e0           mov r16, r28
0x000a29b4      fc00           sxh r28
0x000a29b6      1a58           mov r26, r11
0x000a29b8      b525           br 0xa29fe
0x000a29ba      0d30           mov r13, r6
0x000a29bc      0038           mov r0, r7
0x000a29be      1840           mov r24, r8
0x000a29c0      24572da2       ld.w -24020[gp], r10
0x000a29c4      dc51           add r28, r10
0x000a29c6      0a48           mov r10, r9
0x000a29c8      83ff002e       jarl 0xd57c8, lp
0x000a29cc      0a58           mov r10, r11
0x000a29ce      cbe1           add r11, r28
0x000a29d0      fc00           sxh r28
0x000a29d2      b51d           br 0xa2a08
0x000a29d4      0d30           mov r13, r6
0x000a29d6      013a           mov 1, r7
0x000a29d8      1840           mov r24, r8
0x000a29da      248f2da2       ld.w -24020[gp], r17
0x000a29de      dc89           add r28, r17
0x000a29e0      1148           mov r17, r9
0x000a29e2      83ffe62d       jarl 0xd57c8, lp
0x000a29e6      0a58           mov r10, r11
0x000a29e8      cbe1           add r11, r28
0x000a29ea      fc00           sxh r28
0x000a29ec      e50d           br 0xa2a08
0x000a29ee      1c96feff       addi -2, r28, r18
0x000a29f2      12e0           mov r18, r28
0x000a29f4      fc00           sxh r28
0x000a29f6      950d           br 0xa2a08
0x000a29f8      23e70200       ld.h 2[sp], r28
0x000a29fc      e505           br 0xa2a08
0x000a29fe      615a           cmp 1, r11
0x000a2a00      d2dd           be 0xa29ba
0x000a2a02      635a           cmp 3, r11
0x000a2a04      82ed           be 0xa29d4
0x000a2a06      c5f5           br 0xa29ee
0x000a2a08      e0e1           cmp r0, r28
0x000a2a0a      ae05           bge 0xa2a0e
0x000a2a0c      00e0           mov r0, r28
0x000a2a0e      200e0810       movea 4104, r0, r1
0x000a2a12      e1e1           cmp r1, r28
0x000a2a14      b705           ble 0xa2a1a
0x000a2a16      20e60810       movea 4104, r0, r28
0x000a2a1a      1c30           mov r28, r6
0x000a2a1c      bfff98dc       jarl 0xa06b4, lp
0x000a2a20      d535           br 0xa2a8a
0x000a2a22      61d2           cmp 1, r26
0x000a2a24      a20d           be 0xa2a38
0x000a2a26      6eea           cmp 14, r29
0x000a2a28      f205           be 0xa2a36
0x000a2a2a      1930           mov r25, r6
0x000a2a2c      203e1300       movea 19, r0, r7
0x000a2a30      bfff34dc       jarl 0xa0664, lp
0x000a2a34      b52d           br 0xa2a8a
0x000a2a36      d50d           br 0xa2a50
0x000a2a38      1d9ef2ff       addi -14, r29, r19
0x000a2a3c      13e8           mov r19, r29
0x000a2a3e      55e9           and r21, r29
0x000a2a40      63ea           cmp 3, r29
0x000a2a42      f205           be 0xa2a50
0x000a2a44      1930           mov r25, r6
0x000a2a46      203e1300       movea 19, r0, r7
0x000a2a4a      bfff1adc       jarl 0xa0664, lp
0x000a2a4e      e51d           br 0xa2a8a
0x000a2a50      1b58           mov r27, r11
0x000a2a52      f50d           br 0xa2a70
0x000a2a54      1830           mov r24, r6
0x000a2a56      80ff4200       jarl 0xa2a98, lp
0x000a2a5a      851d           br 0xa2a8a
0x000a2a5c      1830           mov r24, r6
0x000a2a5e      80ff5201       jarl 0xa2bb0, lp
0x000a2a62      c515           br 0xa2a8a
0x000a2a64      1830           mov r24, r6
0x000a2a66      80ffba02       jarl 0xa2d20, lp
0x000a2a6a      8515           br 0xa2a8a
0x000a2a6c      f50d           br 0xa2a8a
0x000a2a6e      e50d           br 0xa2a8a
0x000a2a70      200eef02       movea 751, r0, r1
0x000a2a74      e159           cmp r1, r11
0x000a2a76      b2f5           be 0xa2a5c
0x000a2a78      200e0003       movea 768, r0, r1
0x000a2a7c      e159           cmp r1, r11
0x000a2a7e      b2ed           be 0xa2a54
0x000a2a80      200ee703       movea 999, r0, r1
0x000a2a84      e159           cmp r1, r11
0x000a2a86      f2ed           be 0xa2a64
0x000a2a88      a5f5           br 0xa2a6c
0x000a2a8a      4406fff0       dispose 2, {r24, r25, r26, r27, r28, r29, lp}, lp
0x000a2a8e      8407e1f0       prepare {r24, r25, r26, r27, r28, r29, lp}, 2
0x000a2a92      bf07e6fd       jr 0xa2878
0x000a2a96      0000           nop
