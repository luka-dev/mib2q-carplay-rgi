/* ============================================================================
 * apac::APACAudioSceneDecoder
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 4 function(s), 3580 bytes of .text.
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
/* 0x00074a30     76  apac::APACAudioSceneDecoder::DecodeFrame(float**) */
/* 0x00074f60   2164  apac::APACAudioSceneDecoder::Initialize(apac::ParamList const&, SC_AudioScenes*) */
/* 0x00076310    676  apac::APACAudioSceneDecoder::~APACAudioSceneDecoder() */
/* 0x000765c0    664  apac::APACAudioSceneDecoder::~APACAudioSceneDecoder() */

/* ------------------------------------------------------------------------ */
/* 0x00074a30  size 76 bytes  19 insns
 *   apac::APACAudioSceneDecoder::DecodeFrame(float**)
 *   [_ZN4apac21APACAudioSceneDecoder11DecodeFrameEPPf]
 *   data refs: 0x42ce00, 0x43e520
 */
__int64 __fastcall apac::APACAudioSceneDecoder::DecodeFrame(apac__APACAudioSceneDecoder *this, float **buf)
{
  __int64 v3; // [xsp+20h] [xbp+20h]
  return SC_SceneController::ProcessAudio(*((SC_SceneController **)this + 1), (const float **)buf, buf, 0x400u);
}


/* ------------------------------------------------------------------------ */
/* 0x00074f60  size 2164 bytes  541 insns
 *   apac::APACAudioSceneDecoder::Initialize(apac::ParamList const&, SC_AudioScenes*)
 *   [_ZN4apac21APACAudioSceneDecoder10InitializeERKNS_9ParamListEP14SC_AudioScenes]
 *   data refs: 0x760b0, 0x30ea60, 0x30ea78, 0x42a4e0, 0x42c9f8, 0x42ce00, 0x42d3d0, 0x42d440, 0x42d748, 0x42d918, 0x42de90, 0x43e3d0, 0x43e520, 0x43e7e8
 */
