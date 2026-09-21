0x180003590: push      rdi
0x180003591: push      r12
0x180003593: push      r13
0x180003595: push      r14
0x180003597: push      r15
0x180003599: push      rbp
0x18000359a: sub       rsp, 0xb8
0x1800035a1: mov       rbp, rcx
0x1800035a4: cmp       dword ptr [rbp + 0x13b0], 0
0x1800035ab: je        0x180003c68
0x1800035b1: cmp       dword ptr [rbp + 4], 0
0x1800035b5: je        0x1800035c0
0x1800035b7: cmp       dword ptr [rbp + 0x778], 0
0x1800035be: jne       0x1800035c7
0x1800035c0: mov       edi, 1
0x1800035c5: jmp       0x1800035c9
0x1800035c7: xor       edi, edi
0x1800035c9: mov       eax, dword ptr [rbp + 0x60]
0x1800035cc: cmp       eax, dword ptr [rbp + 0xc8]
0x1800035d2: je        0x1800035da
0x1800035d4: mov       dword ptr [rbp + 0xc8], eax
0x1800035da: mov       r9d, dword ptr [rbp + 0x128]
0x1800035e1: mov       r10d, dword ptr [rbp + 0x11c]
0x1800035e8: mov       eax, dword ptr [rbp + 0x118]
0x1800035ee: mov       dword ptr [rbp + 0x12c], r9d
0x1800035f5: cmp       r10d, eax
0x1800035f8: je        0x18000360d
0x1800035fa: mov       dword ptr [rbp + 0x6d0], 1
0x180003604: mov       r10d, eax
0x180003607: mov       dword ptr [rbp + 0x11c], eax
0x18000360d: mov       r8d, dword ptr [rbp + 0x124]
0x180003614: mov       eax, dword ptr [rbp + 0x120]
0x18000361a: cmp       r8d, eax
0x18000361d: je        0x180003632
0x18000361f: mov       dword ptr [rbp + 0x6d0], 1
0x180003629: mov       r8d, eax
0x18000362c: mov       dword ptr [rbp + 0x124], eax
0x180003632: test      r9d, r9d
0x180003635: je        0x1800036d8
0x18000363b: cmp       r10d, 0xf
0x18000363f: ja        0x18000367e
0x180003641: lea       r9, [rip - 0x3648]
0x180003648: movzx     eax, byte ptr [r9 + r10 + 0x2e8768]
0x180003651: lea       rax, [r9 + rax + 0x365b]
0x180003659: jmp       rax
0x18000365b: mov       eax, 0xc
0x180003660: jmp       0x180003683
0x180003662: mov       eax, 0xa
0x180003667: jmp       0x180003683
0x180003669: mov       eax, 8
0x18000366e: jmp       0x180003683
0x180003670: mov       eax, 6
0x180003675: jmp       0x180003683
0x180003677: mov       eax, 1
0x18000367c: jmp       0x180003683
0x18000367e: mov       eax, 2
0x180003683: imul      r8d, eax
0x180003687: dec       r8d
0x18000368a: mov       eax, r8d
0x18000368d: cmp       r8d, -1
0x180003691: je        0x1800036d8
0x180003693: mov       r9d, dword ptr [rbp + 0x6d0]
0x18000369a: movss     xmm1, dword ptr [rbp + rax*4 + 0x130]
0x1800036a3: dec       r8d
0x1800036a6: movss     xmm0, dword ptr [rbp + rax*4 + 0x310]
0x1800036af: movss     dword ptr [rbp + rax*4 + 0x310], xmm1
0x1800036b8: mov       eax, r8d
0x1800036bb: cmpneqss  xmm0, xmm1
0x1800036c0: movd      r10d, xmm0
0x1800036c5: neg       r10d
0x1800036c8: or        r9d, r10d
0x1800036cb: cmp       r8d, -1
0x1800036cf: jne       0x18000369a
0x1800036d1: mov       dword ptr [rbp + 0x6d0], r9d
0x1800036d8: mov       r8, qword ptr [rbp + 0x30]
0x1800036dc: lea       rcx, [rbp + 0xe98]
0x1800036e3: mov       eax, dword ptr [rbp + 0x780]
0x1800036e9: mov       dword ptr [rbp + 0x77c], eax
0x1800036ef: mov       rdx, qword ptr [r8 + 0x40]
0x1800036f3: call      0x18001e7b0
0x1800036f8: mov       eax, dword ptr [rbp + 0x7ac]
0x1800036fe: mov       dword ptr [rbp + 0x7a8], eax
0x180003704: mov       eax, dword ptr [rbp + 0x794]
0x18000370a: cmp       dword ptr [rbp + 0x790], 0
0x180003711: je        0x180003729
0x180003713: test      eax, eax
0x180003715: jne       0x180003729
0x180003717: mov       rcx, qword ptr [rbp + 0x1418]
0x18000371e: call      0x180014670
0x180003723: mov       eax, dword ptr [rbp + 0x794]
0x180003729: mov       dword ptr [rbp + 0x790], eax
0x18000372f: lea       rcx, [rbp + 0x750]
0x180003736: mov       eax, dword ptr [rcx + 0x4c]
0x180003739: mov       edx, edi
0x18000373b: mov       r8d, dword ptr [rcx + 0x54]
0x18000373f: mov       dword ptr [rcx + 0x48], eax
0x180003742: mov       dword ptr [rcx + 0x50], r8d
0x180003746: call      0x180006f30
0x18000374b: mov       eax, dword ptr [rbp + 0xc]
0x18000374e: test      eax, 0xfffffffe
0x180003753: jne       0x180003764
0x180003755: lea       rcx, [rbp + 0xa88]
0x18000375c: call      0x1800074c0
0x180003761: mov       eax, dword ptr [rbp + 0xc]
0x180003764: test      eax, eax
0x180003766: jne       0x1800037a5
0x180003768: lea       rcx, [rbp + 0xb20]
0x18000376f: call      0x18001e8d0
0x180003774: lea       rcx, [rbp + 0xdc8]
0x18000377b: call      0x18001eb20
0x180003780: lea       rcx, [rbp + 0xef8]
0x180003787: lea       rdx, [rbp + 0x1308]
0x18000378e: call      0x18001f280
0x180003793: cmp       dword ptr [rbp + 0xc], 0
0x180003797: jne       0x1800037a5
0x180003799: mov       rcx, qword ptr [rbp + 0xef0]
0x1800037a0: call      0x1800168a0
0x1800037a5: mov       eax, dword ptr [rbp + 0xb10]
0x1800037ab: lea       rcx, [rbp + 0x6f8]
0x1800037b2: mov       dword ptr [rbp + 0xb14], eax
0x1800037b8: call      0x1800067a0
0x1800037bd: cmp       dword ptr [rbp + 8], 0
0x1800037c1: je        0x1800037cb
0x1800037c3: mov       rcx, rbp
0x1800037c6: call      0x18001f800
0x1800037cb: mov       eax, dword ptr [rbp + 0x32d8]
0x1800037d1: mov       dword ptr [rbp + 0x32dc], eax
0x1800037d7: mov       eax, dword ptr [rbp + 0x7d4]
0x1800037dd: mov       edx, dword ptr [rbp + 0x7b4]
0x1800037e3: mov       edi, dword ptr [rbp + 0x7c4]
0x1800037e9: mov       r8d, dword ptr [rbp + 0x7bc]
0x1800037f0: mov       dword ptr [rbp + 0x7b8], edx
0x1800037f6: mov       dword ptr [rbp + 0x7c8], edi
0x1800037fc: mov       dword ptr [rbp + 0x7c0], r8d
0x180003803: cmp       eax, dword ptr [rbp + 0x7d8]
0x180003809: je        0x18000381b
0x18000380b: mov       dword ptr [rbp + 0x7d8], eax
0x180003811: mov       dword ptr [rbp + 0xa30], 1
0x18000381b: mov       eax, dword ptr [rbp + 0x7cc]
0x180003821: mov       dword ptr [rbp + 0x7d0], eax
0x180003827: movss     xmm0, dword ptr [rbp + 0x738]
0x18000382f: movss     xmm1, dword ptr [rbp + 0x730]
0x180003837: test      eax, eax
0x180003839: jne       0x180003840
0x18000383b: comiss    xmm1, xmm0
0x18000383e: jae       0x18000384c
0x180003840: mov       dword ptr [rbp + 0xa80], 0xbf800000
0x18000384a: jmp       0x18000386f
0x18000384c: subss     xmm1, xmm0
0x180003850: addss     xmm1, dword ptr [rip + 0x2e4ee0]
0x180003858: xorps     xmm1, xmmword ptr [rip + 0x2e4e41]
0x18000385f: mulss     xmm1, dword ptr [rip + 0x2e4ed5]
0x180003867: movss     dword ptr [rbp + 0xa80], xmm1
0x18000386f: mov       eax, dword ptr [rbp + 0x7dc]
0x180003875: mov       edx, dword ptr [rbp + 0x7e4]
0x18000387b: mov       dword ptr [rbp + 0x7e0], eax
0x180003881: mov       dword ptr [rbp + 0x7e8], edx
0x180003887: cmp       dword ptr [rbp + 0x7ec], 0
0x18000388e: jne       0x180003c89
0x180003894: mov       eax, dword ptr [rbp + 0x32c0]
0x18000389a: mov       dword ptr [rbp + 0x32c4], eax
0x1800038a0: cmp       dword ptr [rbp + 0x32bc], 0
0x1800038a7: je        0x180003bf8
0x1800038ad: mov       r8d, dword ptr [rbp + 0x2b0c]
0x1800038b4: xor       edi, edi
0x1800038b6: test      r8d, r8d
0x1800038b9: cmovle    r8d, edi
0x1800038bd: mov       eax, dword ptr [rbp + 0x2b10]
0x1800038c3: cmp       eax, r8d
0x1800038c6: mov       edx, dword ptr [rbp + 0x2b14]
0x1800038cc: cmovg     r8d, eax
0x1800038d0: cmp       edx, r8d
0x1800038d3: mov       qword ptr [rsp + 0x70], rbx
0x1800038d8: cmovg     r8d, edx
0x1800038dc: xor       edx, edx
0x1800038de: mov       ebx, dword ptr [rbp + 0x2b18]
0x1800038e4: cmp       ebx, r8d
0x1800038e7: mov       r9d, dword ptr [rbp + 0x2b1c]
0x1800038ee: cmovg     r8d, ebx
0x1800038f2: cmp       r9d, r8d
0x1800038f5: mov       r10d, dword ptr [rbp + 0x2b20]
0x1800038fc: cmovg     r8d, r9d
0x180003900: mov       r9d, 0xffffffc4
0x180003906: cmp       r10d, r8d
0x180003909: mov       r11d, dword ptr [rbp + 0x2b24]
0x180003910: cmovg     r8d, r10d
0x180003914: mov       r10d, 0x3c
0x18000391a: cmp       r11d, r8d
0x18000391d: mov       r12d, dword ptr [rbp + 0x2b28]
0x180003924: cmovg     r8d, r11d
0x180003928: cmp       r12d, r8d
0x18000392b: mov       r13d, dword ptr [rbp + 0x2b2c]
0x180003932: cmovg     r8d, r12d
0x180003936: cmp       r13d, r8d
0x180003939: mov       r14d, dword ptr [rbp + 0x2b30]
0x180003940: cmovg     r8d, r13d
0x180003944: cmp       r14d, r8d
0x180003947: mov       r15d, dword ptr [rbp + 0x2b34]
0x18000394e: cmovg     r8d, r14d
0x180003952: cmp       r15d, r8d
0x180003955: mov       r12, qword ptr [rbp + 0x30]
0x180003959: cmovg     r8d, r15d
0x18000395d: mov       ecx, dword ptr [rbp + 0x2b38]
0x180003963: cmp       ecx, r8d
0x180003966: mov       r11d, dword ptr [r12 + 0xc]
0x18000396b: cmovg     r8d, ecx
0x18000396f: mov       qword ptr [rsp + 0x78], rsi
0x180003974: mov       eax, dword ptr [rbp + rdx*4 + 0x2b0c]
0x18000397b: xor       ebx, ebx
0x18000397d: neg       eax
0x18000397f: pxor      xmm1, xmm1
0x180003983: add       eax, r8d
0x180003986: cmp       eax, 0x3c
0x180003989: cmovge    eax, r10d
0x18000398d: cmp       eax, -0x3c
0x180003990: cmovle    eax, r9d
0x180003994: shl       eax, 0x17
0x180003997: neg       eax
0x180003999: add       eax, 0x3f800000
0x18000399e: mov       dword ptr [rsp + 0xa8], eax
0x1800039a5: xor       eax, eax
0x1800039a7: movss     xmm0, dword ptr [rsp + 0xa8]
0x1800039b0: test      r11d, r11d
0x1800039b3: jbe       0x180003b03
0x1800039b9: xor       r14d, r14d
0x1800039bc: lea       r13, [rdx + rdx*4]
0x1800039c0: mov       rsi, r13
0x1800039c3: shl       rsi, 6
0x1800039c7: shl       r13, 4
0x1800039cb: add       rsi, rbp
0x1800039ce: mov       r15, qword ptr [r12 + 0x50]
0x1800039d3: add       r13, rbp
0x1800039d6: mov       dword ptr [rsp + 0xa0], edi
0x1800039dd: xor       edi, edi
0x1800039df: mov       qword ptr [rsp + 0x98], rdx
0x1800039e7: mov       qword ptr [rsp + 0x90], r12
0x1800039ef: mov       dword ptr [rsp + 0x88], r8d
0x1800039f7: mov       qword ptr [rsp + 0x80], rbp
0x1800039ff: movss     xmm1, dword ptr [r13 + rbx*4 + 0x2b3c]
0x180003a09: mov       r8d, eax
0x180003a0c: mov       ebp, dword ptr [r15 + rbx*4]
0x180003a10: mulss     xmm1, xmm0
0x180003a14: cmp       eax, ebp
0x180003a16: jae       0x180003ac4
0x180003a1c: sub       ebp, eax
0x180003a1e: cmp       ebp, 8
0x180003a21: jl        0x180003c82
0x180003a27: mov       eax, eax
0x180003a29: lea       rcx, [rsi + rax*4 + 0x1c0c]
0x180003a31: and       rcx, 0xf
0x180003a35: mov       edx, ecx
0x180003a37: neg       edx
0x180003a39: add       edx, 0x10
0x180003a3c: shr       edx, 2
0x180003a3f: test      ecx, ecx
0x180003a41: cmovne    ecx, edx
0x180003a44: lea       ebx, [rcx + 8]
0x180003a47: cmp       ebp, ebx
0x180003a49: jl        0x180003c82
0x180003a4f: mov       ebx, ebp
0x180003a51: mov       edx, edi
0x180003a53: sub       ebx, ecx
0x180003a55: mov       r12d, eax
0x180003a58: and       ebx, 7
0x180003a5b: neg       ebx
0x180003a5d: add       ebx, ebp
0x180003a5f: test      ecx, ecx
0x180003a61: jbe       0x180003a76
0x180003a63: inc       edx
0x180003a65: movss     dword ptr [rsi + r12*4 + 0x1c0c], xmm1
0x180003a6f: inc       r12d
0x180003a72: cmp       edx, ecx
0x180003a74: jb        0x180003a63
0x180003a76: movaps    xmm2, xmm1
0x180003a79: shufps    xmm2, xmm2, 0
0x180003a7d: lea       edx, [rax + rcx]
0x180003a80: movups    xmmword ptr [rsi + rdx*4 + 0x1c0c], xmm2
0x180003a88: lea       r12d, [rax + rcx + 4]
0x180003a8d: add       ecx, 8
0x180003a90: movups    xmmword ptr [rsi + r12*4 + 0x1c0c], xmm2
0x180003a99: cmp       ecx, ebx
0x180003a9b: jb        0x180003a7d
0x180003a9d: add       eax, ecx
0x180003a9f: neg       ecx
0x180003aa1: lea       eax, [rax + rcx - 1]
0x180003aa5: lea       eax, [rax + rbx + 1]
0x180003aa9: cmp       ebx, ebp
0x180003aab: jae       0x180003ac4
0x180003aad: lea       eax, [rbx + r8]
0x180003ab1: inc       ebx
0x180003ab3: movss     dword ptr [rsi + rax*4 + 0x1c0c], xmm1
0x180003abc: cmp       ebx, ebp
0x180003abe: jb        0x180003aad
0x180003ac0: lea       eax, [r8 + rbx]
0x180003ac4: inc       r14d
0x180003ac7: mov       ebx, r14d
0x180003aca: cmp       r14d, r11d
0x180003acd: jb        0x1800039ff
0x180003ad3: mov       edi, dword ptr [rsp + 0xa0]
0x180003ada: mov       rdx, qword ptr [rsp + 0x98]
0x180003ae2: mov       r12, qword ptr [rsp + 0x90]
0x180003aea: mov       r8d, dword ptr [rsp + 0x88]
0x180003af2: mov       rbp, qword ptr [rsp + 0x80]
0x180003afa: cmp       eax, 0x50
0x180003afd: jae       0x180003bd0
0x180003b03: mov       esi, eax
0x180003b05: neg       esi
0x180003b07: add       esi, 0x50
0x180003b0a: cmp       esi, 8
0x180003b0d: jl        0x180003c7a
0x180003b13: mov       eax, eax
0x180003b15: lea       r14, [rdx + rdx*4]
0x180003b19: shl       r14, 6
0x180003b1d: add       r14, rbp
0x180003b20: lea       rbx, [r14 + rax*4 + 0x1c0c]
0x180003b28: and       rbx, 0xf
0x180003b2c: mov       r13d, ebx
0x180003b2f: neg       r13d
0x180003b32: add       r13d, 0x10
0x180003b36: shr       r13d, 2
0x180003b3a: test      ebx, ebx
0x180003b3c: cmovne    ebx, r13d
0x180003b40: lea       r15d, [rbx + 8]
0x180003b44: cmp       esi, r15d
0x180003b47: jl        0x180003c7a
0x180003b4d: mov       r13d, esi
0x180003b50: xor       r15d, r15d
0x180003b53: sub       r13d, ebx
0x180003b56: mov       ecx, eax
0x180003b58: and       r13d, 7
0x180003b5c: neg       r13d
0x180003b5f: add       r13d, esi
0x180003b62: test      ebx, ebx
0x180003b64: jbe       0x180003b7a
0x180003b66: inc       r15d
0x180003b69: movss     dword ptr [r14 + rcx*4 + 0x1c0c], xmm1
0x180003b73: inc       ecx
0x180003b75: cmp       r15d, ebx
0x180003b78: jb        0x180003b66
0x180003b7a: movaps    xmm0, xmm1
0x180003b7d: lea       r15d, [rax + rbx]
0x180003b81: shufps    xmm0, xmm0, 0
0x180003b85: lea       ecx, [rax + rbx + 4]
0x180003b89: add       ebx, 8
0x180003b8c: movups    xmmword ptr [r14 + r15*4 + 0x1c0c], xmm0
0x180003b95: add       r15d, 8
0x180003b99: movups    xmmword ptr [r14 + rcx*4 + 0x1c0c], xmm0
0x180003ba2: add       ecx, 8
0x180003ba5: cmp       ebx, r13d
0x180003ba8: jb        0x180003b89
0x180003baa: add       eax, r13d
0x180003bad: cmp       r13d, esi
0x180003bb0: jae       0x180003bd0
0x180003bb2: lea       rdx, [rdx + rdx*4]
0x180003bb6: shl       rdx, 6
0x180003bba: add       rdx, rbp
0x180003bbd: inc       r13d
0x180003bc0: movss     dword ptr [rdx + rax*4 + 0x1c0c], xmm1
0x180003bc9: inc       eax
0x180003bcb: cmp       r13d, esi
0x180003bce: jb        0x180003bbd
0x180003bd0: inc       edi
0x180003bd2: mov       edx, edi
0x180003bd4: cmp       edi, 0xc
0x180003bd7: jb        0x180003974
0x180003bdd: mov       rbx, qword ptr [rsp + 0x70]
0x180003be2: mov       rsi, qword ptr [rsp + 0x78]
0x180003be7: mov       dword ptr [rbp + 0x1c08], r8d
0x180003bee: mov       dword ptr [rbp + 0x32bc], 0
0x180003bf8: movdqu    xmm0, xmmword ptr [rbp + 0x1874]
0x180003c00: mov       edx, dword ptr [rbp + 0x1888]
0x180003c06: mov       dword ptr [rbp + 0x1870], edx
0x180003c0c: mov       eax, dword ptr [rbp + 0x1884]
0x180003c12: mov       edi, dword ptr [rbp + 0x1890]
0x180003c18: mov       edx, dword ptr [rbp + 0x36c4]
0x180003c1e: movdqu    xmmword ptr [rbp + 0x185c], xmm0
0x180003c26: mov       dword ptr [rbp + 0x186c], eax
0x180003c2c: mov       dword ptr [rbp + 0x188c], edi
0x180003c32: mov       dword ptr [rbp + 0x36c0], edx
0x180003c38: cmp       dword ptr [rbp + 0x28], 0
0x180003c3c: je        0x180003c5e
0x180003c3e: cmp       dword ptr [rbp + 0xc], 0
0x180003c42: jne       0x180003c5e
0x180003c44: cmp       qword ptr [rbp + 0x18], 0
0x180003c49: je        0x180003c5e
0x180003c4b: cmp       qword ptr [rbp + 0x20], 0
0x180003c50: je        0x180003c5e
0x180003c52: lea       rcx, [rbp + 0x36c8]
0x180003c59: call      0x1800075b0
0x180003c5e: mov       dword ptr [rbp + 0x13b0], 0
0x180003c68: add       rsp, 0xb8
0x180003c6f: pop       rbp
0x180003c70: pop       r15
0x180003c72: pop       r14
0x180003c74: pop       r13
0x180003c76: pop       r12
0x180003c78: pop       rdi
0x180003c79: ret       
0x180003c7a: xor       r13d, r13d
