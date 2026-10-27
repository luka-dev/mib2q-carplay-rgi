0x000dd814      b50d           br 0xdd82a
0x000dd816      63370100       st.w r6, 0[sp]
0x000dd81a      035f0000       ld.b 0[sp], r11
0x000dd81e      0b30           mov r11, r6
0x000dd820      80ffbcd2       jarl 0xeaadc, lp
0x000dd824      0a58           mov r10, r11
0x000dd826      42063f00       dispose 1, {lp}, lp
0x000dd82a      82072100       prepare {lp}, 1
0x000dd82e      c5f5           br 0xdd816
0x000dd830      b50d           br 0xdd846
0x000dd832      63370100       st.w r6, 0[sp]
0x000dd836      035f0000       ld.b 0[sp], r11
0x000dd83a      0b30           mov r11, r6
0x000dd83c      80ff08d3       jarl 0xeab44, lp
0x000dd840      0a58           mov r10, r11
0x000dd842      42063f00       dispose 1, {lp}, lp
0x000dd846      82072100       prepare {lp}, 1
0x000dd84a      c5f5           br 0xdd832
