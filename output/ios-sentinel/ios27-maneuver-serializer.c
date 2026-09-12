bool __fastcall iap2_navigation_routeGuidanceManeuverUpdateHandler(__int64 a1, const __CFArray *a2)
{
  NSObject *v4; // x22
  __int64 v5; // x0
  char v6; // nf
  char v7; // vf
  __int64 v8; // x0
  char v9; // zf
  NSObject *v10; // x0
  os_log_type_t v11; // w1
  __int64 v12; // x0
  __int64 v13; // x0
  void *v14; // x0
  os_log_t v15; // x1
  os_log_type_t v16; // w2
  const char *v17; // x3
  uint8_t *v18; // x4
  __int64 v19; // x20
  __int64 v20; // x0
  __int64 v21; // x8
  __int64 v22; // x0
  __int64 v23; // x0
  __int64 v24; // x0
  __int64 v25; // x8
  __int64 v26; // x9
  __int64 v27; // x10
  void *v28; // x0
  _BOOL8 result; // x0
  const __CFNull *ValueAtIndex; // x0
  const __CFArray *v31; // x23
  const __CFNull *v32; // x25
  const __CFNull *v33; // x0
  __int64 v34; // x1
  __int64 v35; // x2
  bool v36; // cc
  int v37; // w9
  const __CFNull *v38; // x22
  __int64 v39; // x0
  NSObject *v40; // x0
  os_log_type_t v41; // w1
  __int64 v42; // x0
  __int64 v43; // x0
  __int64 v44; // x0
  __int64 v45; // x0
  __int64 v46; // x0
  void *v47; // x0
  os_log_t v48; // x1
  os_log_type_t v49; // w2
  const char *v50; // x3
  uint8_t *v51; // x4
  __int64 v52; // x21
  CFIndex Count; // x0
  CFIndex v54; // x24
  int v55; // w26
  CFIndex i; // x25
  const void *v57; // x0
  int v58; // w0
  int v59; // w1
  CFNumberRef v60; // x3
  void *v61; // x0
  __int64 v62; // x0
  int v63; // w0
  int v64; // w1
  CFNumberRef v65; // x3
  int v66; // w23
  void *v67; // x0
  __int64 v68; // x0
  int v69; // w0
  int v70; // w1
  CFStringRef v71; // x3
  _BOOL4 v72; // w24
  void *v73; // x0
  __int64 v74; // x0
  __int64 v75; // x1
  int v76; // w0
  int v77; // w1
  CFNumberRef v78; // x3
  int v79; // w23
  void *v80; // x0
  __int64 v81; // x0
  int v82; // w0
  int v83; // w1
  CFStringRef v84; // x3
  _BOOL4 v85; // w24
  void *v86; // x0
  __int64 v87; // x0
  int v88; // w0
  int v89; // w1
  CFNumberRef v90; // x3
  int v91; // w23
  void *v92; // x0
  __int64 v93; // x0
  int v94; // w0
  int v95; // w1
  CFStringRef v96; // x3
  _BOOL4 v97; // w24
  void *v98; // x0
  __int64 v99; // x0
  __int64 v100; // x0
  __int64 v101; // x1
  int v102; // w0
  int v103; // w1
  CFNumberRef v104; // x3
  int v105; // w24
  void *v106; // x0
  __int64 v107; // x0
  __int64 v108; // x1
  int v109; // w0
  int v110; // w1
  CFNumberRef v111; // x3
  int v112; // w24
  void *v113; // x0
  __int64 v114; // x0
  __int64 v115; // x1
  int v116; // w0
  int v117; // w1
  CFNumberRef v118; // x3
  int v119; // w26
  void *v120; // x0
  const __CFArray *v121; // x0
  const __CFArray *v122; // x23
  CFIndex v123; // x0
  CFIndex v124; // x24
  CFIndex j; // x25
  const void *v126; // x0
  int v127; // w0
  int v128; // w1
  CFNumberRef v129; // x3
  void *v130; // x0
  __int64 v131; // x0
  int v132; // w0
  int v133; // w1
  CFNumberRef v134; // x3
  int v135; // w23
  void *v136; // x0
  __int64 v137; // x0
  int v138; // w0
  int v139; // w1
  CFStringRef v140; // x3
  int v141; // w8
  __int64 isIdentifiedForOutgoingMessageID; // x0
  __int64 v143; // x1
  __int64 v144; // x2
  void *v145; // x0
  int v146; // w0
  int v147; // w1
  CFNumberRef v148; // x3
  __int64 v149; // x0
  NSObject *v150; // x0
  os_log_type_t v151; // w1
  __int64 MsgID; // x0
  __int64 v153; // x0
  __int16 v154; // w8
  __int64 v155; // x0
  void *v156; // x0
  os_log_t v157; // x1
  os_log_type_t v158; // w2
  const char *v159; // x3
  uint8_t *v160; // x4
  dispatch_once_t *v161; // x0
  dispatch_block_t v162; // x1
  dispatch_once_t *v163; // x0
  dispatch_block_t v164; // x1
  dispatch_once_t *v165; // x0
  dispatch_block_t v166; // x1
  dispatch_once_t *v167; // x0
  dispatch_block_t v168; // x1
  dispatch_once_t *v169; // x0
  dispatch_block_t v170; // x1
  int v171; // [xsp+0h] [xbp-70h] BYREF
  __int64 v172; // [xsp+4h] [xbp-6Ch]
  __int16 v173; // [xsp+Ch] [xbp-64h]
  _BYTE v174[10]; // [xsp+Eh] [xbp-62h]
  const __CFNull *v175; // [xsp+18h] [xbp-58h]

  v4 = (NSObject *)(&OBJC_PROTOCOL___ACCTransportPluginManagerProtocol + 32); /*0x1001755d8*/
  if ( !gLogObjects || (v5 = ((__int64 (*)(void))OUTLINED_FUNCTION_33_8)(), v6 ^ v7) ) /*0x1001755e4*/
  {
    v8 = OUTLINED_FUNCTION_59_3(); /*0x1001755f0*/
    if ( !v9 ) /*0x1001755f4*/
    {
      v161 = (dispatch_once_t *)OUTLINED_FUNCTION_8_24(v8); /*0x100175b24*/
      dispatch_once(v161, v162); /*0x100175b28*/
    }
    v5 = OUTLINED_FUNCTION_5(v8); /*0x1001755f8*/
  }
  v10 = (NSObject *)OUTLINED_FUNCTION_22(v5); /*0x1001755fc*/
  if ( os_log_type_enabled(v10, v11) ) /*0x100175600*/
  {
    v12 = OUTLINED_FUNCTION_43(COERCE_DOUBLE(138412290)); /*0x100175620*/
    v13 = OUTLINED_FUNCTION_2_0(v12); /*0x100175624*/
    v14 = (void *)OUTLINED_FUNCTION_10_2(v13); /*0x100175630*/
    _os_log_impl(v14, v15, v16, v17, v18, 0xCu); /*0x100175638*/
  }
  if ( !a1 ) /*0x10017563c*/
    return 0; /*0x10017563c*/
  v19 = OUTLINED_FUNCTION_95(); /*0x100175644*/
  if ( !gLogObjects || (v20 = ((__int64 (*)(void))OUTLINED_FUNCTION_33_8)(), v6 ^ v7) ) /*0x100175654*/
  {
    v22 = OUTLINED_FUNCTION_59_3(); /*0x100175660*/
    if ( !v9 ) /*0x100175664*/
    {
      v163 = (dispatch_once_t *)OUTLINED_FUNCTION_8_24(v22); /*0x100175b30*/
      dispatch_once(v163, v164); /*0x100175b34*/
    }
    v20 = OUTLINED_FUNCTION_24(v22); /*0x100175668*/
  }
  else
  {
    v4 = *(NSObject **)(v21 + 392); /*0x100175658*/
  }
  if ( (unsigned int)OUTLINED_FUNCTION_50(v20) ) /*0x10017566c*/
  {
    v23 = OUTLINED_FUNCTION_86_1(); /*0x100175674*/
    v24 = OUTLINED_FUNCTION_85_3(v23); /*0x10017567c*/
    if ( !v19 ) /*0x10017568c*/
      v25 = v27; /*0x10017568c*/
    v171 = 138412546; /*0x100175690*/
    v172 = v26; /*0x100175694*/
    v173 = 2080; /*0x10017569c*/
    *(_QWORD *)v174 = v25; /*0x1001756a0*/
    v28 = (void *)OUTLINED_FUNCTION_2_0(v24); /*0x1001756a4*/
    _os_log_impl( /*0x1001756c0*/
      v28,
      v4,
      OS_LOG_TYPE_INFO,
      "iAP2Navigation RouteGuidanceManeuverUpdate %@ navigation %s",
      (uint8_t *)&v171,
      0x16u);
  }
  result = 0; /*0x1001756c4*/
  if ( a2 && v19 ) /*0x1001756cc*/
  {
    ValueAtIndex = (const __CFNull *)CFArrayGetValueAtIndex(a2, 1); /*0x1001756d8*/
    v31 = ValueAtIndex; /*0x1001756dc*/
    if ( ValueAtIndex == kCFNull ) /*0x1001756f0*/
      v32 = nullptr; /*0x1001756f0*/
    else
      v32 = ValueAtIndex; /*0x1001756f0*/
    v33 = (const __CFNull *)CFArrayGetValueAtIndex(a2, 2); /*0x1001756fc*/
    if ( gLogObjects ) /*0x100175710*/
      v36 = gNumLogObjects <= 49; /*0x100175710*/
    else
      v36 = 1; /*0x100175710*/
    v37 = !v36; /*0x100175714*/
    if ( v33 != kCFNull ) /*0x10017571c*/
    {
      v38 = v33; /*0x100175720*/
      if ( v33 ) /*0x100175724*/
      {
        if ( !v37 ) /*0x100175728*/
        {
          v44 = OUTLINED_FUNCTION_9_24(v33, v34, v35); /*0x100175780*/
          if ( !v9 ) /*0x100175784*/
          {
            v167 = (dispatch_once_t *)OUTLINED_FUNCTION_8_24(v44); /*0x100175b48*/
            dispatch_once(v167, v168); /*0x100175b4c*/
          }
          v33 = (const __CFNull *)OUTLINED_FUNCTION_23(v44); /*0x100175788*/
        }
        if ( (unsigned int)OUTLINED_FUNCTION_49(v33) ) /*0x10017578c*/
        {
          v45 = OUTLINED_FUNCTION_43(COERCE_DOUBLE(138412802)); /*0x1001757a0*/
          v173 = 2112; /*0x1001757a8*/
          *(_QWORD *)v174 = v32; /*0x1001757ac*/
          *(_WORD *)&v174[8] = 2112; /*0x1001757b0*/
          v175 = v38; /*0x1001757b4*/
          v46 = OUTLINED_FUNCTION_2_0(v45); /*0x1001757b8*/
          v47 = (void *)OUTLINED_FUNCTION_14_5(v46); /*0x1001757c8*/
          _os_log_impl(v47, v48, v49, v50, v51, 0x20u); /*0x1001757d0*/
        }
        v52 = OUTLINED_FUNCTION_24_1(a1 + 128, 20994, *(_QWORD *)(a1 + 200)); /*0x1001757e4*/
        if ( v32 && (Count = CFArrayGetCount(v31), Count >= 1) ) /*0x1001757f8*/
        {
          v54 = Count; /*0x1001757fc*/
          v55 = 0; /*0x100175800*/
          for ( i = 0; i != v54; ++i ) /*0x100175804*/
          {
            v57 = CFArrayGetValueAtIndex(v31, i); /*0x100175810*/
            v58 = OUTLINED_FUNCTION_13_22(v57); /*0x100175814*/
            v55 += __addU16NumberParameterToMsg(v58, v59, 0, v60); /*0x100175820*/
          }
        }
        else
        {
          v55 = 0; /*0x100175834*/
        }
        v61 = (void *)platform_navigation_maneuverInfoKeyForType(1); /*0x10017583c*/
        v62 = OUTLINED_FUNCTION_57_4(v61); /*0x100175840*/
        v63 = OUTLINED_FUNCTION_13_22(v62); /*0x100175844*/
        v66 = v55 + __addU16NumberParameterToMsg(v63, v64, 1, v65); /*0x100175850*/
        v67 = (void *)platform_navigation_maneuverInfoKeyForType(2); /*0x100175858*/
        v68 = OUTLINED_FUNCTION_57_4(v67); /*0x10017585c*/
        v72 = false; /*0x100175874*/
        if ( v68 ) /*0x100175860*/
        {
          v69 = OUTLINED_FUNCTION_13_22(v68); /*0x100175864*/
          if ( iAP2MsgAddCFStringParam(v69, v70, 2, v71) ) /*0x10017586c*/
            v72 = true; /*0x100175860*/
        }
        v73 = (void *)platform_navigation_maneuverInfoKeyForType(3); /*0x100175884*/
        v74 = OUTLINED_FUNCTION_57_4(v73); /*0x100175888*/
        v76 = OUTLINED_FUNCTION_65_0(v74, v75); /*0x1001758a0*/
        v79 = v66 + v72 + __addEnumNumberParameterToMsg(v76, v77, 3, v78); /*0x1001758b0*/
        v80 = (void *)platform_navigation_maneuverInfoKeyForType(4); /*0x1001758b8*/
        v81 = OUTLINED_FUNCTION_57_4(v80); /*0x1001758bc*/
        v85 = false; /*0x1001758d4*/
        if ( v81 ) /*0x1001758c0*/
        {
          v82 = OUTLINED_FUNCTION_13_22(v81); /*0x1001758c4*/
          if ( iAP2MsgAddCFStringParam(v82, v83, 4, v84) ) /*0x1001758cc*/
            v85 = true; /*0x1001758c0*/
        }
        v86 = (void *)platform_navigation_maneuverInfoKeyForType(5); /*0x1001758e4*/
        v87 = OUTLINED_FUNCTION_57_4(v86); /*0x1001758e8*/
        v88 = OUTLINED_FUNCTION_13_22(v87); /*0x1001758ec*/
        v91 = v79 + v85 + __addU32NumberParameterToMsg(v88, v89, 5, v90); /*0x1001758fc*/
        v92 = (void *)platform_navigation_maneuverInfoKeyForType(6); /*0x100175904*/
        v93 = OUTLINED_FUNCTION_57_4(v92); /*0x100175908*/
        v97 = false; /*0x100175920*/
        if ( v93 ) /*0x10017590c*/
        {
          v94 = OUTLINED_FUNCTION_13_22(v93); /*0x100175910*/
          if ( iAP2MsgAddCFStringParam(v94, v95, 6, v96) ) /*0x100175918*/
            v97 = true; /*0x10017590c*/
        }
        v98 = (void *)platform_navigation_maneuverInfoKeyForType(7); /*0x100175930*/
        v99 = OUTLINED_FUNCTION_57_4(v98); /*0x100175934*/
        v100 = OUTLINED_FUNCTION_14_20(v99); /*0x100175938*/
        v102 = OUTLINED_FUNCTION_65_0(v100, v101); /*0x10017593c*/
        v105 = v97 + __addEnumNumberParameterToMsg(v102, v103, 7, v104); /*0x100175948*/
        v106 = (void *)platform_navigation_maneuverInfoKeyForType(8); /*0x100175950*/
        v107 = OUTLINED_FUNCTION_57_4(v106); /*0x100175954*/
        v109 = OUTLINED_FUNCTION_65_0(v107, v108); /*0x10017596c*/
        v112 = v105 + __addEnumNumberParameterToMsg(v109, v110, 8, v111); /*0x100175978*/
        v113 = (void *)platform_navigation_maneuverInfoKeyForType(9); /*0x100175980*/
        v114 = OUTLINED_FUNCTION_57_4(v113); /*0x100175984*/
        v116 = OUTLINED_FUNCTION_65_0(v114, v115); /*0x10017599c*/
        v119 = v91 + v112 + __addEnumNumberParameterToMsg(v116, v117, 9, v118); /*0x1001759ac*/
        v120 = (void *)platform_navigation_maneuverInfoKeyForType(10); /*0x1001759b4*/
        v121 = (const __CFArray *)OUTLINED_FUNCTION_57_4(v120); /*0x1001759b8*/
        if ( v121 ) /*0x1001759bc*/
        {
          v122 = v121; /*0x1001759c0*/
          v123 = CFArrayGetCount(v121); /*0x1001759c4*/
          if ( v123 >= 1 ) /*0x1001759cc*/
          {
            v124 = v123; /*0x1001759d0*/
            for ( j = 0; j != v124; ++j ) /*0x1001759d4*/
            {
              v126 = CFArrayGetValueAtIndex(v122, j); /*0x1001759e0*/
              v127 = OUTLINED_FUNCTION_13_22(v126); /*0x1001759e4*/
              v119 += __addU16NumberParameterToMsg(v127, v128, 10, v129); /*0x1001759f0*/
            }
          }
        }
        v130 = (void *)platform_navigation_maneuverInfoKeyForType(11); /*0x100175a04*/
        v131 = OUTLINED_FUNCTION_57_4(v130); /*0x100175a08*/
        v132 = OUTLINED_FUNCTION_13_22(v131); /*0x100175a0c*/
        v135 = v119 + __addU16NumberParameterToMsg(v132, v133, 11, v134); /*0x100175a18*/
        if ( *(_BYTE *)(v19 + 18) == 1 ) /*0x100175a24*/
        {
          v136 = (void *)platform_navigation_maneuverInfoKeyForType(13); /*0x100175a2c*/
          v137 = OUTLINED_FUNCTION_57_4(v136); /*0x100175a30*/
          v141 = false; /*0x100175a48*/
          if ( v137 ) /*0x100175a34*/
          {
            v138 = OUTLINED_FUNCTION_13_22(v137); /*0x100175a38*/
            if ( iAP2MsgAddCFStringParam(v138, v139, 13, v140) ) /*0x100175a40*/
              v141 = true; /*0x100175a34*/
          }
          v135 += v141; /*0x100175a54*/
        }
        isIdentifiedForOutgoingMessageID = iap2_identification_isIdentifiedForOutgoingMessageID(a1, 20996); /*0x100175a60*/
        if ( (_DWORD)isIdentifiedForOutgoingMessageID ) /*0x100175a64*/
        {
          v145 = (void *)platform_navigation_maneuverInfoKeyForType(12); /*0x100175a6c*/
          isIdentifiedForOutgoingMessageID = OUTLINED_FUNCTION_57_4(v145); /*0x100175a70*/
          if ( isIdentifiedForOutgoingMessageID ) /*0x100175a74*/
          {
            v146 = OUTLINED_FUNCTION_13_22(isIdentifiedForOutgoingMessageID); /*0x100175a78*/
            isIdentifiedForOutgoingMessageID = __addU16NumberParameterToMsg(v146, v147, 12, v148); /*0x100175a80*/
            v135 += isIdentifiedForOutgoingMessageID; /*0x100175a84*/
          }
        }
        if ( v135 ) /*0x100175a88*/
        {
          if ( !gLogObjects /*0x100175a98*/
            || (isIdentifiedForOutgoingMessageID = OUTLINED_FUNCTION_33_8(isIdentifiedForOutgoingMessageID, v143),
                v6 ^ v7) )
          {
            v149 = OUTLINED_FUNCTION_9_24(isIdentifiedForOutgoingMessageID, v143, v144); /*0x100175aa4*/
            if ( !v9 ) /*0x100175aa8*/
            {
              v169 = (dispatch_once_t *)OUTLINED_FUNCTION_8_24(v149); /*0x100175b54*/
              dispatch_once(v169, v170); /*0x100175b58*/
            }
            isIdentifiedForOutgoingMessageID = OUTLINED_FUNCTION_5(v149); /*0x100175aac*/
          }
          v150 = (NSObject *)OUTLINED_FUNCTION_22(isIdentifiedForOutgoingMessageID); /*0x100175ab0*/
          if ( os_log_type_enabled(v150, v151) ) /*0x100175ab4*/
          {
            MsgID = iAP2MsgGetMsgID(v52); /*0x100175ac4*/
            v153 = OUTLINED_FUNCTION_68_2(MsgID, COERCE_DOUBLE(138412802)); /*0x100175ad0*/
            *(_WORD *)&v174[4] = v154; /*0x100175ad4*/
            *(_DWORD *)&v174[6] = v135; /*0x100175ad8*/
            v155 = OUTLINED_FUNCTION_2_0(v153); /*0x100175adc*/
            v156 = (void *)OUTLINED_FUNCTION_10_2(v155); /*0x100175ae8*/
            _os_log_impl(v156, v157, v158, v159, v160, 0x18u); /*0x100175af0*/
          }
          iap2_sessionControl_sendOutgoingMessage(a1, v52); /*0x100175afc*/
        }
        return 1; /*0x100175b00*/
      }
    }
    if ( !v37 ) /*0x100175734*/
    {
      v39 = OUTLINED_FUNCTION_9_24(v33, v34, v35); /*0x100175740*/
      if ( !v9 ) /*0x100175744*/
      {
        v165 = (dispatch_once_t *)OUTLINED_FUNCTION_8_24(v39); /*0x100175b3c*/
        dispatch_once(v165, v166); /*0x100175b40*/
      }
      v33 = (const __CFNull *)OUTLINED_FUNCTION_5(v39); /*0x100175748*/
    }
    v40 = (NSObject *)OUTLINED_FUNCTION_50_4(v33); /*0x10017574c*/
    result = os_log_type_enabled(v40, v41); /*0x100175750*/
    if ( result ) /*0x100175754*/
    {
      v42 = OUTLINED_FUNCTION_43(COERCE_DOUBLE(138412290)); /*0x100175760*/
      v43 = OUTLINED_FUNCTION_2_0(v42); /*0x100175764*/
      OUTLINED_FUNCTION_74_4(v43); /*0x100175774*/
      return 0; /*0x100175778*/
    }
  }
  return result; /*0x100175b20*/
}