__int64 __fastcall apac::APACAudioSceneDecoder::Initialize(
        apac__APACAudioSceneDecoder *this,
        const apac__ParamList *a,
        SC_AudioScenes *scenes)
{
  __int64 v3; // x27
  __int64 v4; // x28
  __int64 v8; // x0
  _QWORD *v9; // x26
  void *v10; // x0
  void *v11; // x0
  void **v12; // x19
  void **v13; // x20
  void *v14; // x0
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  void *v18; // x0
  void *v19; // x0
  void *v20; // x0
  void *v21; // x0
  void *v22; // x0
  void *v23; // x0
  _QWORD *v24; // x19
  _QWORD *v25; // x20
  void *v26; // x0
  void *v27; // x0
  __int64 v28; // x19
  void *v29; // x20
  void *v30; // x0
  void *v31; // x0
  void *v32; // x0
  _QWORD *v33; // x20
  _QWORD *v34; // x27
  _QWORD *v35; // x19
  _QWORD *v36; // x22
  void *v37; // x0
  void *v38; // x0
  void *v39; // x0
  void *v40; // x0
  _QWORD *v41; // x0
  void *v42; // x0
  int *v43; // x22
  int *v44; // x0
  int *v45; // x3
  int *v46; // x2
  int *v47; // x2
  int *v48; // x20
  int *v49; // x19
  __int64 v50; // x0
  _QWORD *v51; // x0
  char *v52; // x0
  unsigned int v53; // w19
  int *v55; // x19
  __int64 v56; // x0
  _QWORD *v57; // x0
  char *v58; // x0
  __int64 v59; // x19
  apac__GlobalConfig__ASCConfigData *v60; // x22
  __int64 (__fastcall **v61)(apac__GlobalConfig__ASCConfigData *); // x1
  __int64 (__fastcall *v62)(apac__GlobalConfig__ASCConfigData *); // x1
  __int64 v63; // x0
  _QWORD *v64; // x19
  __int64 v65; // x0
  _QWORD *v66; // x22
  unsigned int v67; // w21
  _DWORD *end; // x1
  _DWORD *v69; // x1
  int v70; // w0
  unsigned int v71; // w1
  int v72; // w0
  void *cap; // x3
  _QWORD **v74; // x0
  __int64 (__fastcall *v75)(apac__GlobalConfig__ASCConfigData *); // x1
  __int64 v76; // [xsp+50h] [xbp+50h]
  __int64 v77; // [xsp+58h] [xbp+58h]
  int *v78; // [xsp+60h] [xbp+60h]
  std_vector *v79; // [xsp+68h] [xbp+68h]
  unsigned int v80; // [xsp+74h] [xbp+74h] BYREF
  int v81; // [xsp+78h] [xbp+78h] BYREF
  int v82; // [xsp+7Ch] [xbp+7Ch]
  std_vector v83; // [xsp+80h] [xbp+80h] BYREF
  std_vector v84; // [xsp+98h] [xbp+98h] BYREF
  std_vector v85; // [xsp+B0h] [xbp+B0h] BYREF
  __int64 v86; // [xsp+C8h] [xbp+C8h]
  __int64 v87; // [xsp+D0h] [xbp+D0h]
  v8 = operator new(0x268u);
  *(_WORD *)v8 = 0;
  *(_QWORD *)(v8 + 4) = 0;
  *(_WORD *)(v8 + 12) = 0;
  *(_QWORD *)(v8 + 16) = 0;
  *(_QWORD *)(v8 + 24) = 0;
  *(_QWORD *)(v8 + 32) = 0;
  *(_QWORD *)(v8 + 40) = 0;
  *(_QWORD *)(v8 + 48) = 0;
  *(_WORD *)(v8 + 56) = 0;
  *(_BYTE *)(v8 + 58) = 0;
  *(_QWORD *)(v8 + 72) = v8 + 88;
  *(_QWORD *)(v8 + 80) = 0;
  *(_BYTE *)(v8 + 88) = 0;
  *(_BYTE *)(v8 + 104) = 0;
  *(_DWORD *)(v8 + 112) = 0;
  *(_BYTE *)(v8 + 116) = 0;
  *(_QWORD *)(v8 + 120) = 0;
  *(_QWORD *)(v8 + 128) = 0;
  *(_QWORD *)(v8 + 136) = 0;
  *(_BYTE *)(v8 + 144) = 0;
  *(_WORD *)(v8 + 146) = 0;
  *(_QWORD *)(v8 + 152) = 0;
  *(_QWORD *)(v8 + 160) = 0;
  *(_QWORD *)(v8 + 168) = 0;
  *(_QWORD *)(v8 + 176) = 0;
  *(_QWORD *)(v8 + 184) = 0;
  *(_QWORD *)(v8 + 192) = 0;
  *(_BYTE *)(v8 + 200) = 0;
  *(_WORD *)(v8 + 204) = 0;
  *(_QWORD *)(v8 + 208) = 0;
  *(_QWORD *)(v8 + 216) = 0;
  *(_QWORD *)(v8 + 224) = 0;
  *(_QWORD *)(v8 + 232) = 0;
  *(_QWORD *)(v8 + 240) = 0;
  *(_QWORD *)(v8 + 248) = 0;
  *(_QWORD *)(v8 + 256) = 0;
  *(_QWORD *)(v8 + 264) = 0;
  *(_QWORD *)(v8 + 272) = 0;
  *(_QWORD *)(v8 + 280) = 0;
  *(_QWORD *)(v8 + 288) = 0;
  *(_QWORD *)(v8 + 296) = 0;
  *(_QWORD *)(v8 + 304) = 0;
  *(_QWORD *)(v8 + 312) = 0;
  *(_QWORD *)(v8 + 320) = 0;
  *(_QWORD *)(v8 + 328) = 0;
  *(_QWORD *)(v8 + 336) = 0;
  *(_QWORD *)(v8 + 344) = 0;
  *(_QWORD *)(v8 + 352) = 0;
  *(_QWORD *)(v8 + 360) = 0;
  *(_QWORD *)(v8 + 368) = 0;
  *(_QWORD *)(v8 + 376) = 0;
  *(_QWORD *)(v8 + 384) = 0;
  *(_QWORD *)(v8 + 392) = 0;
  *(_QWORD *)(v8 + 400) = 0;
  *(_QWORD *)(v8 + 408) = 0;
  *(_QWORD *)(v8 + 416) = 0;
  *(_QWORD *)(v8 + 424) = 0;
  *(_QWORD *)(v8 + 432) = 0;
  *(_QWORD *)(v8 + 440) = 0;
  *(_QWORD *)(v8 + 448) = 0;
  *(_QWORD *)(v8 + 456) = 0;
  *(_DWORD *)(v8 + 464) = 0;
  *(_QWORD *)(v8 + 472) = 0;
  *(_DWORD *)(v8 + 480) = 0;
  *(_QWORD *)(v8 + 488) = 0;
  *(_QWORD *)(v8 + 496) = 0;
  *(_QWORD *)(v8 + 504) = 0;
  *(_QWORD *)(v8 + 512) = 0;
  *(_QWORD *)(v8 + 520) = 0;
  *(_QWORD *)(v8 + 528) = 0;
  *(_QWORD *)(v8 + 536) = 0;
  *(_QWORD *)(v8 + 544) = 0;
  v9 = *((_QWORD **)this + 1);
  *(_QWORD *)(v8 + 552) = 0;
  *(_DWORD *)(v8 + 560) = 0;
  *(_QWORD *)(v8 + 568) = 0;
  *(_DWORD *)(v8 + 576) = 0;
  *(_QWORD *)(v8 + 584) = 0;
  *(_QWORD *)(v8 + 592) = 0;
  *((_QWORD *)this + 1) = v8;
  *(_QWORD *)(v8 + 600) = 0;
  *(_QWORD *)(v8 + 608) = 0;
  if ( v9 )
  {
    v76 = v3;
    v77 = v4;
    v10 = (void *)v9[74];
    if ( v10 )
      operator delete(v10);
    v11 = (void *)v9[69];
    if ( v11 )
      operator delete(v11);
    v12 = (void **)v9[65];
    v13 = (void **)v9[66];
    if ( v12 != v13 )
    {
      do
      {
        if ( *v12 )
          operator delete(*v12);
        v12 += 3;
      }
      while ( v13 != v12 );
      v13 = (void **)v9[65];
    }
    if ( v13 )
      operator delete(v13);
    v14 = (void *)v9[62];
    if ( v14 )
      operator delete(v14);
    v15 = (void *)v9[57];
    if ( v15 )
      operator delete(v15);
    v16 = (void *)v9[54];
    if ( v16 )
      operator delete(v16);
    v17 = (void *)v9[51];
    if ( v17 )
      operator delete(v17);
    v18 = (void *)v9[48];
    if ( v18 )
      operator delete(v18);
    v19 = (void *)v9[45];
    if ( v19 )
      operator delete(v19);
    v20 = (void *)v9[42];
    if ( v20 )
      operator delete(v20);
    v21 = (void *)v9[39];
    if ( v21 )
      operator delete(v21);
    v22 = (void *)v9[35];
    if ( v22 )
      operator delete(v22);
    v23 = (void *)v9[32];
    if ( v23 )
      operator delete(v23);
    v24 = (_QWORD *)v9[29];
    v25 = (_QWORD *)v9[30];
    if ( v24 != v25 )
    {
      do
      {
        v26 = (void *)v24[8];
        if ( v26 )
          operator delete(v26);
        v27 = (void *)v24[5];
        if ( v27 )
          operator delete(v27);
        v24 += 11;
      }
      while ( v25 != v24 );
      v25 = (_QWORD *)v9[29];
    }
    if ( v25 )
      operator delete(v25);
    v28 = v9[26];
    v29 = (void *)v9[27];
    if ( (void *)v28 != v29 )
    {
      do
      {
        v30 = *(void **)(v28 + 24);
        if ( v30 )
          operator delete(v30);
        if ( *(_QWORD *)v28 )
          operator delete(*(void **)v28);
        v28 += 64;
      }
      while ( v29 != (void *)v28 );
      v29 = (void *)v9[26];
    }
    if ( v29 )
      operator delete(v29);
    v31 = (void *)v9[22];
    if ( v31 )
      operator delete(v31);
    v32 = (void *)v9[19];
    if ( v32 )
      operator delete(v32);
    v33 = (_QWORD *)v9[15];
    v34 = (_QWORD *)v9[16];
    if ( v33 != v34 )
    {
      do
      {
        v35 = (_QWORD *)v33[8];
        v36 = (_QWORD *)v33[9];
        if ( v35 != v36 )
        {
          do
          {
            v37 = (void *)v35[7];
            if ( v37 )
              operator delete(v37);
            v38 = (void *)v35[4];
            if ( v38 )
              operator delete(v38);
            v35 += 10;
          }
          while ( v36 != v35 );
          v36 = (_QWORD *)v33[8];
        }
        if ( v36 )
          operator delete(v36);
        v39 = (void *)v33[4];
        if ( v39 )
          operator delete(v39);
        v40 = (void *)v33[1];
        if ( v40 )
          operator delete(v40);
        v33 += 11;
      }
      while ( v34 != v33 );
      v34 = (_QWORD *)v9[15];
    }
    if ( v34 )
      operator delete(v34);
    v41 = (_QWORD *)v9[9];
    if ( v41 != v9 + 11 )
      operator delete(v41);
    v42 = (void *)v9[2];
    if ( v42 )
      operator delete(v42);
    operator delete(v9);
    v3 = v76;
    v4 = v77;
  }
  v43 = (int *)((char *)a + 8);
  v44 = *((int **)a + 2);
  if ( !v44 )
    goto LABEL_100;
  v45 = (int *)((char *)a + 8);
  v46 = *((int **)a + 2);
  do
  {
    while ( v46[8] > 2 )
    {
      v45 = v46;
      v46 = *((int **)v46 + 2);
      if ( !v46 )
        goto LABEL_82;
    }
    v46 = *((int **)v46 + 3);
  }
  while ( v46 );
LABEL_82:
  if ( v45 == v43 || v45[8] > 3 )
  {
LABEL_100:
    v53 = 1;
    goto LABEL_101;
  }
  v47 = (int *)((char *)a + 8);
  v48 = *((int **)a + 2);
  v76 = v3;
  v77 = v4;
  do
  {
    while ( v48[8] > 4 )
    {
      v47 = v48;
      v48 = *((int **)v48 + 2);
      if ( !v48 )
        goto LABEL_87;
    }
    v48 = *((int **)v48 + 3);
  }
  while ( v48 );
LABEL_87:
  if ( v43 != v47 && v47[8] <= 5 )
  {
    v49 = (int *)((char *)a + 8);
    do
    {
      while ( v44[8] > 4 )
      {
        v49 = v44;
        v44 = *((int **)v44 + 2);
        if ( !v44 )
          goto LABEL_92;
      }
      v44 = *((int **)v44 + 3);
    }
    while ( v44 );
LABEL_92:
    if ( v43 == v49 || v49[8] > 5 )
      start();
    v50 = *((_QWORD *)v49 + 5);
    if ( v50 )
      v51 = (_QWORD *)(*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v50 + 16LL))(v50);
    else
      v51 = `typeinfo for'void;
    v52 = (char *)v51[1];
    if ( v52 != "P16DRCConfiguration" && (*v52 == 42 || strcmp(v52, "P16DRCConfiguration")) )
    {
      v85.begin = off_42A4F0;
      sub_70A44();
    }
    v44 = *((int **)a + 2);
    v48 = *(int **)(*((_QWORD *)v49 + 5) + 8LL);
    if ( !v44 )
      goto LABEL_158;
  }
  v55 = (int *)((char *)a + 8);
  do
  {
    while ( v44[8] > 2 )
    {
      v55 = v44;
      v44 = *((int **)v44 + 2);
      if ( !v44 )
        goto LABEL_113;
    }
    v44 = *((int **)v44 + 3);
  }
  while ( v44 );
LABEL_113:
  if ( v43 == v55 || v55[8] > 3 )
LABEL_158:
    start();
  v56 = *((_QWORD *)v55 + 5);
  if ( v56 )
    v57 = (_QWORD *)(*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v56 + 16LL))(v56);
  else
    v57 = `typeinfo for'void;
  v58 = (char *)v57[1];
  if ( v58 != "PKN4apac12GlobalConfigE" && (*v58 == 42 || strcmp(v58, "PKN4apac12GlobalConfigE")) )
  {
    v85.begin = off_42A4F0;
    sub_70A44();
  }
  v59 = *(_QWORD *)(*((_QWORD *)v55 + 5) + 8LL);
  if ( !v59 )
  {
    v53 = 1;
    v3 = v76;
    v4 = v77;
    goto LABEL_101;
  }
  memset(&v84, 0, sizeof(v84));
  v60 = (apac__GlobalConfig__ASCConfigData *)(v59 + 16);
  memset(&v85, 0, sizeof(v85));
  v80 = 0;
  v61 = *(__int64 (__fastcall ***)(apac__GlobalConfig__ASCConfigData *))(v59 + 16);
  memset(&v83, 0, sizeof(v83));
  v62 = *v61;
  if ( v62 == apac::GlobalConfig::ASCConfigData::configs )
  {
    v63 = *(_QWORD *)(v59 + 24);
    if ( !*(_BYTE *)(v63 + 249) )
    {
      v65 = v63 + 176;
      v64 = *(_QWORD **)v65;
      goto LABEL_125;
    }
    v64 = *(_QWORD **)(v63 + 200);
  }
  else
  {
    v74 = (_QWORD **)v62((apac__GlobalConfig__ASCConfigData *)(v59 + 16));
    v75 = **(__int64 (__fastcall ***)(apac__GlobalConfig__ASCConfigData *))(v59 + 16);
    v64 = *v74;
    if ( v75 != apac::GlobalConfig::ASCConfigData::configs )
    {
      v65 = v75(v60);
      goto LABEL_125;
    }
    v63 = *((_QWORD *)v60 + 1);
    if ( !*(_BYTE *)(v63 + 249) )
    {
      v65 = v63 + 176;
      goto LABEL_125;
    }
  }
  v65 = v63 + 200;
LABEL_125:
  v66 = *(_QWORD **)(v65 + 8);
  if ( v66 != v64 )
  {
    v67 = 0;
    v78 = &v81;
    v79 = &v83;
    while ( 1 )
    {
      while ( 1 )
      {
        v72 = (*(__int64 (__fastcall **)(_QWORD))(*(_QWORD *)*v64 + 24LL))(*v64);
        v81 = v72;
        cap = v83.cap;
        if ( v67 < (unsigned int)*(unsigned __int16 *)(*v64 + 42LL) + v72 )
          v67 = *(unsigned __int16 *)(*v64 + 42LL) + v72;
        if ( v83.end == v83.cap )
          break;
        *(_DWORD *)v83.end = v72;
        v83.end = (char *)v83.end + 4;
        end = v84.end;
        if ( v84.end == v84.cap )
          goto LABEL_134;
LABEL_128:
        *end = v80;
        v69 = v85.end;
        v84.end = (char *)v84.end + 4;
        v70 = *(unsigned __int16 *)(*v64 + 42LL);
        v82 = v70;
        if ( v85.end != v85.cap )
          goto LABEL_129;
LABEL_135:
        std::vector<unsigned int>::_M_realloc_insert<unsigned int>(&v85);
        ++v64;
        v71 = v80 + v81;
        v80 += v81;
        if ( v66 == v64 )
          goto LABEL_136;
      }
      std::vector<unsigned int>::_M_realloc_insert<unsigned int const&>(v79, v83.end, v78, v83.cap);
      end = v84.end;
      if ( v84.end != v84.cap )
        goto LABEL_128;
LABEL_134:
      std::vector<unsigned int>::_M_realloc_insert<unsigned int const&>(&v84, end, &v80, cap);
      v69 = v85.end;
      v70 = *(unsigned __int16 *)(*v64 + 42LL);
      v82 = v70;
      if ( v85.end == v85.cap )
        goto LABEL_135;
LABEL_129:
      *v69 = v70;
      ++v64;
      v85.end = (char *)v85.end + 4;
      v71 = v80 + v81;
      v80 += v81;
      if ( v66 == v64 )
        goto LABEL_136;
    }
  }
  v71 = v80;
  v67 = 0;
LABEL_136:
  v53 = SC_SceneController::Initialize(
          *((SC_SceneController **)this + 1),
          v71,
          v67,
          &v83,
          &v84,
          &v85,
          scenes,
          (DRCConfiguration *)v48);
  if ( v85.begin )
    operator delete(v85.begin);
  if ( v84.begin )
    operator delete(v84.begin);
  if ( v83.begin )
    operator delete(v83.begin);
  v3 = v76;
  v4 = v77;
LABEL_101:
  if ( v86 != _stack_chk_guard )
  {
    v76 = v3;
    v77 = v4;
  }
  return v53;
}


