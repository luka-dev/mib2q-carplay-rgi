void __fastcall platform_navigation_routeGuidanceManeuverUpdateHandler(void *a1, void *a2, void *a3)
{
  id v5; // x19
  const __CFNull *v6; // x20
  const __CFNull *v7; // x21
  void *v8; // x8
  NSObject *v9; // x22
  CFNumberRef v10; // x0
  CFNumberRef v11; // x23
  CFNullRef v12; // x8
  CFNullRef v13; // x9
  CFArrayRef v14; // x22
  int valuePtr; // [xsp+Ch] [xbp-64h] BYREF
  _BYTE buf[24]; // [xsp+10h] [xbp-60h] BYREF
  const __CFNull *v17; // [xsp+28h] [xbp-48h]

  v5 = objc_retain(a1); /*0x1000dbe90*/
  v6 = objc_retain(a2); /*0x1000dbe98*/
  v7 = objc_retain(a3); /*0x1000dbea0*/
  if ( gLogObjects && gNumLogObjects >= 50 ) /*0x1000dbebc*/
  {
    v8 = *(void **)(gLogObjects + 392); /*0x1000dbec0*/
  }
  else
  {
    if ( logObjectForModule_onceToken_126 != -1 ) /*0x1000dbed4*/
      platform_navigation_accessoryAttached_cold_1(); /*0x1000dc004*/
    v8 = &_os_log_default; /*0x1000dbedc*/
  }
  v9 = objc_retain(v8); /*0x1000dbee4*/
  if ( os_log_type_enabled(v9, OS_LOG_TYPE_INFO) )
  {
    *(_DWORD *)buf = 138412802; /*0x1000dbf00*/
    *(_QWORD *)&buf[4] = v5; /*0x1000dbf04*/
    *(_WORD *)&buf[12] = 2112; /*0x1000dbf0c*/
    *(_QWORD *)&buf[14] = v6; /*0x1000dbf10*/
    *(_WORD *)&buf[22] = 2112; /*0x1000dbf14*/
    v17 = v7; /*0x1000dbf18*/
    _os_log_impl(
      (void *)&_mh_execute_header,
      v9,
      OS_LOG_TYPE_INFO,
      "navigation routeGuidanceManeuverUpdateHandler: %@, componentIdList=%@, updateInfo=%@",
      buf,
      0x20u);
  }
  objc_release(v9); /*0x1000dbf40*/
  valuePtr = 1; /*0x1000dbf44*/
  v10 = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &valuePtr); /*0x1000dbf60*/
  v11 = v10; /*0x1000dbf64*/
  v12 = kCFNull; /*0x1000dbf70*/
  if ( v6 ) /*0x1000dbf78*/
    v13 = v6; /*0x1000dbf78*/
  else
    v13 = kCFNull; /*0x1000dbf78*/
  *(_QWORD *)buf = v10; /*0x1000dbf7c*/
  *(_QWORD *)&buf[8] = v13; /*0x1000dbf7c*/
  if ( v7 ) /*0x1000dbf84*/
    v12 = v7; /*0x1000dbf84*/
  *(_QWORD *)&buf[16] = v12; /*0x1000dbf88*/
  v14 = CFArrayCreate(kCFAllocatorDefault, (const void **)buf, 3, &kCFTypeArrayCallBacks); /*0x1000dbfa4*/
  accFeatureHandlers_invokeHandler(v5, 1701, v14); /*0x1000dbfb4*/
  CFRelease(v14); /*0x1000dbfbc*/
  CFRelease(v11); /*0x1000dbfc4*/
  objc_release(v7); /*0x1000dbfc8*/
  objc_release(v6); /*0x1000dbfcc*/
  objc_release(v5); /*0x1000dbfd0*/
}
