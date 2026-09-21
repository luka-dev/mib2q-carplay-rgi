0x18000850f: cmp       dword ptr [rbp + 0x77c], 0
0x180008516: je        0x180008994
0x18000851c: test      rdx, rdx
0x18000851f: je        0x18000b77e
0x180008525: cmp       ebx, 2
0x180008528: jne       0x180008994
0x18000852e: mov       rax, qword ptr [rsp + 0x970]
0x180008536: cmp       dword ptr [rax + 4], 0
0x18000853a: jne       0x180008994
0x180008540: cmp       dword ptr [rsp + 0x3b0], 2
0x180008548: ja        0x180008558
0x18000854a: cmp       dword ptr [rsp + 0x3b8], 0
0x180008552: je        0x180008994
0x180008558: cmp       dword ptr [rbp + 0x32dc], 0
0x18000855f: jne       0x180008994
0x180008565: mov       r10d, dword ptr [rbp + 0x11c]
0x18000856c: xor       ebx, ebx
0x18000856e: test      r15d, r15d
0x180008571: mov       eax, dword ptr [rbp + 0x790]
0x180008577: sete      bl
0x18000857a: test      rdx, rdx
0x18000857d: mov       dword ptr [rsp + 0xd8], ebx
0x180008584: mov       rbx, qword ptr [rsp + 0x970]
0x18000858c: cmove     rbx, r12
0x180008590: mov       dword ptr [rsp + 0xe0], eax
0x180008597: mov       r15d, dword ptr [rbx + 8]
0x18000859b: cmp       r10d, 0xf
0x18000859f: ja        0x1800085f8
0x1800085a1: lea       r11, [rip - 0x85a8]
0x1800085a8: movzx     eax, byte ptr [r11 + r10 + 0x2e8930]
0x1800085b1: lea       rax, [r11 + rax + 0x85bb]
0x1800085b9: jmp       rax
0x1800085bb: mov       r11d, 0xa
0x1800085c1: jmp       0x1800085fb
0x1800085c3: mov       r11d, 6
0x1800085c9: cmp       r15d, 8
0x1800085cd: cmove     r11d, r15d
0x1800085d1: jmp       0x1800085fb
0x1800085d3: mov       eax, 0xa
0x1800085d8: mov       r11d, 8
0x1800085de: cmp       r15d, 8
0x1800085e2: cmove     r11d, eax
0x1800085e6: jmp       0x1800085fb
0x1800085e8: mov       r11d, 8
0x1800085ee: jmp       0x1800085fb
0x1800085f0: mov       r11d, 6
0x1800085f6: jmp       0x1800085fb
0x1800085f8: xor       r11d, r11d
0x1800085fb: cmp       dword ptr [rsp + 0x3b8], 0
0x180008603: je        0x180008615
0x180008605: mov       eax, r10d
0x180008608: and       eax, 0xfffffffb
0x18000860b: cmp       eax, 9
0x18000860e: je        0x18000861e
0x180008610: cmp       eax, 0xb
0x180008613: je        0x18000861e
0x180008615: lea       eax, [r10 - 6]
0x180008619: cmp       eax, 1
0x18000861c: ja        0x18000862b
0x18000861e: mov       dword ptr [rsp + 0xc0], 1
0x180008629: jmp       0x180008636
0x18000862b: mov       dword ptr [rsp + 0xc0], 0
0x180008636: cmp       r15d, 2
0x18000863a: jne       0x1800086af
0x18000863c: cmp       r11d, 6
0x180008640: je        0x18000869b
0x180008642: cmp       r11d, 8
0x180008646: jne       0x18000866f
0x180008648: cmp       dword ptr [rsp + 0xc0], 0
0x180008650: lea       rax, [rip + 0x2e46c9]
0x180008657: lea       r10, [rip + 0x2e4692]
0x18000865e: cmovne    r10, rax
0x180008662: mov       qword ptr [rsp + 0xc8], r10
0x18000866a: jmp       0x180008846
0x18000866f: cmp       r11d, 0xa
0x180008673: jne       0x18000883a
0x180008679: cmp       dword ptr [rsp + 0xc0], 0
0x180008681: je        0x18000883a
0x180008687: lea       rax, [rip + 0x2e46c2]
0x18000868e: mov       qword ptr [rsp + 0xc8], rax
0x180008696: jmp       0x180008846
0x18000869b: lea       rax, [rip + 0x2e461e]
0x1800086a2: mov       qword ptr [rsp + 0xc8], rax
0x1800086aa: jmp       0x180008846
0x1800086af: cmp       r15d, 6
0x1800086b3: jne       0x180008793
0x1800086b9: cmp       r11d, 6
0x1800086bd: jne       0x1800086e6
0x1800086bf: cmp       dword ptr [rsp + 0xe0], 0
0x1800086c7: lea       rax, [rip + 0x2e475a]
0x1800086ce: lea       r10, [rip + 0x2e4723]
0x1800086d5: cmovne    r10, rax
0x1800086d9: mov       qword ptr [rsp + 0xc8], r10
0x1800086e1: jmp       0x180008846
0x1800086e6: cmp       r11d, 8
0x1800086ea: jne       0x180008762
0x1800086ec: cmp       dword ptr [rsp + 0xc0], 0
0x1800086f4: je        0x18000873b
0x1800086f6: and       r10d, 0xfffffffb
0x1800086fa: cmp       r10d, 9
0x1800086fe: je        0x180008727
0x180008700: cmp       dword ptr [rsp + 0xe0], 0
0x180008708: lea       rax, [rip + 0x2e47f1]
0x18000870f: lea       r10, [rip + 0x2e47a2]
0x180008716: cmovne    r10, rax
0x18000871a: mov       qword ptr [rsp + 0xc8], r10
0x180008722: jmp       0x180008846
0x180008727: lea       rax, [rip + 0x2e481a]
0x18000872e: mov       qword ptr [rsp + 0xc8], rax
0x180008736: jmp       0x180008846
0x18000873b: cmp       dword ptr [rsp + 0xe0], 0
0x180008743: lea       rax, [rip + 0x2e4666]
0x18000874a: lea       r10, [rip + 0x2e462f]
0x180008751: cmovne    r10, rax
0x180008755: mov       qword ptr [rsp + 0xc8], r10
0x18000875d: jmp       0x180008846
0x180008762: cmp       r11d, 0xa
0x180008766: jne       0x18000883a
0x18000876c: cmp       dword ptr [rsp + 0xe0], 0
0x180008774: lea       rax, [rip + 0x2e4845]
0x18000877b: lea       r10, [rip + 0x2e47f6]
0x180008782: cmovne    r10, rax
0x180008786: mov       qword ptr [rsp + 0xc8], r10
0x18000878e: jmp       0x180008846
0x180008793: cmp       r15d, 8
0x180008797: jne       0x18000883a
0x18000879d: cmp       r11d, 8
0x1800087a1: jne       0x1800087f5
0x1800087a3: cmp       dword ptr [rsp + 0xc0], 0
0x1800087ab: je        0x1800087d1
0x1800087ad: cmp       dword ptr [rsp + 0xe0], 0
0x1800087b5: lea       rax, [rip + 0x2e4864]
0x1800087bc: lea       r10, [rip + 0x2e4845]
0x1800087c3: cmovne    r10, rax
0x1800087c7: mov       qword ptr [rsp + 0xc8], r10
0x1800087cf: jmp       0x180008846
0x1800087d1: cmp       dword ptr [rsp + 0xe0], 0
0x1800087d9: lea       rax, [rip + 0x2e46a8]
0x1800087e0: lea       r10, [rip + 0x2e4671]
0x1800087e7: cmovne    r10, rax
0x1800087eb: mov       qword ptr [rsp + 0xc8], r10
0x1800087f3: jmp       0x180008846
0x1800087f5: cmp       r11d, 0xa
0x1800087f9: jne       0x18000883a
0x1800087fb: and       r10d, 0xfffffffb
0x1800087ff: cmp       r10d, 9
0x180008803: je        0x180008829
0x180008805: cmp       dword ptr [rsp + 0xe0], 0
0x18000880d: lea       rax, [rip + 0x2e483c]
0x180008814: lea       r10, [rip + 0x2e481d]
0x18000881b: cmovne    r10, rax
0x18000881f: mov       qword ptr [rsp + 0xc8], r10
0x180008827: jmp       0x180008846
0x180008829: lea       rax, [rip + 0x2e4838]
0x180008830: mov       qword ptr [rsp + 0xc8], rax
0x180008838: jmp       0x180008846
0x18000883a: mov       qword ptr [rsp + 0xc8], 0
0x180008846: mov       rax, qword ptr [rsp + 0xc8]
0x18000884e: cmp       rax, qword ptr [rbp + 0x788]
0x180008855: je        0x180008871
0x180008857: lea       rdx, [rip + 0x2e5312]
0x18000885e: mov       rcx, qword ptr [rbp + 0x13b8]
0x180008865: mov       qword ptr [rbp + 0x788], rax
0x18000886c: call      0x1800117b0
0x180008871: mov       r10, qword ptr [rbp + 0x30]
0x180008875: mov       rcx, rsp
0x180008878: mov       r8, qword ptr [rbx + 0x30]
0x18000887c: lea       rdx, [rbx + 8]
0x180008880: mov       rax, qword ptr [r10 + 0x50]
0x180008884: mov       r11, qword ptr [r10 + 0x98]
0x18000888b: mov       qword ptr [rcx + 0x20], rax
0x18000888f: lea       rax, [rip + 0x2e52da]
0x180008896: mov       qword ptr [rcx + 0x28], r11
0x18000889a: mov       qword ptr [rcx + 0x30], rax
0x18000889e: mov       r10d, dword ptr [rsp + 0xd8]
0x1800088a6: mov       dword ptr [rcx + 0x38], r10d
0x1800088aa: mov       dword ptr [rcx + 0x40], r10d
0x1800088ae: mov       qword ptr [rcx + 0x48], r13
0x1800088b2: mov       rcx, qword ptr [rbp + 0x13b8]
0x1800088b9: mov       r9, qword ptr [rsp + 0xc8]
0x1800088c1: call      0x1800217c0
0x1800088c6: cmp       r15d, 2
0x1800088ca: jne       0x18000897b
0x1800088d0: cmp       dword ptr [rsp + 0xf0], 0
0x1800088d8: jne       0x18000897b
0x1800088de: xor       r15d, r15d
0x1800088e1: cmp       dword ptr [rbx + 0xc], 0
0x1800088e5: jbe       0x180008973
0x1800088eb: mov       dword ptr [rbx], 0
0x1800088f1: mov       qword ptr [rsp + 0x670], r12
0x1800088f9: mov       rax, qword ptr [rbx + 0x18]
0x1800088fd: mov       r12d, r15d
0x180008900: mov       r8, qword ptr [rax + 0x18]
0x180008904: mov       rcx, qword ptr [r8 + r12*8]
0x180008908: lea       r10, [rcx + 0xf]
0x18000890c: and       r10, 0xfffffffffffffff0
0x180008910: cmp       rcx, r10
0x180008913: jne       0x180008921
0x180008915: mov       edx, 0x50
0x18000891a: call      0x1800ea910
0x18000891f: jmp       0x18000892b
0x180008921: mov       edx, 0x50
0x180008926: call      0x1800ea910
0x18000892b: mov       r10, qword ptr [rbx + 0x30]
0x18000892f: test      r10, r10
0x180008932: je        0x180008962
0x180008934: mov       rax, qword ptr [r10 + 0x10]
0x180008938: mov       edx, dword ptr [r10 + 0x20]
0x18000893c: mov       r8, qword ptr [rax + 0x18]
0x180008940: mov       rcx, qword ptr [r8 + r12*8]
0x180008944: lea       r10, [rcx + 0xf]
0x180008948: and       r10, 0xfffffffffffffff0
0x18000894c: cmp       rcx, r10
0x18000894f: jne       0x18000895d
0x180008951: test      dl, 3
0x180008954: jne       0x18000895d
0x180008956: call      0x1800ea910
0x18000895b: jmp       0x180008962
0x18000895d: call      0x1800ea910
0x180008962: inc       r15d
0x180008965: cmp       r15d, dword ptr [rbx + 0xc]
0x180008969: jb        0x1800088f9
0x18000896b: mov       r12, qword ptr [rsp + 0x670]
0x180008973: mov       dword ptr [rbx], 8
0x180008979: jmp       0x180008981
0x18000897b: mov       dword ptr [rbx], 0
0x180008981: mov       eax, dword ptr [rsp + 0xc0]
0x180008988: mov       dword ptr [rbx + 4], eax
0x18000898b: mov       rdx, qword ptr [rbp + 0xd0]
0x180008992: jmp       0x18000899f
0x180008994: mov       qword ptr [rbp + 0x788], 0
