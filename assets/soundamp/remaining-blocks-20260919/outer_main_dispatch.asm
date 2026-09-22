0x00099774      80075805       jr 0x99ccc
0x00099778      63371100       st.w r6, 16[sp]                         ; 0x10
0x0009977c      23ef1100       ld.w 16[sp], r29                        ; 0x10
0x00099780      633f1500       st.w r7, 20[sp]                         ; 0x14
0x00099784      23cf1500       ld.w 20[sp], r25                        ; 0x14
0x00099788      99df0b00       ld.bu 10[r25], r27                      ; 0xa
0x0009978c      b9c70b00       ld.bu 11[r25], r24                      ; 0xb
0x00099790      f9bf0900       ld.hu 8[r25], r23
0x00099794      19570c00       ld.b 12[r25], r10                       ; 0xc
0x00099798      43570d00       st.b r10, 13[sp]                        ; 0xd
0x0009979c      397f0600       ld.h 6[r25], r15
0x000997a0      7d7f0600       st.h r15, 6[r29]
0x000997a4      5ddf0a00       st.b r27, 10[r29]                       ; 0xa
0x000997a8      7dbf0800       st.h r23, 8[r29]
0x000997ac      61da           cmp 1, r27
0x000997ae      fa05           bne 0x997bc
0x000997b0      84ff54d2       jarl 0xe6a04, lp
0x000997b4      0a58           mov r10, r11
0x000997b6      5d5f0b00       st.b r11, 11[r29]                       ; 0xb
0x000997ba      b505           br 0x997c0
0x000997bc      5dc70b00       st.b r24, 11[r29]                       ; 0xb
0x000997c0      0058           mov r0, r11
0x000997c2      00d0           mov r0, r26
0x000997c4      43070300       st.b r0, 3[sp]
0x000997c8      0162           mov 1, r12
0x000997ca      f4d9           cmp r20, r27
0x000997cc      b205           be 0x997d2
0x000997ce      f4c1           cmp r20, r24
0x000997d0      aa0d           bne 0x997e4
0x000997d2      2487e9a0       ld.w -24344[gp], r16
0x000997d6      e081           cmp r0, r16
0x000997d8      e205           be 0x997e4
0x000997da      24e7e9a0       ld.w -24344[gp], r28
0x000997de      846feda0       ld.bu -24340[gp], r13
0x000997e2      d50d           br 0x997fc
0x000997e4      3c06f8010400   mov 0x401f8, r28
0x000997ea      038f0d00       ld.b 13[sp], r17                        ; 0xd
0x000997ee      698a           cmp 9, r17
0x000997f0      e9970000       setf nc/nl, r18
0x000997f4      e6974002       mul 6, r18, r0
0x000997f8      126e0600       addi 6, r18, r13
0x000997fc      e069           cmp r0, r13
0x000997fe      b24d           be 0x99894
0x00099800      9c770100       ld.bu 0[r28], r14
0x00099804      f4d9           cmp r20, r27
0x00099806      fa05           bne 0x99814
0x00099808      6172           cmp 1, r14
0x0009980a      d205           be 0x99814
0x0009980c      200ef000       movea 240, r0, r1
0x00099810      e171           cmp r1, r14
0x00099812      b605           blt 0x99818
0x00099814      eed9           cmp r14, r27
0x00099816      ca35           bne 0x9987e
0x00099818      5d770a00       st.b r14, 10[r29]                       ; 0xa
0x0009981c      e0c1           cmp r0, r24
0x0009981e      da0d           bne 0x99838
0x00099820      3c9f0500       ld.w 4[r28], r19                        ; 4
0x00099824      e099           cmp r0, r19
0x00099826      f205           be 0x99834
0x00099828      3c7f0500       ld.w 4[r28], r15                        ; 4
0x0009982c      0f570000       ld.b 0[r15], r10
0x00099830      5d570b00       st.b r10, 11[r29]                       ; 0xb
0x00099834      0060           mov r0, r12
0x00099836      c525           br 0x9987e
0x00099838      f4c1           cmp r20, r24
0x0009983a      ca15           bne 0x99862
0x0009983c      3c870500       ld.w 4[r28], r16                        ; 4
0x00099840      e081           cmp r0, r16
0x00099842      820d           be 0x99852
0x00099844      3c8f0500       ld.w 4[r28], r17                        ; 4
0x00099848      11970000       ld.b 0[r17], r18
0x0009984c      5d970b00       st.b r18, 11[r29]                       ; 0xb
0x00099850      f505           br 0x9985e
0x00099852      1c9f0000       ld.b 0[r28], r19
0x00099856      619a           cmp 1, r19
0x00099858      b205           be 0x9985e
0x0009985a      5d070b00       st.b r0, 11[r29]                        ; 0xb
0x0009985e      0060           mov r0, r12
0x00099860      f50d           br 0x9987e
0x00099862      3c770500       ld.w 4[r28], r14                        ; 4
0x00099866      e071           cmp r0, r14
0x00099868      a20d           be 0x9987c
0x0009986a      8e7f0100       ld.bu 0[r14], r15
0x0009986e      0f30           mov r15, r6
0x00099870      f831           cmp r24, r6
0x00099872      ea570000       setf nz, r10
0x00099876      c152           shl 1, r10
0x00099878      0a60           mov r10, r12
0x0009987a      a505           br 0x9987e
0x0009987c      0060           mov r0, r12
0x0009987e      e061           cmp r0, r12
0x00099880      e205           be 0x9988c
0x00099882      4ce2           add 12, r28
0x00099884      0d86ffff       addi -1, r13, r16
0x00099888      1068           mov r16, r13
0x0009988a      5469           and r20, r13
0x0009988c      e061           cmp r0, r12
0x0009988e      b205           be 0x99894
0x00099890      e069           cmp r0, r13
0x00099892      fab5           bne 0x99800
0x00099894      f4c1           cmp r20, r24
0x00099896      b205           be 0x9989c
0x00099898      f4d9           cmp r20, r27
0x0009989a      ba15           bne 0x998c0
0x0009989c      e069           cmp r0, r13
0x0009989e      aa0d           bne 0x998b2
0x000998a0      4407eca0       st.b r0, -24340[gp]
0x000998a4      248fe9a0       ld.w -24344[gp], r17
0x000998a8      e089           cmp r0, r17
0x000998aa      b205           be 0x998b0
0x000998ac      6407e9a0       st.w r0, -24344[gp]
0x000998b0      850d           br 0x998c0
0x000998b2      64e7e9a0       st.w r28, -24344[gp]
0x000998b6      446feca0       st.b r13, -24340[gp]
0x000998ba      0192           mov 1, r18
0x000998bc      43970300       st.b r18, 3[sp]
0x000998c0      e061           cmp r0, r12
0x000998c2      f21d           be 0x99900
0x000998c4      039f0d00       ld.b 13[sp], r19                        ; 0xd
0x000998c8      699a           cmp 9, r19
0x000998ca      d115           bl 0x998f4
0x000998cc      1d30           mov r29, r6
0x000998ce      1938           mov r25, r7
0x000998d0      83ff842d       jarl 0xcc654, lp
0x000998d4      0a58           mov r10, r11
0x000998d6      cb7ea000       andi 160, r11, r15
0x000998da      5479           and r20, r15
0x000998dc      0fd1           or r15, r26
0x000998de      cb5e5f00       andi 95, r11, r11
0x000998e2      e059           cmp r0, r11
0x000998e4      fa05           bne 0x998f2
0x000998e6      1d30           mov r29, r6
0x000998e8      bfff18ef       jarl 0x98800, lp
0x000998ec      1a58           mov r26, r11
0x000998ee      8007d803       jr 0x99cc6
0x000998f2      f505           br 0x99900
0x000998f4      1d30           mov r29, r6
0x000998f6      0c38           mov r12, r7
0x000998f8      80fff004       jarl 0x99de8, lp
0x000998fc      0a58           mov r10, r11
0x000998fe      0f5a           mov 15, r11
0x00099900      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x00099904      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099908      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x0009990c      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x00099910      e059           cmp r0, r11
0x00099912      b205           be 0x99918
0x00099914      8007d802       jr 0x99bec
0x00099918      3cb70900       ld.w 8[r28], r22
0x0009991c      f6e70100       ld.hu 0[r22], r28
0x00099920      48b2           add 8, r22
0x00099922      f5e1           cmp r21, r28
0x00099924      fa05           bne 0x99932
0x00099926      1d30           mov r29, r6
0x00099928      033a           mov 3, r7
0x0009992a      80ffbe04       jarl 0x99de8, lp
0x0009992e      0a58           mov r10, r11
0x00099930      0f5a           mov 15, r11
0x00099932      fcb9           cmp r28, r23
0x00099934      b205           be 0x9993a
0x00099936      e059           cmp r0, r11
0x00099938      a2f5           be 0x9991c
0x0009993a      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x0009993e      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099942      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x00099946      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x0009994a      e059           cmp r0, r11
0x0009994c      b205           be 0x99952
0x0009994e      80079e02       jr 0x99bec
0x00099952      58b2           add -8, r22
0x00099954      36e70500       ld.w 4[r22], r28                        ; 4
0x00099958      9cbf0100       ld.bu 0[r28], r23
0x0009995c      4ce2           add 12, r28
0x0009995e      f4b9           cmp r20, r23
0x00099960      9a0d           bne 0x99972
0x00099962      1d30           mov r29, r6
0x00099964      043a           mov 4, r7
0x00099966      a3470d00       ld.bu 13[sp], r8                        ; 0xd
0x0009996a      80ffaa04       jarl 0x99e14, lp
0x0009996e      0a58           mov r10, r11
0x00099970      0f5a           mov 15, r11
0x00099972      a3870d00       ld.bu 13[sp], r16                       ; 0xd
0x00099976      f781           cmp r23, r16
0x00099978      b205           be 0x9997e
0x0009997a      e059           cmp r0, r11
0x0009997c      e2ed           be 0x99958
0x0009997e      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x00099982      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099986      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x0009998a      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x0009998e      e059           cmp r0, r11
0x00099990      b205           be 0x99996
0x00099992      80075a02       jr 0x99bec
0x00099996      54e2           add -12, r28
0x00099998      bcbf0100       ld.bu 1[r28], r23
0x0009999c      d78ef000       andi 240, r23, r17
0x000999a0      e089           cmp r0, r17
0x000999a2      ba05           bne 0x999a8
0x000999a4      80070601       jr 0x99aaa
0x000999a8      3c6f0500       ld.w 4[r28], r13                        ; 4
0x000999ac      43070500       st.b r0, 5[sp]
0x000999b0      d766f000       andi 240, r23, r12
0x000999b4      e555           br 0x99a60
0x000999b6      1d30           mov r29, r6
0x000999b8      1938           mov r25, r7
0x000999ba      450e0a00       movhi 10, tp, r1
0x000999be      21fec499       movea -26172, r1, lp
0x000999c2      6d00           jmp [r13]
0x000999c4      0a58           mov r10, r11
0x000999c6      43bf0700       st.b r23, 7[sp]
0x000999ca      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x000999ce      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x000999d2      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x000999d6      63e70900       st.w r28, 8[sp]
0x000999da      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x000999de      d55d           br 0x99a98
0x000999e0      450e0a00       movhi 10, tp, r1
0x000999e4      21feea99       movea -26134, r1, lp
0x000999e8      6d00           jmp [r13]
0x000999ea      0a58           mov r10, r11
0x000999ec      43bf0700       st.b r23, 7[sp]
0x000999f0      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x000999f4      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x000999f8      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x000999fc      63e70900       st.w r28, 8[sp]
0x00099a00      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x00099a04      a54d           br 0x99a98
0x00099a06      1930           mov r25, r6
0x00099a08      450e0a00       movhi 10, tp, r1
0x00099a0c      21fe129a       movea -26094, r1, lp
0x00099a10      6d00           jmp [r13]
0x00099a12      0a58           mov r10, r11
0x00099a14      43bf0700       st.b r23, 7[sp]
0x00099a18      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x00099a1c      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099a20      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x00099a24      63e70900       st.w r28, 8[sp]
0x00099a28      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x00099a2c      e535           br 0x99a98
0x00099a2e      1d30           mov r29, r6
0x00099a30      450e0a00       movhi 10, tp, r1
0x00099a34      21fe3a9a       movea -26054, r1, lp
0x00099a38      6d00           jmp [r13]
0x00099a3a      0a58           mov r10, r11
0x00099a3c      43bf0700       st.b r23, 7[sp]
0x00099a40      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x00099a44      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099a48      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x00099a4c      63e70900       st.w r28, 8[sp]
0x00099a50      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x00099a54      a525           br 0x99a98
0x00099a56      835f0500       ld.bu 4[sp], r11                        ; 4
0x00099a5a      83d70700       ld.bu 6[sp], r26
0x00099a5e      d51d           br 0x99a98
0x00099a60      200e1000       movea 16, r0, r1
0x00099a64      e161           cmp r1, r12
0x00099a66      d2bd           be 0x999e0
0x00099a68      200e5000       movea 80, r0, r1                        ; 'P'
0x00099a6c      e161           cmp r1, r12
0x00099a6e      c2cd           be 0x99a06
0x00099a70      200e9000       movea 144, r0, r1
0x00099a74      e161           cmp r1, r12
0x00099a76      c2dd           be 0x99a2e
0x00099a78      200ed000       movea 208, r0, r1
0x00099a7c      e161           cmp r1, r12
0x00099a7e      c29d           be 0x999b6
0x00099a80      43bf0700       st.b r23, 7[sp]
0x00099a84      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x00099a88      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099a8c      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x00099a90      63e70900       st.w r28, 8[sp]
0x00099a94      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x00099a98      cb96a000       andi 160, r11, r18
0x00099a9c      5491           and r20, r18
0x00099a9e      12d1           or r18, r26
0x00099aa0      cb5e5f00       andi 95, r11, r11
0x00099aa4      a3bf0700       ld.bu 7[sp], r23
0x00099aa8      e50d           br 0x99ac4
0x00099aaa      019a           mov 1, r19
0x00099aac      439f0500       st.b r19, 5[sp]
0x00099ab0      43c70e00       st.b r24, 14[sp]                        ; 0xe
0x00099ab4      63cf1500       st.w r25, 20[sp]                        ; 0x14
0x00099ab8      43df0f00       st.b r27, 15[sp]                        ; 0xf
0x00099abc      63e70900       st.w r28, 8[sp]
0x00099ac0      63ef1100       st.w r29, 16[sp]                        ; 0x10
0x00099ac4      200ef000       movea 240, r0, r1
0x00099ac8      e1b9           cmp r1, r23
0x00099aca      b605           blt 0x99ad0
0x00099acc      80072001       jr 0x99bec
0x00099ad0      e059           cmp r0, r11
0x00099ad2      b205           be 0x99ad8
0x00099ad4      80071801       jr 0x99bec
0x00099ad8      d77e0f00       andi 15, r23, r15
0x00099adc      e079           cmp r0, r15
0x00099ade      ba05           bne 0x99ae4
0x00099ae0      80070c01       jr 0x99bec
0x00099ae4      03870500       ld.b 5[sp], r16
0x00099ae8      e081           cmp r0, r16
0x00099aea      9215           be 0x99b0c
0x00099aec      d78e0f00       andi 15, r23, r17
0x00099af0      628a           cmp 2, r17
0x00099af2      d20d           be 0x99b0c
0x00099af4      23571500       ld.w 20[sp], r10                        ; 0x14
0x00099af8      2a970400       ld.h 4[r10], r18                        ; 4
0x00099afc      e091           cmp r0, r18
0x00099afe      f205           be 0x99b0c
0x00099b00      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099b04      053a           mov 5, r7
0x00099b06      80ffe202       jarl 0x99de8, lp
0x00099b0a      0a58           mov r10, r11
0x00099b0c      e059           cmp r0, r11
0x00099b0e      fa6d           bne 0x99bec
0x00099b10      239f0900       ld.w 8[sp], r19
0x00099b14      336f0900       ld.w 8[r19], r13
0x00099b18      d7660f00       andi 15, r23, r12
0x00099b1c      d555           br 0x99bc6
0x00099b1e      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099b22      450e0a00       movhi 10, tp, r1
0x00099b26      21fe2c9b       movea -25812, r1, lp
0x00099b2a      6d00           jmp [r13]
0x00099b2c      0a58           mov r10, r11
0x00099b2e      955d           br 0x99be0
0x00099b30      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099b34      233f1500       ld.w 20[sp], r7                         ; 0x14
0x00099b38      450e0a00       movhi 10, tp, r1
0x00099b3c      21fe429b       movea -25790, r1, lp
0x00099b40      6d00           jmp [r13]
0x00099b42      0a58           mov r10, r11
0x00099b44      e54d           br 0x99be0
0x00099b46      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099b4a      267f0100       ld.w 0[r6], r15
0x00099b4e      0d870000       ld.b 0[r13], r16
0x00099b52      4f870000       st.b r16, 0[r15]
0x00099b56      018a           mov 1, r17
0x00099b58      668f0400       st.h r17, 4[r6]                         ; 4
0x00099b5c      0c5a           mov 12, r11
0x00099b5e      9545           br 0x99be0
0x00099b60      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099b64      26970100       ld.w 0[r6], r18
0x00099b68      0d9f0000       ld.b 0[r13], r19
0x00099b6c      529f0000       st.b r19, 0[r18]
0x00099b70      0152           mov 1, r10
0x00099b72      66570400       st.h r10, 4[r6]                         ; 4
0x00099b76      0c5a           mov 12, r11
0x00099b78      c535           br 0x99be0
0x00099b7a      23ef1100       ld.w 16[sp], r29                        ; 0x10
0x00099b7e      3d7f0100       ld.w 0[r29], r15
0x00099b82      0f30           mov r15, r6
0x00099b84      0d38           mov r13, r7
0x00099b86      80ff4e01       jarl 0x99cd4, lp
0x00099b8a      0282           mov 2, r16
0x00099b8c      7d870400       st.h r16, 4[r29]                        ; 4
0x00099b90      0c5a           mov 12, r11
0x00099b92      f525           br 0x99be0
0x00099b94      23ef1100       ld.w 16[sp], r29                        ; 0x10
0x00099b98      3d8f0100       ld.w 0[r29], r17
0x00099b9c      1130           mov r17, r6
0x00099b9e      0d38           mov r13, r7
0x00099ba0      80ffa401       jarl 0x99d44, lp
0x00099ba4      0492           mov 4, r18
0x00099ba6      7d970400       st.h r18, 4[r29]                        ; 4
0x00099baa      0c5a           mov 12, r11
0x00099bac      a51d           br 0x99be0
0x00099bae      23371100       ld.w 16[sp], r6                         ; 0x10
0x00099bb2      0d38           mov r13, r7
0x00099bb4      80ff0404       jarl 0x99fb8, lp
0x00099bb8      0e5a           mov 14, r11
0x00099bba      b515           br 0x99be0
0x00099bbc      835f0500       ld.bu 4[sp], r11                        ; 4
0x00099bc0      83d70700       ld.bu 6[sp], r26
0x00099bc4      e50d           br 0x99be0
0x00099bc6      0c98           mov r12, r19
0x00099bc8      010a           mov 1, r1
0x00099bca      a199           sub r1, r19
0x00099bcc      669a           cmp 6, r19
0x00099bce      9b0d           bh 0x99be0
0x00099bd0      5300           switch r19
