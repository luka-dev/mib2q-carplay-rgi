0x180004190: push      rsi
0x180004191: push      rdi
0x180004192: push      r12
0x180004194: sub       rsp, 0x20
0x180004198: mov       rdi, rcx
0x18000419b: xor       esi, esi
0x18000419d: test      edx, edx
0x18000419f: setne     sil
0x1800041a3: lea       r12, [rdi + 0x38]
0x1800041a7: mov       rcx, r12
0x1800041aa: call      0x180010920
0x1800041af: cmp       esi, dword ptr [rdi + 0x780]
0x1800041b5: je        0x1800041c7
0x1800041b7: mov       dword ptr [rdi + 0x780], esi
0x1800041bd: mov       dword ptr [rdi + 0x13b0], 1
0x1800041c7: mov       rcx, r12
0x1800041ca: add       rsp, 0x20
0x1800041ce: pop       r12
0x1800041d0: pop       rdi
0x1800041d1: pop       rsi
0x1800041d2: jmp       0x180010960
