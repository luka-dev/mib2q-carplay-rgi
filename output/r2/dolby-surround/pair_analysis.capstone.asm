0x180031410: push      rsi
0x180031411: push      rdi
0x180031412: push      r12
0x180031414: push      r13
0x180031416: push      r14
0x180031418: push      r15
0x18003141a: push      rbp
0x18003141b: sub       rsp, 0xd0
0x180031422: mov       rsi, r9
0x180031425: mov       rbp, qword ptr [rsp + 0x160]
0x18003142d: mov       r15, r8
0x180031430: mov       r14, rdx
0x180031433: mov       r13, rcx
0x180031436: lea       rdi, [rbp + 0x1f]
0x18003143a: and       rdi, 0xffffffffffffffe0
0x18003143e: cmp       dword ptr [rsp + 0x130], 0
0x180031446: je        0x180031455
0x180031448: xor       r8d, r8d
0x18003144b: cmp       dword ptr [rsi + 4], 0
0x18003144f: setge     r8b
0x180031453: jmp       0x180031458
0x180031455: xor       r8d, r8d
0x180031458: lea       r12, [rsi + 8]
0x18003145c: mov       rdx, r12
0x18003145f: lea       rcx, [r13 + 0x6838]
0x180031466: call      0x18004de30
0x18003146b: mov       rax, rsp
0x18003146e: mov       rcx, r13
0x180031471: mov       r10, qword ptr [r14]
0x180031474: mov       r9, r15
0x180031477: mov       r14d, dword ptr [r14 + 8]
0x18003147b: mov       r11d, dword ptr [rsp + 0x158]
0x180031483: mov       rdx, qword ptr [rsp + 0x148]
0x18003148b: mov       r8, qword ptr [rsp + 0x150]
0x180031493: mov       qword ptr [rax + 0x20], r10
0x180031497: mov       dword ptr [rax + 0x28], r14d
0x18003149b: mov       dword ptr [rax + 0x30], r11d
0x18003149f: call      0x18004d0b0
0x1800314a4: mov       r9d, dword ptr [rsp + 0x140]
0x1800314ac: cmp       dword ptr [rsp + 0x138], 0
0x1800314b4: je        0x1800314d6
0x1800314b6: mov       r10d, dword ptr [rsi + 0x30]
0x1800314ba: mov       r14, qword ptr [rsi + 0x38]
0x1800314be: mov       dword ptr [rdi + 4], r10d
0x1800314c2: test      r9d, r9d
0x1800314c5: je        0x1800317e0
0x1800314cb: mov       r11, qword ptr [rsi + 0x50]
0x1800314cf: movss     xmm1, dword ptr [rsi + 0x48]
0x1800314d4: jmp       0x1800314f9
0x1800314d6: mov       r14, qword ptr [rsi + 0x40]
0x1800314da: mov       r10d, 2
0x1800314e0: mov       dword ptr [rdi + 4], 2
0x1800314e7: test      r9d, r9d
0x1800314ea: je        0x1800317e0
0x1800314f0: mov       r11, qword ptr [rsi + 0x58]
0x1800314f4: movss     xmm1, dword ptr [rsi + 0x4c]
0x1800314f9: mov       dword ptr [rdi], 1
0x1800314ff: lea       rbp, [rbp + 0x31e]
0x180031506: imul      r10d, dword ptr [rsi]
0x18003150a: and       rbp, 0xffffffffffffffe0
0x18003150e: movss     xmm2, dword ptr [rip + 0x2c3d9a]
0x180031516: movss     xmm0, dword ptr [rip + 0x2c3d96]
0x18003151e: movups    xmmword ptr [rsp + 0x90], xmm6
0x180031526: movups    xmmword ptr [rsp + 0xa0], xmm14
0x18003152f: mov       esi, r10d
0x180031532: xor       r15d, r15d
0x180031535: and       esi, 7
0x180031538: xor       r9d, r9d
0x18003153b: neg       esi
0x18003153d: mov       qword ptr [rdi + 8], rbp
0x180031541: add       esi, r10d
0x180031544: movups    xmmword ptr [rsp + 0xb0], xmm15
0x18003154d: mov       qword ptr [rsp + 0x40], rdi
0x180031552: mov       qword ptr [rsp + 0xc0], rbx
0x18003155a: movss     xmm6, dword ptr [r13 + r15*4 + 0x140]
0x180031564: mov       rax, rbp
0x180031567: movaps    xmm5, xmm6
0x18003156a: movaps    xmm4, xmm6
0x18003156d: mulss     xmm5, xmm6
0x180031571: movss     xmm3, dword ptr [rip + 0x2c3d3f]
0x180031579: subss     xmm3, xmm5
0x18003157d: movaps    xmm5, xmm2
0x180031580: sqrtss    xmm3, xmm3
0x180031584: mulss     xmm4, xmm3
0x180031588: mulss     xmm4, xmm1
0x18003158c: mulss     xmm4, xmm2
0x180031590: addss     xmm4, xmm0
0x180031594: sqrtss    xmm4, xmm4
0x180031598: divss     xmm5, xmm4
0x18003159c: mulss     xmm6, xmm5
0x1800315a0: mulss     xmm5, xmm3
0x1800315a4: test      r10d, r10d
0x1800315a7: jbe       0x1800317a7
0x1800315ad: cmp       r10d, 8
0x1800315b1: jb        0x180031850
0x1800315b7: mov       rdi, rbp
0x1800315ba: and       rdi, 0xf
0x1800315be: mov       edi, edi
0x1800315c0: test      edi, edi
0x1800315c2: je        0x180031618
0x1800315c4: neg       edi
0x1800315c6: add       edi, 0x10
0x1800315c9: shr       edi, 2
0x1800315cc: lea       ebx, [rdi + 8]
0x1800315cf: cmp       r10d, ebx
0x1800315d2: jb        0x18003183d
0x1800315d8: mov       ebx, r10d
0x1800315db: xor       edx, edx
0x1800315dd: sub       ebx, edi
0x1800315df: xor       r15d, r15d
0x1800315e2: and       ebx, 7
0x1800315e5: neg       ebx
0x1800315e7: add       ebx, r10d
0x1800315ea: test      edi, edi
0x1800315ec: je        0x18003161a
0x1800315ee: movss     xmm4, dword ptr [r11 + rdx*4]
0x1800315f4: movss     xmm3, dword ptr [r14 + rdx*4]
0x1800315fa: inc       edx
0x1800315fc: mulss     xmm4, xmm6
0x180031600: mulss     xmm3, xmm5
0x180031604: addss     xmm4, xmm3
0x180031608: movss     dword ptr [rbp + r15*4], xmm4
0x18003160f: inc       r15
0x180031612: cmp       edx, edi
0x180031614: jb        0x1800315ee
0x180031616: jmp       0x18003161a
0x180031618: mov       ebx, esi
0x18003161a: lea       r15, [r14 + rdi*4]
0x18003161e: test      r15, 0xf
0x180031625: je        0x18003168a
0x180031627: movaps    xmm4, xmm6
0x18003162a: movaps    xmm3, xmm5
0x18003162d: shufps    xmm4, xmm4, 0
0x180031631: shufps    xmm3, xmm3, 0
0x180031635: nop       dword ptr [rax]
0x180031639: nop       dword ptr [rax]
0x180031640: mov       edi, edi
0x180031642: movups    xmm15, xmmword ptr [r11 + rdi*4]
0x180031647: movups    xmm14, xmmword ptr [r14 + rdi*4]
0x18003164c: mulps     xmm15, xmm4
0x180031650: mulps     xmm14, xmm3
0x180031654: movsxd    rdi, edi
0x180031657: addps     xmm15, xmm14
0x18003165b: movups    xmmword ptr [rbp + rdi*4], xmm15
0x180031661: lea       r15d, [rdi + 4]
0x180031665: movups    xmm15, xmmword ptr [r11 + r15*4]
0x18003166a: movups    xmm14, xmmword ptr [r14 + r15*4]
0x18003166f: mulps     xmm15, xmm4
0x180031673: mulps     xmm14, xmm3
0x180031677: addps     xmm15, xmm14
0x18003167b: movups    xmmword ptr [rbp + rdi*4 + 0x10], xmm15
0x180031681: add       edi, 8
0x180031684: cmp       edi, ebx
0x180031686: jb        0x180031640
0x180031688: jmp       0x1800316e8
0x18003168a: movaps    xmm4, xmm6
0x18003168d: movaps    xmm3, xmm5
0x180031690: shufps    xmm4, xmm4, 0
0x180031694: shufps    xmm3, xmm3, 0
0x180031698: nop       dword ptr [rax + rax]
0x1800316a0: mov       edi, edi
0x1800316a2: movups    xmm15, xmmword ptr [r11 + rdi*4]
0x1800316a7: movups    xmm14, xmmword ptr [r14 + rdi*4]
0x1800316ac: mulps     xmm15, xmm4
0x1800316b0: mulps     xmm14, xmm3
0x1800316b4: movsxd    rdi, edi
0x1800316b7: addps     xmm15, xmm14
0x1800316bb: movups    xmmword ptr [rbp + rdi*4], xmm15
0x1800316c1: lea       r15d, [rdi + 4]
0x1800316c5: movups    xmm15, xmmword ptr [r11 + r15*4]
0x1800316ca: movups    xmm14, xmmword ptr [r14 + r15*4]
0x1800316cf: mulps     xmm15, xmm4
0x1800316d3: mulps     xmm14, xmm3
0x1800316d7: addps     xmm15, xmm14
0x1800316db: movups    xmmword ptr [rbp + rdi*4 + 0x10], xmm15
0x1800316e1: add       edi, 8
0x1800316e4: cmp       edi, ebx
0x1800316e6: jb        0x1800316a0
0x1800316e8: movsxd    r15, edi
0x1800316eb: sub       edi, ebx
0x1800316ed: sub       r15, rdi
0x1800316f0: lea       rbp, [rbp + r15*4]
0x1800316f5: lea       edi, [rbx + 1]
0x1800316f8: cmp       edi, r10d
0x1800316fb: ja        0x1800317a7
0x180031701: mov       ecx, r10d
0x180031704: sub       ecx, ebx
0x180031706: cmp       ecx, 4
0x180031709: jb        0x180031849
0x18003170f: mov       edx, ecx
0x180031711: lea       ebp, [rbx*4]
0x180031718: movsxd    rdi, ebp
0x18003171b: and       edx, 0xfffffffc
0x18003171e: xor       r15d, r15d
0x180031721: lea       r8d, [rbx + r15]
0x180031725: add       r15d, 4
0x180031729: movups    xmm15, xmmword ptr [r11 + r8*4]
0x18003172e: movups    xmm14, xmmword ptr [r14 + r8*4]
0x180031733: mulps     xmm15, xmm4
0x180031737: mulps     xmm14, xmm3
0x18003173b: addps     xmm15, xmm14
0x18003173f: movups    xmmword ptr [rdi + rax], xmm15
0x180031744: add       rdi, 0x10
0x180031748: cmp       r15d, edx
0x18003174b: jb        0x180031721
0x18003174d: lea       ebp, [rbp + r15*4]
0x180031752: sub       r15d, edx
0x180031755: movsxd    rbp, ebp
0x180031758: shl       r15, 2
0x18003175c: add       rbp, rax
0x18003175f: sub       rbp, r15
0x180031762: cmp       edx, ecx
0x180031764: jae       0x1800317a7
0x180031766: lea       ebp, [rdx + rbx]
0x180031769: movss     xmm4, dword ptr [r11 + rbp*4]
0x18003176f: lea       edi, [rdx*4]
0x180031776: movss     xmm3, dword ptr [r14 + rbp*4]
0x18003177c: lea       r15d, [rdi + rbx*4]
0x180031780: mulss     xmm4, xmm6
0x180031784: inc       edx
0x180031786: mulss     xmm3, xmm5
0x18003178a: movsxd    r15, r15d
0x18003178d: addss     xmm4, xmm3
0x180031791: movss     dword ptr [r15 + rax], xmm4
0x180031797: cmp       edx, ecx
0x180031799: jb        0x180031766
0x18003179b: shl       edx, 2
0x18003179e: lea       ebp, [rdx + rbx*4]
0x1800317a1: movsxd    rbp, ebp
0x1800317a4: add       rbp, rax
0x1800317a7: inc       r9d
0x1800317aa: mov       r15d, r9d
0x1800317ad: cmp       r9d, 0x14
0x1800317b1: jb        0x18003155a
0x1800317b7: movups    xmm6, xmmword ptr [rsp + 0x90]
0x1800317bf: movups    xmm14, xmmword ptr [rsp + 0xa0]
0x1800317c8: movups    xmm15, xmmword ptr [rsp + 0xb0]
0x1800317d1: mov       rdi, qword ptr [rsp + 0x40]
0x1800317d6: mov       rbx, qword ptr [rsp + 0xc0]
0x1800317de: jmp       0x1800317ea
0x1800317e0: mov       qword ptr [rdi + 8], r14
0x1800317e4: mov       dword ptr [rdi], 0
0x1800317ea: lea       rsi, [r13 + 0x50]
0x1800317ee: mov       rcx, rsi
0x1800317f1: lea       rbp, [r13 + 0xa0]
0x1800317f8: mov       rdx, rbp
0x1800317fb: lea       r8, [rsp + 0x40]
0x180031800: call      0x18004e1c0
0x180031805: mov       r10, rsp
0x180031808: lea       rcx, [rdi + 0x10]
0x18003180c: mov       rdx, r12
0x18003180f: mov       r8, r13
0x180031812: mov       r9, rsi
0x180031815: mov       qword ptr [r10 + 0x20], rbp
0x180031819: lea       rbp, [rsp + 0x40]
0x18003181e: mov       qword ptr [r10 + 0x28], rbp
0x180031822: call      0x18004e2c0
0x180031827: mov       rax, rdi
0x18003182a: add       rsp, 0xd0
0x180031831: pop       rbp
0x180031832: pop       r15
0x180031834: pop       r14
0x180031836: pop       r13
0x180031838: pop       r12
0x18003183a: pop       rdi
0x18003183b: pop       rsi
0x18003183c: ret       
0x18003183d: xor       ebx, ebx
0x18003183f: mov       ecx, r10d
0x180031842: xor       edx, edx
0x180031844: jmp       0x180031762
0x180031849: xor       edx, edx
0x18003184b: jmp       0x180031762
0x180031850: cmp       r10d, 1
0x180031854: jae       0x18003183d
0x180031856: jmp       0x1800317a7
0x18003185b: nop       dword ptr [rax + rax]