/* ------------------------------------------------------------------------ */
/* 0x00076310  size 676 bytes  169 insns
 *   apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()
 *   apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()
 *   [_ZN4apac21APACAudioSceneDecoderD1Ev]
 *   [_ZN4apac21APACAudioSceneDecoderD2Ev]
 *   data refs: 0x42a508, 0x42ce00, 0x42cf80, 0x43e520
 */
// Alternative name is '_ZN4apac21APACAudioSceneDecoderD2Ev'
void __fastcall apac::APACAudioSceneDecoder::~APACAudioSceneDecoder(apac__APACAudioSceneDecoder *this)
{
  __int64 v1; // x21
  __int64 v2; // x22
  _QWORD *v3; // x19
  void *v4; // x0
  void *v5; // x0
  void **v6; // x20
  void **v7; // x21
  void *v8; // x0
  void *v9; // x0
  void *v10; // x0
  void *v11; // x0
  void *v12; // x0
  void *v13; // x0
  void *v14; // x0
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  _QWORD *v18; // x20
  _QWORD *v19; // x21
  void *v20; // x0
  void *v21; // x0
  __int64 v22; // x20
  void *v23; // x21
  void *v24; // x0
  void *v25; // x0
  void *v26; // x0
  _QWORD *v27; // x20
  _QWORD *v28; // x24
  _QWORD *v29; // x21
  _QWORD *v30; // x22
  void *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  _QWORD *v35; // x0
  void *v36; // x0
  __int64 v37; // [xsp+20h] [xbp+20h]
  __int64 v38; // [xsp+28h] [xbp+28h]
  __int64 v39; // [xsp+48h] [xbp+48h]
  __int64 v40; // [xsp+50h] [xbp+50h]
  v3 = *((_QWORD **)this + 1);
  *(_QWORD *)this = off_42A518;
  if ( v3 )
  {
    v4 = (void *)v3[74];
    if ( v4 )
      operator delete(v4);
    v5 = (void *)v3[69];
    if ( v5 )
      operator delete(v5);
    v6 = (void **)v3[65];
    v7 = (void **)v3[66];
    if ( v6 != v7 )
    {
      do
      {
        if ( *v6 )
          operator delete(*v6);
        v6 += 3;
      }
      while ( v7 != v6 );
      v7 = (void **)v3[65];
    }
    if ( v7 )
      operator delete(v7);
    v8 = (void *)v3[62];
    if ( v8 )
      operator delete(v8);
    v9 = (void *)v3[57];
    if ( v9 )
      operator delete(v9);
    v10 = (void *)v3[54];
    if ( v10 )
      operator delete(v10);
    v11 = (void *)v3[51];
    if ( v11 )
      operator delete(v11);
    v12 = (void *)v3[48];
    if ( v12 )
      operator delete(v12);
    v13 = (void *)v3[45];
    if ( v13 )
      operator delete(v13);
    v14 = (void *)v3[42];
    if ( v14 )
      operator delete(v14);
    v15 = (void *)v3[39];
    if ( v15 )
      operator delete(v15);
    v16 = (void *)v3[35];
    if ( v16 )
      operator delete(v16);
    v17 = (void *)v3[32];
    if ( v17 )
      operator delete(v17);
    v18 = (_QWORD *)v3[29];
    v19 = (_QWORD *)v3[30];
    if ( v18 != v19 )
    {
      do
      {
        v20 = (void *)v18[8];
        if ( v20 )
          operator delete(v20);
        v21 = (void *)v18[5];
        if ( v21 )
          operator delete(v21);
        v18 += 11;
      }
      while ( v19 != v18 );
      v19 = (_QWORD *)v3[29];
    }
    if ( v19 )
      operator delete(v19);
    v22 = v3[26];
    v23 = (void *)v3[27];
    if ( (void *)v22 != v23 )
    {
      do
      {
        v24 = *(void **)(v22 + 24);
        if ( v24 )
          operator delete(v24);
        if ( *(_QWORD *)v22 )
          operator delete(*(void **)v22);
        v22 += 64;
      }
      while ( v23 != (void *)v22 );
      v23 = (void *)v3[26];
    }
    if ( v23 )
      operator delete(v23);
    v25 = (void *)v3[22];
    if ( v25 )
      operator delete(v25);
    v26 = (void *)v3[19];
    if ( v26 )
      operator delete(v26);
    v27 = (_QWORD *)v3[15];
    v28 = (_QWORD *)v3[16];
    if ( v27 != v28 )
    {
      do
      {
        v29 = (_QWORD *)v27[8];
        v30 = (_QWORD *)v27[9];
        if ( v29 != v30 )
        {
          do
          {
            v31 = (void *)v29[7];
            if ( v31 )
              operator delete(v31);
            v32 = (void *)v29[4];
            if ( v32 )
              operator delete(v32);
            v29 += 10;
          }
          while ( v30 != v29 );
          v30 = (_QWORD *)v27[8];
        }
        if ( v30 )
          operator delete(v30);
        v33 = (void *)v27[4];
        if ( v33 )
          operator delete(v33);
        v34 = (void *)v27[1];
        if ( v34 )
          operator delete(v34);
        v27 += 11;
      }
      while ( v28 != v27 );
      v28 = (_QWORD *)v3[15];
    }
    if ( v28 )
      operator delete(v28);
    v35 = (_QWORD *)v3[9];
    if ( v35 != v3 + 11 )
      operator delete(v35);
    v36 = (void *)v3[2];
    if ( v36 )
      operator delete(v36);
    operator delete(v3);
  }
  else if ( v39 != _stack_chk_guard )
  {
    v37 = v1;
    v38 = v2;
  }
}


