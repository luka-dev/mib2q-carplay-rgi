0x000eaadc      1f42           mov -1, r8
0x000eaade      206ecbff       movea -53, r0, r13
0x000eaae2      850d           br 0xeaaf2
0x000eaae4      0042           mov 0, r8
0x000eaae6      206e95ff       movea -107, r0, r13
0x000eaaea      c505           br 0xeaaf2
0x000eaaec      0740           mov r7, r8
0x000eaaee      206e55ff       movea -171, r0, r13
0x000eaaf2      0660           mov r6, r12
0x000eaaf4      800fc008       jarl 0xeb3b4, r1
0x000eaaf8      065f03b2       ld.b -19965[r6], r11
0x000eaafc      0052           mov 0, r10
0x000eaafe      605a           cmp 0, r11
0x000eab00      e205           be 0xeab0c
0x000eab02      5f5a           add -1, r11
0x000eab04      465f03b2       st.b r11, -19965[r6]
0x000eab08      8007c408       jr 0xeb3cc
0x000eab0c      6042           cmp 0, r8
0x000eab0e      f215           be 0xeab3c
0x000eab10      006704b0       ld.b 0xffffb004, r12
0x000eab14      0c68           mov r12, r13
0x000eab16      c36a           shl 3, r13
0x000eab18      800fd008       jarl 0xeb3e8, r1
0x000eab1c      4d3721b0       st.b r6, -20447[r13]
0x000eab20      205e0600       movea 6, r0, r11
0x000eab24      4d5f22b0       st.b r11, -20446[r13]
0x000eab28      265effb1       movea -19969, r6, r11
0x000eab2c      0d3720b0       ld.b -20448[r13], r6
0x000eab30      800f1409       jarl 0xeb444, r1
0x000eab34      1f52           mov -1, r10
0x000eab36      80077a09       jr 0xeb4b0
0x000eab3a      0000           nop
0x000eab3c      2056abff       movea -85, r0, r10
0x000eab40      80078c08       jr 0xeb3cc
0x000eab44      206ec9ff       movea -55, r0, r13
0x000eab48      0660           mov r6, r12
0x000eab4a      800f6a08       jarl 0xeb3b4, r1
0x000eab4e      0647ffb1       ld.b -19969[r6], r8
0x000eab52      7f42           cmp -1, r8
0x000eab54      ea0d           bne 0xeab70
0x000eab56      064703b2       ld.b -19965[r6], r8
0x000eab5a      204e7f00       movea 127, r0, r9                       ; '\x7f'
0x000eab5e      e849           cmp r8, r9
0x000eab60      e21d           be 0xeab9c
0x000eab62      4142           add 1, r8
0x000eab64      464703b2       st.b r8, -19965[r6]
0x000eab68      0052           mov 0, r10
0x000eab6a      80076208       jr 0xeb3cc
0x000eab6e      0000           nop
0x000eab70      2656ffb1       movea -19969, r6, r10
0x000eab74      0830           mov r8, r6
0x000eab76      803f0e09       jarl 0xeb484, r7
0x000eab7a      066fefed       ld.b -4625[r6], r13
0x000eab7e      0052           mov 0, r10
0x000eab80      c36a           shl 3, r13
0x000eab82      2d5f22b0       ld.h -20446[r13], r11
0x000eab86      cb5ee0ff       andi 65504, r11, r11
0x000eab8a      8b5e0004       ori 1024, r11, r11
0x000eab8e      6d5f22b0       st.h r11, -20446[r13]
0x000eab92      4d5721b0       st.b r10, -20447[r13]
0x000eab96      80072609       jr 0xeb4bc
0x000eab9a      0000           nop
0x000eab9c      2056b7ff       movea -73, r0, r10
0x000eaba0      80072c08       jr 0xeb3cc
