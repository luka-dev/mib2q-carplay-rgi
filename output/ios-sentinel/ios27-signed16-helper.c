// Verified 24A5418b: nil CFNumber omits TLV; CFNumberGetValue uses kCFNumberSInt16Type. Existing +/-1000, -1, zero are serialized as their signed 16-bit bits; no angle sentinel or directional mapping occurs here. Used by maneuver TLV 10 and 11.
__int64 __fastcall __addU16NumberParameterToMsg(
        __int64 message,
        __int64 parentParameter,
        __int64 parameterId,
        CFNumberRef number)
{
  __int64 result; // x0
  signed __int16 signed16Bits; // [xsp+Eh] [xbp-22h] BYREF

  if ( !number ) /*0x1000038d0*/
    return 0; /*0x100003948*/
  signed16Bits = -21846; /*0x1000038fc*/
  result = CFNumberGetValue(number, kCFNumberSInt16Type, &signed16Bits); /*0x10000390c*/
  if ( (_DWORD)result ) /*0x100003910*/
    return iAP2MsgAddU16Param(message, parentParameter, parameterId, (unsigned __int16)signed16Bits) != 0; /*0x10000392c*/
  return result; /*0x100003944*/
}
