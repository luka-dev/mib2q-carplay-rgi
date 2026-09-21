0x18003c040: sub       rsp, 0x28
0x18003c044: lea       rdx, [rsp + 0x24]
0x18003c049: call      0x18013f710
0x18003c04e: movss     xmm1, dword ptr [rip + 0x2eafd2]
0x18003c056: movaps    xmm2, xmm0
0x18003c059: movss     dword ptr [rsp + 0x20], xmm0
0x18003c05f: pxor      xmm3, xmm3
0x18003c063: mulss     xmm2, xmm0
0x18003c067: mulss     xmm0, xmm1
0x18003c06b: cvtsi2ss  xmm3, dword ptr [rsp + 0x24]
0x18003c071: mulss     xmm2, dword ptr [rip + 0x2eafb3]
0x18003c079: addss     xmm0, dword ptr [rip + 0x2eafa3]
0x18003c081: addss     xmm0, xmm2
0x18003c085: addss     xmm0, xmm3
0x18003c089: add       rsp, 0x28
0x18003c08d: ret       
