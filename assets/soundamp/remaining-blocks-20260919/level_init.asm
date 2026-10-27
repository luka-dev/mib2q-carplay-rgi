0x000aee1e      0058           mov r0, r11
0x000aee20      24360888       movea -30712, gp, r6
0x000aee24      403e009c       movhi -25600, r0, r7
0x000aee28      24460488       movea -30716, gp, r8
0x000aee2c      244e0088       movea -30720, gp, r9
0x000aee30      206e9cff       movea -100, r0, r13
0x000aee34      24761888       movea -30696, gp, r14
0x000aee38      24cefc87       movea -30724, gp, r25
0x000aee3c      24d6d387       movea -30765, gp, r26
0x000aee40      24dee787       movea -30745, gp, r27
0x000aee44      24e6e387       movea -30749, gp, r28
0x000aee48      24eeec87       movea -30740, gp, r29
0x000aee4c      0b60           mov r11, r12
0x000aee4e      c262           shl 2, r12
0x000aee50      0e50           mov r14, r10
0x000aee52      cc51           add r12, r10
0x000aee54      6a070100       st.w r0, 0[r10]
0x000aee58      0678           mov r6, r15
0x000aee5a      cc79           add r12, r15
0x000aee5c      6f3f0100       st.w r7, 0[r15]
0x000aee60      0880           mov r8, r16
0x000aee62      cb81           add r11, r16
0x000aee64      506f0000       st.b r13, 0[r16]
0x000aee68      0988           mov r9, r17
0x000aee6a      cb89           add r11, r17
0x000aee6c      516f0000       st.b r13, 0[r17]
0x000aee70      1990           mov r25, r18
0x000aee72      cb91           add r11, r18
0x000aee74      526f0000       st.b r13, 0[r18]
0x000aee78      1a98           mov r26, r19
0x000aee7a      cb99           add r11, r19
0x000aee7c      536f0000       st.b r13, 0[r19]
0x000aee80      1b50           mov r27, r10
0x000aee82      cb51           add r11, r10
0x000aee84      4a070000       st.b r0, 0[r10]
0x000aee88      1c78           mov r28, r15
0x000aee8a      cb79           add r11, r15
0x000aee8c      4f070000       st.b r0, 0[r15]
0x000aee90      1d80           mov r29, r16
0x000aee92      cc81           add r12, r16
0x000aee94      70070100       st.w r0, 0[r16]
0x000aee98      0b8e0100       addi 1, r11, r17
0x000aee9c      1158           mov r17, r11
0x000aee9e      5459           and r20, r11
0x000aeea0      645a           cmp 4, r11
0x000aeea2      d6d5           blt 0xaee4c
0x000aeea4      20eea500       movea 165, r0, r29
0x000aeea8      44efe287       st.b r29, -30750[gp]
0x000aeeac      44efe187       st.b r29, -30751[gp]
0x000aeeb0      44efe087       st.b r29, -30752[gp]
0x000aeeb4      6407dd87       st.w r0, -30756[gp]
0x000aeeb8      20967bfc       movea -901, r0, r18
0x000aeebc      6497d987       st.w r18, -30760[gp]
0x000aeec0      049f9fc2       ld.b -15713[gp], r19
0x000aeec4      449fd287       st.b r19, -30766[gp]
0x000aeec8      6407d087       st.h r0, -30768[gp]
0x000aeecc      6407cd87       st.w r0, -30772[gp]
0x000aeed0      6407c987       st.w r0, -30776[gp]
0x000aeed4      6407c587       st.w r0, -30780[gp]
0x000aeed8      6407c187       st.w r0, -30784[gp]
0x000aeedc      045772ba       ld.b -17806[gp], r10
0x000aeee0      4457bf87       st.b r10, -30785[gp]
0x000aeee4      047fd0c1       ld.b -15920[gp], r15
0x000aeee8      447fbe87       st.b r15, -30786[gp]
0x000aeeec      44efbc87       st.b r29, -30788[gp]
0x000aeef0      04878fba       ld.b -17777[gp], r16
0x000aeef4      4487bd87       st.b r16, -30787[gp]
0x000aeef8      0332           mov 3, r6
0x000aeefa      1438           mov r20, r7
0x000aeefc      82ff04e3       jarl 0xdd200, lp
0x000aef00      44ef04a1       st.b r29, -24316[gp]
0x000aef04      4006ff70       dispose 0, {r25, r26, r27, r28, r29, lp}, lp
0x000aef08      8007e170       prepare {r25, r26, r27, r28, r29, lp}, 0
0x000aef0c      958d           br 0xaee1e
0x000aef0e      0000           nop
