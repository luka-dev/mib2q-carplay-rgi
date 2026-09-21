00132bb4  707e 0a3f       I4=R4
00132bb6  1607 ffff fffe  I7=MODIFY(I7,-2)
00132bb9  717e 813f       R2=I5
00132bbb  9d08 07fd       DM(-3,I6)=R15
00132bbd  9d08 017e       DM(-2,I6)=R2
00132bbf  043f 313f       I5=MODIFY(I4,M6)
00132bc1  727e 87bf       R15=M5
00132bc3  4bbe 0a3f       I4=DM(M6,I5)
00132bc5  70ff 9202 9ff0  R15=R15+1 || M4=R15
00132bc8  9b5c            R12=DM(I5,M5)
00132bc9  727e 823f       R4=M5
00132bcb  493e 043f       R8=DM(M4,I4)
00132bcd  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
00132bd0  9ff2            DM(I7,M7)=R2
00132bd1  9fc0 0013 2bd3  DM(I7,M7)=0x132bd3
00132bd4  023e 7800 ff2f  R2=LSHIFT R15 BY -1
00132bd7  0710 00ff ffec  IF SZ JUMP 0x132bc3
00132bda  4dfe 0e3f       I12=DM(M7,I6)
00132bdc  9c08 0afe       I5=DM(-2,I6)
00132bde  9c08 07fd       R15=DM(-3,I6)
00132be0  083f 343f       JUMP (M14,I12) (DB)
00132be2  1901            RFRAME
00132be3  0001            NOP
