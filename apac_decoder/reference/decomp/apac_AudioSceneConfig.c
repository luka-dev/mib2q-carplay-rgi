/* ============================================================================
 * apac::AudioSceneConfig
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 1412 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 *
 * NOTE: not required for the CarPlay 5.1 / 5.1.2 target (Ambisonics scene
 *       or low-bitrate path). Included for completeness only.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00072620   1412  apac::AudioSceneConfig::Deserialize(TBitstreamReader<unsigned int>&) */

/* ------------------------------------------------------------------------ */
/* 0x00072620  size 1412 bytes  353 insns
 *   apac::AudioSceneConfig::Deserialize(TBitstreamReader<unsigned int>&)
 *   [_ZN4apac16AudioSceneConfig11DeserializeER16TBitstreamReaderIjE]
 *   data refs: 0x42ce00, 0x43e520
 */
bool __fastcall apac::AudioSceneConfig::Deserialize(apac__AudioSceneConfig *this, TBitstreamReader_u32 *bs)
{
  __int64 v2; // x19
  __int64 v3; // x20
  __int64 v4; // x25
  __int64 v5; // x26
  _QWORD *v8; // x0
  _QWORD *v9; // x21
  void *v10; // x0
  void *v11; // x0
  void *v12; // x0
  void *v13; // x0
  void *v14; // x0
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  _QWORD *v18; // x19
  _QWORD *v19; // x26
  _QWORD *v20; // x20
  _QWORD *v21; // x25
  void *v22; // x0
  void *v23; // x0
  void *v24; // x0
  void *v25; // x0
  _QWORD *v26; // x0
  _QWORD *v27; // x19
  _QWORD *v28; // x25
  _QWORD *v29; // x0
  _QWORD *v30; // x0
  void *v31; // x0
  void *v32; // x0
  __int64 v33; // x20
  void *v34; // x26
  void *v35; // x0
  void *v36; // x0
  void *v37; // x0
  void *v38; // x0
  void *v39; // x0
  void *v40; // x0
  void *v41; // x0
  void *v42; // x0
  void *v43; // x0
  __int64 v44; // x20
  void *v45; // x26
  void *v46; // x0
  void *v47; // x0
  void *v48; // x0
  void *v49; // x0
  void *v50; // x0
  void *v51; // x0
  _QWORD *v52; // x20
  _QWORD *v53; // x26
  void *v54; // x0
  void *v55; // x0
  void *v56; // x0
  void *v57; // x0
  void *v58; // x0
  void *v59; // x0
  void *v60; // x0
  void *v61; // x0
  void *v62; // x0
  __int64 v63; // x20
  void *v64; // x26
  void *v65; // x0
  void *v66; // x0
  void *v67; // x0
  void *v68; // x0
  __int64 v69; // x20
  void *v70; // x26
  void *v71; // x0
  __int64 v72; // x20
  void *v73; // x26
  void *v74; // x0
  void *v75; // x0
  void *v76; // x0
  void *v77; // x0
  _QWORD *v78; // x0
  bool result; // w0
  __int64 v80; // [xsp+10h] [xbp+10h]
  __int64 v81; // [xsp+18h] [xbp+18h]
  __int64 v82; // [xsp+40h] [xbp+40h]
  __int64 v83; // [xsp+48h] [xbp+48h]
  __int64 v84; // [xsp+58h] [xbp+58h]
  __int64 v85; // [xsp+60h] [xbp+60h]
  v8 = (_QWORD *)operator new(0x168u);
  *(_DWORD *)v8 = 0;
  *((_BYTE *)v8 + 4) = 0;
  v8[1] = 0;
  v8[2] = 0;
  v8[3] = 0;
  *((_BYTE *)v8 + 32) = 0;
  *((_WORD *)v8 + 20) = 0;
  *((_BYTE *)v8 + 42) = 0;
  v8[7] = v8 + 9;
  v8[8] = 0;
  *((_BYTE *)v8 + 72) = 0;
  *((_BYTE *)v8 + 88) = 0;
  *((_DWORD *)v8 + 24) = 0;
  *((_BYTE *)v8 + 100) = 0;
  v8[13] = 0;
  v8[14] = 0;
  v8[15] = 0;
  *((_BYTE *)v8 + 128) = 0;
  *((_WORD *)v8 + 65) = 0;
  v8[17] = 0;
  v8[18] = 0;
  v8[19] = 0;
  v8[20] = 0;
  v8[21] = 0;
  v8[22] = 0;
  *((_BYTE *)v8 + 184) = 0;
  v8[24] = 0;
  v8[25] = 0;
  v8[26] = 0;
  *((_BYTE *)v8 + 216) = 0;
  v8[28] = 0;
  v8[29] = 0;
  v8[30] = 0;
  v9 = *(_QWORD **)this;
  *(_QWORD *)this = v8;
  *((_BYTE *)v8 + 248) = 0;
  *((_WORD *)v8 + 125) = 0;
  v8[32] = 0;
  v8[33] = 0;
  v8[34] = 0;
  v8[35] = 0;
  v8[36] = 0;
  v8[37] = 0;
  *((_BYTE *)v8 + 304) = 0;
  *((_WORD *)v8 + 153) = 0;
  v8[39] = 0;
  v8[40] = 0;
  v8[41] = 0;
  v8[42] = 0;
  v8[43] = 0;
  v8[44] = 0;
  if ( v9 )
  {
    v80 = v2;
    v81 = v3;
    v82 = v4;
    v83 = v5;
    v10 = (void *)v9[42];
    if ( v10 )
      operator delete(v10);
    v11 = (void *)v9[39];
    if ( v11 )
      operator delete(v11);
    v12 = (void *)v9[35];
    if ( v12 )
      operator delete(v12);
    v13 = (void *)v9[32];
    if ( v13 )
      operator delete(v13);
    v14 = (void *)v9[28];
    if ( v14 )
      operator delete(v14);
    v15 = (void *)v9[24];
    if ( v15 )
      operator delete(v15);
    v16 = (void *)v9[20];
    if ( v16 )
      operator delete(v16);
    v17 = (void *)v9[17];
    if ( v17 )
      operator delete(v17);
    v18 = (_QWORD *)v9[13];
    v19 = (_QWORD *)v9[14];
    if ( v18 != v19 )
    {
      do
      {
        v20 = (_QWORD *)v18[8];
        v21 = (_QWORD *)v18[9];
        if ( v20 != v21 )
        {
          do
          {
            v22 = (void *)v20[7];
            if ( v22 )
              operator delete(v22);
            v23 = (void *)v20[4];
            if ( v23 )
              operator delete(v23);
            v20 += 10;
          }
          while ( v21 != v20 );
          v21 = (_QWORD *)v18[8];
        }
        if ( v21 )
          operator delete(v21);
        v24 = (void *)v18[4];
        if ( v24 )
          operator delete(v24);
        v25 = (void *)v18[1];
        if ( v25 )
          operator delete(v25);
        v18 += 11;
      }
      while ( v19 != v18 );
      v19 = (_QWORD *)v9[13];
    }
    if ( v19 )
      operator delete(v19);
    v26 = (_QWORD *)v9[7];
    if ( v26 != v9 + 9 )
      operator delete(v26);
    v27 = (_QWORD *)v9[1];
    v28 = (_QWORD *)v9[2];
    if ( v27 != v28 )
    {
      do
      {
        v29 = (_QWORD *)v27[56];
        if ( v29 != v27 + 58 )
          operator delete(v29);
        v30 = (_QWORD *)v27[50];
        if ( v30 != v27 + 52 )
          operator delete(v30);
        v31 = (void *)v27[46];
        if ( v31 )
          operator delete(v31);
        v32 = (void *)v27[43];
        if ( v32 )
          operator delete(v32);
        v33 = v27[39];
        v34 = (void *)v27[40];
        if ( (void *)v33 != v34 )
        {
          do
          {
            v35 = *(void **)(v33 + 152);
            if ( v35 )
              operator delete(v35);
            v36 = *(void **)(v33 + 128);
            if ( v36 )
              operator delete(v36);
            v37 = *(void **)(v33 + 96);
            if ( v37 )
              operator delete(v37);
            v38 = *(void **)(v33 + 64);
            if ( v38 )
              operator delete(v38);
            v39 = *(void **)(v33 + 32);
            if ( v39 )
              operator delete(v39);
            if ( *(_QWORD *)v33 )
              operator delete(*(void **)v33);
            v33 += 176;
          }
          while ( v34 != (void *)v33 );
          v34 = (void *)v27[39];
        }
        if ( v34 )
          operator delete(v34);
        v40 = (void *)v27[35];
        if ( v40 )
          operator delete(v40);
        v41 = (void *)v27[32];
        if ( v41 )
          operator delete(v41);
        v42 = (void *)v27[28];
        if ( v42 )
          operator delete(v42);
        v43 = (void *)v27[24];
        if ( v43 )
          operator delete(v43);
        v44 = v27[20];
        v45 = (void *)v27[21];
        if ( (void *)v44 != v45 )
        {
          do
          {
            v46 = *(void **)(v44 + 216);
            if ( v46 )
              operator delete(v46);
            v47 = *(void **)(v44 + 168);
            if ( v47 != (void *)(v44 + 184) )
              operator delete(v47);
            v48 = *(void **)(v44 + 120);
            if ( v48 )
              operator delete(v48);
            v49 = *(void **)(v44 + 88);
            if ( v49 )
              operator delete(v49);
            v50 = *(void **)(v44 + 64);
            if ( v50 )
              operator delete(v50);
            v51 = *(void **)(v44 + 32);
            if ( v51 )
              operator delete(v51);
            if ( *(_QWORD *)v44 )
              operator delete(*(void **)v44);
            v44 += 248;
          }
          while ( v45 != (void *)v44 );
          v45 = (void *)v27[20];
        }
        if ( v45 )
          operator delete(v45);
        v52 = (_QWORD *)v27[16];
        v53 = (_QWORD *)v27[17];
        if ( v52 != v53 )
        {
          do
          {
            v54 = (void *)v52[29];
            if ( v54 )
              operator delete(v54);
            v55 = (void *)v52[26];
            if ( v55 )
              operator delete(v55);
            v56 = (void *)v52[22];
            if ( v56 )
              operator delete(v56);
            v57 = (void *)v52[19];
            if ( v57 )
              operator delete(v57);
            v58 = (void *)v52[15];
            if ( v58 )
              operator delete(v58);
            v59 = (void *)v52[11];
            if ( v59 )
              operator delete(v59);
            v60 = (void *)v52[7];
            if ( v60 )
              operator delete(v60);
            v61 = (void *)v52[4];
            if ( v61 )
              operator delete(v61);
            v62 = (void *)v52[1];
            if ( v62 )
              operator delete(v62);
            v52 += 32;
          }
          while ( v53 != v52 );
          v53 = (_QWORD *)v27[16];
        }
        if ( v53 )
          operator delete(v53);
        v63 = v27[13];
        v64 = (void *)v27[14];
        if ( (void *)v63 != v64 )
        {
          do
          {
            v65 = *(void **)(v63 + 120);
            if ( v65 )
              operator delete(v65);
            v66 = *(void **)(v63 + 96);
            if ( v66 )
              operator delete(v66);
            v67 = *(void **)(v63 + 64);
            if ( v67 )
              operator delete(v67);
            v68 = *(void **)(v63 + 32);
            if ( v68 )
              operator delete(v68);
            if ( *(_QWORD *)v63 )
              operator delete(*(void **)v63);
            v63 += 144;
          }
          while ( v64 != (void *)v63 );
          v64 = (void *)v27[13];
        }
        if ( v64 )
          operator delete(v64);
        v69 = v27[10];
        v70 = (void *)v27[11];
        if ( (void *)v69 != v70 )
        {
          do
          {
            v71 = *(void **)(v69 + 32);
            if ( v71 != (void *)(v69 + 48) )
              operator delete(v71);
            if ( *(_QWORD *)v69 )
              operator delete(*(void **)v69);
            v69 += 80;
          }
          while ( v70 != (void *)v69 );
          v70 = (void *)v27[10];
        }
        if ( v70 )
          operator delete(v70);
        v72 = v27[7];
        v73 = (void *)v27[8];
        if ( (void *)v72 != v73 )
        {
          do
          {
            v74 = *(void **)(v72 + 120);
            if ( v74 )
              operator delete(v74);
            v75 = *(void **)(v72 + 96);
            if ( v75 )
              operator delete(v75);
            v76 = *(void **)(v72 + 64);
            if ( v76 )
              operator delete(v76);
            v77 = *(void **)(v72 + 32);
            if ( v77 )
              operator delete(v77);
            if ( *(_QWORD *)v72 )
              operator delete(*(void **)v72);
            v72 += 144;
          }
          while ( v73 != (void *)v72 );
          v73 = (void *)v27[7];
        }
        if ( v73 )
          operator delete(v73);
        v78 = (_QWORD *)v27[2];
        if ( v78 != v27 + 4 )
          operator delete(v78);
        v27 += 61;
      }
      while ( v28 != v27 );
      v28 = (_QWORD *)v9[1];
    }
    if ( v28 )
      operator delete(v28);
    operator delete(v9);
    v2 = v80;
    v3 = v81;
    v4 = v82;
    v5 = v83;
    v8 = *(_QWORD **)this;
  }
  result = (unsigned int)SC_AudioScenes::Deserialize((SC_AudioScenes *)v8, bs) != 0;
  if ( v84 != _stack_chk_guard )
  {
    v80 = v2;
    v81 = v3;
    v82 = v4;
    v83 = v5;
  }
  return result;
}


