0012faf2  1607 ffff fffe  I7=MODIFY(I7,-2)
0012faf5  717e 813f       R2=I5
0012faf7  9d08 017e       DM(-2,I6)=R2
0012faf9  1002 000b 903a  R2=DM(0xb903a)
0012fafc  0180 b024       COMPU(R2,R4)
0012fafe  0724 0000 0038  IF GT JUMP 0x12fb36
0012fb01  707e 0a00 2242  R2=R4-R2 || I4=R4
0012fb04  1015 000b 903b  I5=DM(0xb903b)
0012fb07  703f 123f       M4=R2
0012fb09  497e 013f       R2=DM(M5,I4)
0012fb0b  4b3e 00bf       R1=DM(M4,I5)
0012fb0d  cb12            COMP(F1,F2)
0012fb0e  0700 0000 0028  IF EQ JUMP 0x12fb36
0012fb11  0f15 000b 8cfd  I5=0xb8cfd
0012fb14  0189 2c12       F12=ABS(F1-F2)
0012fb16  4b3e 003f       R0=DM(M4,I5)
0012fb18  018b 0800       F8=ABS F0
0012fb1a  cbc8            COMP(F12,F8)
0012fb1b  0724 0000 0011  IF GT JUMP 0x12fb2c
0012fb1e  497e 80bf       DM(M5,I4)=R1
0012fb20  0f9c 0000       I12=0x0
0012fb22  4b3e 8e3f       DM(M4,I5)=I12
0012fb24  4dfe 0e3f       I12=DM(M7,I6)
0012fb26  9c08 0afe       I5=DM(-2,I6)
0012fb28  083f 343f       JUMP (M14,I12) (DB)
0012fb2a  1901            RFRAME
0012fb2b  0001            NOP
0012fb2c  c820            F2+=F0
0012fb2d  9972            DM(I4,M5)=R2
0012fb2e  4dfe 0e3f       I12=DM(M7,I6)
0012fb30  9c08 0afe       I5=DM(-2,I6)
0012fb32  083f 343f       JUMP (M14,I12) (DB)
0012fb34  1901            RFRAME
0012fb35  0001            NOP
0012fb36  4dfe 0e3f       I12=DM(M7,I6)
0012fb38  727e 803f       R0=M5
0012fb3a  9c08 0afe       I5=DM(-2,I6)
0012fb3c  083f 343f       JUMP (M14,I12) (DB)
0012fb3e  1901            RFRAME
0012fb3f  0001            NOP
