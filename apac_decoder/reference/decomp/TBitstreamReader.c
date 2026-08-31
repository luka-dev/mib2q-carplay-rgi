/* ============================================================================
 * TBitstreamReader
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 8 function(s), 1376 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x000b1430    168  TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int) */
/* 0x000b14e0    196  TBitstreamReader<unsigned int>::GetBits(unsigned int) */
/* 0x000b15b0     80  TBitstreamReader<unsigned int>::BitsRemaining() const */
/* 0x000b1600    176  TBitstreamReader<unsigned int>::FillCacheFrom(unsigned char const*) */
/* 0x000b16b0    176  TBitstreamReader<unsigned int>::GetBit() */
/* 0x000b1760    236  TBitstreamReader<unsigned int>::SkipBits(unsigned int) */
/* 0x000b1850    192  TBitstreamReader<unsigned int>::AlignToNextByte() */
/* 0x000b1910    152  TBitstreamReader<unsigned int>::PutBack(unsigned int) */

/* ------------------------------------------------------------------------ */
/* 0x000b1430  size 168 bytes  42 insns
 *   TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int)
 *   TBitstreamReader<unsigned int>::TBitstreamReader(unsigned char const*, unsigned int)
 *   [_ZN16TBitstreamReaderIjEC2EPKhj]
 *   [_ZN16TBitstreamReaderIjEC1EPKhj]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN16TBitstreamReaderIjEC1EPKhj'
__int64 __fastcall TBitstreamReader<unsigned int>::TBitstreamReader(
        TBitstreamReader_u32 *this,
        const unsigned __int8 *a,
        unsigned int n)
{
  const unsigned __int8 *v3; // x2
  const unsigned __int8 *v5; // x1
  int v6; // w4
  int v7; // w5
  _BOOL4 v8; // w3
  _BOOL4 v9; // w7
  int v10; // w8
  const unsigned __int8 *v11; // x6
  __int64 v13; // [xsp+18h] [xbp+18h]
  __int64 v14; // [xsp+20h] [xbp+20h]
  v3 = &a[n];
  this->cur = a;
  this->end = v3;
  *(_QWORD *)&this->cache = 0;
  if ( ((unsigned __int8)a & 3) != 0 && v3 > a )
  {
    v5 = a + 1;
    v6 = 8;
    v7 = 0;
    do
    {
      v8 = ((unsigned __int8)v5 & 3) == 0;
      v9 = v3 <= v5;
      v10 = v6;
      v7 = *(v5 - 1) | (v7 << 8);
      v11 = v5;
      v6 += 8;
      ++v5;
    }
    while ( !v8 && !v9 );
    this->cur = v11;
    this->cache = v7 << -(char)v10;
    this->bitsLeft = v10;
  }
  return v13 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x000b1430, for verification */
