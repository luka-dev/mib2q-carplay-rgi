0012d3e0  707e 0a3f       I4=R4
0012d3e2  727e 823f       R4=M5
0012d3e4  9984 0001       DM(I4,M6)=1
0012d3e6  9958            R8=DM(I4,M5)
0012d3e7  49be 063f       R12=DM(M6,I4)
0012d3e9  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
0012d3ec  9ff2            DM(I7,M7)=R2
0012d3ed  9fc0 0012 d3ef  DM(I7,M7)=0x12d3ef
0012d3f0  4dfe 0e3f       I12=DM(M7,I6)
0012d3f2  083f 343f       JUMP (M14,I12) (DB)
0012d3f4  1901            RFRAME
0012d3f5  0001            NOP
