/* ============================================================================
 * APACICSInfo
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 944 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00099e60    736  APACICSInfo::Deserialize(TBitstreamReader<unsigned int>&, apac::CodingType) */
/* 0x0009a6e0     64  APACICSInfo::~APACICSInfo() */
/* 0x0009a720     80  APACICSInfo::Reset() */
/* 0x0009a770     64  APACICSInfo::~APACICSInfo() */

/* ------------------------------------------------------------------------ */
/* 0x00099e60  size 736 bytes  184 insns
 *   APACICSInfo::Deserialize(TBitstreamReader<unsigned int>&, apac::CodingType)
 *   [_ZN11APACICSInfo11DeserializeER16TBitstreamReaderIjEN4apac10CodingTypeE]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall APACICSInfo::Deserialize(APACICSInfo *this, TBitstreamReader_u32 *bs, int n)
{
  unsigned int cache; // w3
  int v7; // w1
  unsigned int v8; // w21
  unsigned int v9; // w0
  int v10; // w1
  unsigned int v11; // w20
  int v12; // w3
  unsigned int v13; // w0
  bool result; // w0
  unsigned int v15; // w0
  unsigned int v16; // w0
  unsigned int v17; // w1
  int v18; // w0
  unsigned int v19; // w20
  unsigned int v20; // w1
  unsigned int v21; // w1
  int v22; // w0
  unsigned int v23; // w20
  int v24; // w1
  int v25; // w2
  int v26; // w5
  unsigned int v27; // w0
  int bitsLeft; // w1
  unsigned int v29; // w0
  unsigned int v30; // w0
  int v31; // w1
  unsigned int v32; // w0
  unsigned int v33; // w0
  int v34; // w1
  unsigned int v35; // w0
  unsigned int v36; // w2
  int v37; // w3
  const unsigned __int8 *cur; // x1
  int v39; // w1
  __int64 v40; // [xsp+50h] [xbp+50h]
  cache = bs->cache;
  v7 = bs->bitsLeft - 2;
  bs->bitsLeft = v7;
  v8 = cache >> 30;
  if ( v7 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v27 = bs->cache;
    bitsLeft = bs->bitsLeft;
    LOBYTE(v8) = v8 | (v27 >> (bitsLeft + 32));
    v29 = v27 << -(char)bitsLeft;
    if ( bitsLeft == -32 )
      v29 = 0;
    bs->cur += 4;
    bs->cache = v29;
    bs->bitsLeft = bitsLeft + 32;
  }
  else
  {
    bs->cache = 4 * cache;
  }
  *((_BYTE *)this + 10) = v8;
  *(_WORD *)((char *)this + 11) = 256;
  *((_DWORD *)this + 4) = 1;
  if ( (unsigned __int8)v8 == 2 )
  {
    v16 = *((unsigned __int8 *)this + 9);
    if ( *((_BYTE *)this + 9) )
    {
      if ( n == 1 )
      {
        *((_BYTE *)this + 11) = 7;
        if ( v16 <= 6 )
          return 1;
        v12 = 7;
        v24 = 0;
      }
      else
      {
        v17 = bs->cache;
        v18 = bs->bitsLeft - 4;
        bs->bitsLeft = v18;
        v19 = v17 >> 28;
        if ( v18 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v33 = bs->cache;
          v34 = bs->bitsLeft;
          LOBYTE(v19) = v19 | (v33 >> (v34 + 32));
          v35 = v33 << -(char)v34;
          if ( v34 == -32 )
            v35 = 0;
          bs->cur += 4;
          bs->cache = v35;
          bs->bitsLeft = v34 + 32;
        }
        else
        {
          bs->cache = 16 * v17;
        }
        v20 = *((unsigned __int8 *)this + 9);
        *((_BYTE *)this + 11) = v19;
        if ( v20 < (unsigned __int8)v19 )
          return 1;
        v21 = bs->cache;
        v22 = bs->bitsLeft - 7;
        bs->bitsLeft = v22;
        v23 = v21 >> 25;
        if ( v22 < 0 )
        {
          TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
          v36 = bs->cache;
          v37 = bs->bitsLeft;
          cur = bs->cur;
          bs->bitsLeft = v37 + 32;
          bs->cur = cur + 4;
          v39 = (v36 >> (v37 + 32)) | v23;
          if ( v37 == -32 )
          {
            bs->cache = 0;
            v24 = v39 << 25;
          }
          else
          {
            v24 = v39 << 25;
            bs->cache = v36 << -(char)v37;
          }
          v12 = *((unsigned __int8 *)this + 11);
          n = *((_DWORD *)this + 4);
        }
        else
        {
          bs->cache = v21 << 7;
          v24 = v23 << 25;
          v12 = *((unsigned __int8 *)this + 11);
          n = *((_DWORD *)this + 4);
        }
      }
      v25 = 7;
      v26 = 1;
      do
      {
        if ( (v24 & 0x80000000) == 0 )
        {
          v26 *= 16;
          ++*((_BYTE *)this + 12);
        }
        n += v26;
        v24 *= 2;
        --v25;
      }
      while ( v25 );
      *((_DWORD *)this + 4) = n;
      goto LABEL_8;
    }
    return 1;
  }
  if ( n == 1 )
  {
    v15 = *((unsigned __int8 *)this + 8);
    *((_BYTE *)this + 11) = 27;
    return v15 <= 0x1A;
  }
  v9 = bs->cache;
  v10 = bs->bitsLeft - 6;
  bs->bitsLeft = v10;
  v11 = v9 >> 26;
  if ( v10 < 0 )
  {
    TBitstreamReader<unsigned int>::FillCacheFrom(bs, bs->cur);
    v30 = bs->cache;
    v31 = bs->bitsLeft;
    v11 |= v30 >> (v31 + 32);
    v32 = v30 << -(char)v31;
    if ( v31 == -32 )
      v32 = 0;
    bs->cur += 4;
    bs->cache = v32;
    bs->bitsLeft = v31 + 32;
  }
  else
  {
    bs->cache = v9 << 6;
  }
  v12 = (unsigned __int8)v11;
  v13 = *((unsigned __int8 *)this + 8);
  *((_BYTE *)this + 11) = v11;
  if ( v13 < (unsigned __int8)v11 )
    return 1;
LABEL_8:
  result = 0;
  if ( !v12 )
    *((_BYTE *)this + 12) = 0;
  return result;
}


#if 0 /* disassembly of 0x00099e60, for verification */
00099E60  e91340d1  SUB             X9, SP, #4,LSL#12
00099E64  3fd907f9  STR             XZR, [X9,#0xFB0]
00099E68  fd7bbba9  STP             X29, X30, [SP,#var_50]!
00099E6C  fd030091  MOV             X29, SP
00099E70  f55b02a9  STP             X21, X22, [SP,#0x50+var_30]
00099E74  961c00f0  ADRP            X22, #__stack_chk_guard_ptr@PAGE
00099E78  c30247f9  LDR             X3, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
00099E7C  f35301a9  STP             X19, X20, [SP,#0x50+var_40]
00099E80  f30301aa  MOV             X19, X1
00099E84  f71b00f9  STR             X23, [SP,#0x50+var_20]
00099E88  f70300aa  MOV             X23, X0
00099E8C  f403022a  MOV             W20, W2
00099E90  211440b9  LDR             W1, [X1,#0x14]
00099E94  600040f9  LDR             X0, [X3]
00099E98  e02700f9  STR             X0, [SP,#0x50+var_8]
00099E9C  000080d2  MOV             X0, #0
00099EA0  631240b9  LDR             W3, [X19,#0x10]
00099EA4  21080051  SUB             W1, W1, #2
00099EA8  611600b9  STR             W1, [X19,#0x14]
00099EAC  757c1e53  LSR             W21, W3, #0x1E
00099EB0  e10af837  TBNZ            W1, #0x1F, loc_9A00C
00099EB4  63741e53  LSL             W3, W3, #2
00099EB8  631200b9  STR             W3, [X19,#0x10]
00099EBC  a11e0012  AND             W1, W21, #0xFF
00099EC0  22008052  MOV             W2, #1
00099EC4  03208052  MOV             W3, #0x100
00099EC8  e12a0039  STRB            W1, [X23,#0xA]
00099ECC  e3b20078  STURH           W3, [X23,#0xB]
00099ED0  3f080071  CMP             W1, #2
00099ED4  e21200b9  STR             W2, [X23,#0x10]
00099ED8  a0040054  B.EQ            loc_99F6C
00099EDC  9f02026b  CMP             W20, W2
00099EE0  40030054  B.EQ            loc_99F48
00099EE4  60064229  LDP             W0, W1, [X19,#0x10]
00099EE8  21180051  SUB             W1, W1, #6
00099EEC  611600b9  STR             W1, [X19,#0x14]
00099EF0  147c1a53  LSR             W20, W0, #0x1A
00099EF4  810bf837  TBNZ            W1, #0x1F, loc_9A064
00099EF8  00641a53  LSL             W0, W0, #6
00099EFC  601200b9  STR             W0, [X19,#0x10]
00099F00  831e0012  AND             W3, W20, #0xFF
00099F04  e0224039  LDRB            W0, [X23,#8]
00099F08  e32e0039  STRB            W3, [X23,#0xB]
00099F0C  1f00036b  CMP             W0, W3
00099F10  a3020054  B.CC            loc_99F64
00099F14  00008052  MOV             W0, #0
00099F18  43000035  CBNZ            W3, loc_99F20
00099F1C  ff320039  STRB            WZR, [X23,#0xC]
00099F20  c10247f9  LDR             X1, [X22,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
00099F24  e22740f9  LDR             X2, [SP,#0x50+var_8]
00099F28  210040f9  LDR             X1, [X1]
00099F2C  410001ca  EOR             X1, X2, X1
00099F30  611000b5  CBNZ            X1, loc_9A13C
00099F34  f35341a9  LDP             X19, X20, [SP,#0x50+var_40]
00099F38  f55b42a9  LDP             X21, X22, [SP,#0x50+var_30]
00099F3C  f71b40f9  LDR             X23, [SP,#0x50+var_20]
00099F40  fd7bc5a8  LDP             X29, X30, [SP+0x50+var_50],#0x50
00099F44  c0035fd6  RET
00099F48  e0224039  LDRB            W0, [X23,#8]
00099F4C  61038052  MOV             W1, #0x1B
00099F50  e12e0039  STRB            W1, [X23,#0xB]
00099F54  1f680071  CMP             W0, #0x1A
00099F58  69000054  B.LS            loc_99F64
00099F5C  00008052  MOV             W0, #0
00099F60  f0ffff17  B               loc_99F20
00099F64  20008052  MOV             W0, #1
00099F68  eeffff17  B               loc_99F20
00099F6C  e0264039  LDRB            W0, [X23,#9]
00099F70  a0ffff34  CBZ             W0, loc_99F64
00099F74  9f060071  CMP             W20, #1
00099F78  80060054  B.EQ            loc_9A048
00099F7C  61024229  LDP             W1, W0, [X19,#0x10]
00099F80  00100051  SUB             W0, W0, #4
00099F84  601600b9  STR             W0, [X19,#0x14]
00099F88  347c1c53  LSR             W20, W1, #0x1C
00099F8C  a008f837  TBNZ            W0, #0x1F, loc_9A0A0
00099F90  216c1c53  LSL             W1, W1, #4
00099F94  611200b9  STR             W1, [X19,#0x10]
00099F98  801e0012  AND             W0, W20, #0xFF
00099F9C  e1264039  LDRB            W1, [X23,#9]
00099FA0  e02e0039  STRB            W0, [X23,#0xB]
00099FA4  3f00006b  CMP             W1, W0
00099FA8  e3fdff54  B.CC            loc_99F64
00099FAC  61024229  LDP             W1, W0, [X19,#0x10]
00099FB0  001c0051  SUB             W0, W0, #7
00099FB4  601600b9  STR             W0, [X19,#0x14]
00099FB8  347c1953  LSR             W20, W1, #0x19
00099FBC  0009f837  TBNZ            W0, #0x1F, loc_9A0DC
00099FC0  21601953  LSL             W1, W1, #7
00099FC4  611200b9  STR             W1, [X19,#0x10]
00099FC8  811a0753  LSL             W1, W20, #0x19
00099FCC  e32e4039  LDRB            W3, [X23,#0xB]
00099FD0  f41240b9  LDR             W20, [X23,#0x10]
00099FD4  e2008052  MOV             W2, #7
00099FD8  25008052  MOV             W5, #1
00099FDC  1f2003d5  NOP
00099FE0  a100f837  TBNZ            W1, #0x1F, loc_99FF4
00099FE4  e4324039  LDRB            W4, [X23,#0xC]
00099FE8  a56c1c53  LSL             W5, W5, #4
00099FEC  84040011  ADD             W4, W4, #1
00099FF0  e4320039  STRB            W4, [X23,#0xC]
00099FF4  9402050b  ADD             W20, W20, W5
00099FF8  2100010b  ADD             W1, W1, W1
00099FFC  42040071  SUBS            W2, W2, #1
0009A000  01ffff54  B.NE            loc_99FE0
0009A004  f41200b9  STR             W20, [X23,#0x10]
0009A008  c3ffff17  B               loc_99F14
0009A00C  610240f9  LDR             X1, [X19]
0009A010  e00313aa  MOV             X0, X19
0009A014  7b5d0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A018  60064229  LDP             W0, W1, [X19,#0x10]
0009A01C  630240f9  LDR             X3, [X19]
0009A020  22800031  ADDS            W2, W1, #0x20 ; ' '
0009A024  e103014b  NEG             W1, W1
0009A028  0424c21a  LSR             W4, W0, W2
0009A02C  b502042a  ORR             W21, W21, W4
0009A030  0020c11a  LSL             W0, W0, W1
0009A034  61100091  ADD             X1, X3, #4
0009A038  e003801a  CSEL            W0, WZR, W0, EQ
0009A03C  610200f9  STR             X1, [X19]
0009A040  600a0229  STP             W0, W2, [X19,#0x10]
0009A044  9effff17  B               loc_99EBC
0009A048  e1008052  MOV             W1, #7
0009A04C  e12e0039  STRB            W1, [X23,#0xB]
0009A050  1f180071  CMP             W0, #6
0009A054  89f8ff54  B.LS            loc_99F64
0009A058  e3008052  MOV             W3, #7
0009A05C  01008052  MOV             W1, #0
0009A060  ddffff17  B               loc_99FD4
0009A064  610240f9  LDR             X1, [X19]
0009A068  e00313aa  MOV             X0, X19
0009A06C  655d0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A070  60064229  LDP             W0, W1, [X19,#0x10]
0009A074  630240f9  LDR             X3, [X19]
0009A078  22800031  ADDS            W2, W1, #0x20 ; ' '
0009A07C  e103014b  NEG             W1, W1
0009A080  0424c21a  LSR             W4, W0, W2
0009A084  9402042a  ORR             W20, W20, W4
0009A088  0020c11a  LSL             W0, W0, W1
0009A08C  61100091  ADD             X1, X3, #4
0009A090  e003801a  CSEL            W0, WZR, W0, EQ
0009A094  610200f9  STR             X1, [X19]
0009A098  600a0229  STP             W0, W2, [X19,#0x10]
0009A09C  99ffff17  B               loc_99F00
0009A0A0  610240f9  LDR             X1, [X19]
0009A0A4  e00313aa  MOV             X0, X19
0009A0A8  565d0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A0AC  60064229  LDP             W0, W1, [X19,#0x10]
0009A0B0  630240f9  LDR             X3, [X19]
0009A0B4  22800031  ADDS            W2, W1, #0x20 ; ' '
0009A0B8  e103014b  NEG             W1, W1
0009A0BC  0424c21a  LSR             W4, W0, W2
0009A0C0  9402042a  ORR             W20, W20, W4
0009A0C4  0020c11a  LSL             W0, W0, W1
0009A0C8  61100091  ADD             X1, X3, #4
0009A0CC  e003801a  CSEL            W0, WZR, W0, EQ
0009A0D0  610200f9  STR             X1, [X19]
0009A0D4  600a0229  STP             W0, W2, [X19,#0x10]
0009A0D8  b0ffff17  B               loc_99F98
0009A0DC  610240f9  LDR             X1, [X19]
0009A0E0  e00313aa  MOV             X0, X19
0009A0E4  475d0094  BL              _ZN16TBitstreamReaderIjE13FillCacheFromEPKh; TBitstreamReader<uint>::FillCacheFrom(uchar const*)
0009A0E8  620e4229  LDP             W2, W3, [X19,#0x10]
0009A0EC  610240f9  LDR             X1, [X19]
0009A0F0  60800011  ADD             W0, W3, #0x20 ; ' '
0009A0F4  601600b9  STR             W0, [X19,#0x14]
0009A0F8  21100091  ADD             X1, X1, #4
0009A0FC  610200f9  STR             X1, [X19]
0009A100  4124c01a  LSR             W1, W2, W0
0009A104  2100142a  ORR             W1, W1, W20
0009A108  c0000035  CBNZ            W0, loc_9A120
0009A10C  7f1200b9  STR             WZR, [X19,#0x10]
0009A110  21180753  LSL             W1, W1, #0x19
0009A114  e32e4039  LDRB            W3, [X23,#0xB]
0009A118  f41240b9  LDR             W20, [X23,#0x10]
0009A11C  aeffff17  B               loc_99FD4
0009A120  e303034b  NEG             W3, W3
0009A124  21180753  LSL             W1, W1, #0x19
0009A128  4220c31a  LSL             W2, W2, W3
0009A12C  621200b9  STR             W2, [X19,#0x10]
0009A130  e32e4039  LDRB            W3, [X23,#0xB]
0009A134  f41240b9  LDR             W20, [X23,#0x10]
0009A138  a7ffff17  B               loc_99FD4
0009A13C  7556ff97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009a6e0  size 64 bytes  16 insns
 *   APACICSInfo::~APACICSInfo()
 *   APACICSInfo::~APACICSInfo()
 *   [_ZN11APACICSInfoD1Ev]
 *   [_ZN11APACICSInfoD2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN11APACICSInfoD2Ev'
void __fastcall APACICSInfo::~APACICSInfo(APACICSInfo *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
}


#if 0 /* disassembly of 0x0009a6e0, for verification */
0009A6E0  e91340d1  SUB             X9, SP, #4,LSL#12
0009A6E4  3ff107f9  STR             XZR, [X9,#0xFE0]
0009A6E8  801c00d0  ADRP            X0, #__stack_chk_guard_ptr@PAGE
0009A6EC  fd7bbea9  STP             X29, X30, [SP,#var_20]!
0009A6F0  fd030091  MOV             X29, SP
0009A6F4  000047f9  LDR             X0, [X0,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A6F8  010040f9  LDR             X1, [X0]
0009A6FC  e10f00f9  STR             X1, [SP,#0x20+var_8]
0009A700  010080d2  MOV             X1, #0
0009A704  e10f40f9  LDR             X1, [SP,#0x20+var_8]
0009A708  000040f9  LDR             X0, [X0]
0009A70C  200000ca  EOR             X0, X1, X0
0009A710  600000b5  CBNZ            X0, loc_9A71C
0009A714  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
0009A718  c0035fd6  RET
0009A71C  fd54ff97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009a720  size 80 bytes  20 insns
 *   APACICSInfo::Reset()
 *   [_ZN11APACICSInfo5ResetEv]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall APACICSInfo::Reset(APACICSInfo *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  *((_WORD *)this + 5) = 0;
  *((_BYTE *)this + 12) = 0;
  *((_DWORD *)this + 4) = 1;
  return v2 ^ _stack_chk_guard;
}


#if 0 /* disassembly of 0x0009a720, for verification */
0009A720  e91340d1  SUB             X9, SP, #4,LSL#12
0009A724  3ff107f9  STR             XZR, [X9,#0xFE0]
0009A728  811c00d0  ADRP            X1, #__stack_chk_guard_ptr@PAGE
0009A72C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
0009A730  22008052  MOV             W2, #1
0009A734  fd030091  MOV             X29, SP
0009A738  210047f9  LDR             X1, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A73C  1f140079  STRH            WZR, [X0,#0xA]
0009A740  1f300039  STRB            WZR, [X0,#0xC]
0009A744  230040f9  LDR             X3, [X1]
0009A748  e30f00f9  STR             X3, [SP,#0x20+var_8]
0009A74C  030080d2  MOV             X3, #0
0009A750  021000b9  STR             W2, [X0,#0x10]
0009A754  e20f40f9  LDR             X2, [SP,#0x20+var_8]
0009A758  200040f9  LDR             X0, [X1]
0009A75C  400000ca  EOR             X0, X2, X0
0009A760  600000b5  CBNZ            X0, loc_9A76C
0009A764  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
0009A768  c0035fd6  RET
0009A76C  e954ff97  BL              .__stack_chk_fail
#endif

/* ------------------------------------------------------------------------ */
/* 0x0009a770  size 64 bytes  16 insns
 *   APACICSInfo::~APACICSInfo()
 *   [_ZN11APACICSInfoD0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall APACICSInfo::~APACICSInfo(APACICSInfo *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


#if 0 /* disassembly of 0x0009a770, for verification */
0009A770  e91340d1  SUB             X9, SP, #4,LSL#12
0009A774  3ff107f9  STR             XZR, [X9,#0xFE0]
0009A778  811c00d0  ADRP            X1, #__stack_chk_guard_ptr@PAGE
0009A77C  fd7bbea9  STP             X29, X30, [SP,#var_20]!
0009A780  fd030091  MOV             X29, SP
0009A784  210047f9  LDR             X1, [X1,#__stack_chk_guard_ptr@PAGEOFF]; __stack_chk_guard
0009A788  220040f9  LDR             X2, [X1]
0009A78C  e20f00f9  STR             X2, [SP,#0x20+var_8]
0009A790  020080d2  MOV             X2, #0
0009A794  e20f40f9  LDR             X2, [SP,#0x20+var_8]
0009A798  210040f9  LDR             X1, [X1]
0009A79C  410001ca  EOR             X1, X2, X1
0009A7A0  610000b5  CBNZ            X1, loc_9A7AC
0009A7A4  fd7bc2a8  LDP             X29, X30, [SP+0x20+var_20],#0x20
0009A7A8  3e56ff17  B               ._ZdlPv; operator delete(void *)
0009A7AC  d954ff97  BL              .__stack_chk_fail
#endif

