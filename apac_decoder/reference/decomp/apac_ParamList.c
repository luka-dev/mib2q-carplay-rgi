/* ============================================================================
 * apac::ParamList
 *
 * Hex-Rays 9.3 decompilation of Apple's APAC decoder as linked into
 * Pioneer SPHERA 1.030010  libSPF_CP_PlugInWrapper.so  (AArch64 Linux ELF).
 * 1 function(s), 576 bytes of .text.
 *
 * Addresses are file/virtual offsets in that .so (image base 0).
 * This is REFERENCE MATERIAL for a hand port - it does not compile.
 * Types: parameter types come from the Itanium mangling and are exact;
 *        class structs are forward-declared only, so member accesses appear
 *        as raw offsets off `this`. See CLASSES.md for the offset tables.
 * ========================================================================== */

/* ---- index -------------------------------------------------------------- */
/* 0x00083a50    576  unsigned int apac::ParamList::get<unsigned int>(int) const */

/* ------------------------------------------------------------------------ */
/* 0x00083a50  size 576 bytes  142 insns
 *   unsigned int apac::ParamList::get<unsigned int>(int) const
 *   [_ZNK4apac9ParamList3getIjEET_i]
 *   data refs: 0x76d20, 0x42a478, 0x42a4e0, 0x42a578, 0x42a5d0, 0x42a690, 0x42a6e0, 0x42a720, 0x42c9f8, 0x42cd00, 0x42ce00, 0x42d198, 0x42d3d0, 0x42d6c0, 0x42d700, 0x42dad0, 0x42de28, 0x42de90, 0x43e3d0, 0x43e520, 0x43e7e8, 0x43ea60
 */
unsigned int __fastcall apac::ParamList::get<unsigned int>(apac__ParamList *this, int n)
{
  __int64 v2; // x21
  __int64 v3; // x22
  char *v4; // x2
  char *v5; // x19
  __int64 v6; // x0
  _QWORD *v7; // x0
  const char *v8; // x0
  const char *v9; // x1
  unsigned int result; // w0
  _DWORD *exception; // x0
  _QWORD *v12; // x19
  __int64 v13; // [xsp+20h] [xbp+20h]
  __int64 v14; // [xsp+28h] [xbp+28h]
  void (__fastcall **v15)(boost::bad_any_cast *__hidden); // [xsp+30h] [xbp+30h]
  __int64 (__fastcall **v16)(std::bad_cast *); // [xsp+38h] [xbp+38h] BYREF
  exception v17; // [xsp+40h] [xbp+40h] BYREF
  __int64 v18; // [xsp+68h] [xbp+68h]
  __int64 v19; // [xsp+70h] [xbp+70h]
  v4 = *((char **)this + 2);
  if ( !v4 )
    goto LABEL_18;
  v5 = (char *)this + 8;
  do
  {
    while ( *((_DWORD *)v4 + 8) >= n )
    {
      v5 = v4;
      v4 = *((char **)v4 + 2);
      if ( !v4 )
        goto LABEL_5;
    }
    v4 = *((char **)v4 + 3);
  }
  while ( v4 );
LABEL_5:
  if ( (char *)this + 8 == v5 || *((_DWORD *)v5 + 8) > n )
  {
LABEL_18:
    v13 = v2;
    v14 = v3;
    exception = __cxa_allocate_exception(4u);
    *exception = 3;
    __cxa_throw(exception, (struct type_info *)&`typeinfo for'int, nullptr);
  }
  v6 = *((_QWORD *)v5 + 5);
  if ( v6 )
    v7 = (_QWORD *)(*(__int64 (__fastcall **)(__int64))(*(_QWORD *)v6 + 16LL))(v6);
  else
    v7 = `typeinfo for'void;
  v8 = (const char *)v7[1];
  v9 = (const char *)`typeinfo for'unsigned int[1];
  if ( v8 != v9 && (*v8 == 42 || strcmp(v8, v9)) )
  {
    v13 = v2;
    v14 = v3;
    v15 = off_42A4F0;
    v12 = __cxa_allocate_exception(0x38u);
    v12[2] = 0;
    v12[3] = 0;
    v12[4] = 0;
    *((_DWORD *)v12 + 10) = -1;
    *v12 = &unk_42A690;
    v12[1] = off_42A620;
    v12[6] = &off_42A6E0;
    v16 = off_42A588;
    *(_QWORD *)&v17.type = off_42A5B0;
    memset(&v17.name, 0, 24);
    LODWORD(v17.retval) = -1;
    boost::exception_detail::copy_boost_exception(
      (boost::exception_detail *)(v12 + 1),
      &v17,
      (const exception *)off_42A5B0);
    *v12 = off_42A738;
    v12[1] = off_42A770;
    v12[6] = &off_42A7A8;
    boost::exception_detail::error_info_injector<boost::bad_any_cast>::~error_info_injector((std::bad_cast *)&v16);
    __cxa_throw(
      v12,
      (struct type_info *)&`typeinfo for'boost::wrapexcept<boost::bad_any_cast>,
      (void (*)(void *))boost::wrapexcept<boost::bad_any_cast>::~wrapexcept);
  }
  result = *(_DWORD *)(*((_QWORD *)v5 + 5) + 8LL);
  if ( v18 != _stack_chk_guard )
  {
    v13 = v2;
    v14 = v3;
  }
  return result;
}


