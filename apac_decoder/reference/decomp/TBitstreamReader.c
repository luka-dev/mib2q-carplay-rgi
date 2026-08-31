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


