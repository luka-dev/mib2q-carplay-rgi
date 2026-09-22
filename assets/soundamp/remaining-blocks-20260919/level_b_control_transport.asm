0x000ac648      e53d           br 0xac6c4
0x000ac64a      63370100       st.w r6, 0[sp]
0x000ac64e      23370100       ld.w 0[sp], r6
0x000ac652      633f0500       st.w r7, 4[sp]                          ; 4
0x000ac656      0058           mov r0, r11
0x000ac658      2a060a0a0a0a   mov 0xa0a0a0a, r10                      ; '\n\n\n\n'
0x000ac65e      6457c1bc       st.w r10, -17216[gp]
0x000ac662      247f41be       ld.w -16832[gp], r15
0x000ac666      400e0040       movhi 16384, r0, r1
0x000ac66a      2179           xor r1, r15
0x000ac66c      647f41be       st.w r15, -16832[gp]
0x000ac670      40660500       movhi 5, r0, r12
0x000ac674      016a           mov 1, r13
0x000ac676      2476c4bc       movea -17212, gp, r14
0x000ac67a      248741be       ld.w -16832[gp], r16
0x000ac67e      210602000500   mov 0x50002, r1
0x000ac684      0181           or r1, r16
0x000ac686      6487c5bc       st.w r16, -17212[gp]
0x000ac68a      024a           mov 2, r9
0x000ac68c      3106e14a0c00   mov 0xc4ae1, r17
0x000ac692      648fc9bc       st.w r17, -17208[gp]
0x000ac696      0040           mov r0, r8
0x000ac698      6437cdbc       st.w r6, -17204[gp]
0x000ac69c      043a           mov 4, r7
0x000ac69e      23970500       ld.w 4[sp], r18                         ; 4
0x000ac6a2      6497d1bc       st.w r18, -17200[gp]
0x000ac6a6      bfff5ec6       jarl 0xa8d04, lp
0x000ac6aa      bfffc2c5       jarl 0xa8c6c, lp
0x000ac6ae      243628bb       movea -17624, gp, r6
0x000ac6b2      203e1800       movea 24, r0, r7
0x000ac6b6      83ff1e6e       jarl 0xe34d4, lp
0x000ac6ba      0432           mov 4, r6
0x000ac6bc      83ffdc07       jarl 0xdce98, lp
0x000ac6c0      44063f00       dispose 2, {lp}, lp
0x000ac6c4      84072100       prepare {lp}, 2
0x000ac6c8      95c5           br 0xac64a
0x000ac6ca      0000           nop
