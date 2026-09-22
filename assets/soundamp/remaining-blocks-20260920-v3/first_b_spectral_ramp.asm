0012ad70  1607 ffff fff8  I7=MODIFY(I7,-8)
0012ad73  6cbf c382 1740  R7=PASS R4 || DM(-8,I6)=R7
0012ad76  6cbf cc82 1980  R9=PASS R8 || DM(-7,I6)=R9
0012ad79  6cbf ef08 2494  F4=F9-F4 || DM(-3,I6)=R14
0012ad7c  9d08 07fe       DM(-2,I6)=R15
0012ad7e  9d08 057a       DM(-6,I6)=R10
0012ad80  9d08 05fb       DM(-5,I6)=R11
0012ad82  9d08 06fc       DM(-4,I6)=R13
0012ad84  70fe 07bf       R15=R12
0012ad86  4dbe 053f       R10=DM(M6,I6)
0012ad88  6c3e 1680 21af  R1=R10-R15 || R13=DM(+2,I6)
0012ad8b  6c3e 1d8c a810  F8=FLOAT R1 || R11=DM(+3,I6)
0012ad8e  07be 04ff 95c6  CALL 0x124354 (DB)
0012ad91  0f02 4f80 0000  R2=0x4f800000
0012ad94  0102 0008 1882  IF LT F8=F8+F2
0012ad97  703e 0702 a2d0  R2=R13-1 || R14=R0
0012ad9a  0180 b02f       COMPU(R2,R15)
0012ad9c  0702 0000 0053  IF LT JUMP 0x12adef
0012ad9f  0180 b02a       COMPU(R2,R10)
0012ada1  0702 0000 004e  IF LT JUMP 0x12adef
0012ada4  0180 b0af       COMPU(R10,R15)
0012ada6  0702 0000 0049  IF LT JUMP 0x12adef
0012ada9  6c3e 1c02 12f0  R2=PASS R15 || R8=DM(+3,I6)
0012adac  7241 07bf       IF EQ R15=M6
0012adae  707f 8202 1cf0  R12=PASS R15 || R4=R7
0012adb1  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
0012adb4  9ff2            DM(I7,M7)=R2
0012adb5  9fc0 0012 adb7  DM(I7,M7)=0x12adb7
0012adb8  0180 b0fa       COMPU(R15,R10)
0012adba  0722 0000 0015  IF GE JUMP 0x12adcf
0012adbd  c62b            R2=R11-1
0012adbe  c02f            R2+=R15
0012adbf  703f 0a00 21af  R1=R10-R15 || I4=R2
0012adc2  c611            R1=R1-1
0012adc3  0700 0400 000b  IF EQ JUMP 0x12adce (DB)
0012adc6  9992            R2=DM(I4,M6)
0012adc7  c82e            F2+=F14
0012adc8  0d01 0000 0003  LCNTR=R1, DO 0x12adcb UNTIL LCE
0012adcb  59be 8108 122e  F2=F2+F14 || DM(I4,M6)=R2
0012adce  99b2            DM(I4,M6)=R2
0012adcf  70be 8200 18ba  R8=R11+R10 || R4=R9
0012add2  0180 2cda       R12=R13-R10
0012add4  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
0012add7  9ff2            DM(I7,M7)=R2
0012add8  9fc0 0012 adda  DM(I7,M7)=0x12adda
0012addb  4dfe 0e3f       I12=DM(M7,I6)
0012addd  9c08 03f8       R7=DM(-8,I6)
0012addf  9c08 04f9       R9=DM(-7,I6)
0012ade1  9c08 057a       R10=DM(-6,I6)
0012ade3  9c08 05fb       R11=DM(-5,I6)
0012ade5  9c08 06fc       R13=DM(-4,I6)
0012ade7  9c08 077d       R14=DM(-3,I6)
0012ade9  9c08 07fe       R15=DM(-2,I6)
0012adeb  083f 343f       JUMP (M14,I12) (DB)
0012aded  1901            RFRAME
0012adee  0001            NOP
0012adef  727e 823f       R4=M5
0012adf1  9c08 0602       R12=DM(+2,I6)
0012adf3  9c08 0403       R8=DM(+3,I6)
0012adf5  1804 0012 d7dc  CJUMP 0x12d7dc (DB); R2=I6, I6=I7
0012adf8  9ff2            DM(I7,M7)=R2
0012adf9  9fc0 0012 adfb  DM(I7,M7)=0x12adfb
0012adfc  073e 00ff ffad  JUMP 0x12ada9
