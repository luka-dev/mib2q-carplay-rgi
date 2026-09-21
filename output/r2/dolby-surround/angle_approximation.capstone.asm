0x18005b150: movaps    xmm5, xmm1
0x18005b153: xorps     xmm5, xmmword ptr [rip + 0x2dd246]
0x18005b15a: movaps    xmm4, xmm0
0x18005b15d: xorps     xmm0, xmmword ptr [rip + 0x2dd23c]
0x18005b164: maxss     xmm5, xmm1
0x18005b168: maxss     xmm0, xmm4
0x18005b16c: pxor      xmm3, xmm3
0x18005b170: movss     xmm2, dword ptr [rip + 0x2dd238]
0x18005b178: comiss    xmm5, xmm0
0x18005b17b: jbe       0x18005b19e
0x18005b17d: divss     xmm0, xmm5
0x18005b181: movaps    xmm5, xmm0
0x18005b184: mulss     xmm5, xmm0
0x18005b188: mulss     xmm0, xmm2
0x18005b18c: movss     xmm2, dword ptr [rip + 0x2dd220]
0x18005b194: mulss     xmm5, xmm2
0x18005b198: addss     xmm0, xmm5
0x18005b19c: jmp       0x18005b1c9
0x18005b19e: divss     xmm5, xmm0
0x18005b1a2: movaps    xmm0, xmm5
0x18005b1a5: mulss     xmm0, xmm5
0x18005b1a9: mulss     xmm5, xmm2
0x18005b1ad: movss     xmm2, dword ptr [rip + 0x2dd1ff]
0x18005b1b5: mulss     xmm2, xmm0
0x18005b1b9: movss     xmm0, dword ptr [rip + 0x2dd1f7]
0x18005b1c1: addss     xmm5, xmm2
0x18005b1c5: subss     xmm0, xmm5
0x18005b1c9: comiss    xmm3, xmm1
0x18005b1cc: movss     xmm2, dword ptr [rip + 0x2dd1e8]
0x18005b1d4: subss     xmm2, xmm0
0x18005b1d8: ja        0x18005b1dd
0x18005b1da: movaps    xmm2, xmm0
0x18005b1dd: comiss    xmm3, xmm4
0x18005b1e0: movaps    xmm0, xmm2
0x18005b1e3: xorps     xmm0, xmmword ptr [rip + 0x2dd1b6]
0x18005b1ea: ja        0x18005b1ef
0x18005b1ec: movaps    xmm0, xmm2
0x18005b1ef: ret       
