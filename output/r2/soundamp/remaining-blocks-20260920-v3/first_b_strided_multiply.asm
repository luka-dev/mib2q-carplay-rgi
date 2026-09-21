0012d6a3  1607 ffff fffc  I7=MODIFY(I7,-4)
0012d6a6  723f 013f       R2=M2
0012d6a8  9d08 017b       DM(-5,I6)=R2
0012d6aa  723f 813f       R2=M3
0012d6ac  9d08 017c       DM(-4,I6)=R2
0012d6ae  713f 813f       R2=I3
0012d6b0  9d08 017d       DM(-3,I6)=R2
0012d6b2  717e 813f       R2=I5
0012d6b4  9d08 017e       DM(-2,I6)=R2
0012d6b6  9c08 0104       R2=DM(+4,I6)
0012d6b8  70be 1202 1220  R2=PASS R2 || M4=R8
0012d6bb  707e 0a3f       I4=R4
0012d6bd  70fe 0abf       I5=R12
0012d6bf  4dbe 11bf       M3=DM(M6,I6)
0012d6c1  0700 0400 002a  IF EQ JUMP 0x12d6eb (DB)
0012d6c4  9c08 0982       I3=DM(+2,I6)
0012d6c6  9c08 1103       M2=DM(+3,I6)
0012d6c8  593e 0102 a020  R0=R2-1 || R2=DM(I4,M4)
0012d6cb  9ad1            R1=DM(I5,M3)
0012d6cc  0700 0000 000f  IF EQ JUMP 0x12d6db
0012d6cf  0d00 0000 0009  LCNTR=R0, DO 0x12d6d8 UNTIL LCE
0012d6d2  593e 0113 0112  F1=F1*F2 || R2=DM(I4,M4)
0012d6d5  56be 8080 0000  DM(I3,M2)=R1
0012d6d8  5afe 0080 0000  R1=DM(I5,M3)
0012d6db  cf21            F2*=F1
0012d6dc  96b2            DM(I3,M2)=R2
0012d6dd  4dfe 0e3f       I12=DM(M7,I6)
0012d6df  9c08 117b       M2=DM(-5,I6)
0012d6e1  9c08 11fc       M3=DM(-4,I6)
0012d6e3  9c08 0afe       I5=DM(-2,I6)
0012d6e5  9c08 09fd       I3=DM(-3,I6)
0012d6e7  083f 343f       JUMP (M14,I12) (DB)
0012d6e9  1901            RFRAME
0012d6ea  0001            NOP
0012d6eb  4dfe 0e3f       I12=DM(M7,I6)
0012d6ed  9c08 117b       M2=DM(-5,I6)
0012d6ef  9c08 11fc       M3=DM(-4,I6)
0012d6f1  9c08 09fd       I3=DM(-3,I6)
0012d6f3  9c08 0afe       I5=DM(-2,I6)
0012d6f5  083f 343f       JUMP (M14,I12) (DB)
0012d6f7  1901            RFRAME
0012d6f8  0001            NOP
