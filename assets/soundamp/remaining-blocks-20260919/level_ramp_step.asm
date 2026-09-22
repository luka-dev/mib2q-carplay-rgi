0x000af828      0050           mov r0, r10
0x000af82a      2466ec87       movea -30740, gp, r12
0x000af82e      0a78           mov r10, r15
0x000af830      c27a           shl 2, r15
0x000af832      cc79           add r12, r15
0x000af834      2f870100       ld.w 0[r15], r16
0x000af838      e081           cmp r0, r16
0x000af83a      ba05           bne 0xaf840
0x000af83c      80076402       jr 0xafaa0
0x000af840      24ceacc3       movea -15444, gp, r25
0x000af844      0a88           mov r10, r17
0x000af846      c28a           shl 2, r17
0x000af848      d989           add r25, r17
0x000af84a      11970200       ld.b 2[r17], r18
0x000af84e      6192           cmp 1, r18
0x000af850      b205           be 0xaf856
0x000af852      80078201       jr 0xaf9d4
0x000af856      24e60888       movea -30712, gp, r28
0x000af85a      24de1888       movea -30696, gp, r27
0x000af85e      40d6804b       movhi 19328, r0, r26
0x000af862      2e06588d0600   mov 0x68d58, r14
0x000af868      249efc87       movea -30724, gp, r19
0x000af86c      ca99           add r10, r19
0x000af86e      137f0000       ld.b 0[r19], r15
0x000af872      e87f440a       mul 40, r15, r1
0x000af876      2f78           not r15, r15
0x000af878      417a           add 1, r15
0x000af87a      ce79           add r14, r15
0x000af87c      2f874106       ld.w 1600[r15], r16                     ; 0x640 ; 1600
0x000af880      1a30           mov r26, r6
0x000af882      1038           mov r16, r7
0x000af884      83ff00c1       jarl 0xeb984, lp
0x000af888      0680           mov r6, r16
0x000af88a      1030           mov r16, r6
0x000af88c      83fff6c2       jarl 0xebb82, lp
0x000af890      0680           mov r6, r16
0x000af892      63870100       st.w r16, 0[sp]
0x000af896      0ab8           mov r10, r23
0x000af898      c2ba           shl 2, r23
0x000af89a      0c88           mov r12, r17
0x000af89c      d789           add r23, r17
0x000af89e      31c70100       ld.w 0[r17], r24
0x000af8a2      1b90           mov r27, r18
0x000af8a4      d791           add r23, r18
0x000af8a6      329f0100       ld.w 0[r18], r19
0x000af8aa      247f35a1       ld.w -24268[gp], r15
0x000af8ae      b379           sub r19, r15
0x000af8b0      f87f2002       mul r24, r15, r0
0x000af8b4      23ef0100       ld.w 0[sp], r29
0x000af8b8      cfe9           add r15, r29
0x000af8ba      1d30           mov r29, r6
0x000af8bc      83ffe8c3       jarl 0xebca4, lp
0x000af8c0      0680           mov r6, r16
0x000af8c2      1a30           mov r26, r6
0x000af8c4      1038           mov r16, r7
0x000af8c6      83ff06c3       jarl 0xebbcc, lp
0x000af8ca      0680           mov r6, r16
0x000af8cc      1068           mov r16, r13
0x000af8ce      1c88           mov r28, r17
0x000af8d0      d789           add r23, r17
0x000af8d2      31970100       ld.w 0[r17], r18
0x000af8d6      400e0001       movhi 256, r0, r1
0x000af8da      e197c002       div r1, r18, r0
0x000af8de      1258           mov r18, r11
0x000af8e0      e0c1           cmp r0, r24
0x000af8e2      e735           ble 0xaf94e
0x000af8e4      0b98           mov r11, r19
0x000af8e6      e89f4402       mul 40, r19, r0
0x000af8ea      3398           not r19, r19
0x000af8ec      419a           add 1, r19
0x000af8ee      ce99           add r14, r19
0x000af8f0      337f4106       ld.w 1600[r19], r15                     ; 0x640 ; 1600
0x000af8f4      0d30           mov r13, r6
0x000af8f6      0f38           mov r15, r7
0x000af8f8      e5474000       stsr psw, r8
0x000af8fc      83fff0c1       jarl 0xebaec, lp
0x000af900      e62f2000       ldsr r6, psw
0x000af904      c925           bnl 0xaf94c
0x000af906      24ee0088       movea -30720, gp, r29
0x000af90a      1d80           mov r29, r16
0x000af90c      ca81           add r10, r16
0x000af90e      108f0000       ld.b 0[r16], r17
0x000af912      eb89           cmp r11, r17
0x000af914      c71d           ble 0xaf94c
0x000af916      0b960100       addi 1, r11, r18
0x000af91a      1258           mov r18, r11
0x000af91c      ab00           sxb r11
0x000af91e      0b98           mov r11, r19
0x000af920      e89f4402       mul 40, r19, r0
0x000af924      3398           not r19, r19
0x000af926      419a           add 1, r19
0x000af928      ce99           add r14, r19
0x000af92a      337f4106       ld.w 1600[r19], r15                     ; 0x640 ; 1600
0x000af92e      0d30           mov r13, r6
0x000af930      0f38           mov r15, r7
0x000af932      e5474000       stsr psw, r8
0x000af936      83ffb6c1       jarl 0xebaec, lp
0x000af93a      e62f2000       ldsr r6, psw
0x000af93e      f905           bnl 0xaf94c
0x000af940      1d80           mov r29, r16
0x000af942      ca81           add r10, r16
0x000af944      108f0000       ld.b 0[r16], r17
0x000af948      eb89           cmp r11, r17
0x000af94a      efe5           bgt 0xaf916
0x000af94c      c53d           br 0xaf9c4
0x000af94e      0a90           mov r10, r18
0x000af950      c292           shl 2, r18
0x000af952      cc91           add r12, r18
0x000af954      329f0100       ld.w 0[r18], r19
0x000af958      e099           cmp r0, r19
0x000af95a      de35           bge 0xaf9c4
0x000af95c      0b78           mov r11, r15
0x000af95e      e87f4402       mul 40, r15, r0
0x000af962      2f78           not r15, r15
0x000af964      417a           add 1, r15
0x000af966      ce79           add r14, r15
0x000af968      2f874106       ld.w 1600[r15], r16                     ; 0x640 ; 1600
0x000af96c      0d30           mov r13, r6
0x000af96e      1038           mov r16, r7
0x000af970      e5474000       stsr psw, r8
0x000af974      83ff78c1       jarl 0xebaec, lp
0x000af978      e62f2000       ldsr r6, psw
0x000af97c      c325           bnh 0xaf9c4
0x000af97e      24ee0088       movea -30720, gp, r29
0x000af982      1d88           mov r29, r17
0x000af984      ca89           add r10, r17
0x000af986      11970000       ld.b 0[r17], r18
0x000af98a      eb91           cmp r11, r18
0x000af98c      ce1d           bge 0xaf9c4
0x000af98e      0b9effff       addi -1, r11, r19
0x000af992      1358           mov r19, r11
0x000af994      ab00           sxb r11
0x000af996      0b78           mov r11, r15
0x000af998      e87f4402       mul 40, r15, r0
0x000af99c      2f78           not r15, r15
0x000af99e      417a           add 1, r15
0x000af9a0      ce79           add r14, r15
0x000af9a2      2f874106       ld.w 1600[r15], r16                     ; 0x640 ; 1600
0x000af9a6      0d30           mov r13, r6
0x000af9a8      1038           mov r16, r7
0x000af9aa      e5474000       stsr psw, r8
0x000af9ae      83ff3ec1       jarl 0xebaec, lp
0x000af9b2      e62f2000       ldsr r6, psw
0x000af9b6      f305           bnh 0xaf9c4
0x000af9b8      1d88           mov r29, r17
0x000af9ba      ca89           add r10, r17
0x000af9bc      11970000       ld.b 0[r17], r18
0x000af9c0      eb91           cmp r11, r18
0x000af9c2      e6e5           blt 0xaf98e
0x000af9c4      0a98           mov r10, r19
0x000af9c6      c29a           shl 2, r19
0x000af9c8      dc99           add r28, r19
0x000af9ca      0b78           mov r11, r15
0x000af9cc      d87a           shl 24, r15
0x000af9ce      737f0100       st.w r15, 0[r19]
0x000af9d2      f51d           br 0xafa10
0x000af9d4      24e60888       movea -30712, gp, r28
0x000af9d8      24de1888       movea -30696, gp, r27
0x000af9dc      0a58           mov r10, r11
0x000af9de      c25a           shl 2, r11
0x000af9e0      1c80           mov r28, r16
0x000af9e2      cb81           add r11, r16
0x000af9e4      1b88           mov r27, r17
0x000af9e6      cb89           add r11, r17
0x000af9e8      31970100       ld.w 0[r17], r18
0x000af9ec      249f35a1       ld.w -24268[gp], r19
0x000af9f0      b299           sub r18, r19
0x000af9f2      0c78           mov r12, r15
0x000af9f4      cb79           add r11, r15
0x000af9f6      2f8f0100       ld.w 0[r15], r17
0x000af9fa      f19f200a       mul r17, r19, r1
0x000af9fe      2496fc87       movea -30724, gp, r18
0x000afa02      ca91           add r10, r18
0x000afa04      127f0000       ld.b 0[r18], r15
0x000afa08      d87a           shl 24, r15
0x000afa0a      cf99           add r15, r19
0x000afa0c      709f0100       st.w r19, 0[r16]
0x000afa10      0a88           mov r10, r17
0x000afa12      c28a           shl 2, r17
0x000afa14      cc89           add r12, r17
0x000afa16      31970100       ld.w 0[r17], r18
0x000afa1a      e091           cmp r0, r18
0x000afa1c      e70d           ble 0xafa38
0x000afa1e      247e0088       movea -30720, gp, r15
0x000afa22      ca79           add r10, r15
0x000afa24      0f870000       ld.b 0[r15], r16
0x000afa28      d882           shl 24, r16
0x000afa2a      0a98           mov r10, r19
0x000afa2c      c29a           shl 2, r19
0x000afa2e      dc99           add r28, r19
0x000afa30      338f0100       ld.w 0[r19], r17
0x000afa34      f181           cmp r17, r16
0x000afa36      c625           blt 0xafa7e
0x000afa38      0a90           mov r10, r18
0x000afa3a      c292           shl 2, r18
0x000afa3c      cc91           add r12, r18
0x000afa3e      327f0100       ld.w 0[r18], r15
0x000afa42      e079           cmp r0, r15
0x000afa44      ee0d           bge 0xafa60
0x000afa46      249e0088       movea -30720, gp, r19
0x000afa4a      ca99           add r10, r19
0x000afa4c      13870000       ld.b 0[r19], r16
0x000afa50      d882           shl 24, r16
0x000afa52      0a88           mov r10, r17
0x000afa54      c28a           shl 2, r17
0x000afa56      dc89           add r28, r17
0x000afa58      31970100       ld.w 0[r17], r18
0x000afa5c      f281           cmp r18, r16
0x000afa5e      8f15           bgt 0xafa7e
0x000afa60      0a58           mov r10, r11
0x000afa62      c25a           shl 2, r11
0x000afa64      1b78           mov r27, r15
0x000afa66      cb79           add r11, r15
0x000afa68      2f9f0100       ld.w 0[r15], r19
0x000afa6c      1988           mov r25, r17
0x000afa6e      cb89           add r11, r17
0x000afa70      f1870100       ld.hu 0[r17], r16
0x000afa74      d099           add r16, r19
0x000afa76      249735a1       ld.w -24268[gp], r18
0x000afa7a      f299           cmp r18, r19
0x000afa7c      9b15           bh 0xafa9e
0x000afa7e      0a58           mov r10, r11
0x000afa80      c25a           shl 2, r11
0x000afa82      1c78           mov r28, r15
0x000afa84      cb79           add r11, r15
0x000afa86      248e0088       movea -30720, gp, r17
0x000afa8a      ca89           add r10, r17
0x000afa8c      11870000       ld.b 0[r17], r16
0x000afa90      d882           shl 24, r16
0x000afa92      6f870100       st.w r16, 0[r15]
0x000afa96      0c98           mov r12, r19
0x000afa98      cb99           add r11, r19
0x000afa9a      73070100       st.w r0, 0[r19]
0x000afa9e      c52d           br 0xafaf6
0x000afaa0      24e60888       movea -30712, gp, r28
0x000afaa4      0a90           mov r10, r18
0x000afaa6      c292           shl 2, r18
0x000afaa8      dc91           add r28, r18
0x000afaaa      248e0088       movea -30720, gp, r17
0x000afaae      ca89           add r10, r17
0x000afab0      117f0000       ld.b 0[r17], r15
0x000afab4      d87a           shl 24, r15
0x000afab6      727f0100       st.w r15, 0[r18]
0x000afaba      e051           cmp r0, r10
0x000afabc      9a15           bne 0xafade
0x000afabe      0682           mov 6, r16
0x000afac0      4487aec3       st.b r16, -15442[gp]
0x000afac4      209e2c01       movea 300, r0, r19
0x000afac8      649facc3       st.h r19, -15444[gp]
0x000afacc      048f8fba       ld.b -17777[gp], r17
0x000afad0      e089           cmp r0, r17
0x000afad2      da05           bne 0xafadc
0x000afad4      2096a500       movea 165, r0, r18
0x000afad8      4497bc87       st.b r18, -30788[gp]
0x000afadc      d50d           br 0xafaf6
0x000afade      0a78           mov r10, r15
0x000afae0      c27a           shl 2, r15
0x000afae2      245eacc3       movea -15444, gp, r11
0x000afae6      cf59           add r15, r11
0x000afae8      0282           mov 2, r16
0x000afaea      4b870200       st.b r16, 2[r11]
0x000afaee      209e3200       movea 50, r0, r19                       ; '2'
0x000afaf2      6b9f0000       st.h r19, 0[r11]
0x000afaf6      0a8e0100       addi 1, r10, r17
0x000afafa      1150           mov r17, r10
0x000afafc      5451           and r20, r10
0x000afafe      6452           cmp 4, r10
0x000afb00      be05           bge 0xafb06
0x000afb02      bf072cfd       jr 0xaf82e
0x000afb06      4206fff1       dispose 1, {r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000afb0a      8207e1f1       prepare {r23, r24, r25, r26, r27, r28, r29, lp}, 1
0x000afb0e      bf071afd       jr 0xaf828
0x000afb12      0000           nop
