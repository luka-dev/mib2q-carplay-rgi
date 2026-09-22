0x000a8904      mov r0, r11
0x000a8906      mov r0, r27
0x000a8908      movhi 16256, r0, r22
0x000a890c      movea 8, sp, r24
0x000a8910      mov r27, r10
0x000a8912      shl 2, r10
0x000a8914      add r24, r10
0x000a8916      st.w r22, 0[r10]
0x000a891a      mov r0, r28
0x000a891c      mov 4, r6
0x000a891e      mov 5, r7
0x000a8920      mov 1, r8
0x000a8922      jarl 0xa8bb4, lp
0x000a8926      movea 0, sp, r23
0x000a892a      mov 3, r12
0x000a892c      mov 4, r15
0x000a892e      st.w r15, -17204[gp]
0x000a8932      mov 4, r11
0x000a8934      mov 5, r29
0x000a8936      mov r27, r16
0x000a8938      mul 12, r16, r0
0x000a893c      mov r28, r17
0x000a893e      shl 2, r17
0x000a8940      add r17, r16
0x000a8942      mov 0xc016a, r1
0x000a8948      add r1, r16
0x000a894a      st.w r16, -17200[gp]
0x000a894e      mov r28, r18
0x000a8950      shl 3, r18
0x000a8952      movhi -1, gp, r1
0x000a8956      movea -10524, r1, r19
0x000a895a      add r18, r19
0x000a895c      mov r23, r10
0x000a895e      ld.w 0[r19], r15
0x000a8962      st.w r15, 0[r10]
0x000a8966      ld.w 4[r19], r17                                       ; 4
0x000a896a      st.w r17, 4[r10]                                       ; 4
0x000a896e      mov r27, r16
0x000a8970      mul 6, r16, r0
0x000a8974      movhi -1, gp, r1
0x000a8978      movea -10500, r1, r18
0x000a897c      add r16, r18
0x000a897e      mov r28, r15
0x000a8980      shl 1, r15
0x000a8982      add r15, r18
0x000a8984      ld.h 0[r18], r17
0x000a8988      st.h r17, 2[sp]
0x000a898c      ld.b 4[sp], r10                                        ; 4
0x000a8990      cmp r0, r10
0x000a8992      bne 0xa899a
0x000a8994      mov 1, r19
0x000a8996      st.b r19, 4[sp]                                        ; 4
0x000a899a      mov r23, r6
0x000a899c      jarl 0xa96c4, lp
0x000a89a0      movea -17216, gp, r26
0x000a89a4      mov r27, r16
0x000a89a6      shl 2, r16
0x000a89a8      add r24, r16
0x000a89aa      mov r16, r11
0x000a89ac      ld.w -17240[gp], r15
0x000a89b0      ld.w -17228[gp], r18
0x000a89b4      mov r18, r6
0x000a89b6      mov r15, r7
0x000a89b8      jarl 0xeb984, lp
0x000a89bc      mov r6, r15
0x000a89be      ld.w 0[r11], r17
0x000a89c2      mov r17, r6
0x000a89c4      mov r15, r7
0x000a89c6      jarl 0xeb984, lp
0x000a89ca      mov r6, r15
0x000a89cc      st.w r15, 0[r11]
0x000a89d0      mov r29, r12
0x000a89d2      addi 1, r12, r10
0x000a89d6      mov r10, r29
0x000a89d8      and r20, r29
0x000a89da      mov r12, r19
0x000a89dc      shl 2, r19
0x000a89de      add r26, r19
0x000a89e0      ld.w -17220[gp], r16
0x000a89e4      mov r0, r18
0x000a89e6      mov r16, r6
0x000a89e8      mov r18, r7
0x000a89ea      jarl 0xebb70, lp
0x000a89ee      mov r6, r18
0x000a89f0      ld.w -17228[gp], r17
0x000a89f4      mov r18, r6
0x000a89f6      mov r17, r7
0x000a89f8      jarl 0xeb984, lp
0x000a89fc      mov r6, r17
0x000a89fe      st.w r17, 0[r19]
0x000a8a02      mov r29, r13
0x000a8a04      addi 1, r13, r15
0x000a8a08      mov r15, r29
0x000a8a0a      and r20, r29
0x000a8a0c      mov r13, r10
0x000a8a0e      shl 2, r10
0x000a8a10      add r26, r10
0x000a8a12      ld.w -17224[gp], r16
0x000a8a16      mov r0, r18
0x000a8a18      mov r16, r6
0x000a8a1a      mov r18, r7
0x000a8a1c      jarl 0xebb70, lp
0x000a8a20      mov r6, r18
0x000a8a22      ld.w -17228[gp], r19
0x000a8a26      mov r18, r6
0x000a8a28      mov r19, r7
0x000a8a2a      jarl 0xeb984, lp
0x000a8a2e      mov r6, r19
0x000a8a30      st.w r19, 0[r10]
0x000a8a34      mov r29, r14
0x000a8a36      addi 1, r14, r17
0x000a8a3a      mov r17, r29
0x000a8a3c      and r20, r29
0x000a8a3e      mov r14, r15
0x000a8a40      shl 2, r15
0x000a8a42      add r26, r15
0x000a8a44      ld.w -17232[gp], r16
0x000a8a48      ld.w -17240[gp], r18
0x000a8a4c      mov r18, r6
0x000a8a4e      mov r16, r7
0x000a8a50      jarl 0xebbcc, lp
0x000a8a54      mov r6, r16
0x000a8a56      st.w r16, 0[r15]
0x000a8a5a      mov r29, r25
0x000a8a5c      addi 1, r25, r10
0x000a8a60      mov r10, r29
0x000a8a62      and r20, r29
0x000a8a64      mov r25, r19
0x000a8a66      shl 2, r19
0x000a8a68      add r26, r19
0x000a8a6a      ld.w -17236[gp], r17
0x000a8a6e      ld.w -17240[gp], r18
0x000a8a72      mov r18, r6
0x000a8a74      mov r17, r7
0x000a8a76      jarl 0xebbcc, lp
0x000a8a7a      mov r6, r17
0x000a8a7c      st.w r17, 0[r19]
0x000a8a80      jarl 0xa8d04, lp
0x000a8a84      jarl 0xa8c6c, lp
0x000a8a88      movea -17624, gp, r29
0x000a8a8c      mov r29, r6
0x000a8a8e      movea 40, r0, r7                                       ; '('
0x000a8a92      jarl 0xe34d4, lp
0x000a8a96      addi 1, r28, r15
0x000a8a9a      mov r15, r28
0x000a8a9c      and r20, r28
0x000a8a9e      cmp 3, r28
0x000a8aa0      bge 0xa8aa6
0x000a8aa2      jr 0xa891c
0x000a8aa6      addi 1, r27, r16
0x000a8aaa      mov r16, r27
0x000a8aac      and r20, r27
0x000a8aae      cmp 15, r27
0x000a8ab0      bgt 0xa8ab6
0x000a8ab2      jr 0xa8910
0x000a8ab6      movea 16, r0, r6
0x000a8aba      mov 5, r7
0x000a8abc      mov 1, r8
0x000a8abe      jarl 0xa8bb4, lp
0x000a8ac2      mov 3, r14
0x000a8ac4      movea 16, r0, r10
0x000a8ac8      st.w r10, -17204[gp]
0x000a8acc      mov 4, r13
0x000a8ace      mov 5, r11
0x000a8ad0      mov 0xc015a, r18
0x000a8ad6      st.w r18, -17200[gp]
0x000a8ada      mov r0, r12
0x000a8adc      mov r11, r13
0x000a8ade      addi 1, r13, r19
0x000a8ae2      mov r19, r11
0x000a8ae4      and r20, r11
0x000a8ae6      mov r13, r17
0x000a8ae8      shl 2, r17
0x000a8aea      add r26, r17
0x000a8aec      mov r12, r15
0x000a8aee      shl 2, r15
0x000a8af0      add r24, r15
0x000a8af2      ld.w 0[r15], r16
0x000a8af6      st.w r16, 0[r17]
0x000a8afa      addi 1, r12, r10
0x000a8afe      mov r10, r12
0x000a8b00      and r20, r12
0x000a8b02      cmp 15, r12
0x000a8b04      ble 0xa8adc
0x000a8b06      jarl 0xa8d04, lp
0x000a8b0a      jarl 0xa8c6c, lp
0x000a8b0e      mov r29, r6
0x000a8b10      movea 88, r0, r7                                       ; 'X'
0x000a8b14      jarl 0xe34d4, lp
0x000a8b18      dispose 18, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000a8b1c      prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 18
0x000a8b20      jr 0xa8904
0x000a8b24      br 0xa8bae
0x000a8b26      mov 8, r6
0x000a8b28      mov 5, r7
0x000a8b2a      mov 1, r8
0x000a8b2c      jarl 0xa8bb4, lp
0x000a8b30      mov 3, r14
0x000a8b32      mov 8, r10
0x000a8b34      st.w r10, -17204[gp]
0x000a8b38      mov 4, r13
0x000a8b3a      mov 5, r12
0x000a8b3c      mov 0xe0082, r15
0x000a8b42      st.w r15, -17200[gp]
0x000a8b46      mov r0, r11
0x000a8b48      movea 165, r0, r13
0x000a8b4c      ld.bu -15914[gp], r16
0x000a8b50      cmp r13, r16
0x000a8b52      bne 0xa8b72
0x000a8b54      mov r12, r14
0x000a8b56      addi 1, r14, r17
0x000a8b5a      mov r17, r12
0x000a8b5c      and r20, r12
0x000a8b5e      mov r14, r18
0x000a8b60      shl 2, r18
0x000a8b62      movea -17216, gp, r19
0x000a8b66      add r18, r19
0x000a8b68      ld.bu -15913[gp], r10
0x000a8b6c      st.w r10, 0[r19]
0x000a8b70      br 0xa8b8a
0x000a8b72      mov r12, r14
0x000a8b74      addi 1, r14, r15
0x000a8b78      mov r15, r12
0x000a8b7a      and r20, r12
0x000a8b7c      mov r14, r16
0x000a8b7e      shl 2, r16
0x000a8b80      movea -17216, gp, r17
0x000a8b84      add r16, r17
0x000a8b86      st.w r0, 0[r17]
0x000a8b8a      addi 1, r11, r18
0x000a8b8e      mov r18, r11
0x000a8b90      and r20, r11
0x000a8b92      cmp 8, r11
0x000a8b94      blt 0xa8b4c
0x000a8b96      jarl 0xa8d04, lp
0x000a8b9a      jarl 0xa8c6c, lp
0x000a8b9e      movea -17624, gp, r6
0x000a8ba2      movea 56, r0, r7                                       ; '8'
0x000a8ba6      jarl 0xe34d4, lp
0x000a8baa      dispose 0, {lp}, lp
0x000a8bae      prepare {lp}, 0
0x000a8bb2      br 0xa8b26
0x000ac0e2      mov 8, r25
0x000ac0e4      mov r25, r6
0x000ac0e6      mov 5, r7
0x000ac0e8      mov 1, r8
0x000ac0ea      jarl 0xa8bb4, lp
0x000ac0ee      mov r26, r14
0x000ac0f0      st.w r25, -17204[gp]
0x000ac0f4      mov r27, r13
0x000ac0f6      mov r28, r12
0x000ac0f8      mov 0xc0090, r16
0x000ac0fe      st.w r16, -17200[gp]
0x000ac102      mov r0, r11
0x000ac104      cmp r0, r25
0x000ac106      bnh 0xac13a
0x000ac108      mov 0x68b90, r13
0x000ac10e      movea -17216, gp, r14
0x000ac112      mov r12, r15
0x000ac114      shl 2, r15
0x000ac116      add r14, r15
0x000ac118      mov r11, r17
0x000ac11a      shl 2, r17
0x000ac11c      add r13, r17
0x000ac11e      ld.w 0[r17], r18
0x000ac122      st.w r18, 0[r15]
0x000ac126      addi 1, r12, r19
0x000ac12a      mov r19, r12
0x000ac12c      and r20, r12
0x000ac12e      addi 1, r11, r10
0x000ac132      mov r10, r11
0x000ac134      and r21, r11
0x000ac136      cmp r25, r11
0x000ac138      bl 0xac112
0x000ac13a      jarl 0xa8d04, lp
0x000ac13e      jarl 0xa8c6c, lp
0x000ac142      mov r29, r6
0x000ac144      mov r25, r16
0x000ac146      shl 2, r16
0x000ac148      addi 24, r16, r16
0x000ac14c      and r21, r16
0x000ac14e      mov r16, r7
0x000ac150      jarl 0xe34d4, lp
0x000ac154      mov 6, r25
0x000ac156      mov r25, r6
0x000a8f34      br 0xa901e
0x000a8f36      st.w r6, 8[sp]
0x000a8f3a      ld.hu 8[sp], r28
0x000a8f3e      mov 5, r6
0x000a8f40      mov 5, r7
0x000a8f42      mov 1, r8
0x000a8f44      jarl 0xa8bb4, lp
0x000a8f48      mov 3, r12
0x000a8f4a      mov 5, r10
0x000a8f4c      st.w r10, -17204[gp]
0x000a8f50      mov 4, r11
0x000a8f52      mov 5, r29
0x000a8f54      mov r28, r13
0x000a8f56      mul 5, r13, r0
0x000a8f5a      mov 0xe0068, r1                                        ; 'h'
0x000a8f60      add r1, r13
0x000a8f62      st.w r13, -17200[gp]
0x000a8f66      cmp 2, r28
0x000a8f68      bgt 0xa8fc0
0x000a8f6a      mov r28, r14
0x000a8f6c      shl 3, r14
0x000a8f6e      movhi -1, gp, r1
0x000a8f72      movea -10672, r1, r15
0x000a8f76      add r14, r15
0x000a8f78      movea 0, sp, r16
0x000a8f7c      ld.w 0[r15], r17
0x000a8f80      st.w r17, 0[r16]
0x000a8f84      ld.w 4[r15], r18                                       ; 4
0x000a8f88      st.w r18, 4[r16]                                       ; 4
0x000a8f8c      mov r28, r19
0x000a8f8e      mul 38, r19, r0
0x000a8f92      movhi -1, gp, r1
0x000a8f96      movea -11108, r1, r10
0x000a8f9a      add r19, r10
0x000a8f9c      movea -15912, gp, r13
0x000a8fa0      add r28, r13
0x000a8fa2      ld.b 0[r13], r14
0x000a8fa6      shl 1, r14
0x000a8fa8      add r14, r10
0x000a8faa      ld.h 18[r10], r17                                      ; 0x12
0x000a8fae      movea -15909, gp, r18
0x000a8fb2      add r28, r18
0x000a8fb4      ld.b 0[r18], r16
0x000a8fb8      add r16, r17
0x000a8fba      st.h r17, 2[sp]
0x000a8fbe      br 0xa8ff6
0x000a8fc0      cmp 3, r28
0x000a8fc2      bne 0xa8ff2
0x000a8fc4      ld.bu -17810[gp], r15
0x000a8fc8      shl 3, r15
0x000a8fca      movhi -1, gp, r1
0x000a8fce      movea -10804, r1, r19
0x000a8fd2      add r15, r19
0x000a8fd4      movea 0, sp, r13
0x000a8fd8      ld.w 0[r19], r14
0x000a8fdc      st.w r14, 0[r13]
0x000a8fe0      ld.w 4[r19], r10                                       ; 4
0x000a8fe4      st.w r10, 4[r13]                                       ; 4
0x000a8fe8      ld.b -15906[gp], r18
0x000a8fec      st.h r18, 2[sp]
0x000a8ff0      br 0xa8ff6
0x000a8ff2      st.b r0, 5[sp]
0x000a8ff6      movea 0, sp, r6
0x000a8ffa      jarl 0xa96c4, lp
0x000a8ffe      mov r29, r6
0x000a9000      jarl 0xabf78, lp
0x000a9004      mov r10, r29
0x000a9006      jarl 0xa8d04, lp
0x000a900a      jarl 0xa8c6c, lp
0x000a900e      movea -17624, gp, r6
0x000a9012      movea 44, r0, r7                                       ; ','
0x000a9016      jarl 0xe34d4, lp
0x000a901a      dispose 3, {r28, r29, lp}, lp
0x000a901e      prepare {r28, r29, lp}, 3
0x000a9022      br 0xa8f36
0x000a9bdc      br 0xa9c88
0x000a9bde      st.w r6, 8[sp]
0x000a9be2      ld.hu 8[sp], r28
0x000a9be6      mov 3, r29
0x000a9be8      mov 5, r6
0x000a9bea      mov 5, r7
0x000a9bec      mov 1, r8
0x000a9bee      jarl 0xa8bb4, lp
0x000a9bf2      movea -17216, gp, r12
0x000a9bf6      movea 0, sp, r11
0x000a9bfa      mov r29, r14
0x000a9bfc      addi 1, r14, r10
0x000a9c00      mov r10, r29
0x000a9c02      and r20, r29
0x000a9c04      mov r14, r15
0x000a9c06      shl 2, r15
0x000a9c08      add r12, r15
0x000a9c0a      mov 5, r16
0x000a9c0c      st.w r16, 0[r15]
0x000a9c10      mov r29, r13
0x000a9c12      addi 1, r13, r17
0x000a9c16      mov r17, r29
0x000a9c18      and r20, r29
0x000a9c1a      mov r13, r18
0x000a9c1c      shl 2, r18
0x000a9c1e      add r12, r18
0x000a9c20      mov r28, r19
0x000a9c22      mul 5, r19, r0
0x000a9c26      mov 0xe0068, r1                                        ; 'h'
0x000a9c2c      add r1, r19
0x000a9c2e      st.w r19, 0[r18]
0x000a9c32      mov r28, r10
0x000a9c34      shl 3, r10
0x000a9c36      movhi -1, gp, r1
0x000a9c3a      movea -10564, r1, r15
0x000a9c3e      add r10, r15
0x000a9c40      mov r11, r16
0x000a9c42      ld.w 0[r15], r17
0x000a9c46      st.w r17, 0[r16]
0x000a9c4a      ld.w 4[r15], r18                                       ; 4
0x000a9c4e      st.w r18, 4[r16]                                       ; 4
0x000a9c52      ld.b -15839[gp], r19
0x000a9c56      movea 90, r0, r1                                       ; 'Z'
0x000a9c5a      cmp r1, r19
0x000a9c5c      bne 0xa9c62
0x000a9c5e      st.h r0, 2[sp]
0x000a9c62      mov r11, r6
0x000a9c64      jarl 0xa96c4, lp
0x000a9c68      mov r29, r6
0x000a9c6a      jarl 0xabf78, lp
0x000a9c6e      mov r10, r29
0x000a9c70      jarl 0xa8d04, lp
0x000a9c74      jarl 0xa8c6c, lp
0x000a9c78      movea -17624, gp, r6
0x000a9c7c      movea 44, r0, r7                                       ; ','
0x000a9c80      jarl 0xe34d4, lp
0x000a9c84      dispose 3, {r28, r29, lp}, lp
0x000a9c88      prepare {r28, r29, lp}, 3
0x000ab6a4      br 0xab732
0x000ab6a6      mov 3, r25
0x000ab6a8      mov 5, r6
0x000ab6aa      mov 5, r7
0x000ab6ac      mov 2, r8
0x000ab6ae      jarl 0xa8bb4, lp
0x000ab6b2      mov r0, r26
0x000ab6b4      movea -17216, gp, r27
0x000ab6b8      mov 5, r28
0x000ab6ba      mov 0x68a70, r29
0x000ab6c0      mov r25, r12
0x000ab6c2      addi 1, r12, r10
0x000ab6c6      mov r10, r25
0x000ab6c8      and r20, r25
0x000ab6ca      mov r12, r13
0x000ab6cc      shl 2, r13
0x000ab6ce      add r27, r13
0x000ab6d0      st.w r28, 0[r13]
0x000ab6d4      mov r25, r11
0x000ab6d6      addi 1, r11, r14
0x000ab6da      mov r14, r25
0x000ab6dc      and r20, r25
0x000ab6de      mov r11, r15
0x000ab6e0      shl 2, r15
0x000ab6e2      add r27, r15
0x000ab6e4      mov r26, r16
0x000ab6e6      shl 2, r16
0x000ab6e8      add r29, r16
0x000ab6ea      ld.w 0[r16], r17
0x000ab6ee      st.w r17, 0[r15]
0x000ab6f2      mov r26, r18
0x000ab6f4      shl 3, r18
0x000ab6f6      movhi -1, gp, r1
0x000ab6fa      movea -10332, r1, r19
0x000ab6fe      add r18, r19
0x000ab700      mov r19, r6
0x000ab702      jarl 0xa96c4, lp
0x000ab706      mov r25, r6
0x000ab708      jarl 0xabf78, lp
0x000ab70c      mov r10, r25
0x000ab70e      addi 1, r26, r13
0x000ab712      mov r13, r26
0x000ab714      and r20, r26
0x000ab716      cmp 2, r26
0x000ab718      blt 0xab6c0
0x000ab71a      jarl 0xa8d04, lp
0x000ab71e      jarl 0xa8c6c, lp
0x000ab722      movea -17624, gp, r6
0x000ab726      movea 72, r0, r7                                       ; 'H'
0x000ab72a      jarl 0xe34d4, lp
0x000ab72e      dispose 0, {r25, r26, r27, r28, r29, lp}, lp
0x000ab732      truncated from 0 to 0
0x000a9d80      movea -15836, gp, r11
0x000a9d84      ld.bu -15764[gp], r10
0x000a9d88      shl 2, r10
0x000a9d8a      add r11, r10
0x000a9d8c      ld.b 0[r10], r15
0x000a9d90      cmp r0, r15
0x000a9d92      bne 0xa9d98
0x000a9d94      mov r0, r28
0x000a9d96      br 0xa9daa
0x000a9d98      ld.bu -15764[gp], r16
0x000a9d9c      shl 2, r16
0x000a9d9e      add r11, r16
0x000a9da0      ld.hu 2[r16], r17
0x000a9da4      mulu 48, r17, r1
0x000a9da8      mov r17, r28
0x000a9daa      movea 2880, r0, r7                                     ; '@\v'
0x000a9dae      mov r0, r11
0x000a9db0      movhi -1, gp, r1
0x000a9db4      movea -10615, r1, r14
0x000a9db8      mov r14, r18
0x000a9dba      add r11, r18
0x000a9dbc      ld.b 0[r18], r19
0x000a9dc0      cmp 15, r19
0x000a9dc2      bh 0xa9e1c
0x000a9dc4      mov r14, r10
0x000a9dc6      add r11, r10
0x000a9dc8      ld.bu 0[r10], r15
0x000a9dcc      movhi -1, gp, r1
0x000a9dd0      movea -10647, r1, r16
0x000a9dd4      add r15, r16
0x000a9dd6      ld.bu 0[r16], r17
0x000a9dda      mov r17, r6
0x000a9ddc      mov r6, r12
0x000a9dde      shl 1, r12
0x000a9de0      mov r11, r18
0x000a9de2      shl 1, r18
0x000a9de4      movhi -1, gp, r1
0x000a9de8      movea -11900, r1, r19
0x000a9dec      add r18, r19
0x000a9dee      ld.h 0[r19], r10
0x000a9df2      movhi -1, gp, r1
0x000a9df6      movea -13864, r1, r15
0x000a9dfa      add r12, r15
0x000a9dfc      ld.h 0[r15], r16
0x000a9e00      add r16, r10
0x000a9e02      movhi -1, gp, r1
0x000a9e06      movea -13880, r1, r17
0x000a9e0a      add r12, r17
0x000a9e0c      ld.h 0[r17], r18
0x000a9e10      add r18, r10
0x000a9e12      mov r10, r13
0x000a9e14      and r21, r13
0x000a9e16      cmp r7, r13
0x000a9e18      bnl 0xa9e1c
0x000a9e1a      mov r13, r7
0x000a9e1c      addi 1, r11, r19
0x000a9e20      mov r19, r11
0x000a9e22      and r20, r11
0x000a9e24      cmp 15, r11
0x000a9e26      ble 0xa9db8
0x000a9e28      addi 1248, r7, r15
0x000a9e2c      and r21, r15
0x000a9e2e      mov r15, r7
0x000a9e30      cmp r28, r7
0x000a9e32      bnl 0xa9e38
0x000a9e34      sub r7, r28
0x000a9e36      br 0xa9e3a
0x000a9e38      mov r0, r28
0x000a9e3a      movea 9600, r0, r11
0x000a9e3e      cmp r11, r28
0x000a9e40      bnh 0xa9e44
0x000a9e42      mov r11, r28
0x000a9e44      mov 3, r29
0x000a9e46      mov 8, r6
0x000a9e48      mov 5, r7
0x000a9e4a      mov 1, r8
0x000a9e4c      jarl 0xa8bb4, lp
0x000a9e50      movea -17216, gp, r12
0x000a9e54      mov r29, r14
0x000a9e56      addi 1, r14, r16
0x000a9e5a      mov r16, r29
0x000a9e5c      and r20, r29
0x000a9e5e      mov r14, r17
0x000a9e60      shl 2, r17
0x000a9e62      add r12, r17
0x000a9e64      mov 8, r18
0x000a9e66      st.w r18, 0[r17]
0x000a9e6a      mov r29, r13
0x000a9e6c      addi 1, r13, r10
0x000a9e70      mov r10, r29
0x000a9e72      and r20, r29
0x000a9e74      mov r13, r19
0x000a9e76      shl 2, r19
0x000a9e78      add r12, r19
0x000a9e7a      mov 0xe000c, r15                                       ; '\f'
0x000a9e80      st.w r15, 0[r19]
0x000a9e84      mov r0, r11
0x000a9e86      mov r29, r13
0x000a9e88      addi 1, r13, r16
0x000a9e8c      mov r16, r29
0x000a9e8e      and r20, r29
0x000a9e90      mov r13, r17
0x000a9e92      shl 2, r17
0x000a9e94      add r12, r17
0x000a9e96      st.w r28, 0[r17]
0x000a9e9a      addi 1, r11, r18
0x000a9e9e      mov r18, r11
0x000a9ea0      and r20, r11
0x000a9ea2      cmp 8, r11
0x000a9ea4      blt 0xa9e86
0x000a9ea6      jarl 0xa8d04, lp
0x000a9eaa      jarl 0xa8c6c, lp
0x000a9eae      movea -17624, gp, r6
0x000a9eb2      movea 56, r0, r7                                       ; '8'
0x000a9eb6      jarl 0xe34d4, lp
0x000a9eba      dispose 0, {r28, r29, lp}, lp
0x000a9ebe      truncated from 0 to 0
0x000b0066      movea 165, r0, r6
0x000b006a      ld.bu -17779[gp], r10
0x000b006e      cmp r6, r10
0x000b0070      bne 0xb0078
0x000b0072      ld.bu -17772[gp], r13
0x000b0076      br 0xb007c
0x000b0078      ld.bu -15406[gp], r13
0x000b007c      ld.w -30712[gp], r12
0x000b0080      movhi 256, r0, r1
0x000b0084      div r1, r12, r0
0x000b0088      mov r0, r11
0x000b008a      movhi -1, gp, r1
0x000b008e      ld.b -10404[r1], r15
0x000b0092      cmp r15, r12
0x000b0094      bge 0xb00ba
0x000b0096      cmp 15, r11
0x000b0098      bgt 0xb00ba
0x000b009a      movhi -1, gp, r1
0x000b009e      movea -10404, r1, r14
0x000b00a2      addi 1, r11, r16
0x000b00a6      mov r16, r11
0x000b00a8      and r20, r11
0x000b00aa      mov r14, r17
0x000b00ac      add r11, r17
0x000b00ae      ld.b 0[r17], r18
0x000b00b2      cmp r12, r18
0x000b00b4      ble 0xb00ba
0x000b00b6      cmp 15, r11
0x000b00b8      ble 0xb00a2
0x000b00ba      cmp 15, r11
0x000b00bc      ble 0xb00c0
0x000b00be      mov 15, r11
0x000b00c0      ld.bu -30788[gp], r19
0x000b00c4      cmp r19, r6
0x000b00c6      bne 0xb00e8
0x000b00c8      ld.bu -15913[gp], r10
0x000b00cc      cmp r10, r11
0x000b00ce      bne 0xb00d8
0x000b00d0      ld.bu -15914[gp], r15
0x000b00d4      cmp r13, r15
0x000b00d6      be 0xb00e8
0x000b00d8      st.b r13, -15914[gp]
0x000b00dc      st.b r11, -15913[gp]
0x000b00e0      movea 17, r0, r6
0x000b00e4      jarl 0xc3990, lp
0x000b00e8      dispose 0, {lp}, lp
0x000b00ec      prepare {lp}, 0
0x000b00f0      br 0xb0066
0x000b00f2      nop
0x000b00f4      br 0xb0106
0x000b00f6      st.b r0, -15913[gp]
0x000b00fa      movea 17, r0, r6
0x000b00fe      jarl 0xc3990, lp
0x000b0102      dispose 0, {lp}, lp
0x000b0106      prepare {lp}, 0
0x000b010a      br 0xb00f6
0x000ab850      st.w r6, 0[sp]
0x000ab854      ld.w 0[sp], r13
0x000ab858      st.w r7, 4[sp]                                         ; 4
0x000ab85c      ld.w 4[sp], r12                                        ; 4
0x000ab860      movhi 16256, r0, r11
0x000ab864      mov r11, r6
0x000ab866      mov r12, r7
0x000ab868      stsr psw, r8
0x000ab86c      jarl 0xebaec, lp
0x000ab870      ldsr r6, psw
0x000ab874      bl 0xab8ce
0x000ab876      mov 0, r10
0x000ab878      mov r12, r14
0x000ab87a      mov r13, r6
0x000ab87c      mov r14, r7
0x000ab87e      jarl 0xeba38, lp
0x000ab882      mov r6, r14
0x000ab884      st.w r14, -17240[gp]
0x000ab888      mov r12, r15
0x000ab88a      mov r13, r6
0x000ab88c      mov r15, r7
0x000ab88e      jarl 0xebb70, lp
0x000ab892      mov r6, r15
0x000ab894      st.w r15, -17236[gp]
0x000ab898      st.w r10, -17232[gp]
0x000ab89c      mov r11, r16
0x000ab89e      mov r13, r6
0x000ab8a0      mov r16, r7
0x000ab8a2      jarl 0xeba38, lp
0x000ab8a6      mov r6, r16
0x000ab8a8      mov r11, r17
0x000ab8aa      mov r16, r6
0x000ab8ac      mov r17, r7
0x000ab8ae      jarl 0xebbcc, lp
0x000ab8b2      mov r6, r17
0x000ab8b4      st.w r17, -17228[gp]
0x000ab8b8      mov r11, r18
0x000ab8ba      mov r13, r6
0x000ab8bc      mov r18, r7
0x000ab8be      jarl 0xebb70, lp
0x000ab8c2      mov r6, r18
0x000ab8c4      st.w r18, -17224[gp]
0x000ab8c8      st.w r10, -17220[gp]
0x000ab8cc      br 0xab932
0x000ab8ce      mov 0, r10
0x000ab8d0      mov r11, r19
0x000ab8d2      mov r12, r6
0x000ab8d4      mov r19, r7
0x000ab8d6      jarl 0xebbcc, lp
0x000ab8da      mov r6, r19
0x000ab8dc      mov r19, r12
0x000ab8de      mov r11, r14
0x000ab8e0      mov r13, r6
0x000ab8e2      mov r14, r7
0x000ab8e4      jarl 0xeba38, lp
0x000ab8e8      mov r6, r14
0x000ab8ea      st.w r14, -17240[gp]
0x000ab8ee      mov r11, r15
0x000ab8f0      mov r13, r6
0x000ab8f2      mov r15, r7
0x000ab8f4      jarl 0xebb70, lp
0x000ab8f8      mov r6, r15
0x000ab8fa      st.w r15, -17236[gp]
0x000ab8fe      st.w r10, -17232[gp]
0x000ab902      mov r12, r16
0x000ab904      mov r13, r6
0x000ab906      mov r16, r7
0x000ab908      jarl 0xeba38, lp
0x000ab90c      mov r6, r16
0x000ab90e      mov r11, r17
0x000ab910      mov r16, r6
0x000ab912      mov r17, r7
0x000ab914      jarl 0xebbcc, lp
0x000ab918      mov r6, r17
0x000ab91a      st.w r17, -17228[gp]
0x000ab91e      mov r12, r18
0x000ab920      mov r13, r6
0x000ab922      mov r18, r7
0x000ab924      jarl 0xebb70, lp
0x000ab928      mov r6, r18
0x000ab92a      st.w r18, -17224[gp]
0x000ab92e      st.w r10, -17220[gp]
0x000ab932      dispose 2, {lp}, lp
0x000ab936      prepare {lp}, 2
0x000ab93a      br 0xab850
0x000ab93c      jr 0xaba3e
0x000a76b0      jr 0xa7d1e
0x000a76b4      st.w r6, 4[sp]                                         ; 4
0x000a76b8      ld.hu 4[sp], r29                                       ; 4
0x000a76bc      mov r29, r13
0x000a76be      jr 0xa795e
0x000a76c2      ld.bu -15938[gp], r10
0x000a76c6      movea 165, r0, r1
0x000a76ca      cmp r1, r10
0x000a76cc      bne 0xa76d6
0x000a76ce      mov r29, r6
0x000a76d0      jarl 0xa8f34, lp
0x000a76d4      br 0xa76dc
0x000a76d6      mov r29, r6
0x000a76d8      jarl 0xa9bdc, lp
0x000a76dc      jr 0xa7d1a
0x000a76e0      jarl 0xa9498, lp
0x000a76e4      jr 0xa7d1a
0x000a76e8      jarl 0xa952c, lp
0x000a76ec      jr 0xa7d1a
0x000a76f0      jarl 0xa95c4, lp
0x000a76f4      jr 0xa7d1a
0x000a76f8      jarl 0xa9c90, lp
0x000a76fc      jr 0xa7d1a
0x000a7700      jarl 0xa9ec8, lp
0x000a7704      jr 0xa7d1a
0x000a7708      jarl 0xa9f5c, lp
0x000a770c      jr 0xa7d1a
0x000a7710      jarl 0xa9d7c, lp
0x000a7714      jr 0xa7d1a
0x000a7718      jarl 0xa9ff0, lp
0x000a771c      jr 0xa7d1a
0x000a7720      jarl 0xa9024, lp
0x000a7724      jr 0xa7d1a
0x000a7728      jarl 0xa8544, lp
0x000a772c      jarl 0xaa140, lp
0x000a7730      jr 0xa7d1a
0x000a7734      mov r29, r6
0x000a7736      jarl 0xa870c, lp
0x000a773a      mov r29, r6
0x000a773c      jarl 0xaa278, lp
0x000a7740      jr 0xa7d1a
0x000a7744      jarl 0xa8b24, lp
0x000a7748      jr 0xa7d1a
0x000a774c      jarl 0xab6a4, lp
0x000a7750      jr 0xa7d1a
0x000a7754      mov r29, r6
0x000a7756      jarl 0xaa388, lp
0x000a775a      jr 0xa7d1a
0x000a775e      mov r29, r6
0x000a7760      jarl 0xac818, lp
0x000a7764      jr 0xa7d1a
0x000a7768      mov r29, r6
0x000a776a      jarl 0xac818, lp
0x000a776e      jr 0xa7d1a
0x000a7772      mov r29, r6
0x000a7774      jarl 0xac8a8, lp
0x000a7778      jr 0xa7d1a
0x000a777c      mov r29, r6
0x000a777e      jarl 0xac8a8, lp
0x000a7782      jr 0xa7d1a
0x000a7786      jarl 0xac93c, lp
0x000a778a      jr 0xa7d1a
0x000a778e      jarl 0xac9c0, lp
0x000a7792      jr 0xa7d1a
0x000a7796      jarl 0xaca9c, lp
0x000a779a      jr 0xa7d1a
0x000a779e      jarl 0xacc80, lp
0x000a77a2      jr 0xa7d1a
0x000a77a6      mov r21, r6
0x000a77a8      jarl 0xacd98, lp
0x000a77ac      jr 0xa7d1a
0x000a77b0      mov r21, r6
0x000a77b2      jarl 0xacea4, lp
0x000a77b6      jr 0xa7d1a
0x000a77ba      jarl 0xacfb0, lp
0x000a77be      jr 0xa7d1a
0x000a77c2      jarl 0xaa638, lp
0x000a77c6      jr 0xa7d1a
0x000a77ca      jarl 0xaa644, lp
0x000a77ce      jarl 0xa903c, lp
0x000a77d2      jr 0xa7d1a
0x000a77d6      jarl 0xaa644, lp
0x000a77da      jr 0xa7d1a
0x000a77de      jarl 0xaa898, lp
0x000a77e2      jr 0xa7d1a
0x000a77e6      jarl 0xaa910, lp
0x000a77ea      jr 0xa7d1a
0x000a77ee      jarl 0xaa9b4, lp
0x000a77f2      jr 0xa7d1a
0x000a77f6      jarl 0xaaa5c, lp
0x000a77fa      jr 0xa7d1a
0x000a77fe      jarl 0xaab40, lp
0x000a7802      jr 0xa7d1a
0x000a7806      jarl 0xaac84, lp
0x000a780a      jr 0xa7d1a
0x000a780e      jarl 0xaad6c, lp
0x000a7812      jr 0xa7d1a
0x000a7816      jarl 0xaafbc, lp
0x000a781a      jr 0xa7d1a
0x000a781e      jarl 0xab0bc, lp
0x000a7822      jr 0xa7d1a
0x000a7826      jarl 0xab1bc, lp
0x000a782a      jr 0xa7d1a
0x000a782e      ld.w -15932[gp], r15
0x000a7832      st.w r15, 0[sp]
0x000a7836      ld.w -15936[gp], r6
0x000a783a      movea 0, sp, r16
0x000a783e      ld.w 0[r16], r7
0x000a7842      jarl 0xac648, lp
0x000a7846      mov 3, r6
0x000a7848      jarl 0xdd830, lp
0x000a784c      jr 0xa7d1a
0x000a7850      jarl 0xac6cc, lp
0x000a7854      jr 0xa7d1a
0x000a7858      jarl 0xac740, lp
0x000a785c      jr 0xa7d1a
0x000a7860      jarl 0xac460, lp
0x000a7864      jr 0xa7d1a
0x000a7868      jarl 0xab224, lp
0x000a786c      jr 0xa7d1a
0x000a7870      jarl 0xab738, lp
0x000a7874      jr 0xa7d1a
0x000a7878      mov r21, r6
0x000a787a      jarl 0xa9858, lp
0x000a787e      jr 0xa7d1a
0x000a7882      mov r21, r6
0x000a7884      jarl 0xa9b00, lp
0x000a7888      jr 0xa7d1a
0x000a788c      mov r21, r6
0x000a788e      jarl 0xa9934, lp
0x000a7892      jr 0xa7d1a
0x000a7896      mov r21, r6
0x000a7898      jarl 0xa9a28, lp
0x000a789c      jr 0xa7d1a
0x000a78a0      jarl 0xa8900, lp
0x000a78a4      jr 0xa7d1a
0x000a78a8      movea 55, r0, r1                                       ; '7'
0x000a78ac      cmp r1, r29
0x000a78ae      blt 0xa78c6
0x000a78b0      movea 135, r0, r1
0x000a78b4      cmp r1, r29
0x000a78b6      bge 0xa78c6
0x000a78b8      addi -55, r29, r17
0x000a78bc      and r21, r17
0x000a78be      mov r17, r6
0x000a78c0      jarl 0xa9858, lp
0x000a78c4      br 0xa7956
0x000a78c6      movea 135, r0, r1
0x000a78ca      cmp r1, r29
0x000a78cc      blt 0xa78e4
0x000a78ce      movea 279, r0, r1
0x000a78d2      cmp r1, r29
0x000a78d4      bge 0xa78e4
0x000a78d6      addi -135, r29, r18
0x000a78da      and r21, r18
0x000a78dc      mov r18, r6
0x000a78de      jarl 0xa9b00, lp
0x000a78e2      br 0xa7956
0x000a78e4      movea 327, r0, r11
0x000a78e8      cmp r11, r29
0x000a78ea      blt 0xa7902
0x000a78ec      movea 351, r0, r1
0x000a78f0      cmp r1, r29
0x000a78f2      bge 0xa7902
0x000a78f4      addi -327, r29, r19
0x000a78f8      and r21, r19
0x000a78fa      mov r19, r6
0x000a78fc      jarl 0xa9934, lp
0x000a7900      br 0xa7956
0x000a7902      movea 279, r0, r1
0x000a7906      cmp r1, r29
0x000a7908      blt 0xa791c
0x000a790a      cmp r11, r29
0x000a790c      bge 0xa791c
0x000a790e      addi -279, r29, r10
0x000a7912      and r21, r10
0x000a7914      mov r10, r6
0x000a7916      jarl 0xa9a28, lp
0x000a791a      br 0xa7956
0x000a791c      movea 352, r0, r1
0x000a7920      cmp r1, r29
0x000a7922      blt 0xa793a
0x000a7924      movea 360, r0, r1
0x000a7928      cmp r1, r29
0x000a792a      bge 0xa793a
0x000a792c      addi -352, r29, r15
0x000a7930      and r21, r15
0x000a7932      mov r15, r6
0x000a7934      jarl 0xacd98, lp
0x000a7938      br 0xa7956
0x000a793a      movea 360, r0, r1
0x000a793e      cmp r1, r29
0x000a7940      blt 0xa7956
0x000a7942      movea 368, r0, r1
0x000a7946      cmp r1, r29
0x000a7948      bge 0xa7956
0x000a794a      addi -360, r29, r16
0x000a794e      and r21, r16
0x000a7950      mov r16, r6
0x000a7952      jarl 0xacea4, lp
0x000a7956      jr 0xa7d1a
0x000a795a      jr 0xa7d1a
0x000a795e      cmp r0, r13
0x000a7960      blt 0xa78a8
0x000a7962      movea 375, r0, r11
0x000a7966      cmp r11, r13
0x000a7968      bgt 0xa78a8
0x000a796a      movea 31, r0, r12
0x000a796e      cmp r12, r13
0x000a7970      be 0xa7896
0x000a7972      cmp r12, r13
0x000a7974      ble 0xa797a
0x000a7976      jr 0xa7b2a
0x000a797a      cmp 15, r13
0x000a797c      bne 0xa7982
0x000a797e      jr 0xa7720
0x000a7982      cmp 15, r13
0x000a7984      bgt 0xa7a3a
0x000a7986      cmp 7, r13
0x000a7988      bne 0xa798e
0x000a798a      jr 0xa7878
0x000a798e      cmp 7, r13
0x000a7990      bgt 0xa79e6
0x000a7992      cmp 3, r13
0x000a7994      bne 0xa799a
0x000a7996      jr 0xa76c2
0x000a799a      cmp 3, r13
0x000a799c      bgt 0xa79c2
0x000a799e      cmp 1, r13
0x000a79a0      bne 0xa79a6
0x000a79a2      jr 0xa76c2
0x000a79a6      cmp 1, r13
0x000a79a8      bgt 0xa79b6
0x000a79aa      cmp r0, r13
0x000a79ac      bne 0xa79b2
0x000a79ae      jr 0xa76c2
0x000a79b2      jr 0xa78a8
0x000a79b6      cmp 2, r13
0x000a79b8      bne 0xa79be
0x000a79ba      jr 0xa76c2
0x000a79be      jr 0xa78a8
0x000a79c2      cmp 5, r13
0x000a79c4      bne 0xa79ca
0x000a79c6      jr 0xa775e
0x000a79ca      cmp 5, r13
0x000a79cc      bgt 0xa79da
0x000a79ce      cmp 4, r13
0x000a79d0      bne 0xa79d6
0x000a79d2      jr 0xa76c2
0x000a79d6      jr 0xa78a8
0x000a79da      cmp 6, r13
0x000a79dc      bne 0xa79e2
0x000a79de      jr 0xa76e0
0x000a79e2      jr 0xa78a8
0x000a79e6      cmp 11, r13
0x000a79e8      bne 0xa79ee
0x000a79ea      jr 0xa76f8
0x000a79ee      cmp 11, r13
0x000a79f0      bgt 0xa7a16
0x000a79f2      cmp 9, r13
0x000a79f4      bne 0xa79fa
0x000a79f6      jr 0xa76f0
0x000a79fa      cmp 9, r13
0x000a79fc      bgt 0xa7a0a
0x000a79fe      cmp 8, r13
0x000a7a00      bne 0xa7a06
0x000a7a02      jr 0xa777c
0x000a7a06      jr 0xa78a8
0x000a7a0a      cmp 10, r13
0x000b69c0      mov r0, r10
0x000b69c2      movea -15836, gp, r14
0x000b69c6      mov r10, r17
0x000b69c8      shl 2, r17
0x000b69ca      add r14, r17
0x000b69cc      mov r17, r11
0x000b69ce      st.b r0, 0[r11]
0x000b69d2      st.h r0, 2[r11]
0x000b69d6      addi 1, r10, r19
0x000b69da      mov r19, r10
0x000b69dc      and r20, r10
0x000b69de      cmp r12, r10
0x000b69e0      blt 0xb69c6
0x000b69e2      movea -30, r0, r9
0x000b69e6      st.b r8, -15714[gp]
0x000b69ea      st.b r0, -17857[gp]
0x000b69ee      st.b r0, -14972[gp]
0x000b69f2      truncated from 0 to 0
