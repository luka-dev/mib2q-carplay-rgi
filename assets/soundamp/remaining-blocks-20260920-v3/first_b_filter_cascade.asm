0012daa5  1607 ffff ffc4  I7=MODIFY(I7,-60)
0012daa8  723f 813f       R2=M3
0012daaa  9d08 014b       DM(-53,I6)=R2
0012daac  713e 813f       R2=I1
0012daae  9d08 014c       DM(-52,I6)=R2
0012dab0  713f 013f       R2=I2
0012dab2  9d08 014d       DM(-51,I6)=R2
0012dab4  713f 813f       R2=I3
0012dab6  9d08 014e       DM(-50,I6)=R2
0012dab8  717e 813f       R2=I5
0012daba  161e ffff ffd0  I5=MODIFY(I6,-48)
0012dabd  9d08 06c8       DM(-56,I6)=R13
0012dabf  9d08 07ca       DM(-54,I6)=R15
0012dac1  70fe 0682 1f80  R15=PASS R8 || R13=R12
0012dac4  707e 093f       I2=R4
0012dac6  9d08 03c4       DM(-60,I6)=R7
0012dac8  9d08 04c5       DM(-59,I6)=R9
0012daca  9d08 0546       DM(-58,I6)=R10
0012dacc  9d08 0749       DM(-55,I6)=R14
0012dace  9d08 014f       DM(-49,I6)=R2
0012dad0  9d08 05c7       DM(-57,I6)=R11
0012dad2  163e ffff fff8  I1=MODIFY(I6,-8)
0012dad5  0f8c 0028       R12=0x28
0012dad7  717e 843f       R8=I5
0012dad9  727e 823f       R4=M5
0012dadb  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
0012dade  9ff2            DM(I7,M7)=R2
0012dadf  9fc0 0012 dae1  DM(I7,M7)=0x12dae1
0012dae2  713e 873f       R14=I1
0012dae4  727e 8202 18e0  R8=PASS R14 || R4=M5
0012dae7  0f8c 0006       R12=0x6
0012dae9  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
0012daec  9ff2            DM(I7,M7)=R2
0012daed  9fc0 0012 daef  DM(I7,M7)=0x12daef
0012daf0  4dbe 058c a2d0  F2=FLOAT R13 || R11=DM(M6,I6)
0012daf3  0f07 4f80 0000  R7=0x4f800000
0012daf6  0102 0008 1227  IF LT F2=F2+F7
0012daf9  0f09 3f00 0000  R9=0x3f000000
0012dafc  0f0a 3e80 0000  R10=0x3e800000
0012daff  cf29            F2*=F9
0012db00  c82a            F2+=F10
0012db01  0f01 4f00 0000  R1=0x4f000000
0012db04  0188 2021       F0=F2-F1
0012db06  0122 0008 2201  IF GE F2=F0-F1
0012db09  018c da20       R10=TRUNC F2
0012db0b  c22d            R2=PASS R13
0012db0c  0700 0000 0023  IF EQ JUMP 0x12db2f
0012db0f  70ff 8a3f       I4=R15
0012db11  162e ffff ffd0  I3=MODIFY(I6,-48)
0012db14  1604 0000 0005  I4=MODIFY(I4,+5)
0012db17  0d0d 0000 0015  LCNTR=R13, DO 0x12db2c UNTIL LCE
0012db1a  687e 293f       R2=DM(I4,+5)
0012db1c  97b2            DM(I3,M6)=R2
0012db1d  9808 017a       R2=DM(-6,I4)
0012db1f  97b2            DM(I3,M6)=R2
0012db20  9808 0179       R2=DM(-7,I4)
0012db22  97b2            DM(I3,M6)=R2
0012db23  a802 ffff fff8  R2=DM(-8,I4)
0012db26  57be 8100 0000  DM(I3,M6)=R2
0012db29  a802 ffff fff7  R2=DM(-9,I4)
0012db2c  57be 8100 0000  DM(I3,M6)=R2
0012db2f  c22a            R2=PASS R10
0012db30  0700 0000 0054  IF EQ JUMP 0x12db84
0012db33  717e 87bf       R15=I5
0012db35  043e a93f       I3=MODIFY(I2,M5)
0012db37  0f87 0005       R7=0x5
0012db39  0f89 000a       R9=0xa
0012db3b  477e 0a00 1cf7  R12=R15+R7 || I4=DM(M5,I3)
0012db3e  5ffe 8582 18f0  R8=PASS R15 || DM(I7,M7)=R11
0012db41  6c3e 1200 1ff9  R15=R15+R9 || R4=DM(+2,I6)
0012db44  497e 013f       R2=DM(M5,I4)
0012db46  9d08 0179       DM(-7,I6)=R2
0012db48  477e 0a3f       I4=DM(M5,I3)
0012db4a  9ffe            DM(I7,M7)=R14
0012db4b  49be 013f       R2=DM(M6,I4)
0012db4d  9d08 017a       DM(-6,I6)=R2
0012db4f  47be 0a3f       I4=DM(M6,I3)
0012db51  497e 013f       R2=DM(M5,I4)
0012db53  9d08 017c       DM(-4,I6)=R2
0012db55  47be 0a3f       I4=DM(M6,I3)
0012db57  49be 013f       R2=DM(M6,I4)
0012db59  9d08 017d       DM(-3,I6)=R2
0012db5b  1804 0012 d9a9  CJUMP 0x12d9a9 (DB); R2=I6, I6=I7
0012db5e  9ff2            DM(I7,M7)=R2
0012db5f  9fc0 0012 db61  DM(I7,M7)=0x12db61
0012db62  1607 0000 0002  I7=MODIFY(I7,+2)
0012db65  477e 0a02 aaa0  R10=R10-1 || I4=DM(M5,I3)
0012db68  9c08 0179       R2=DM(-7,I6)
0012db6a  497e 813f       DM(M5,I4)=R2
0012db6c  477e 0a3f       I4=DM(M5,I3)
0012db6e  9c08 017a       R2=DM(-6,I6)
0012db70  1603 0000 0002  I3=MODIFY(I3,+2)
0012db73  49be 813f       DM(M6,I4)=R2
0012db75  47fe 0a3f       I4=DM(M7,I3)
0012db77  9c08 017c       R2=DM(-4,I6)
0012db79  497e 813f       DM(M5,I4)=R2
0012db7b  47fe 0a3f       I4=DM(M7,I3)
0012db7d  0720 04ff ffbe  IF NE JUMP 0x12db3b (DB)
0012db80  9c08 017d       R2=DM(-3,I6)
0012db82  49be 813f       DM(M6,I4)=R2
0012db84  023e 0033 000d  BTST R13 BY 0
0012db87  0730 0000 0023  IF NOT SZ JUMP 0x12dbaa
0012db8a  4dfe 0e3f       I12=DM(M7,I6)
0012db8c  727e 803f       R0=M5
0012db8e  9c08 11cb       M3=DM(-53,I6)
0012db90  9c08 08cc       I1=DM(-52,I6)
0012db92  9c08 094d       I2=DM(-51,I6)
0012db94  9c08 09ce       I3=DM(-50,I6)
0012db96  9c08 0acf       I5=DM(-49,I6)
0012db98  9c08 03c4       R7=DM(-60,I6)
0012db9a  9c08 04c5       R9=DM(-59,I6)
0012db9c  9c08 0546       R10=DM(-58,I6)
0012db9e  9c08 05c7       R11=DM(-57,I6)
0012dba0  9c08 06c8       R13=DM(-56,I6)
0012dba2  9c08 0749       R14=DM(-55,I6)
0012dba4  9c08 07ca       R15=DM(-54,I6)
0012dba6  083f 343f       JUMP (M14,I12) (DB)
0012dba8  1901            RFRAME
0012dba9  0001            NOP
0012dbaa  043f f802 a2d0  R2=R13-1 || I7=MODIFY(I7,M7)
0012dbad  703f 1182 1ce0  R12=PASS R14 || M3=R2
0012dbb0  0f81 0005       R1=0x5
0012dbb2  717e 8117 0112  R1=R1*R2 (SSI) || R2=I5
0012dbb5  44fe 0a00 1821  R8=R2+R1 || I4=DM(M3,I2)
0012dbb8  9c08 0202       R4=DM(+2,I6)
0012dbba  9ffb            DM(I7,M7)=R11
0012dbbb  497e 013f       R2=DM(M5,I4)
0012dbbd  9d08 0179       DM(-7,I6)=R2
0012dbbf  44fe 0a3f       I4=DM(M3,I2)
0012dbc1  49be 013f       R2=DM(M6,I4)
0012dbc3  9d08 017a       DM(-6,I6)=R2
0012dbc5  1804 0012 da2f  CJUMP 0x12da2f (DB); R2=I6, I6=I7
0012dbc8  9ff2            DM(I7,M7)=R2
0012dbc9  9fc0 0012 dbcb  DM(I7,M7)=0x12dbcb
0012dbcc  1607 0000 0002  I7=MODIFY(I7,+2)
0012dbcf  44fe 0a3f       I4=DM(M3,I2)
0012dbd1  9c08 0179       R2=DM(-7,I6)
0012dbd3  497e 813f       DM(M5,I4)=R2
0012dbd5  44fe 0a3f       I4=DM(M3,I2)
0012dbd7  073e 04ff ffb3  JUMP 0x12db8a (DB)
0012dbda  9c08 017a       R2=DM(-6,I6)
0012dbdc  49be 813f       DM(M6,I4)=R2
