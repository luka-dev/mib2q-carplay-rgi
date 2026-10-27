0012af83  707e 0a3f       I4=R4
0012af85  1607 ffff fffe  I7=MODIFY(I7,-2)
0012af88  717e 813f       R2=I5
0012af8a  9d08 017e       DM(-2,I6)=R2
0012af8c  49be 0abf       I5=DM(M6,I4)
0012af8e  4b7e 013f       R2=DM(M5,I5)
0012af90  9908 0102       DM(+2,I4)=R2
0012af92  4dfe 0e3f       I12=DM(M7,I6)
0012af94  9c08 0afe       I5=DM(-2,I6)
0012af96  083f 343f       JUMP (M14,I12) (DB)
0012af98  1901            RFRAME
0012af99  0001            NOP
