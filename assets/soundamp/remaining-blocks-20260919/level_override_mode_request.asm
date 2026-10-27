0x000a1fb4      63370500       st.w r6, 4[sp]                          ; 4
0x000a1fb8      23ef0500       ld.w 4[sp], r29                         ; 4
0x000a1fbc      633f0900       st.w r7, 8[sp]
0x000a1fc0      e3bf0900       ld.hu 8[sp], r23
0x000a1fc4      20b61300       movea 19, r0, r22
0x000a1fc8      20d65a00       movea 90, r0, r26                       ; 'Z'
0x000a1fcc      205e1200       movea 18, r0, r11
0x000a1fd0      9ddf1300       ld.bu 18[r29], r27                      ; 0x12
0x000a1fd4      1660           mov r22, r12
0x000a1fd6      206e1400       movea 20, r0, r13
0x000a1fda      bd571300       ld.bu 19[r29], r10                      ; 0x13
0x000a1fde      c852           shl 8, r10
0x000a1fe0      5551           and r21, r10
0x000a1fe2      9d7f1500       ld.bu 20[r29], r15                      ; 0x14
0x000a1fe6      cf51           add r15, r10
0x000a1fe8      0ae0           mov r10, r28
0x000a1fea      55e1           and r21, r28
0x000a1fec      20761500       movea 21, r0, r14
0x000a1ff0      bdcf1500       ld.bu 21[r29], r25                      ; 0x15
0x000a1ff4      20361700       movea 23, r0, r6
0x000a1ff8      a4c78dba       ld.bu -17779[gp], r24
0x000a1ffc      0532           mov 5, r6
0x000a1ffe      203e5000       movea 80, r0, r7                        ; 'P'
0x000a2002      1c40           mov r28, r8
0x000a2004      0048           mov r0, r9
0x000a2006      84ffea82       jarl 0xea2f0, lp
0x000a200a      200e7701       movea 375, r0, r1
0x000a200e      e1e1           cmp r1, r28
0x000a2010      b205           be 0xa2016
0x000a2012      8007e000       jr 0xa20f2
0x000a2016      848721b6       ld.bu -18912[gp], r16
0x000a201a      200eefff       movea -17, r0, r1
0x000a201e      4181           and r1, r16
0x000a2020      e081           cmp r0, r16
0x000a2022      b20d           be 0xa2038
0x000a2024      1b30           mov r27, r6
0x000a2026      203e2400       movea 36, r0, r7                        ; '$'
0x000a202a      bfff3ae6       jarl 0xa0664, lp
0x000a202e      20d6a500       movea 165, r0, r26
0x000a2032      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a2036      c555           br 0xa20de
0x000a2038      1958           mov r25, r11
0x000a203a      d54d           br 0xa20d4
0x000a203c      a48f35ba       ld.bu -17867[gp], r17
0x000a2040      200ec000       movea 192, r0, r1
0x000a2044      e189           cmp r1, r17
0x000a2046      aa1d           bne 0xa207a
0x000a2048      20964000       movea 64, r0, r18                       ; '@'
0x000a204c      449735ba       st.b r18, -17867[gp]
0x000a2050      249f35a1       ld.w -24268[gp], r19
0x000a2054      139e8813       addi 5000, r19, r19
0x000a2058      649f95b8       st.w r19, -18284[gp]
0x000a205c      207e5a00       movea 90, r0, r15                       ; 'Z'
0x000a2060      447f8dba       st.b r15, -17779[gp]
0x000a2064      c4a720b6       clr1 4, -18912[gp]
0x000a2068      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000a206c      e1c1           cmp r1, r24
0x000a206e      e205           be 0xa207a
0x000a2070      0932           mov 9, r6
0x000a2072      203e1c00       movea 28, r0, r7
0x000a2076      83ff5eb2       jarl 0xdd2d4, lp
0x000a207a      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a207e      8535           br 0xa20de
0x000a2080      61ba           cmp 1, r23
0x000a2082      820d           be 0xa2092
0x000a2084      1b30           mov r27, r6
0x000a2086      1638           mov r22, r7
0x000a2088      bfffdce5       jarl 0xa0664, lp
0x000a208c      20d6a500       movea 165, r0, r26
0x000a2090      a515           br 0xa20b4
0x000a2092      205ea500       movea 165, r0, r11
0x000a2096      2056c000       movea 192, r0, r10
0x000a209a      445735ba       st.b r10, -17867[gp]
0x000a209e      445f8dba       st.b r11, -17779[gp]
0x000a20a2      c42720b6       set1 4, -18912[gp]
0x000a20a6      f859           cmp r24, r11
0x000a20a8      e205           be 0xa20b4
0x000a20aa      0932           mov 9, r6
0x000a20ac      203e1c00       movea 28, r0, r7
0x000a20b0      83ff24b2       jarl 0xdd2d4, lp
0x000a20b4      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a20b8      b515           br 0xa20de
0x000a20ba      1b30           mov r27, r6
0x000a20bc      203e3100       movea 49, r0, r7                        ; '1'
0x000a20c0      bfffa4e5       jarl 0xa0664, lp
0x000a20c4      20d6a500       movea 165, r0, r26
0x000a20c8      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a20cc      950d           br 0xa20de
0x000a20ce      a3d70300       ld.bu 3[sp], r26
0x000a20d2      e505           br 0xa20de
0x000a20d4      e059           cmp r0, r11
0x000a20d6      b2b5           be 0xa203c
0x000a20d8      635a           cmp 3, r11
0x000a20da      b2d5           be 0xa2080
0x000a20dc      f5ed           br 0xa20ba
0x000a20de      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000a20e2      e1d1           cmp r1, r26
0x000a20e4      da05           bne 0xa20ee
0x000a20e6      23370500       ld.w 4[sp], r6                          ; 4
0x000a20ea      80ff2207       jarl 0xa280c, lp
0x000a20ee      8007d400       jr 0xa21c2
0x000a20f2      a4878dba       ld.bu -17779[gp], r16
0x000a20f6      200ea500       movea 165, r0, r1
0x000a20fa      e181           cmp r1, r16
0x000a20fc      b205           be 0xa2102
0x000a20fe      8007ba00       jr 0xa21b8
0x000a2102      205e2404       movea 1060, r0, r11
0x000a2106      ebe1           cmp r11, r28
0x000a2108      ba15           bne 0xa212e
0x000a210a      63ca           cmp 3, r25
0x000a210c      9a0d           bne 0xa211e
0x000a210e      64ba           cmp 4, r23
0x000a2110      e205           be 0xa211c
0x000a2112      1b30           mov r27, r6
0x000a2114      1638           mov r22, r7
0x000a2116      bfff4ee5       jarl 0xa0664, lp
0x000a211a      c555           br 0xa21c2
0x000a211c      850d           br 0xa212c
0x000a211e      e0b9           cmp r0, r23
0x000a2120      e205           be 0xa212c
0x000a2122      1b30           mov r27, r6
0x000a2124      1638           mov r22, r7
0x000a2126      bfff3ee5       jarl 0xa0664, lp
0x000a212a      c54d           br 0xa21c2
0x000a212c      a515           br 0xa2150
0x000a212e      63ca           cmp 3, r25
0x000a2130      9a0d           bne 0xa2142
0x000a2132      62ba           cmp 2, r23
0x000a2134      e205           be 0xa2140
0x000a2136      1b30           mov r27, r6
0x000a2138      1638           mov r22, r7
0x000a213a      bfff2ae5       jarl 0xa0664, lp
0x000a213e      a545           br 0xa21c2
0x000a2140      850d           br 0xa2150
0x000a2142      e0b9           cmp r0, r23
0x000a2144      e205           be 0xa2150
0x000a2146      1b30           mov r27, r6
0x000a2148      1638           mov r22, r7
0x000a214a      bfff1ae5       jarl 0xa0664, lp
0x000a214e      a53d           br 0xa21c2
0x000a2150      1c60           mov r28, r12
0x000a2152      b51d           br 0xa2188
0x000a2154      1d30           mov r29, r6
0x000a2156      80ff7a00       jarl 0xa21d0, lp
0x000a215a      e52d           br 0xa21b6
0x000a215c      1d30           mov r29, r6
0x000a215e      80ff4e01       jarl 0xa22ac, lp
0x000a2162      a52d           br 0xa21b6
0x000a2164      1d30           mov r29, r6
0x000a2166      80ff0602       jarl 0xa236c, lp
0x000a216a      e525           br 0xa21b6
0x000a216c      1d30           mov r29, r6
0x000a216e      80fffe02       jarl 0xa246c, lp
0x000a2172      a525           br 0xa21b6
0x000a2174      1d30           mov r29, r6
0x000a2176      80ffd203       jarl 0xa2548, lp
0x000a217a      e51d           br 0xa21b6
0x000a217c      1d30           mov r29, r6
0x000a217e      80ffb605       jarl 0xa2734, lp
0x000a2182      a51d           br 0xa21b6
0x000a2184      951d           br 0xa21b6
0x000a2186      851d           br 0xa21b6
0x000a2188      200e7b01       movea 379, r0, r1
0x000a218c      e161           cmp r1, r12
0x000a218e      b2e5           be 0xa2154
0x000a2190      200e7c01       movea 380, r0, r1
0x000a2194      e161           cmp r1, r12
0x000a2196      b2f5           be 0xa217c
0x000a2198      200e2304       movea 1059, r0, r1
0x000a219c      e161           cmp r1, r12
0x000a219e      f2dd           be 0xa215c
0x000a21a0      ec59           cmp r12, r11
0x000a21a2      92e5           be 0xa2164
0x000a21a4      200e2504       movea 1061, r0, r1
0x000a21a8      e161           cmp r1, r12
0x000a21aa      92e5           be 0xa216c
0x000a21ac      200e2704       movea 1063, r0, r1
0x000a21b0      e161           cmp r1, r12
0x000a21b2      92e5           be 0xa2174
0x000a21b4      85ed           br 0xa2184
0x000a21b6      e505           br 0xa21c2
0x000a21b8      1b30           mov r27, r6
0x000a21ba      203e2400       movea 36, r0, r7                        ; '$'
0x000a21be      bfffa6e4       jarl 0xa0664, lp
0x000a21c2      4606fff3       dispose 3, {r22, r23, r24, r25, r26, r27, r28, r29, lp}, lp
0x000a21c6      8607e1f3       prepare {r22, r23, r24, r25, r26, r27, r28, r29, lp}, 3
0x000a21ca      bf07eafd       jr 0xa1fb4
0x000a21ce      0000           nop
