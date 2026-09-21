0x000a254c      63370500       st.w r6, 4[sp]                          ; 4
0x000a2550      23ef0500       ld.w 4[sp], r29                         ; 4
0x000a2554      20365a00       movea 90, r0, r6                        ; 'Z'
0x000a2558      0648           mov r6, r9
0x000a255a      20461200       movea 18, r0, r8
0x000a255e      9d6f1300       ld.bu 18[r29], r13                      ; 0x12
0x000a2562      203e1500       movea 21, r0, r7
0x000a2566      bd671500       ld.bu 21[r29], r12                      ; 0x15
0x000a256a      20761700       movea 23, r0, r14
0x000a256e      0c58           mov r12, r11
0x000a2570      852d           br 0xa25c0
0x000a2572      2056a500       movea 165, r0, r10
0x000a2576      445794ba       st.b r10, -17772[gp]
0x000a257a      434f0300       st.b r9, 3[sp]
0x000a257e      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a2582      c525           br 0xa25ca
0x000a2584      1d78           mov r29, r15
0x000a2586      ce79           add r14, r15
0x000a2588      cfc70000       tst1 0, 0[r15]
0x000a258c      e205           be 0xa2598
0x000a258e      2086a500       movea 165, r0, r16
0x000a2592      448794ba       st.b r16, -17772[gp]
0x000a2596      b505           br 0xa259c
0x000a2598      443794ba       st.b r6, -17772[gp]
0x000a259c      434f0300       st.b r9, 3[sp]
0x000a25a0      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a25a4      b515           br 0xa25ca
0x000a25a6      0d30           mov r13, r6
0x000a25a8      203e3100       movea 49, r0, r7                        ; '1'
0x000a25ac      bfffb8e0       jarl 0xa0664, lp
0x000a25b0      208ea500       movea 165, r0, r17
0x000a25b4      438f0300       st.b r17, 3[sp]
0x000a25b8      63ef0500       st.w r29, 4[sp]                         ; 4
0x000a25bc      f505           br 0xa25ca
0x000a25be      e505           br 0xa25ca
0x000a25c0      e059           cmp r0, r11
0x000a25c2      82dd           be 0xa2572
0x000a25c4      635a           cmp 3, r11
0x000a25c6      f2dd           be 0xa2584
0x000a25c8      f5ed           br 0xa25a6
0x000a25ca      0532           mov 5, r6
0x000a25cc      203e5600       movea 86, r0, r7                        ; 'V'
0x000a25d0      844795ba       ld.bu -17772[gp], r8
0x000a25d4      a44f8dba       ld.bu -17779[gp], r9
0x000a25d8      84ff187d       jarl 0xea2f0, lp
0x000a25dc      03970300       ld.b 3[sp], r18
0x000a25e0      200e5a00       movea 90, r0, r1                        ; 'Z'
0x000a25e4      e191           cmp r1, r18
0x000a25e6      da05           bne 0xa25f0
0x000a25e8      23370500       ld.w 4[sp], r6                          ; 4
0x000a25ec      80ff2002       jarl 0xa280c, lp
0x000a25f0      44067f00       dispose 2, {r29, lp}, lp
0x000a25f4      84076100       prepare {r29, lp}, 2
0x000a25f8      a5ad           br 0xa254c
0x000a25fa      0000           nop
