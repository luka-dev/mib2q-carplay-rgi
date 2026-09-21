0x180022640: sub       rsp, 0x78
0x180022644: mov       r10, rcx
0x180022647: xor       ecx, ecx
0x180022649: xor       eax, eax
0x18002264b: test      r9d, r9d
0x18002264e: jbe       0x18002289f
0x180022654: xor       r11d, r11d
0x180022657: mov       qword ptr [rsp + 0x38], r12
0x18002265c: mov       qword ptr [rsp + 0x20], rbx
0x180022661: mov       qword ptr [rsp + 0x28], rsi
0x180022666: mov       qword ptr [rsp + 0x30], rdi
0x18002266b: mov       qword ptr [rsp + 0x40], r13
0x180022670: mov       qword ptr [rsp + 0x48], r14
0x180022675: mov       qword ptr [rsp + 0x50], r15
0x18002267a: mov       qword ptr [rsp + 0x58], rbp
0x18002267f: xor       ebp, ebp
0x180022681: mov       r12, qword ptr [rsp + 0xa0]
0x180022689: mov       esi, dword ptr [r8 + rax*4]
0x18002268d: cmp       esi, ecx
0x18002268f: ja        0x18002269a
0x180022691: mov       dword ptr [r12 + rax*4], ebp
0x180022695: jmp       0x180022866
0x18002269a: mov       edi, ebp
0x18002269c: pxor      xmm0, xmm0
0x1800226a0: movaps    xmm1, xmm0
0x1800226a3: test      edx, edx
0x1800226a5: jbe       0x180022858
0x1800226ab: mov       qword ptr [rsp + 0x70], rax
0x1800226b0: lea       ebx, [rcx + rcx]
0x1800226b3: neg       ecx
0x1800226b5: mov       qword ptr [rsp + 0x68], r8
0x1800226ba: add       ecx, esi
0x1800226bc: mov       dword ptr [rsp + 0x60], r9d
0x1800226c1: mov       eax, ebp
0x1800226c3: lea       r9d, [rdi + 3]
0x1800226c7: lea       r8d, [rdi + 1]
0x1800226cb: cmp       r9d, edx
0x1800226ce: jb        0x18002278e
0x1800226d4: mov       edi, edi
0x1800226d6: mov       r9, qword ptr [r10 + rdi*8]
0x1800226da: cmp       r8d, edx
0x1800226dd: jb        0x18002271a
0x1800226df: pxor      xmm2, xmm2
0x1800226e3: lea       r12d, [rbx + rax*2]
0x1800226e7: pxor      xmm3, xmm3
0x1800226eb: lea       edi, [r12 + 1]
0x1800226f0: cvtss2sd  xmm2, dword ptr [r9 + rdi*4]
0x1800226f6: cvtss2sd  xmm3, dword ptr [r9 + r12*4]
0x1800226fc: mulsd     xmm2, xmm2
0x180022700: mulsd     xmm3, xmm3
0x180022704: addsd     xmm1, xmm2
0x180022708: addsd     xmm0, xmm3
0x18002270c: inc       eax
0x18002270e: cmp       eax, ecx
0x180022710: jb        0x1800226df
0x180022712: mov       edi, r8d
0x180022715: jmp       0x180022839
0x18002271a: mov       r8d, r8d
0x18002271d: mov       r8, qword ptr [r10 + r8*8]
0x180022721: nop       dword ptr [rax + rax]
0x180022729: nop       dword ptr [rax]
0x180022730: pxor      xmm2, xmm2
0x180022734: lea       r13d, [rbx + rax*2]
0x180022738: pxor      xmm4, xmm4
0x18002273c: lea       r12d, [r13 + 1]
0x180022740: cvtss2sd  xmm2, dword ptr [r9 + r12*4]
0x180022746: cvtss2sd  xmm4, dword ptr [r9 + r13*4]
0x18002274c: mulsd     xmm2, xmm2
0x180022750: mulsd     xmm4, xmm4
0x180022754: addsd     xmm1, xmm2
0x180022758: addsd     xmm0, xmm4
0x18002275c: pxor      xmm3, xmm3
0x180022760: pxor      xmm5, xmm5
0x180022764: cvtss2sd  xmm3, dword ptr [r8 + r12*4]
0x18002276a: cvtss2sd  xmm5, dword ptr [r8 + r13*4]
0x180022770: mulsd     xmm3, xmm3
0x180022774: mulsd     xmm5, xmm5
0x180022778: addsd     xmm1, xmm3
0x18002277c: addsd     xmm0, xmm5
0x180022780: inc       eax
0x180022782: cmp       eax, ecx
0x180022784: jb        0x180022730
0x180022786: add       edi, 2
0x180022789: jmp       0x180022839
0x18002278e: lea       r15d, [rdi + 2]
0x180022792: mov       r13, qword ptr [r10 + r8*8]
0x180022796: mov       r14, qword ptr [r10 + r9*8]
0x18002279a: mov       r12, qword ptr [r10 + rdi*8]
0x18002279e: mov       r8, qword ptr [r10 + r15*8]
0x1800227a2: pxor      xmm5, xmm5
0x1800227a6: lea       r9d, [rbx + rax*2]
0x1800227aa: inc       eax
0x1800227ac: lea       r15d, [r9 + 1]
0x1800227b0: cvtss2sd  xmm2, dword ptr [r12 + r15*4]
0x1800227b6: cvtss2sd  xmm3, dword ptr [r13 + r15*4]
0x1800227bd: cvtss2sd  xmm4, dword ptr [r8 + r15*4]
0x1800227c3: cvtss2sd  xmm5, dword ptr [r14 + r15*4]
0x1800227c9: mulsd     xmm2, xmm2
0x1800227cd: mulsd     xmm3, xmm3
0x1800227d1: mulsd     xmm4, xmm4
0x1800227d5: addsd     xmm1, xmm2
0x1800227d9: mulsd     xmm5, xmm5
0x1800227dd: cvtss2sd  xmm2, dword ptr [r12 + r9*4]
0x1800227e3: addsd     xmm1, xmm3
0x1800227e7: mulsd     xmm2, xmm2
0x1800227eb: addsd     xmm1, xmm4
0x1800227ef: addsd     xmm0, xmm2
0x1800227f3: addsd     xmm1, xmm5
0x1800227f7: pxor      xmm3, xmm3
0x1800227fb: pxor      xmm4, xmm4
0x1800227ff: cvtss2sd  xmm3, dword ptr [r13 + r9*4]
0x180022806: cvtss2sd  xmm4, dword ptr [r8 + r9*4]
0x18002280c: mulsd     xmm3, xmm3
0x180022810: mulsd     xmm4, xmm4
0x180022814: addsd     xmm0, xmm3
0x180022818: pxor      xmm2, xmm2
0x18002281c: addsd     xmm0, xmm4
0x180022820: cvtss2sd  xmm2, dword ptr [r14 + r9*4]
0x180022826: mulsd     xmm2, xmm2
0x18002282a: addsd     xmm0, xmm2
0x18002282e: cmp       eax, ecx
0x180022830: jb        0x1800227a2
0x180022836: add       edi, 4
0x180022839: cmp       edi, edx
0x18002283b: jb        0x1800226c1
0x180022841: mov       r12, qword ptr [rsp + 0xa0]
0x180022849: mov       rax, qword ptr [rsp + 0x70]
0x18002284e: mov       r8, qword ptr [rsp + 0x68]
0x180022853: mov       r9d, dword ptr [rsp + 0x60]
0x180022858: addsd     xmm0, xmm1
0x18002285c: cvtsd2ss  xmm0, xmm0
0x180022860: movss     dword ptr [r12 + rax*4], xmm0
0x180022866: inc       r11d
0x180022869: mov       ecx, esi
0x18002286b: mov       eax, r11d
0x18002286e: cmp       r11d, r9d
0x180022871: jb        0x180022689
0x180022877: mov       rbx, qword ptr [rsp + 0x20]
0x18002287c: mov       rsi, qword ptr [rsp + 0x28]
0x180022881: mov       rdi, qword ptr [rsp + 0x30]
0x180022886: mov       r12, qword ptr [rsp + 0x38]
0x18002288b: mov       r13, qword ptr [rsp + 0x40]
0x180022890: mov       r14, qword ptr [rsp + 0x48]
0x180022895: mov       r15, qword ptr [rsp + 0x50]
0x18002289a: mov       rbp, qword ptr [rsp + 0x58]
0x18002289f: add       rsp, 0x78
0x1800228a3: ret       
0x1800228a4: nop       dword ptr [rax + rax]
0x1800228a9: nop       dword ptr [rax]
