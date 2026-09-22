0012bb91  713f 803f       R0=I3
0012bb93  9ff0            DM(I7,M7)=R0
0012bb94  717e 803f       R0=I5
0012bb96  9ff0            DM(I7,M7)=R0
0012bb97  9ff5            DM(I7,M7)=R5
0012bb98  9ff6            DM(I7,M7)=R6
0012bb99  9ff7            DM(I7,M7)=R7
0012bb9a  9ff9            DM(I7,M7)=R9
0012bb9b  9ffb            DM(I7,M7)=R11
0012bb9c  9ffd            DM(I7,M7)=R13
0012bb9d  707e 09bf       I3=R4
0012bb9f  707e 0abf       I5=R4
0012bba1  70be 0a3f       I4=R8
0012bba3  70fe 0e3f       I12=R12
0012bba5  9c08 0481       R9=DM(+1,I6)
0012bba7  c699            R9=R9-1
0012bba8  0fac 0000       M12=0x0
0012bbaa  c1cc            R12-=R12
0012bbab  2989 20bf       R4=DM(I4,M6), R1=PM(I12,M12)
0012bbad  59be 0293 0741  F7=F4*F1 || R5=DM(I4,M6)
0012bbb0  57be 0693 0b51  F11=F5*F1 || R13=DM(I3,M6)
0012bbb3  59be 0308 127d  F2=F7+F13 || R6=DM(I4,M6)
0012bbb6  0193 0c26       F12=F2*F6
0012bbb8  0d09 0000 0009  LCNTR=R9, DO 0x12bbc1 UNTIL LCE
0012bbbb  57be 05d8 d88c  F13=F2*F4, F8=F11+F12 || R11=DM(I3,M6)
0012bbbe  013e 0058 b29d  F11=F2*F5, F2=F11+F13
0012bbc1  3bb1 2113 0c26  F12=F2*F6 || DM(I5,M6)=R8, PM(I12,M12)=R2
0012bbc4  0188 18bc       F8=F11+F12
0012bbc6  9bb8            DM(I5,M6)=R8
0012bbc7  9e08 0681       R13=DM(+1,I7)
0012bbc9  9e08 0582       R11=DM(+2,I7)
0012bbcb  9e08 0483       R9=DM(+3,I7)
0012bbcd  9e08 0384       R7=DM(+4,I7)
0012bbcf  9e08 0305       R6=DM(+5,I7)
0012bbd1  9e08 0286       R5=DM(+6,I7)
0012bbd3  9e08 0a87       I5=DM(+7,I7)
0012bbd5  9e08 0988       I3=DM(+8,I7)
0012bbd7  4dfe 0e3f       I12=DM(M7,I6)
0012bbd9  083f 343f       JUMP (M14,I12) (DB)
0012bbdb  1901            RFRAME
0012bbdc  0001            NOP
