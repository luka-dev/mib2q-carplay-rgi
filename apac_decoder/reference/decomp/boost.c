/* ============================================================================
 * boost
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 8 function(s), 608 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00095250     64  boost::any::holder<SC_SceneController*>::~holder() */
/* 0x000952d0     64  boost::any::holder<apac::GlobalConfig const*>::~holder() */
/* 0x00095360     72  boost::any::holder<SC_SceneController*>::type() const */
/* 0x00095450     72  boost::any::holder<apac::GlobalConfig const*>::type() const */
/* 0x000954e0     64  boost::any::holder<SC_SceneController*>::~holder() */
/* 0x00095560     64  boost::any::holder<apac::GlobalConfig const*>::~holder() */
/* 0x00095730    104  boost::any::holder<SC_SceneController*>::clone() const */
/* 0x00095810    104  boost::any::holder<apac::GlobalConfig const*>::clone() const */

/* ------------------------------------------------------------------------ */
/* 0x00095250  size 64 bytes  16 insns
 *   boost::any::holder<SC_SceneController*>::~holder()
 *   boost::any::holder<SC_SceneController*>::~holder()
 *   [_ZN5boost3any6holderIP18SC_SceneControllerED2Ev]
 *   [_ZN5boost3any6holderIP18SC_SceneControllerED1Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN5boost3any6holderIP18SC_SceneControllerED1Ev'
__int64 __fastcall boost::any::holder<SC_SceneController *>::~holder(boost__any__holder *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  return v2 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x000952d0  size 64 bytes  16 insns
 *   boost::any::holder<apac::GlobalConfig const*>::~holder()
 *   boost::any::holder<apac::GlobalConfig const*>::~holder()
 *   [_ZN5boost3any6holderIPKN4apac12GlobalConfigEED1Ev]
 *   [_ZN5boost3any6holderIPKN4apac12GlobalConfigEED2Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
// Alternative name is '_ZN5boost3any6holderIPKN4apac12GlobalConfigEED2Ev'
__int64 __fastcall boost::any::holder<apac::GlobalConfig const*>::~holder(boost__any__holder *this)
{
  __int64 v2; // [xsp+18h] [xbp+18h]
  __int64 v3; // [xsp+20h] [xbp+20h]
  return v2 ^ _stack_chk_guard;
}


/* ------------------------------------------------------------------------ */
/* 0x00095360  size 72 bytes  18 insns
 *   boost::any::holder<SC_SceneController*>::type() const
 *   [_ZNK5boost3any6holderIP18SC_SceneControllerE4typeEv]
 *   data refs: 0x42acb0, 0x42ce00, 0x42d3e0, 0x43e520
 */
_UNKNOWN **__fastcall boost::any::holder<SC_SceneController *>::type(boost__any__holder *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return &`typeinfo for'SC_SceneController *;
}


/* ------------------------------------------------------------------------ */
/* 0x00095450  size 72 bytes  18 insns
 *   boost::any::holder<apac::GlobalConfig const*>::type() const
 *   [_ZNK5boost3any6holderIPKN4apac12GlobalConfigEE4typeEv]
 *   data refs: 0x42ad00, 0x42ce00, 0x42d490, 0x43e520
 */
_UNKNOWN **__fastcall boost::any::holder<apac::GlobalConfig const*>::type(boost__any__holder *this)
{
  __int64 v2; // [xsp+20h] [xbp+20h]
  return &`typeinfo for'apac::GlobalConfig const*;
}


/* ------------------------------------------------------------------------ */
/* 0x000954e0  size 64 bytes  16 insns
 *   boost::any::holder<SC_SceneController*>::~holder()
 *   [_ZN5boost3any6holderIP18SC_SceneControllerED0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall boost::any::holder<SC_SceneController *>::~holder(boost__any__holder *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00095560  size 64 bytes  16 insns
 *   boost::any::holder<apac::GlobalConfig const*>::~holder()
 *   [_ZN5boost3any6holderIPKN4apac12GlobalConfigEED0Ev]
 *   data refs: 0x42ce00, 0x43e520
 */
void __fastcall boost::any::holder<apac::GlobalConfig const*>::~holder(boost__any__holder *this)
{
  __int64 v1; // [xsp+20h] [xbp+20h]
  operator delete(this);
}


/* ------------------------------------------------------------------------ */
/* 0x00095730  size 104 bytes  26 insns
 *   boost::any::holder<SC_SceneController*>::clone() const
 *   [_ZNK5boost3any6holderIP18SC_SceneControllerE5cloneEv]
 *   data refs: 0x42aee8, 0x42ce00, 0x42d318, 0x43e520
 */
_QWORD *__fastcall boost::any::holder<SC_SceneController *>::clone(boost__any__holder *this)
{
  _QWORD *result; // x0
  __int64 v3; // x4
  __int64 v4; // [xsp+30h] [xbp+30h]
  result = (_QWORD *)operator new(0x10u);
  v3 = *((_QWORD *)this + 1);
  *result = off_42AEF8;
  result[1] = v3;
  return result;
}


/* ------------------------------------------------------------------------ */
/* 0x00095810  size 104 bytes  26 insns
 *   boost::any::holder<apac::GlobalConfig const*>::clone() const
 *   [_ZNK5boost3any6holderIPKN4apac12GlobalConfigEE5cloneEv]
 *   data refs: 0x42ae58, 0x42ce00, 0x42dcf0, 0x43e520
 */
_QWORD *__fastcall boost::any::holder<apac::GlobalConfig const*>::clone(boost__any__holder *this)
{
  _QWORD *result; // x0
  __int64 v3; // x4
  __int64 v4; // [xsp+30h] [xbp+30h]
  result = (_QWORD *)operator new(0x10u);
  v3 = *((_QWORD *)this + 1);
  *result = off_42AE68;
  result[1] = v3;
  return result;
}