/* ------------------------------------------------------------------------ */
/* 0x000765c0  size 664 bytes  166 insns
 *   apac::APACAudioSceneDecoder::~APACAudioSceneDecoder()
 *   [_ZN4apac21APACAudioSceneDecoderD0Ev]
 *   data refs: 0x42a508, 0x42ce00, 0x42cf80, 0x43e520
 */
void __fastcall apac::APACAudioSceneDecoder::~APACAudioSceneDecoder(apac__APACAudioSceneDecoder *this)
{
  __int64 v1; // x25
  _QWORD *v3; // x19
  void *v4; // x0
  void *v5; // x0
  void **v6; // x20
  void **v7; // x21
  void *v8; // x0
  void *v9; // x0
  void *v10; // x0
  void *v11; // x0
  void *v12; // x0
  void *v13; // x0
  void *v14; // x0
  void *v15; // x0
  void *v16; // x0
  void *v17; // x0
  _QWORD *v18; // x20
  _QWORD *v19; // x21
  void *v20; // x0
  void *v21; // x0
  __int64 v22; // x20
  void *v23; // x21
  void *v24; // x0
  void *v25; // x0
  void *v26; // x0
  _QWORD *v27; // x20
  _QWORD *v28; // x25
  _QWORD *v29; // x21
  _QWORD *v30; // x24
  void *v31; // x0
  void *v32; // x0
  void *v33; // x0
  void *v34; // x0
  _QWORD *v35; // x0
  void *v36; // x0
  __int64 v37; // [xsp+40h] [xbp+40h]
  __int64 v38; // [xsp+58h] [xbp+58h]
  __int64 v39; // [xsp+60h] [xbp+60h]
  v3 = *((_QWORD **)this + 1);
  *(_QWORD *)this = off_42A518;
  if ( v3 )
  {
    v4 = (void *)v3[74];
    if ( v4 )
      operator delete(v4);
    v5 = (void *)v3[69];
    if ( v5 )
      operator delete(v5);
    v6 = (void **)v3[65];
    v7 = (void **)v3[66];
    if ( v6 != v7 )
    {
      do
      {
        if ( *v6 )
          operator delete(*v6);
        v6 += 3;
      }
      while ( v7 != v6 );
      v7 = (void **)v3[65];
    }
    if ( v7 )
      operator delete(v7);
    v8 = (void *)v3[62];
    if ( v8 )
      operator delete(v8);
    v9 = (void *)v3[57];
    if ( v9 )
      operator delete(v9);
    v10 = (void *)v3[54];
    if ( v10 )
      operator delete(v10);
    v11 = (void *)v3[51];
    if ( v11 )
      operator delete(v11);
    v12 = (void *)v3[48];
    if ( v12 )
      operator delete(v12);
    v13 = (void *)v3[45];
    if ( v13 )
      operator delete(v13);
    v14 = (void *)v3[42];
    if ( v14 )
      operator delete(v14);
    v15 = (void *)v3[39];
    if ( v15 )
      operator delete(v15);
    v16 = (void *)v3[35];
    if ( v16 )
      operator delete(v16);
    v17 = (void *)v3[32];
    if ( v17 )
      operator delete(v17);
    v18 = (_QWORD *)v3[29];
    v19 = (_QWORD *)v3[30];
    if ( v18 != v19 )
    {
      do
      {
        v20 = (void *)v18[8];
        if ( v20 )
          operator delete(v20);
        v21 = (void *)v18[5];
        if ( v21 )
          operator delete(v21);
        v18 += 11;
      }
      while ( v19 != v18 );
      v19 = (_QWORD *)v3[29];
    }
    if ( v19 )
      operator delete(v19);
    v22 = v3[26];
    v23 = (void *)v3[27];
    if ( (void *)v22 != v23 )
    {
      do
      {
        v24 = *(void **)(v22 + 24);
        if ( v24 )
          operator delete(v24);
        if ( *(_QWORD *)v22 )
          operator delete(*(void **)v22);
        v22 += 64;
      }
      while ( v23 != (void *)v22 );
      v23 = (void *)v3[26];
    }
    if ( v23 )
      operator delete(v23);
    v25 = (void *)v3[22];
    if ( v25 )
      operator delete(v25);
    v26 = (void *)v3[19];
    if ( v26 )
      operator delete(v26);
    v27 = (_QWORD *)v3[15];
    v28 = (_QWORD *)v3[16];
    if ( v27 != v28 )
    {
      do
      {
        v29 = (_QWORD *)v27[8];
        v30 = (_QWORD *)v27[9];
        if ( v29 != v30 )
        {
          do
          {
            v31 = (void *)v29[7];
            if ( v31 )
              operator delete(v31);
            v32 = (void *)v29[4];
            if ( v32 )
              operator delete(v32);
            v29 += 10;
          }
          while ( v30 != v29 );
          v30 = (_QWORD *)v27[8];
        }
        if ( v30 )
          operator delete(v30);
        v33 = (void *)v27[4];
        if ( v33 )
          operator delete(v33);
        v34 = (void *)v27[1];
        if ( v34 )
          operator delete(v34);
        v27 += 11;
      }
      while ( v28 != v27 );
      v28 = (_QWORD *)v3[15];
    }
    if ( v28 )
      operator delete(v28);
    v35 = (_QWORD *)v3[9];
    if ( v35 != v3 + 11 )
      operator delete(v35);
    v36 = (void *)v3[2];
    if ( v36 )
      operator delete(v36);
    operator delete(v3);
    v1 = v37;
  }
  if ( v38 != _stack_chk_guard )
    v37 = v1;
  operator delete(this);
}