000B1430  e91340d1  SUB             X9, SP, #4,LSL#12
000B1434  3ff107f9  STR             XZR, [X9,#0xFE0]
000B1438  c91b00f0  ADRP            X9, #__stack_chk_guard_ptr@PAGE
000B143C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
000B1440  2240228b  ADD             X2, X1, W2,UXTW
000B1444  fd030091  MOV             X29, SP
000B1448  230147f9  LDR             X3, [X9,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B144C  010800a9  STP             X1, X2, [X0]
000B1450  3f0440f2  TST             X1, #3
000B1454  640040f9  LDR             X4, [X3]
000B1458  e40f00f9  STR             X4, [SP,#0x20+var_8]
000B145C  040080d2  MOV             X4, #0
000B1460  1f0800f9  STR             XZR, [X0,#0x10]
000B1464  401041fa  CCMP            X2, X1, #0, NE
000B1468  89020054  B.LS            loc_B14B8
000B146C  21040091  ADD             X1, X1, #1
000B1470  04018052  MOV             W4, #8
000B1474  05008052  MOV             W5, #0
000B1478  26f05f38  LDURB           W6, [X1,#-1]
000B147C  3f0440f2  TST             X1, #3
000B1480  e3179f1a  CSET            W3, EQ
000B1484  5f0001eb  CMP             X2, X1
000B1488  e7879f1a  CSET            W7, LS
000B148C  e803042a  MOV             W8, W4
000B1490  c520052a  ORR             W5, W6, W5,LSL#8
000B1494  6300072a  ORR             W3, W3, W7
000B1498  e60301aa  MOV             X6, X1
000B149C  84200011  ADD             W4, W4, #8
000B14A0  21040091  ADD             X1, X1, #1
000B14A4  a3feff34  CBZ             W3, loc_B1478
000B14A8  e103084b  NEG             W1, W8
000B14AC  060000f9  STR             X6, [X0]
000B14B0  a520c11a  LSL             W5, W5, W1
000B14B4  05200229  STP             W5, W8, [X0,#0x10]
000B14B8  290147f9  LDR             X9, [X9,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B14BC  e10f40f9  LDR             X1, [SP,#0x20+var_8]
000B14C0  200140f9  LDR             X0, [X9]
000B14C4  200000ca  EOR             X0, X1, X0
000B14C8  600000b5  CBNZ            X0, loc_B14D4
000B14CC  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
000B14D0  c0035fd6  RET
000B14D4  8ff9fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b14e0  size 196 bytes  49 insns
 *   TBitstreamReader<unsigned int>::GetBits(unsigned int)
 *   [_ZN16TBitstreamReaderIjE7GetBitsEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall TBitstreamReader<unsigned int>::GetBits(TBitstreamReader_u32 *this, unsigned int n)
{
  unsigned int v2; // w20
  unsigned int cache; // w3
  int v5; // w2
  unsigned int v7; // w0
  int bitsLeft; // w1
  unsigned int v9; // w0
  __int64 v10; // [xsp+40h] [xbp+40h]
  v2 = 0;
  if ( n - 1 <= 0x1F )
  {
    cache = this->cache;
    v5 = this->bitsLeft - n;
    this->bitsLeft = v5;
    v2 = cache >> -(char)n;
    if ( v5 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(this, this->cur);
      v7 = this->cache;
      bitsLeft = this->bitsLeft;
      v2 |= v7 >> (bitsLeft + 32);
      v9 = v7 << -(char)bitsLeft;
      if ( bitsLeft == -32 )
        v9 = 0;
      this->cur += 4;
      this->cache = v9;
      this->bitsLeft = bitsLeft + 32;
    }
    else
    {
      this->cache = cache << n;
    }
  }
  return v2;
}


#if 0 /* disassembly of 0x000b14e0, for verification */
000B14E0  e91340d1  SUB             X9, SP, #4,LSL#12
000B14E4  3fe107f9  STR             XZR, [X9,#0xFC0]
000B14E8  fd7bbca9  STP             X29, X30, [SP,#var_40]!
000B14EC  23040051  SUB             W3, W1, #1
000B14F0  7f7c0071  CMP             W3, #0x1F
000B14F4  fd030091  MOV             X29, SP
000B14F8  f51300f9  STR             X21, [SP,#0x40+var_20]
000B14FC  d51b00f0  ADRP            X21, #__stack_chk_guard_ptr@PAGE
000B1500  f35301a9  STP             X19, X20, [SP,#0x40+var_30]
000B1504  14008052  MOV             W20, #0
000B1508  a20247f9  LDR             X2, [X21,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B150C  430040f9  LDR             X3, [X2]
000B1510  e31f00f9  STR             X3, [SP,#0x40+var_8]
000B1514  030080d2  MOV             X3, #0
000B1518  48010054  B.HI            loc_B1540
000B151C  03084229  LDP             W3, W2, [X0,#0x10]
000B1520  f403014b  NEG             W20, W1
000B1524  f30300aa  MOV             X19, X0
000B1528  4200014b  SUB             W2, W2, W1
000B152C  021400b9  STR             W2, [X0,#0x14]
000B1530  7424d41a  LSR             W20, W3, W20
000B1534  a201f837  TBNZ            W2, #0x1F, loc_B1568
000B1538  6120c11a  LSL             W1, W3, W1
000B153C  011000b9  STR             W1, [X0,#0x10]
000B1540  b50247f9  LDR             X21, [X21,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1544  e11f40f9  LDR             X1, [SP,#0x40+var_8]
000B1548  a00240f9  LDR             X0, [X21]
000B154C  200000ca  EOR             X0, X1, X0
000B1550  800200b5  CBNZ            X0, loc_B15A0
000B1554  e003142a  MOV             W0, W20
000B1558  f35341a9  LDP             X19, X20, [SP,#0x40+var_30]
000B155C  f51340f9  LDR             X21, [SP,#0x40+var_20]
000B1560  fd7bc4a8  LDP             X29, X30, [SP+0x40+var_40],#0x40
000B1564  c0035fd6  RET
000B1568  010040f9  LDR             X1, [X0]
000B156C  25000094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000B1570  60064229  LDP             W0, W1, [X19,#0x10]
000B1574  630240f9  LDR             X3, [X19]
000B1578  22800031  ADDS            W2, W1, #0x20 ; ' '
000B157C  e103014b  NEG             W1, W1
000B1580  0424c21a  LSR             W4, W0, W2
000B1584  9402042a  ORR             W20, W20, W4
000B1588  0020c11a  LSL             W0, W0, W1
000B158C  61100091  ADD             X1, X3, #4
000B1590  e003801a  CSEL            W0, WZR, W0, EQ
000B1594  610200f9  STR             X1, [X19]
000B1598  600a0229  STP             W0, W2, [X19,#0x10]
000B159C  e9ffff17  B               loc_B1540
000B15A0  5cf9fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b15b0  size 80 bytes  20 insns
 *   TBitstreamReader<unsigned int>::BitsRemaining() const
 *   [_ZNK16TBitstreamReaderIjE13BitsRemainingEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall TBitstreamReader<unsigned int>::BitsRemaining(TBitstreamReader_u32 *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return this->bitsLeft + 8 * ((unsigned int)this->end - (unsigned int)this->cur);
}


#if 0 /* disassembly of 0x000b15b0, for verification */
000B15B0  e91340d1  SUB             X9, SP, #4,LSL#12
000B15B4  3ff107f9  STR             XZR, [X9,#0xFE0]
000B15B8  c21b00f0  ADRP            X2, #__stack_chk_guard_ptr@PAGE
000B15BC  fd7bbea9  STP             X29, X30, [SP,#var_20]!
000B15C0  fd030091  MOV             X29, SP
000B15C4  420047f9  LDR             X2, [X2,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B15C8  410040f9  LDR             X1, [X2]
000B15CC  e10f00f9  STR             X1, [SP,#0x20+var_8]
000B15D0  010080d2  MOV             X1, #0
000B15D4  040440a9  LDP             X4, X1, [X0]
000B15D8  e30f40f9  LDR             X3, [SP,#0x20+var_8]
000B15DC  420040f9  LDR             X2, [X2]
000B15E0  620002ca  EOR             X2, X3, X2
000B15E4  031440b9  LDR             W3, [X0,#0x14]
000B15E8  200004cb  SUB             X0, X1, X4
000B15EC  600c000b  ADD             W0, W3, W0,LSL#3
000B15F0  620000b5  CBNZ            X2, loc_B15FC
000B15F4  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
000B15F8  c0035fd6  RET
000B15FC  45f9fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b1600  size 176 bytes  44 insns
 *   TBitstreamReader<unsigned int>::FillCacheFrom(unsigned char const*)
 *   [_ZN16TBitstreamReaderIjE13FillCacheFromEPKh]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall TBitstreamReader<unsigned int>::FillCacheFrom(TBitstreamReader_u32 *this, const unsigned __int8 *a)
{
  signed __int64 v2; // x4
  unsigned int cache; // w2
  int v4; // w3
  __int64 v5; // x4
  unsigned int v6; // w2
  int v7; // w5
  __int64 v8; // [xsp+20h] [xbp+20h]
  v2 = this->end - a;
  if ( (int)v2 <= 3 )
  {
    cache = this->cache;
    if ( (int)v2 < 0 )
      v4 = 0;
    else
      v4 = LODWORD(this->end) - (_DWORD)a;
    if ( (int)v2 > 0 )
    {
      v5 = 0;
      do
      {
        v6 = cache << 8;
        this->cache = v6;
        v7 = a[v5++];
        cache = v7 | v6;
        this->cache = cache;
      }
      while ( v4 > (int)v5 );
    }
    do
    {
      ++v4;
      cache = (cache << 8) | 0xFF;
    }
    while ( v4 != 4 );
    this->cache = cache;
  }
  else
  {
    this->cache = bswap32(*(_DWORD *)a);
  }
}


#if 0 /* disassembly of 0x000b1600, for verification */
000B1600  e91340d1  SUB             X9, SP, #4,LSL#12
000B1604  3ff107f9  STR             XZR, [X9,#0xFE0]
000B1608  c61b00f0  ADRP            X6, #__stack_chk_guard_ptr@PAGE
000B160C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
000B1610  fd030091  MOV             X29, SP
000B1614  c20047f9  LDR             X2, [X6,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1618  040440f9  LDR             X4, [X0,#8]
000B161C  430040f9  LDR             X3, [X2]
000B1620  e30f00f9  STR             X3, [SP,#0x20+var_8]
000B1624  030080d2  MOV             X3, #0
000B1628  840001cb  SUB             X4, X4, X1
000B162C  9f0c0071  CMP             W4, #3
000B1630  6d010054  B.LE            loc_B165C
000B1634  210040b9  LDR             W1, [X1]
000B1638  2108c05a  REV             W1, W1
000B163C  011000b9  STR             W1, [X0,#0x10]
000B1640  c60047f9  LDR             X6, [X6,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1644  e10f40f9  LDR             X1, [SP,#0x20+var_8]
000B1648  c00040f9  LDR             X0, [X6]
000B164C  200000ca  EOR             X0, X1, X0
000B1650  e00200b5  CBNZ            X0, loc_B16AC
000B1654  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
000B1658  c0035fd6  RET
000B165C  9f000071  CMP             W4, #0
000B1660  021040b9  LDR             W2, [X0,#0x10]
000B1664  83a09f1a  CSEL            W3, W4, WZR, GE
000B1668  4d010054  B.LE            loc_B1690
000B166C  040080d2  MOV             X4, #0
000B1670  425c1853  LSL             W2, W2, #8
000B1674  021000b9  STR             W2, [X0,#0x10]
000B1678  25686438  LDRB            W5, [X1,X4]
000B167C  84040091  ADD             X4, X4, #1
000B1680  7f00046b  CMP             W3, W4
000B1684  a200022a  ORR             W2, W5, W2
000B1688  021000b9  STR             W2, [X0,#0x10]
000B168C  2cffff54  B.GT            loc_B1670
000B1690  425c1853  LSL             W2, W2, #8
000B1694  63040011  ADD             W3, W3, #1
000B1698  421c0032  ORR             W2, W2, #0xFF
000B169C  7f100071  CMP             W3, #4
000B16A0  81ffff54  B.NE            loc_B1690
000B16A4  021000b9  STR             W2, [X0,#0x10]
000B16A8  e6ffff17  B               loc_B1640
000B16AC  19f9fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b16b0  size 176 bytes  44 insns
 *   TBitstreamReader<unsigned int>::GetBit()
 *   [_ZN16TBitstreamReaderIjE6GetBitEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall TBitstreamReader<unsigned int>::GetBit(TBitstreamReader_u32 *this)
{
  unsigned int cache; // w2
  int v3; // w1
  unsigned int v4; // w21
  unsigned int v6; // w0
  int bitsLeft; // w1
  unsigned int v8; // w0
  __int64 v9; // [xsp+40h] [xbp+40h]
  cache = this->cache;
  v3 = this->bitsLeft - 1;
  this->bitsLeft = v3;
  v4 = cache >> 31;
  if ( v3 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(this, this->cur);
    v6 = this->cache;
    bitsLeft = this->bitsLeft;
    v4 |= v6 >> (bitsLeft + 32);
    v8 = v6 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v8 = 0;
    this->cur += 4;
    this->cache = v8;
    this->bitsLeft = bitsLeft + 32;
  }
  else
  {
    this->cache = 2 * cache;
  }
  return v4;
}


#if 0 /* disassembly of 0x000b16b0, for verification */
000B16B0  e91340d1  SUB             X9, SP, #4,LSL#12
000B16B4  3fe107f9  STR             XZR, [X9,#0xFC0]
000B16B8  fd7bbca9  STP             X29, X30, [SP,#var_40]!
000B16BC  fd030091  MOV             X29, SP
000B16C0  f35301a9  STP             X19, X20, [SP,#0x40+var_30]
000B16C4  d41b00f0  ADRP            X20, #__stack_chk_guard_ptr@PAGE
000B16C8  f30300aa  MOV             X19, X0
000B16CC  820247f9  LDR             X2, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B16D0  f51300f9  STR             X21, [SP,#0x40+var_20]
000B16D4  430040f9  LDR             X3, [X2]
000B16D8  e31f00f9  STR             X3, [SP,#0x40+var_8]
000B16DC  030080d2  MOV             X3, #0
000B16E0  02044229  LDP             W2, W1, [X0,#0x10]
000B16E4  21040051  SUB             W1, W1, #1
000B16E8  011400b9  STR             W1, [X0,#0x14]
000B16EC  557c1f53  LSR             W21, W2, #0x1F
000B16F0  a101f837  TBNZ            W1, #0x1F, loc_B1724
000B16F4  4200020b  ADD             W2, W2, W2
000B16F8  021000b9  STR             W2, [X0,#0x10]
000B16FC  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1700  e11f40f9  LDR             X1, [SP,#0x40+var_8]
000B1704  800240f9  LDR             X0, [X20]
000B1708  200000ca  EOR             X0, X1, X0
000B170C  800200b5  CBNZ            X0, loc_B175C
000B1710  e003152a  MOV             W0, W21
000B1714  f35341a9  LDP             X19, X20, [SP,#0x40+var_30]
000B1718  f51340f9  LDR             X21, [SP,#0x40+var_20]
000B171C  fd7bc4a8  LDP             X29, X30, [SP+0x40+var_40],#0x40
000B1720  c0035fd6  RET
000B1724  010040f9  LDR             X1, [X0]
000B1728  b6ffff97  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000B172C  60064229  LDP             W0, W1, [X19,#0x10]
000B1730  630240f9  LDR             X3, [X19]
000B1734  22800031  ADDS            W2, W1, #0x20 ; ' '
000B1738  e103014b  NEG             W1, W1
000B173C  0424c21a  LSR             W4, W0, W2
000B1740  b502042a  ORR             W21, W21, W4
000B1744  0020c11a  LSL             W0, W0, W1
000B1748  61100091  ADD             X1, X3, #4
000B174C  e003801a  CSEL            W0, WZR, W0, EQ
000B1750  610200f9  STR             X1, [X19]
000B1754  600a0229  STP             W0, W2, [X19,#0x10]
000B1758  e9ffff17  B               loc_B16FC
000B175C  edf8fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b1760  size 236 bytes  59 insns
 *   TBitstreamReader<unsigned int>::SkipBits(unsigned int)
 *   [_ZN16TBitstreamReaderIjE8SkipBitsEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall TBitstreamReader<unsigned int>::SkipBits(TBitstreamReader_u32 *this, unsigned int n)
{
  int bitsLeft; // w0
  unsigned int cache; // w2
  int v5; // w0
  __int64 v7; // x0
  int v8; // w1
  __int64 v9; // [xsp+28h] [xbp+28h]
  __int64 v10; // [xsp+30h] [xbp+30h]
  bitsLeft = this->bitsLeft;
  if ( bitsLeft < (int)n )
  {
    *(_QWORD *)&this->cache = 0;
    n -= bitsLeft;
    if ( n > 0x1F )
    {
      v7 = n >> 5;
      n &= 0x1Fu;
      this->cur += 4 * v7;
    }
  }
  if ( n - 1 <= 0x1F )
  {
    cache = this->cache;
    v5 = this->bitsLeft - n;
    this->bitsLeft = v5;
    if ( v5 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(this, this->cur);
      v8 = this->bitsLeft;
      this->cur += 4;
      this->bitsLeft = v8 + 32;
      if ( v8 == -32 )
        this->cache = 0;
      else
        this->cache <<= -(char)v8;
    }
    else
    {
      this->cache = cache << n;
    }
  }
  return v9 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x000b1760, for verification */
000B1760  e91340d1  SUB             X9, SP, #4,LSL#12
000B1764  3fe907f9  STR             XZR, [X9,#0xFD0]
000B1768  fd7bbda9  STP             X29, X30, [SP,#var_30]!
000B176C  fd030091  MOV             X29, SP
000B1770  f35301a9  STP             X19, X20, [SP,#0x30+var_20]
000B1774  d41b00f0  ADRP            X20, #__stack_chk_guard_ptr@PAGE
000B1778  f30300aa  MOV             X19, X0
000B177C  820247f9  LDR             X2, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B1780  001440b9  LDR             W0, [X0,#0x14]
000B1784  430040f9  LDR             X3, [X2]
000B1788  e31700f9  STR             X3, [SP,#0x30+var_8]
000B178C  030080d2  MOV             X3, #0
000B1790  1f00016b  CMP             W0, W1
000B1794  aa000054  B.GE            loc_B17A8
000B1798  7f0a00f9  STR             XZR, [X19,#0x10]
000B179C  2100004b  SUB             W1, W1, W0
000B17A0  3f7c0071  CMP             W1, #0x1F
000B17A4  48020054  B.HI            loc_B17EC
000B17A8  20040051  SUB             W0, W1, #1
000B17AC  1f7c0071  CMP             W0, #0x1F
000B17B0  e8000054  B.HI            loc_B17CC
000B17B4  62024229  LDP             W2, W0, [X19,#0x10]
000B17B8  0000014b  SUB             W0, W0, W1
000B17BC  601600b9  STR             W0, [X19,#0x14]
000B17C0  2002f837  TBNZ            W0, #0x1F, loc_B1804
000B17C4  4120c11a  LSL             W1, W2, W1
000B17C8  611200b9  STR             W1, [X19,#0x10]
000B17CC  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B17D0  e11740f9  LDR             X1, [SP,#0x30+var_8]
000B17D4  800240f9  LDR             X0, [X20]
000B17D8  200000ca  EOR             X0, X1, X0
000B17DC  600300b5  CBNZ            X0, loc_B1848
000B17E0  f35341a9  LDP             X19, X20, [SP,#0x30+var_20]
000B17E4  fd7bc3a8  LDP             X29, X30, [SP+0x30+var_30],#0x30
000B17E8  c0035fd6  RET
000B17EC  620240f9  LDR             X2, [X19]
000B17F0  207c0553  LSR             W0, W1, #5
000B17F4  21100012  AND             W1, W1, #0x1F
000B17F8  4008008b  ADD             X0, X2, X0,LSL#2
000B17FC  600200f9  STR             X0, [X19]
000B1800  eaffff17  B               loc_B17A8
000B1804  610240f9  LDR             X1, [X19]
000B1808  e00313aa  MOV             X0, X19
000B180C  7dffff97  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000B1810  600240f9  LDR             X0, [X19]
000B1814  611640b9  LDR             W1, [X19,#0x14]
000B1818  00100091  ADD             X0, X0, #4
000B181C  600200f9  STR             X0, [X19]
000B1820  22800011  ADD             W2, W1, #0x20 ; ' '
000B1824  621600b9  STR             W2, [X19,#0x14]
000B1828  62000035  CBNZ            W2, loc_B1834
000B182C  7f1200b9  STR             WZR, [X19,#0x10]
000B1830  e7ffff17  B               loc_B17CC
000B1834  601240b9  LDR             W0, [X19,#0x10]
000B1838  e103014b  NEG             W1, W1
000B183C  0120c11a  LSL             W1, W0, W1
000B1840  611200b9  STR             W1, [X19,#0x10]
000B1844  e2ffff17  B               loc_B17CC
000B1848  b2f8fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b1850  size 192 bytes  48 insns
 *   TBitstreamReader<unsigned int>::AlignToNextByte()
 *   [_ZN16TBitstreamReaderIjE15AlignToNextByteEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall TBitstreamReader<unsigned int>::AlignToNextByte(TBitstreamReader_u32 *this)
{
  int bitsLeft; // w1
  int v2; // w2
  int v3; // w1
  int v6; // w1
  __int64 v7; // [xsp+28h] [xbp+28h]
  __int64 v8; // [xsp+30h] [xbp+30h]
  bitsLeft = this->bitsLeft;
  v2 = bitsLeft & 7;
  if ( (bitsLeft & 7) != 0 && (unsigned int)(v2 - 1) <= 0x1F )
  {
    v3 = bitsLeft - v2;
    this->bitsLeft = v3;
    if ( v3 < 0 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(this, this->cur);
      v6 = this->bitsLeft;
      this->cur += 4;
      this->bitsLeft = v6 + 32;
      if ( v6 == -32 )
        this->cache = 0;
      else
        this->cache <<= -(char)v6;
    }
    else
    {
      this->cache <<= v2;
    }
  }
  return v7 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x000b1850, for verification */
000B1850  e91340d1  SUB             X9, SP, #4,LSL#12
000B1854  3fe907f9  STR             XZR, [X9,#0xFD0]
000B1858  fd7bbda9  STP             X29, X30, [SP,#var_30]!
000B185C  fd030091  MOV             X29, SP
000B1860  f35301a9  STP             X19, X20, [SP,#0x30+var_20]
000B1864  d41b00f0  ADRP            X20, #__stack_chk_guard_ptr@PAGE
000B1868  820247f9  LDR             X2, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B186C  011440b9  LDR             W1, [X0,#0x14]
000B1870  430040f9  LDR             X3, [X2]
000B1874  e31700f9  STR             X3, [SP,#0x30+var_8]
000B1878  030080d2  MOV             X3, #0
000B187C  22080072  ANDS            W2, W1, #7
000B1880  60010054  B.EQ            loc_B18AC
000B1884  43040051  SUB             W3, W2, #1
000B1888  7f7c0071  CMP             W3, #0x1F
000B188C  08010054  B.HI            loc_B18AC
000B1890  2100024b  SUB             W1, W1, W2
000B1894  011400b9  STR             W1, [X0,#0x14]
000B1898  031040b9  LDR             W3, [X0,#0x10]
000B189C  f30300aa  MOV             X19, X0
000B18A0  6101f837  TBNZ            W1, #0x1F, loc_B18CC
000B18A4  6220c21a  LSL             W2, W3, W2
000B18A8  021000b9  STR             W2, [X0,#0x10]
000B18AC  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B18B0  e11740f9  LDR             X1, [SP,#0x30+var_8]
000B18B4  800240f9  LDR             X0, [X20]
000B18B8  200000ca  EOR             X0, X1, X0
000B18BC  800200b5  CBNZ            X0, loc_B190C
000B18C0  f35341a9  LDP             X19, X20, [SP,#0x30+var_20]
000B18C4  fd7bc3a8  LDP             X29, X30, [SP+0x30+var_30],#0x30
000B18C8  c0035fd6  RET
000B18CC  010040f9  LDR             X1, [X0]
000B18D0  4cffff97  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000B18D4  600240f9  LDR             X0, [X19]
000B18D8  611640b9  LDR             W1, [X19,#0x14]
000B18DC  00100091  ADD             X0, X0, #4
000B18E0  600200f9  STR             X0, [X19]
000B18E4  22800011  ADD             W2, W1, #0x20 ; ' '
000B18E8  621600b9  STR             W2, [X19,#0x14]
000B18EC  62000035  CBNZ            W2, loc_B18F8
000B18F0  7f1200b9  STR             WZR, [X19,#0x10]
000B18F4  eeffff17  B               loc_B18AC
000B18F8  601240b9  LDR             W0, [X19,#0x10]
000B18FC  e103014b  NEG             W1, W1
000B1900  0120c11a  LSL             W1, W0, W1
000B1904  611200b9  STR             W1, [X19,#0x10]
000B1908  e9ffff17  B               loc_B18AC
000B190C  81f8fe97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x000b1910  size 152 bytes  38 insns
 *   TBitstreamReader<unsigned int>::PutBack(unsigned int)
 *   [_ZN16TBitstreamReaderIjE7PutBackEj]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall TBitstreamReader<unsigned int>::PutBack(TBitstreamReader_u32 *this, unsigned int n)
{
  const unsigned __int8 *cur; // x3
  unsigned int v4; // w1
  int v5; // w2
  const unsigned __int8 *v6; // x1
  __int64 v8; // [xsp+28h] [xbp+28h]
  __int64 v9; // [xsp+30h] [xbp+30h]
  if ( n )
  {
    cur = this->cur;
    v4 = n + this->bitsLeft;
    v5 = v4 & 0x1F;
    this->bitsLeft = v5;
    v6 = &cur[-4 * (v4 >> 5)];
    this->cur = v6;
    if ( v5 )
    {
      TBitstreamReader<unsigned int>::FillCacheFrom(this, v6 - 4);
      this->cache <<= -LOBYTE(this->bitsLeft);
    }
    else
    {
      this->cache = 0;
    }
  }
  return v8 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x000b1910, for verification */
000B1910  e91340d1  SUB             X9, SP, #4,LSL#12
000B1914  3fe907f9  STR             XZR, [X9,#0xFD0]
000B1918  fd7bbda9  STP             X29, X30, [SP,#var_30]!
000B191C  fd030091  MOV             X29, SP
000B1920  f35301a9  STP             X19, X20, [SP,#0x30+var_20]
000B1924  d41b00f0  ADRP            X20, #__stack_chk_guard_ptr@PAGE
000B1928  820247f9  LDR             X2, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B192C  430040f9  LDR             X3, [X2]
000B1930  e31700f9  STR             X3, [SP,#0x30+var_8]
000B1934  030080d2  MOV             X3, #0
000B1938  81010034  CBZ             W1, loc_B1968
000B193C  021440b9  LDR             W2, [X0,#0x14]
000B1940  f30300aa  MOV             X19, X0
000B1944  030040f9  LDR             X3, [X0]
000B1948  2100020b  ADD             W1, W1, W2
000B194C  22100012  AND             W2, W1, #0x1F
000B1950  021400b9  STR             W2, [X0,#0x14]
000B1954  217c0553  LSR             W1, W1, #5
000B1958  610801cb  SUB             X1, X3, X1,LSL#2
000B195C  010000f9  STR             X1, [X0]
000B1960  42010035  CBNZ            W2, loc_B1988
000B1964  1f1000b9  STR             WZR, [X0,#0x10]
000B1968  940247f9  LDR             X20, [X20,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
000B196C  e11740f9  LDR             X1, [SP,#0x30+var_8]
000B1970  800240f9  LDR             X0, [X20]
000B1974  200000ca  EOR             X0, X1, X0
000B1978  600100b5  CBNZ            X0, loc_B19A4
000B197C  f35341a9  LDP             X19, X20, [SP,#0x30+var_20]
000B1980  fd7bc3a8  LDP             X29, X30, [SP+0x30+var_30],#0x30
000B1984  c0035fd6  RET
000B1988  211000d1  SUB             X1, X1, #4
000B198C  1dffff97  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
000B1990  60064229  LDP             W0, W1, [X19,#0x10]
000B1994  e103014b  NEG             W1, W1
000B1998  0020c11a  LSL             W0, W0, W1
000B199C  601200b9  STR             W0, [X19,#0x10]
000B19A0  f2ffff17  B               loc_B1968
000B19A4  5bf8fe97  BL              .__stack_chk_fail
#endif

