0012af9a  1607 ffff fffc  I7=MODIFY(I7,-4)
0012af9d  717e 813f       R2=I5
0012af9f  9d08 077c       DM(-4,I6)=R14
0012afa1  707e 073f       R14=R4
0012afa3  9d08 017e       DM(-2,I6)=R2
0012afa5  9d08 06fb       DM(-5,I6)=R13
0012afa7  6cbf ef82 1f80  R15=PASS R8 || DM(-3,I6)=R15
0012afaa  0f84 4400       R4=0x4400
0012afac  1804 0012 b683  CJUMP 0x12b683 (DB); R2=I6, I6=I7
0012afaf  9ff2            DM(I7,M7)=R2
0012afb0  9fc0 0012 afb2  DM(I7,M7)=0x12afb2
0012afb3  0f8c 2000       R12=0x2000
0012afb5  0f88 000c       R8=0xc
0012afb7  727f 0202 1d00  R13=PASS R0 || R4=M6
0012afba  1804 0013 2d56  CJUMP 0x132d56 (DB); R2=I6, I6=I7
0012afbd  9ff2            DM(I7,M7)=R2
0012afbe  9fc0 0012 afc0  DM(I7,M7)=0x12afc0
0012afc1  703e 0a82 1200  R2=PASS R0 || I5=R0
0012afc4  0700 0000 0030  IF EQ JUMP 0x12aff4
0012afc7  70ff 8a3f       I4=R15
0012afc9  9bbd            DM(I5,M6)=R13
0012afca  9bbf            DM(I5,M6)=R15
0012afcb  497e 013f       R2=DM(M5,I4)
0012afcd  9bb2            DM(I5,M6)=R2
0012afce  9b84 0000       DM(I5,M6)=0
0012afd0  9b84 0000       DM(I5,M6)=0
0012afd2  9b84 0000       DM(I5,M6)=0
0012afd4  9b84 0000       DM(I5,M6)=0
0012afd6  9b84 0000       DM(I5,M6)=0
0012afd8  9b84 0000       DM(I5,M6)=0
0012afda  70ff 0a3f       I4=R14
0012afdc  9b84 0000       DM(I5,M6)=0
0012afde  9b84 0000       DM(I5,M6)=0
0012afe0  9b84 0000       DM(I5,M6)=0
0012afe2  497e 803f       DM(M5,I4)=R0
0012afe4  4dfe 0e3f       I12=DM(M7,I6)
0012afe6  727e 803f       R0=M5
0012afe8  9c08 0afe       I5=DM(-2,I6)
0012afea  9c08 06fb       R13=DM(-5,I6)
0012afec  9c08 077c       R14=DM(-4,I6)
0012afee  9c08 07fd       R15=DM(-3,I6)
0012aff0  083f 343f       JUMP (M14,I12) (DB)
0012aff2  1901            RFRAME
0012aff3  0001            NOP
0012aff4  727f 0602 18d0  R8=PASS R13 || R12=M6
0012aff7  0f04 0000 8000  R4=0x8000
0012affa  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012affd  9ff2            DM(I7,M7)=R2
0012affe  9fc0 0012 b000  DM(I7,M7)=0x12b000
0012b001  4dfe 0e3f       I12=DM(M7,I6)
0012b003  9c08 0afe       I5=DM(-2,I6)
0012b005  9c08 06fb       R13=DM(-5,I6)
0012b007  9c08 077c       R14=DM(-4,I6)
0012b009  9c08 07fd       R15=DM(-3,I6)
0012b00b  083f 343f       JUMP (M14,I12) (DB)
0012b00d  1901            RFRAME
0012b00e  0001            NOP
