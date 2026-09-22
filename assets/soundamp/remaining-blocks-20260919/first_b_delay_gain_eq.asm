0012eb9b  1607 ffff fff6  I7=MODIFY(I7,-10)
0012eb9e  717e 813f       R2=I5
0012eba0  6cbf ad82 1bc0  R11=PASS R12 || DM(-11,I6)=R11
0012eba3  9d08 0777       DM(-9,I6)=R14
0012eba5  6cbf b682 1d80  R13=PASS R8 || DM(-10,I6)=R13
0012eba8  9d08 0179       DM(-7,I6)=R2
0012ebaa  9d08 07f8       DM(-8,I6)=R15
0012ebac  9c08 0102       R2=DM(+2,I6)
0012ebae  9d08 017d       DM(-3,I6)=R2
0012ebb0  9d08 017a       DM(-6,I6)=R2
0012ebb2  9c08 0103       R2=DM(+3,I6)
0012ebb4  1616 ffff fffd  I4=MODIFY(I6,-3)
0012ebb7  9d08 017e       DM(-2,I6)=R2
0012ebb9  9d08 017b       DM(-5,I6)=R2
0012ebbb  707e 0abf       I5=R4
0012ebbd  043f f83f       I7=MODIFY(I7,M7)
0012ebbf  717e 013f       R2=I4
0012ebc1  9ff2            DM(I7,M7)=R2
0012ebc2  4dbe 07bf       R15=DM(M6,I6)
0012ebc4  9fc4 0001       DM(I7,M7)=1
0012ebc6  9fff            DM(I7,M7)=R15
0012ebc7  4bbe 023f       R4=DM(M6,I5)
0012ebc9  1804 0012 f411  CJUMP 0x12f411 (DB); R2=I6, I6=I7
0012ebcc  9ff2            DM(I7,M7)=R2
0012ebcd  9fc0 0012 ebcf  DM(I7,M7)=0x12ebcf
0012ebd0  703e 073f       R14=R0
0012ebd2  c22e            R2=PASS R14
0012ebd3  1607 0000 0004  I7=MODIFY(I7,+4)
0012ebd6  0700 0000 001f  IF EQ JUMP 0x12ebf5
0012ebd9  4b7e 0402 14e0  R4=PASS R14 || R8=DM(M5,I5)
0012ebdc  0f8c 0006       R12=0x6
0012ebde  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012ebe1  9ff2            DM(I7,M7)=R2
0012ebe2  9fc0 0012 ebe4  DM(I7,M7)=0x12ebe4
0012ebe5  4dfe 0e3f       I12=DM(M7,I6)
0012ebe7  9c08 0af9       I5=DM(-7,I6)
0012ebe9  9c08 05f5       R11=DM(-11,I6)
0012ebeb  9c08 06f6       R13=DM(-10,I6)
0012ebed  9c08 0777       R14=DM(-9,I6)
0012ebef  9c08 07f8       R15=DM(-8,I6)
0012ebf1  083f 343f       JUMP (M14,I12) (DB)
0012ebf3  1901            RFRAME
0012ebf4  0001            NOP
0012ebf5  1616 ffff fffe  I4=MODIFY(I6,-2)
0012ebf8  717e 0102 1cb0  R12=PASS R11 || R2=I4
0012ebfb  043f f802 98d0  R8=R13+1 || I7=MODIFY(I7,M7)
0012ebfe  9ff2            DM(I7,M7)=R2
0012ebff  9fc4 0001       DM(I7,M7)=1
0012ec01  9fff            DM(I7,M7)=R15
0012ec02  9a08 0202       R4=DM(+2,I5)
0012ec04  1804 0012 f411  CJUMP 0x12f411 (DB); R2=I6, I6=I7
0012ec07  9ff2            DM(I7,M7)=R2
0012ec08  9fc0 0012 ec0a  DM(I7,M7)=0x12ec0a
0012ec0b  c220            R2=PASS R0
0012ec0c  1607 0000 0004  I7=MODIFY(I7,+4)
0012ec0f  0700 0000 001f  IF EQ JUMP 0x12ec2e
0012ec12  4b7e 0402 1400  R4=PASS R0 || R8=DM(M5,I5)
0012ec15  0f8c 0006       R12=0x6
0012ec17  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012ec1a  9ff2            DM(I7,M7)=R2
0012ec1b  9fc0 0012 ec1d  DM(I7,M7)=0x12ec1d
0012ec1e  4dfe 0e3f       I12=DM(M7,I6)
0012ec20  9c08 0af9       I5=DM(-7,I6)
0012ec22  9c08 05f5       R11=DM(-11,I6)
0012ec24  9c08 06f6       R13=DM(-10,I6)
0012ec26  9c08 0777       R14=DM(-9,I6)
0012ec28  9c08 07f8       R15=DM(-8,I6)
0012ec2a  083f 343f       JUMP (M14,I12) (DB)
0012ec2c  1901            RFRAME
0012ec2d  0001            NOP
0012ec2e  043f f802 18b0  R8=PASS R11 || I7=MODIFY(I7,M7)
0012ec31  1616 ffff fffa  I4=MODIFY(I6,-6)
0012ec34  0f8c 0002       R12=0x2
0012ec36  717e 0780 14dc  R4=R13+R12 || R15=I4
0012ec39  9fff            DM(I7,M7)=R15
0012ec3a  1804 0013 2d0b  CJUMP 0x132d0b (DB); R2=I6, I6=I7
0012ec3d  9ff2            DM(I7,M7)=R2
0012ec3e  9fc0 0012 ec40  DM(I7,M7)=0x12ec40
0012ec41  9f08 0782       DM(+2,I7)=R15
0012ec43  0f82 0003       R2=0x3
0012ec45  0f9c 0002       I12=0x2
0012ec47  4fbe 8e00 18d2  R8=R13+R2 || DM(M6,I7)=I12
0012ec4a  6a3e 1a02 1cb0  R12=PASS R11 || R4=DM(+3,I5)
0012ec4d  1804 0012 dbde  CJUMP 0x12dbde (DB); R2=I6, I6=I7
0012ec50  9ff2            DM(I7,M7)=R2
0012ec51  9fc0 0012 ec53  DM(I7,M7)=0x12ec53
0012ec54  703e 0202 10e0  R0=PASS R14 || R4=R0
0012ec57  c224            R2=PASS R4
0012ec58  0700 04ff ff8d  IF EQ JUMP 0x12ebe5 (DB)
0012ec5b  1607 0000 0002  I7=MODIFY(I7,+2)
0012ec5e  0001            NOP
0012ec5f  4b7e 043f       R8=DM(M5,I5)
0012ec61  0f8c 0006       R12=0x6
0012ec63  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012ec66  9ff2            DM(I7,M7)=R2
0012ec67  9fc0 0012 ec69  DM(I7,M7)=0x12ec69
0012ec6a  4dfe 0e3f       I12=DM(M7,I6)
0012ec6c  9c08 0af9       I5=DM(-7,I6)
0012ec6e  9c08 05f5       R11=DM(-11,I6)
0012ec70  9c08 06f6       R13=DM(-10,I6)
0012ec72  9c08 0777       R14=DM(-9,I6)
0012ec74  9c08 07f8       R15=DM(-8,I6)
0012ec76  083f 343f       JUMP (M14,I12) (DB)
0012ec78  1901            RFRAME
0012ec79  0001            NOP
