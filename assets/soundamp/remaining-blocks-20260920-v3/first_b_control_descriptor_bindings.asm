0012bc52  1607 ffff fffc  I7=MODIFY(I7,-4)
0012bc55  9d08 077d       DM(-3,I6)=R14
0012bc57  707e 073f       R14=R4
0012bc59  6cbf dd82 1b80  R11=PASS R8 || DM(-5,I6)=R11
0012bc5c  9d08 06fc       DM(-4,I6)=R13
0012bc5e  9d08 07fe       DM(-2,I6)=R15
0012bc60  0f84 2500       R4=0x2500
0012bc62  1804 0012 b683  CJUMP 0x12b683 (DB); R2=I6, I6=I7
0012bc65  9ff2            DM(I7,M7)=R2
0012bc66  9fc0 0012 bc68  DM(I7,M7)=0x12bc68
0012bc69  0f82 002c       R2=0x2c
0012bc6b  703e 0780 14e2  R4=R14+R2 || R15=R0
0012bc6e  0f82 0023       R2=0x23
0012bc70  0180 18b2       R8=R11+R2
0012bc72  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bc75  9ff2            DM(I7,M7)=R2
0012bc76  9fc0 0012 bc78  DM(I7,M7)=0x12bc78
0012bc79  703e 06bf       R13=R0
0012bc7b  c22d            R2=PASS R13
0012bc7c  0700 0000 001d  IF EQ JUMP 0x12bc99
0012bc7f  70ff 8402 14d0  R4=PASS R13 || R8=R15
0012bc82  0f8c 0005       R12=0x5
0012bc84  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bc87  9ff2            DM(I7,M7)=R2
0012bc88  9fc0 0012 bc8a  DM(I7,M7)=0x12bc8a
0012bc8b  4dfe 0e3f       I12=DM(M7,I6)
0012bc8d  9c08 05fb       R11=DM(-5,I6)
0012bc8f  9c08 06fc       R13=DM(-4,I6)
0012bc91  9c08 077d       R14=DM(-3,I6)
0012bc93  9c08 07fe       R15=DM(-2,I6)
0012bc95  083f 343f       JUMP (M14,I12) (DB)
0012bc97  1901            RFRAME
0012bc98  0001            NOP
0012bc99  0f82 002d       R2=0x2d
0012bc9b  0180 14e2       R4=R14+R2
0012bc9d  0f82 004f       R2=0x4f
0012bc9f  0180 18b2       R8=R11+R2
0012bca1  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bca4  9ff2            DM(I7,M7)=R2
0012bca5  9fc0 0012 bca7  DM(I7,M7)=0x12bca7
0012bca8  c220            R2=PASS R0
0012bca9  0700 0000 001d  IF EQ JUMP 0x12bcc6
0012bcac  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bcaf  0f8c 0005       R12=0x5
0012bcb1  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bcb4  9ff2            DM(I7,M7)=R2
0012bcb5  9fc0 0012 bcb7  DM(I7,M7)=0x12bcb7
0012bcb8  4dfe 0e3f       I12=DM(M7,I6)
0012bcba  9c08 05fb       R11=DM(-5,I6)
0012bcbc  9c08 06fc       R13=DM(-4,I6)
0012bcbe  9c08 077d       R14=DM(-3,I6)
0012bcc0  9c08 07fe       R15=DM(-2,I6)
0012bcc2  083f 343f       JUMP (M14,I12) (DB)
0012bcc4  1901            RFRAME
0012bcc5  0001            NOP
0012bcc6  0f82 002e       R2=0x2e
0012bcc8  0180 14e2       R4=R14+R2
0012bcca  0f82 007a       R2=0x7a
0012bccc  0180 18b2       R8=R11+R2
0012bcce  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bcd1  9ff2            DM(I7,M7)=R2
0012bcd2  9fc0 0012 bcd4  DM(I7,M7)=0x12bcd4
0012bcd5  c220            R2=PASS R0
0012bcd6  0700 0000 001d  IF EQ JUMP 0x12bcf3
0012bcd9  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bcdc  0f8c 0005       R12=0x5
0012bcde  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bce1  9ff2            DM(I7,M7)=R2
0012bce2  9fc0 0012 bce4  DM(I7,M7)=0x12bce4
0012bce5  4dfe 0e3f       I12=DM(M7,I6)
0012bce7  9c08 05fb       R11=DM(-5,I6)
0012bce9  9c08 06fc       R13=DM(-4,I6)
0012bceb  9c08 077d       R14=DM(-3,I6)
0012bced  9c08 07fe       R15=DM(-2,I6)
0012bcef  083f 343f       JUMP (M14,I12) (DB)
0012bcf1  1901            RFRAME
0012bcf2  0001            NOP
0012bcf3  0f82 002f       R2=0x2f
0012bcf5  0180 14e2       R4=R14+R2
0012bcf7  0f82 0093       R2=0x93
0012bcf9  0180 18b2       R8=R11+R2
0012bcfb  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bcfe  9ff2            DM(I7,M7)=R2
0012bcff  9fc0 0012 bd01  DM(I7,M7)=0x12bd01
0012bd02  c220            R2=PASS R0
0012bd03  0700 0000 001d  IF EQ JUMP 0x12bd20
0012bd06  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bd09  0f8c 0005       R12=0x5
0012bd0b  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bd0e  9ff2            DM(I7,M7)=R2
0012bd0f  9fc0 0012 bd11  DM(I7,M7)=0x12bd11
0012bd12  4dfe 0e3f       I12=DM(M7,I6)
0012bd14  9c08 05fb       R11=DM(-5,I6)
0012bd16  9c08 06fc       R13=DM(-4,I6)
0012bd18  9c08 077d       R14=DM(-3,I6)
0012bd1a  9c08 07fe       R15=DM(-2,I6)
0012bd1c  083f 343f       JUMP (M14,I12) (DB)
0012bd1e  1901            RFRAME
0012bd1f  0001            NOP
0012bd20  0f82 0030       R2=0x30
0012bd22  0180 14e2       R4=R14+R2
0012bd24  0f82 00c6       R2=0xc6
0012bd26  0180 18b2       R8=R11+R2
0012bd28  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bd2b  9ff2            DM(I7,M7)=R2
0012bd2c  9fc0 0012 bd2e  DM(I7,M7)=0x12bd2e
0012bd2f  c220            R2=PASS R0
0012bd30  0700 0000 001d  IF EQ JUMP 0x12bd4d
0012bd33  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bd36  0f8c 0005       R12=0x5
0012bd38  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bd3b  9ff2            DM(I7,M7)=R2
0012bd3c  9fc0 0012 bd3e  DM(I7,M7)=0x12bd3e
0012bd3f  4dfe 0e3f       I12=DM(M7,I6)
0012bd41  9c08 05fb       R11=DM(-5,I6)
0012bd43  9c08 06fc       R13=DM(-4,I6)
0012bd45  9c08 077d       R14=DM(-3,I6)
0012bd47  9c08 07fe       R15=DM(-2,I6)
0012bd49  083f 343f       JUMP (M14,I12) (DB)
0012bd4b  1901            RFRAME
0012bd4c  0001            NOP
0012bd4d  0f82 0031       R2=0x31
0012bd4f  0180 14e2       R4=R14+R2
0012bd51  0f82 00f9       R2=0xf9
0012bd53  0180 18b2       R8=R11+R2
0012bd55  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bd58  9ff2            DM(I7,M7)=R2
0012bd59  9fc0 0012 bd5b  DM(I7,M7)=0x12bd5b
0012bd5c  c220            R2=PASS R0
0012bd5d  0700 0000 001d  IF EQ JUMP 0x12bd7a
0012bd60  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bd63  0f8c 0005       R12=0x5
0012bd65  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bd68  9ff2            DM(I7,M7)=R2
0012bd69  9fc0 0012 bd6b  DM(I7,M7)=0x12bd6b
0012bd6c  4dfe 0e3f       I12=DM(M7,I6)
0012bd6e  9c08 05fb       R11=DM(-5,I6)
0012bd70  9c08 06fc       R13=DM(-4,I6)
0012bd72  9c08 077d       R14=DM(-3,I6)
0012bd74  9c08 07fe       R15=DM(-2,I6)
0012bd76  083f 343f       JUMP (M14,I12) (DB)
0012bd78  1901            RFRAME
0012bd79  0001            NOP
0012bd7a  0f82 0032       R2=0x32
0012bd7c  0180 14e2       R4=R14+R2
0012bd7e  0f82 0125       R2=0x125
0012bd80  0180 18b2       R8=R11+R2
0012bd82  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bd85  9ff2            DM(I7,M7)=R2
0012bd86  9fc0 0012 bd88  DM(I7,M7)=0x12bd88
0012bd89  c220            R2=PASS R0
0012bd8a  0700 0000 001d  IF EQ JUMP 0x12bda7
0012bd8d  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bd90  0f8c 0005       R12=0x5
0012bd92  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bd95  9ff2            DM(I7,M7)=R2
0012bd96  9fc0 0012 bd98  DM(I7,M7)=0x12bd98
0012bd99  4dfe 0e3f       I12=DM(M7,I6)
0012bd9b  9c08 05fb       R11=DM(-5,I6)
0012bd9d  9c08 06fc       R13=DM(-4,I6)
0012bd9f  9c08 077d       R14=DM(-3,I6)
0012bda1  9c08 07fe       R15=DM(-2,I6)
0012bda3  083f 343f       JUMP (M14,I12) (DB)
0012bda5  1901            RFRAME
0012bda6  0001            NOP
0012bda7  0f82 0033       R2=0x33
0012bda9  0180 14e2       R4=R14+R2
0012bdab  0f82 0179       R2=0x179
0012bdad  0180 18b2       R8=R11+R2
0012bdaf  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bdb2  9ff2            DM(I7,M7)=R2
0012bdb3  9fc0 0012 bdb5  DM(I7,M7)=0x12bdb5
0012bdb6  c220            R2=PASS R0
0012bdb7  0700 0000 001d  IF EQ JUMP 0x12bdd4
0012bdba  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bdbd  0f8c 0005       R12=0x5
0012bdbf  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bdc2  9ff2            DM(I7,M7)=R2
0012bdc3  9fc0 0012 bdc5  DM(I7,M7)=0x12bdc5
0012bdc6  4dfe 0e3f       I12=DM(M7,I6)
0012bdc8  9c08 05fb       R11=DM(-5,I6)
0012bdca  9c08 06fc       R13=DM(-4,I6)
0012bdcc  9c08 077d       R14=DM(-3,I6)
0012bdce  9c08 07fe       R15=DM(-2,I6)
0012bdd0  083f 343f       JUMP (M14,I12) (DB)
0012bdd2  1901            RFRAME
0012bdd3  0001            NOP
0012bdd4  0f82 0034       R2=0x34
0012bdd6  0180 14e2       R4=R14+R2
0012bdd8  0f82 013c       R2=0x13c
0012bdda  0180 18b2       R8=R11+R2
0012bddc  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bddf  9ff2            DM(I7,M7)=R2
0012bde0  9fc0 0012 bde2  DM(I7,M7)=0x12bde2
0012bde3  c220            R2=PASS R0
0012bde4  0700 0000 001d  IF EQ JUMP 0x12be01
0012bde7  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bdea  0f8c 0005       R12=0x5
0012bdec  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bdef  9ff2            DM(I7,M7)=R2
0012bdf0  9fc0 0012 bdf2  DM(I7,M7)=0x12bdf2
0012bdf3  4dfe 0e3f       I12=DM(M7,I6)
0012bdf5  9c08 05fb       R11=DM(-5,I6)
0012bdf7  9c08 06fc       R13=DM(-4,I6)
0012bdf9  9c08 077d       R14=DM(-3,I6)
0012bdfb  9c08 07fe       R15=DM(-2,I6)
0012bdfd  083f 343f       JUMP (M14,I12) (DB)
0012bdff  1901            RFRAME
0012be00  0001            NOP
0012be01  0f82 0035       R2=0x35
0012be03  0180 14e2       R4=R14+R2
0012be05  0f82 013d       R2=0x13d
0012be07  0180 18b2       R8=R11+R2
0012be09  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012be0c  9ff2            DM(I7,M7)=R2
0012be0d  9fc0 0012 be0f  DM(I7,M7)=0x12be0f
0012be10  c220            R2=PASS R0
0012be11  0700 0000 001d  IF EQ JUMP 0x12be2e
0012be14  70ff 8402 1400  R4=PASS R0 || R8=R15
0012be17  0f8c 0005       R12=0x5
0012be19  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012be1c  9ff2            DM(I7,M7)=R2
0012be1d  9fc0 0012 be1f  DM(I7,M7)=0x12be1f
0012be20  4dfe 0e3f       I12=DM(M7,I6)
0012be22  9c08 05fb       R11=DM(-5,I6)
0012be24  9c08 06fc       R13=DM(-4,I6)
0012be26  9c08 077d       R14=DM(-3,I6)
0012be28  9c08 07fe       R15=DM(-2,I6)
0012be2a  083f 343f       JUMP (M14,I12) (DB)
0012be2c  1901            RFRAME
0012be2d  0001            NOP
0012be2e  0f82 0036       R2=0x36
0012be30  0180 14e2       R4=R14+R2
0012be32  0f82 01a4       R2=0x1a4
0012be34  0180 18b2       R8=R11+R2
0012be36  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012be39  9ff2            DM(I7,M7)=R2
0012be3a  9fc0 0012 be3c  DM(I7,M7)=0x12be3c
0012be3d  c220            R2=PASS R0
0012be3e  0700 0000 001d  IF EQ JUMP 0x12be5b
0012be41  70ff 8402 1400  R4=PASS R0 || R8=R15
0012be44  0f8c 0005       R12=0x5
0012be46  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012be49  9ff2            DM(I7,M7)=R2
0012be4a  9fc0 0012 be4c  DM(I7,M7)=0x12be4c
0012be4d  4dfe 0e3f       I12=DM(M7,I6)
0012be4f  9c08 05fb       R11=DM(-5,I6)
0012be51  9c08 06fc       R13=DM(-4,I6)
0012be53  9c08 077d       R14=DM(-3,I6)
0012be55  9c08 07fe       R15=DM(-2,I6)
0012be57  083f 343f       JUMP (M14,I12) (DB)
0012be59  1901            RFRAME
0012be5a  0001            NOP
0012be5b  0f82 0037       R2=0x37
0012be5d  0180 14e2       R4=R14+R2
0012be5f  0f82 0133       R2=0x133
0012be61  0180 18b2       R8=R11+R2
0012be63  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012be66  9ff2            DM(I7,M7)=R2
0012be67  9fc0 0012 be69  DM(I7,M7)=0x12be69
0012be6a  c220            R2=PASS R0
0012be6b  0700 0000 001d  IF EQ JUMP 0x12be88
0012be6e  70ff 8402 1400  R4=PASS R0 || R8=R15
0012be71  0f8c 0005       R12=0x5
0012be73  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012be76  9ff2            DM(I7,M7)=R2
0012be77  9fc0 0012 be79  DM(I7,M7)=0x12be79
0012be7a  4dfe 0e3f       I12=DM(M7,I6)
0012be7c  9c08 05fb       R11=DM(-5,I6)
0012be7e  9c08 06fc       R13=DM(-4,I6)
0012be80  9c08 077d       R14=DM(-3,I6)
0012be82  9c08 07fe       R15=DM(-2,I6)
0012be84  083f 343f       JUMP (M14,I12) (DB)
0012be86  1901            RFRAME
0012be87  0001            NOP
0012be88  0f82 0038       R2=0x38
0012be8a  0180 14e2       R4=R14+R2
0012be8c  0f82 0138       R2=0x138
0012be8e  0180 18b2       R8=R11+R2
0012be90  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012be93  9ff2            DM(I7,M7)=R2
0012be94  9fc0 0012 be96  DM(I7,M7)=0x12be96
0012be97  c220            R2=PASS R0
0012be98  0700 0000 001d  IF EQ JUMP 0x12beb5
0012be9b  70ff 8402 1400  R4=PASS R0 || R8=R15
0012be9e  0f8c 0005       R12=0x5
0012bea0  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bea3  9ff2            DM(I7,M7)=R2
0012bea4  9fc0 0012 bea6  DM(I7,M7)=0x12bea6
0012bea7  4dfe 0e3f       I12=DM(M7,I6)
0012bea9  9c08 05fb       R11=DM(-5,I6)
0012beab  9c08 06fc       R13=DM(-4,I6)
0012bead  9c08 077d       R14=DM(-3,I6)
0012beaf  9c08 07fe       R15=DM(-2,I6)
0012beb1  083f 343f       JUMP (M14,I12) (DB)
0012beb3  1901            RFRAME
0012beb4  0001            NOP
0012beb5  0f82 0039       R2=0x39
0012beb7  0180 14e2       R4=R14+R2
0012beb9  0f82 0187       R2=0x187
0012bebb  0180 18b2       R8=R11+R2
0012bebd  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012bec0  9ff2            DM(I7,M7)=R2
0012bec1  9fc0 0012 bec3  DM(I7,M7)=0x12bec3
0012bec4  c220            R2=PASS R0
0012bec5  0700 0000 001d  IF EQ JUMP 0x12bee2
0012bec8  70ff 8402 1400  R4=PASS R0 || R8=R15
0012becb  0f8c 0005       R12=0x5
0012becd  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bed0  9ff2            DM(I7,M7)=R2
0012bed1  9fc0 0012 bed3  DM(I7,M7)=0x12bed3
0012bed4  4dfe 0e3f       I12=DM(M7,I6)
0012bed6  9c08 05fb       R11=DM(-5,I6)
0012bed8  9c08 06fc       R13=DM(-4,I6)
0012beda  9c08 077d       R14=DM(-3,I6)
0012bedc  9c08 07fe       R15=DM(-2,I6)
0012bede  083f 343f       JUMP (M14,I12) (DB)
0012bee0  1901            RFRAME
0012bee1  0001            NOP
0012bee2  0f82 003a       R2=0x3a
0012bee4  0180 14e2       R4=R14+R2
0012bee6  0f82 018c       R2=0x18c
0012bee8  0180 18b2       R8=R11+R2
0012beea  1804 0012 af9a  CJUMP 0x12af9a (DB); R2=I6, I6=I7
0012beed  9ff2            DM(I7,M7)=R2
0012beee  9fc0 0012 bef0  DM(I7,M7)=0x12bef0
0012bef1  c220            R2=PASS R0
0012bef2  0720 0000 0015  IF NE JUMP 0x12bf07
0012bef5  70ff 0a3f       I4=R14
0012bef7  9908 0786       DM(+6,I4)=R15
0012bef9  4dfe 0e3f       I12=DM(M7,I6)
0012befb  9c08 05fb       R11=DM(-5,I6)
0012befd  9c08 06fc       R13=DM(-4,I6)
0012beff  9c08 077d       R14=DM(-3,I6)
0012bf01  9c08 07fe       R15=DM(-2,I6)
0012bf03  083f 343f       JUMP (M14,I12) (DB)
0012bf05  1901            RFRAME
0012bf06  0001            NOP
0012bf07  70ff 8402 1400  R4=PASS R0 || R8=R15
0012bf0a  0f8c 0005       R12=0x5
0012bf0c  1804 0012 b6a7  CJUMP 0x12b6a7 (DB); R2=I6, I6=I7
0012bf0f  9ff2            DM(I7,M7)=R2
0012bf10  9fc0 0012 bf12  DM(I7,M7)=0x12bf12
0012bf13  4dfe 0e3f       I12=DM(M7,I6)
0012bf15  9c08 05fb       R11=DM(-5,I6)
0012bf17  9c08 06fc       R13=DM(-4,I6)
0012bf19  9c08 077d       R14=DM(-3,I6)
0012bf1b  9c08 07fe       R15=DM(-2,I6)
0012bf1d  083f 343f       JUMP (M14,I12) (DB)
0012bf1f  1901            RFRAME
0012bf20  0001            NOP
