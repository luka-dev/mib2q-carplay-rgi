0x000c35f0      a4e795ba       ld.bu -17771[gp], r28
0x000c35f4      84ef97ba       ld.bu -17770[gp], r29
0x000c35f8      0232           mov 2, r6
0x000c35fa      0f3a           mov 15, r7
0x000c35fc      a4478dba       ld.bu -17779[gp], r8
0x000c3600      844f97ba       ld.bu -17770[gp], r9
0x000c3604      82ffec6c       jarl 0xea2f0, lp
0x000c3608      a4578dba       ld.bu -17779[gp], r10
0x000c360c      200ea500       movea 165, r0, r1
0x000c3610      e151           cmp r1, r10
0x000c3612      ca3d           bne 0xc368a
0x000c3614      80ff7801       jarl 0xc378c, lp
0x000c3618      80ffb402       jarl 0xc38cc, lp
0x000c361c      0332           mov 3, r6
0x000c361e      063a           mov 6, r7
0x000c3620      81ffb49c       jarl 0xdd2d4, lp
0x000c3624      047f97ba       ld.b -17769[gp], r15
0x000c3628      447f96ba       st.b r15, -17770[gp]
0x000c362c      048797ba       ld.b -17769[gp], r16
0x000c3630      e081           cmp r0, r16
0x000c3632      ea05           bne 0xc363e
0x000c3634      208e5a00       movea 90, r0, r17                       ; 'Z'
0x000c3638      448f95ba       st.b r17, -17771[gp]
0x000c363c      d505           br 0xc3646
0x000c363e      049798ba       ld.b -17768[gp], r18
0x000c3642      449795ba       st.b r18, -17771[gp]
0x000c3646      a49f95ba       ld.bu -17771[gp], r19
0x000c364a      fc99           cmp r28, r19
0x000c364c      aa0d           bne 0xc3660
0x000c364e      845797ba       ld.bu -17770[gp], r10
0x000c3652      fd51           cmp r29, r10
0x000c3654      ea05           bne 0xc3660
0x000c3656      20361600       movea 22, r0, r6
0x000c365a      80ff3603       jarl 0xc3990, lp
0x000c365e      c515           br 0xc3686
0x000c3660      a47f95ba       ld.bu -17771[gp], r15
0x000c3664      fc79           cmp r28, r15
0x000c3666      b20d           be 0xc367c
0x000c3668      848797ba       ld.bu -17770[gp], r16
0x000c366c      fd81           cmp r29, r16
0x000c366e      fa05           bne 0xc367c
0x000c3670      0132           mov 1, r6
0x000c3672      203e1c00       movea 28, r0, r7
0x000c3676      81ff5e9c       jarl 0xdd2d4, lp
0x000c367a      e505           br 0xc3686
0x000c367c      0132           mov 1, r6
0x000c367e      203e1400       movea 20, r0, r7
0x000c3682      81ff529c       jarl 0xdd2d4, lp
0x000c3686      8007f800       jr 0xc377e
0x000c368a      c4878fba       clr1 0, -17777[gp]
0x000c368e      0068           mov r0, r13
0x000c3690      245e89c5       movea -14967, gp, r11
0x000c3694      24669ac2       movea -15718, gp, r12
0x000c3698      0c88           mov r12, r17
0x000c369a      cd89           add r13, r17
0x000c369c      0b90           mov r11, r18
0x000c369e      cd91           add r13, r18
0x000c36a0      129f0000       ld.b 0[r18], r19
0x000c36a4      519f0000       st.b r19, 0[r17]
0x000c36a8      0d560100       addi 1, r13, r10
0x000c36ac      0a68           mov r10, r13
0x000c36ae      5469           and r20, r13
0x000c36b0      646a           cmp 4, r13
0x000c36b2      b6f5           blt 0xc3698
0x000c36b4      0058           mov r0, r11
0x000c36b6      20361200       movea 18, r0, r6
0x000c36ba      243e9fc5       movea -14945, gp, r7
0x000c36be      246685c2       movea -15739, gp, r12
0x000c36c2      246e8dc5       movea -14963, gp, r13
0x000c36c6      247672c2       movea -15758, gp, r14
0x000c36ca      0c78           mov r12, r15
0x000c36cc      cb79           add r11, r15
0x000c36ce      0d80           mov r13, r16
0x000c36d0      cb81           add r11, r16
0x000c36d2      10970000       ld.b 0[r16], r18
0x000c36d6      4f970000       st.b r18, 0[r15]
0x000c36da      0e88           mov r14, r17
0x000c36dc      cb89           add r11, r17
0x000c36de      0798           mov r7, r19
0x000c36e0      cb99           add r11, r19
0x000c36e2      13570000       ld.b 0[r19], r10
0x000c36e6      51570000       st.b r10, 0[r17]
0x000c36ea      0b860100       addi 1, r11, r16
0x000c36ee      1058           mov r16, r11
0x000c36f0      5459           and r20, r11
0x000c36f2      e659           cmp r6, r11
0x000c36f4      b6ed           blt 0xc36ca
0x000c36f6      047fb1c5       ld.b -14927[gp], r15
0x000c36fa      447f97c2       st.b r15, -15721[gp]
0x000c36fe      0497b2c5       ld.b -14926[gp], r18
0x000c3702      449798c2       st.b r18, -15720[gp]
0x000c3706      049fb3c5       ld.b -14925[gp], r19
0x000c370a      449f99c2       st.b r19, -15719[gp]
0x000c370e      208ea500       movea 165, r0, r17
0x000c3712      448fbec1       st.b r17, -15938[gp]
0x000c3716      0132           mov 1, r6
0x000c3718      0b3a           mov 11, r7
0x000c371a      81ffba9b       jarl 0xdd2d4, lp
0x000c371e      20363100       movea 49, r0, r6                        ; '1'
0x000c3722      80ff6e02       jarl 0xc3990, lp
0x000c3726      0332           mov 3, r6
0x000c3728      063a           mov 6, r7
0x000c372a      81ffaa9b       jarl 0xdd2d4, lp
0x000c372e      0457e6c5       ld.b -14874[gp], r10
0x000c3732      445796ba       st.b r10, -17770[gp]
0x000c3736      048798ba       ld.b -17768[gp], r16
0x000c373a      448795ba       st.b r16, -17771[gp]
0x000c373e      847f99ba       ld.bu -17768[gp], r15
0x000c3742      fc79           cmp r28, r15
0x000c3744      aa0d           bne 0xc3758
0x000c3746      849797ba       ld.bu -17770[gp], r18
0x000c374a      fd91           cmp r29, r18
0x000c374c      ea05           bne 0xc3758
0x000c374e      20361600       movea 22, r0, r6
0x000c3752      80ff3e02       jarl 0xc3990, lp
0x000c3756      c515           br 0xc377e
0x000c3758      a49f95ba       ld.bu -17771[gp], r19
0x000c375c      fc99           cmp r28, r19
0x000c375e      b20d           be 0xc3774
0x000c3760      848f97ba       ld.bu -17770[gp], r17
0x000c3764      fd89           cmp r29, r17
0x000c3766      fa05           bne 0xc3774
0x000c3768      0132           mov 1, r6
0x000c376a      203e1c00       movea 28, r0, r7
0x000c376e      81ff669b       jarl 0xdd2d4, lp
0x000c3772      e505           br 0xc377e
0x000c3774      0132           mov 1, r6
0x000c3776      203e1400       movea 20, r0, r7
0x000c377a      81ff5a9b       jarl 0xdd2d4, lp
0x000c377e      4006ff00       dispose 0, {r28, r29, lp}, lp
0x000c3782      8007e100       prepare {r28, r29, lp}, 0
0x000c3786      bf076afe       jr 0xc35f0
0x000c378a      0000           nop
