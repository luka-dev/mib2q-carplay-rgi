/* ============================================================================
 * apac::ChanDecElement
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 3 function(s), 476 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00093ad0    228  apac::ChanDecElement::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>) */
/* 0x00093bc0    128  apac::ChanDecElement::~ChanDecElement() */
/* 0x00093c40    120  apac::ChanDecElement::~ChanDecElement() */

/* ------------------------------------------------------------------------ */
/* 0x00093ad0  size 228 bytes  57 insns
 *   apac::ChanDecElement::Initialize(apac::ParamList const&, apac::observer_ptr<apac::IASCConfig>)
 *   [_ZN4apac14ChanDecElement10InitializeERKNS_9ParamListENS_12observer_ptrINS_10IASCConfigEEE]
 *   data refs: 0x7e7f0, 0x42aaa0, 0x42cd10, 0x42ce00, 0x42d6c8, 0x43e520
 */
__int64 __fastcall apac::ChanDecElement::Initialize(apac__ChanDecElement *this, const apac__ParamList *a, void *a2)
{
  APACCoreDecoder *v6; // x19
  __int64 v7; // x0
  __int64 v9; // [xsp+50h] [xbp+50h]
  v6 = (APACCoreDecoder *)operator new(0x110u);
  APACCoreDecoder::APACCoreDecoder(v6);
  *((_QWORD *)v6 + 33) = 0;
  v7 = *((_QWORD *)this + 2);
  *((_QWORD *)this + 2) = v6;
  *(_QWORD *)v6 = off_42AAB0;
  if ( !v7 )
    return APACCoreLBRDecoder::Initialize(v6, a, a2);
  (*(void (__fastcall **)(__int64))(*(_QWORD *)v7 + 8LL))(v7);
  return (*(__int64 (__fastcall **)(_QWORD, const apac__ParamList *, void *))(**((_QWORD **)this + 2) + 16LL))(
           *((_QWORD *)this + 2),
           a,
           a2);
}


/* ------------------------------------------------------------------------ */
/* 0x00093bc0  size 128 bytes  32 insns
 *   apac::ChanDecElement::~ChanDecElement()
 *   apac::ChanDecElement::~ChanDecElement()
 *   [_ZN4apac14ChanDecElementD2Ev]
 *   [_ZN4apac14ChanDecElementD1Ev]
 *   data refs: 0x42ac30, 0x42ce00, 0x42d3a0, 0x43e520
 */
// Alternative name is '_ZN4apac14ChanDecElementD1Ev'
void __fastcall apac::ChanDecElement::~ChanDecElement(apac__ChanDecElement *this)
{
  __int64 v2; // x0
  __int64 v3; // [xsp+20h] [xbp+20h]
  v2 = *((_QWORD *)this + 2);
  *(_QWORD *)this = &unk_42AC40;
  if ( v2 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v2 + 8LL))(v2);
}


/* ------------------------------------------------------------------------ */
/* 0x00093c40  size 120 bytes  30 insns
 *   apac::ChanDecElement::~ChanDecElement()
 *   [_ZN4apac14ChanDecElementD0Ev]
 *   data refs: 0x42ac30, 0x42ce00, 0x42d3a0, 0x43e520
 */
void __fastcall apac::ChanDecElement::~ChanDecElement(apac__ChanDecElement *this)
{
  __int64 v2; // x0
  __int64 v3; // [xsp+30h] [xbp+30h]
  v2 = *((_QWORD *)this + 2);
  *(_QWORD *)this = &unk_42AC40;
  if ( v2 )
    (*(void (__fastcall **)(__int64))(*(_QWORD *)v2 + 8LL))(v2);
  operator delete(this);
}


